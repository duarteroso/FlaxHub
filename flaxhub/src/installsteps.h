#ifndef INSTALLSTEPS_H
#define INSTALLSTEPS_H

#include "installrunner.h"

#include <QObject>
#include <QString>
#include <QStringList>

// The three steps of an install as commands: the clone, the engine's generate script and its build tool. Each one runs
// in the InstallRunner, and what the runner reports comes back with the version of the install the step is for. The
// model decides which step runs next.
class InstallSteps : public QObject
{
    Q_OBJECT

public:
    explicit InstallSteps(QObject *parent = nullptr);

    // Clones the release tag into path, which must be missing or empty.
    void clone(const QString &version, const QString &path);
    // Runs the engine's generate script in the cloned folder.
    void generate(const QString &version, const QString &path);
    // Builds the editor in the Release configuration.
    void build(const QString &version, const QString &path);

signals:
    void logReceived(const QString &version, const QString &text);
    void progressChanged(const QString &version, double fraction);
    void finished(const QString &version);
    void failed(const QString &version, const QString &message);

private:
    void start(const QString &version, const QString &program, const QStringList &arguments,
               const QString &workingDirectory);

    InstallRunner m_runner;
    // The install whose step the runner is running, or the last one it ran.
    QString m_version;
};

#endif
