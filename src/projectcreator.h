#ifndef PROJECTCREATOR_H
#define PROJECTCREATOR_H

#include <QByteArray>
#include <QObject>
#include <QPointer>
#include <QString>

// Runs the engine's editor once to make a new project, and reports whether the project file was written. A run that
// fails removes the folder the editor made. ProjectListModel starts it and keeps the list. A creator deletes itself
// after it reports, so the caller must not keep a pointer to it.
class QProcess;

class ProjectCreator : public QObject
{
    Q_OBJECT

public:
    explicit ProjectCreator(QObject *parent = nullptr);
    ~ProjectCreator() override;

    // Starts editor to make the project in projectDir, which the editor creates when it is missing. The run succeeds when
    // the editor exits without an error and projectFile exists afterwards. The result comes through a signal.
    void start(const QString &editor, const QString &projectDir, const QString &projectFile);

    // Kills the editor and removes the folder this run made. Nothing is reported. Quitting the app calls it, and so does
    // the destructor.
    void stop();

signals:
    void created();
    // The run failed. The message is for display, and it ends with the last line the editor printed, when there is one.
    void failed(const QString &message);

private:
    void succeed();
    void fail(const QString &message);
    // Removes the folder when this run made it. Returns false when the folder is still there afterwards.
    bool removeMadeFolder();

    QString m_projectDir;
    QPointer<QProcess> m_process;
    // Whether the folder was there before the run. Only a folder the editor made is removed on failure.
    bool m_existed = false;
    // Everything the editor printed during the run.
    QByteArray m_output;
};

#endif
