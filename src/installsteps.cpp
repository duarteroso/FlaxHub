#include "installsteps.h"

#include "installlayout.h"

#include <QDir>

InstallSteps::InstallSteps(QObject *parent)
    : QObject(parent)
{
    connect(&m_runner, &InstallRunner::logReceived, this, [this](const QString &text) {
        emit logReceived(m_version, text);
    });
    connect(&m_runner, &InstallRunner::progressChanged, this, [this](double fraction) {
        emit progressChanged(m_version, fraction);
    });
    connect(&m_runner, &InstallRunner::finished, this, [this]() {
        emit finished(m_version);
    });
    connect(&m_runner, &InstallRunner::failed, this, [this](const QString &message) {
        emit failed(m_version, message);
    });
}

// --depth 1 fetches only that tagged commit, not the history. If git fails, it removes the folder it made, so a failed
// clone leaves nothing behind.
void InstallSteps::clone(const QString &version, const QString &path)
{
    // The "--" ends the options, so the repository and the folder are never read as options.
    const QStringList arguments = {"clone", "--progress", "--depth", "1", "--branch", version, "--",
                                   "https://github.com/FlaxEngine/FlaxEngine.git", path};
    start(version, QStringLiteral("git"), arguments, {});
}

void InstallSteps::generate(const QString &version, const QString &path)
{
    const QString script = QDir(path).filePath(InstallLayout::generateScriptName());
    start(version, script, {}, path);
}

// The generate step only builds the C# bindings, so this is the step that compiles FlaxEditor.
void InstallSteps::build(const QString &version, const QString &path)
{
    // Without -log the tool writes its output to a log file only, so there would be nothing for the log to show.
    const QStringList arguments = {
        QStringLiteral("-build"),
        QStringLiteral("-log"),
        QStringLiteral("-platform=%1").arg(InstallLayout::buildPlatform()),
        QStringLiteral("-arch=%1").arg(InstallLayout::buildArchitecture()),
        QStringLiteral("-configuration=%1").arg(InstallLayout::buildConfiguration()),
        QStringLiteral("-buildtargets=FlaxEditor"),
    };
    const QString tool = QDir(path).filePath(InstallLayout::buildToolRelativePath());
    start(version, tool, arguments, path);
}

void InstallSteps::start(const QString &version, const QString &program, const QStringList &arguments,
                         const QString &workingDirectory)
{
    m_version = version;
    m_runner.start(program, arguments, workingDirectory);
}
