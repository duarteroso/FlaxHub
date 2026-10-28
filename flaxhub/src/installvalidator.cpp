#include "installvalidator.h"
#include "installlayout.h"
#include "installrules.h"

#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QStringList>

namespace {

// A git command on a local checkout finishes in milliseconds. The limit only stops a hung git from stalling startup.
constexpr int gitTimeoutMs = 10000;

// What one git command came to. Unavailable means git did not run to an exit: it is not installed, could not start,
// was killed or did not finish in time. Failed means git ran and reported an error.
enum class GitOutcome {
    Succeeded,
    Failed,
    Unavailable
};

// Runs git against the folder's own .git. Naming the git dir stops git from finding a parent repository when the
// folder's .git is broken. On success the output is stored in output. The error output is dropped, since nothing
// reads it and a full pipe would stall git until the timeout.
GitOutcome runGit(const QString &gitDir, const QStringList &arguments, QString &output)
{
    QProcess git;
    git.setStandardErrorFile(QProcess::nullDevice());
    git.start(QStringLiteral("git"), QStringList{QStringLiteral("--git-dir=%1").arg(gitDir)} + arguments);
    if (!git.waitForStarted())
        return GitOutcome::Unavailable;
    if (!git.waitForFinished(gitTimeoutMs)) {
        git.kill();
        return GitOutcome::Unavailable;
    }
    if (git.exitStatus() != QProcess::NormalExit)
        return GitOutcome::Unavailable;
    if (git.exitCode() != 0)
        return GitOutcome::Failed;

    output = QString::fromUtf8(git.readAllStandardOutput()).trimmed();
    return GitOutcome::Succeeded;
}

// The result of the git check on a folder.
enum class CheckoutState {
    Complete,
    Incomplete,
    GitUnavailable
};

// A clone writes .git/index last, once every file is checked out, and git holds a lock file while it checks out.
// So a clone that was cut short has no index, or a lock left behind. A clone that was cut short while it fetched
// has no tag and an unresolved HEAD, which the commit comparison catches. The folder is complete when HEAD is the
// commit of the version's tag.
CheckoutState checkGitCheckout(const QString &path, const QString &version)
{
    const QString gitDir = QDir(path).filePath(QStringLiteral(".git"));
    if (!QFileInfo::exists(QDir(gitDir).filePath(QStringLiteral("index"))))
        return CheckoutState::Incomplete;
    if (!QDir(gitDir).entryList({QStringLiteral("*.lock")}, QDir::Files).isEmpty())
        return CheckoutState::Incomplete;

    QString head;
    GitOutcome outcome = runGit(gitDir, {QStringLiteral("rev-parse"), QStringLiteral("--verify"), QStringLiteral("-q"),
                                         QStringLiteral("HEAD^{commit}")}, head);
    if (outcome == GitOutcome::Unavailable)
        return CheckoutState::GitUnavailable;
    if (outcome == GitOutcome::Failed || head.isEmpty())
        return CheckoutState::Incomplete;

    // Annotated tags point at a tag object, so the tag is peeled to its commit before the comparison.
    QString tag;
    outcome = runGit(gitDir, {QStringLiteral("rev-parse"), QStringLiteral("--verify"), QStringLiteral("-q"),
                              QStringLiteral("refs/tags/%1^{commit}").arg(version)}, tag);
    if (outcome == GitOutcome::Unavailable)
        return CheckoutState::GitUnavailable;

    return outcome == GitOutcome::Succeeded && tag == head ? CheckoutState::Complete : CheckoutState::Incomplete;
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

} // namespace

namespace InstallValidator {

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

QList<Failure> validate(const QString &path, const QString &version)
{
    QList<Failure> failures;
    if (!QFileInfo(path).isDir()) {
        failures.append(Failure::PathMissing);
    } else if (InstallRules::isEmptyFolder(path)) {
        failures.append(Failure::FolderEmpty);
    }

    switch (checkGitCheckout(path, version)) {
    case CheckoutState::Complete: break;
    case CheckoutState::Incomplete: failures.append(Failure::NoGitCheckout); break;
    case CheckoutState::GitUnavailable: failures.append(Failure::GitUnavailable); break;
    }
    if (!hasGeneratedFiles(path))
        failures.append(Failure::NoGenerateOutput);
    if (!hasReleaseEditor(path))
        failures.append(Failure::NoBinary);

    return failures;
}

bool hasCompleteCheckout(const QString &path, const QString &version)
{
    return checkGitCheckout(path, version) == CheckoutState::Complete;
}

QString describe(Failure failure)
{
    switch (failure) {
    case Failure::PathMissing: return QStringLiteral("Folder not found");
    case Failure::FolderEmpty: return QStringLiteral("Folder is empty");
    case Failure::NoGitCheckout: return QStringLiteral("Git checkout is incomplete or not at this version");
    case Failure::GitUnavailable: return QStringLiteral("Git is not available, so the checkout could not be checked");
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
