#include "installvalidator.h"
#include "installlayout.h"

#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QStringList>

namespace {

// A git command on a local checkout finishes in milliseconds. The limit only stops a hung git from stalling startup.
constexpr int gitTimeoutMs = 10000;

// Runs git against the folder's own .git. Naming the git dir stops git from finding a parent repository when the
// folder's .git is broken. Returns the output, or an empty string when git fails or does not finish. The error
// output is dropped, since nothing reads it and a full pipe would stall git until the timeout.
QString runGit(const QString &gitDir, const QStringList &arguments)
{
    QProcess git;
    git.setStandardErrorFile(QProcess::nullDevice());
    git.start(QStringLiteral("git"), QStringList{QStringLiteral("--git-dir=%1").arg(gitDir)} + arguments);
    if (!git.waitForStarted())
        return {};
    if (!git.waitForFinished(gitTimeoutMs)) {
        git.kill();
        return {};
    }
    if (git.exitStatus() != QProcess::NormalExit || git.exitCode() != 0)
        return {};

    return QString::fromUtf8(git.readAllStandardOutput()).trimmed();
}

// A clone writes .git/index last, once every file is checked out, and git holds a lock file while it checks out.
// So a clone that was cut short has no index, or a lock left behind. A clone that was cut short while it fetched
// has no tag and an unresolved HEAD, which the commit comparison catches. The folder is complete when HEAD is the
// commit of the version's tag.
bool hasCompleteGitCheckout(const QString &path, const QString &version)
{
    const QString gitDir = QDir(path).filePath(QStringLiteral(".git"));
    if (!QFileInfo::exists(QDir(gitDir).filePath(QStringLiteral("index"))))
        return false;
    if (!QDir(gitDir).entryList({QStringLiteral("*.lock")}, QDir::Files).isEmpty())
        return false;

    const QString head = runGit(gitDir, {QStringLiteral("rev-parse"), QStringLiteral("--verify"), QStringLiteral("-q"),
                                         QStringLiteral("HEAD^{commit}")});
    if (head.isEmpty())
        return false;

    // Annotated tags point at a tag object, so the tag is peeled to its commit before the comparison.
    const QString tag = runGit(gitDir, {QStringLiteral("rev-parse"), QStringLiteral("--verify"), QStringLiteral("-q"),
                                        QStringLiteral("refs/tags/%1^{commit}").arg(version)});
    return tag == head;
}

// The generate script writes the build tool, and the bindings build writes the C# files of each configuration. The
// editor needs the Release files, so those are the ones checked.
bool hasGeneratedFiles(const QString &path)
{
    const QDir root(path);
    const QString editorDirectory = InstallLayout::editorDirectoryRelativePath();
    const QStringList generated = {
        InstallLayout::buildToolRelativePath(),
        editorDirectory + QStringLiteral("/FlaxEditor.Build.json"),
        editorDirectory + QStringLiteral("/FlaxEngine.CSharp.dll"),
    };
    for (const QString &file : generated) {
        if (!QFileInfo(root.filePath(file)).isFile())
            return false;
    }

    return true;
}

// The editor is a file. On Linux and macOS it must also be executable, since a file without the executable bit
// cannot be started.
bool hasReleaseEditor(const QString &path)
{
    const QFileInfo editor(QDir(path).filePath(InstallLayout::editorRelativePath()));
    if (!editor.isFile())
        return false;

#if defined(Q_OS_WIN)
    return true;
#else
    return editor.isExecutable();
#endif
}

} // namespace

namespace InstallValidator {

QList<Failure> validate(const QString &path, const QString &version)
{
    QList<Failure> failures;
    if (!QFileInfo(path).isDir()) {
        failures.append(Failure::PathMissing);
    } else if (QDir(path).isEmpty(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System)) {
        // Hidden entries count, so a folder that holds only a .git directory is not empty.
        failures.append(Failure::FolderEmpty);
    }

    if (!hasCompleteGitCheckout(path, version))
        failures.append(Failure::NoGitCheckout);
    if (!hasGeneratedFiles(path))
        failures.append(Failure::NoGenerateOutput);
    if (!hasReleaseEditor(path))
        failures.append(Failure::NoBinary);

    return failures;
}

QString describe(Failure failure)
{
    switch (failure) {
    case Failure::PathMissing: return QStringLiteral("Folder not found");
    case Failure::FolderEmpty: return QStringLiteral("Folder is empty");
    case Failure::NoGitCheckout: return QStringLiteral("Git checkout is incomplete or not at this version");
    case Failure::NoGenerateOutput: return QStringLiteral("Generated project files are missing");
    case Failure::NoBinary: return QStringLiteral("Release editor is missing or not executable");
    }

    return {};
}

QString describe(const QList<Failure> &failures)
{
    QStringList reasons;
    for (const Failure failure : failures)
        reasons.append(describe(failure));

    return reasons.join(QLatin1Char('\n'));
}

} // namespace InstallValidator
