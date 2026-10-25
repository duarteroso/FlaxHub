#ifndef INSTALLRUNNER_H
#define INSTALLRUNNER_H

#include <QByteArray>
#include <QObject>
#include <QString>
#include <QStringList>

class QProcess;

// Runs one step of an install (the git clone, the engine's generate script or its build tool) in a process, and
// reports what the process prints. The output is merged, and the last of it is reported before the step ends. Only
// one step runs at a time. The owner decides what the next step is.
class InstallRunner : public QObject
{
    Q_OBJECT

public:
    explicit InstallRunner(QObject *parent = nullptr);
    // Quitting the app, or destroying the runner, stops a running step and the processes it started.
    ~InstallRunner() override;

    // Starts program with arguments in workingDirectory, or in the current folder when it is empty. Does nothing while
    // a step is running, since the owner must wait for the step to end first.
    void start(const QString &program, const QStringList &arguments, const QString &workingDirectory);

signals:
    // Text the step printed. A carriage return stays in it, so the owner can tell a progress line that the next output replaces.
    void logReceived(const QString &text);
    // The clone's progress, from 0 to 1, after an output that has a progress line. Only git's clone reports one.
    void progressChanged(double fraction);
    // The step exited with 0.
    void finished();
    // The step did not succeed: it exited with another code, crashed or did not start. The message says why.
    void failed(const QString &message);

private:
    void emitOutput(const QByteArray &output, bool flush);
    // Kills the step's process and the processes it started, without reporting anything. On Unix the step has its own
    // session, so its children are in the same process group. On Windows taskkill walks the process tree instead.
    void stopStep();

    QProcess *m_process = nullptr;
    // Output after the last line break that has not been reported yet, so a read that ends inside a character or a
    // progress figure is completed by the next one.
    QByteArray m_pendingOutput;
};

#endif
