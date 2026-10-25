#include "projectcreator.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QStringList>

ProjectCreator::ProjectCreator(QObject *parent)
    : QObject(parent)
{
    // Quitting the app stops a running editor before anything is destroyed. The destructor does the same for a creator
    // that is destroyed earlier.
    if (QCoreApplication *app = QCoreApplication::instance())
        connect(app, &QCoreApplication::aboutToQuit, this, &ProjectCreator::stop);
}

ProjectCreator::~ProjectCreator()
{
    stop();
}

void ProjectCreator::stop()
{
    if (!m_process)
        return;

    QProcess *process = m_process;
    m_process = nullptr;
    process->blockSignals(true);
    process->kill();
    process->waitForFinished(5000);
    removeMadeFolder();
}

void ProjectCreator::start(const QString &editor, const QString &projectDir, const QString &projectFile)
{
    m_projectDir = projectDir;
    // The editor makes the folder itself, so the folder is only removed on failure when this run was the one to make it.
    m_existed = QFileInfo::exists(projectDir);
    m_output.clear();

    // On Linux and macOS the engine joins its arguments with spaces and reads the quotes itself (Main/MainUtil.h), so the
    // quotes stay in the argument. The engine then reads a path with spaces up to the closing quote, as the editor does
    // for itself.
    const QStringList arguments = {
        QStringLiteral("-new"),
        QStringLiteral("-exit"),
        QStringLiteral("-headless"),
        QStringLiteral("-std"),
        QStringLiteral("-project=\"%1\"").arg(projectDir),
    };

    // The output is kept, so a failure can say what the editor printed last.
    QProcess *process = new QProcess(this);
    m_process = process;
    process->setProcessChannelMode(QProcess::MergedChannels);
    process->setStandardInputFile(QProcess::nullDevice());

    connect(process, &QProcess::readyReadStandardOutput, this, [this, process]() {
        m_output += process->readAllStandardOutput();
    });
    connect(process, &QProcess::finished, this, [this, process, projectFile](int exitCode, QProcess::ExitStatus exitStatus) {
        m_output += process->readAllStandardOutput();
        m_process = nullptr;

        if (exitStatus == QProcess::CrashExit)
            fail(QStringLiteral("FlaxEditor crashed."));
        else if (exitCode != 0)
            fail(QStringLiteral("FlaxEditor exited with code %1.").arg(exitCode));
        else if (!QFileInfo::exists(projectFile))
            fail(QStringLiteral("FlaxEditor did not create %1.").arg(projectFile));
        else
            succeed();
    });
    connect(process, &QProcess::errorOccurred, this, [this, process](QProcess::ProcessError error) {
        // Any other error is followed by finished, which reports it.
        if (error != QProcess::FailedToStart)
            return;

        m_process = nullptr;
        fail(QStringLiteral("Could not start FlaxEditor: ") + process->errorString());
    });

    process->start(editor, arguments);
}

void ProjectCreator::succeed()
{
    emit created();
    deleteLater();
}

bool ProjectCreator::removeMadeFolder()
{
    // The check is for a folder that exists now, so an empty project path can never remove the current directory.
    if (m_existed || !QFileInfo(m_projectDir).isDir())
        return true;

    QDir(m_projectDir).removeRecursively();
    return !QFileInfo::exists(m_projectDir);
}

void ProjectCreator::fail(const QString &message)
{
    // A folder the editor made is removed again, so a retry is not blocked by it.
    const bool removed = removeMadeFolder();

    // The last line the editor printed usually says why it failed, so it follows the message.
    const QStringList lines = QString::fromUtf8(m_output).split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    const QString reason = lines.isEmpty() ? QString() : lines.last().trimmed();

    QString full = reason.isEmpty() ? message : message + QLatin1Char(' ') + reason;
    if (!removed)
        full += QStringLiteral(" The folder %1 could not be removed.").arg(m_projectDir);

    emit failed(full);
    deleteLater();
}
