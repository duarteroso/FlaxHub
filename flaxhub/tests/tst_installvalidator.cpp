#include "installvalidator.h"
#include "installlayout.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QProcessEnvironment>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

using Failure = InstallValidator::Failure;

namespace {

// Sets PATH for one test, and puts the old value back when the test ends.
class ScopedPath
{
public:
    explicit ScopedPath(const QByteArray &path)
        : m_saved(qgetenv("PATH"))
    {
        qputenv("PATH", path);
    }

    ~ScopedPath()
    {
        qputenv("PATH", m_saved);
    }

private:
    QByteArray m_saved;
};

bool writeFile(const QString &path, const QByteArray &content, bool executable = false)
{
    if (!QDir().mkpath(QFileInfo(path).absolutePath()))
        return false;

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        return false;
    file.write(content);
    file.close();

    if (executable)
        return file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner
                                   | QFileDevice::ReadGroup | QFileDevice::ExeGroup | QFileDevice::ReadOther
                                   | QFileDevice::ExeOther);
    return true;
}

// Runs git in dir with a fixed identity, so a commit never depends on the user's configuration.
bool runGit(const QString &dir, const QStringList &arguments)
{
    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    environment.insert(QStringLiteral("GIT_AUTHOR_NAME"), QStringLiteral("FlaxHub test"));
    environment.insert(QStringLiteral("GIT_AUTHOR_EMAIL"), QStringLiteral("test@example.invalid"));
    environment.insert(QStringLiteral("GIT_COMMITTER_NAME"), QStringLiteral("FlaxHub test"));
    environment.insert(QStringLiteral("GIT_COMMITTER_EMAIL"), QStringLiteral("test@example.invalid"));

    QProcess git;
    git.setProcessEnvironment(environment);
    git.setWorkingDirectory(dir);
    git.start(QStringLiteral("git"), arguments);
    return git.waitForStarted() && git.waitForFinished(30000) && git.exitStatus() == QProcess::NormalExit
        && git.exitCode() == 0;
}

// Makes dir a checkout of the engine at version: a repository with one commit that is tagged, holding the files the
// generate and build steps make. The editor is written only when withEditor is set.
bool makeCheckout(const QString &dir, const QString &version, bool withEditor)
{
    const QString editorDirectory = QDir(dir).filePath(InstallLayout::editorDirectoryRelativePath());
    if (!writeFile(QDir(dir).filePath(InstallLayout::buildToolRelativePath()), "#!/bin/sh\n", true))
        return false;
    if (!writeFile(editorDirectory + QStringLiteral("/FlaxEditor.Build.json"), "{}\n"))
        return false;
    if (!writeFile(editorDirectory + QStringLiteral("/FlaxEngine.CSharp.dll"), "dll\n"))
        return false;
    if (withEditor && !writeFile(QDir(dir).filePath(InstallLayout::editorRelativePath()), "#!/bin/sh\n", true))
        return false;

    return runGit(dir, {QStringLiteral("init"), QStringLiteral("-q")})
        && runGit(dir, {QStringLiteral("add"), QStringLiteral("-A")})
        && runGit(dir, {QStringLiteral("-c"), QStringLiteral("commit.gpgsign=false"), QStringLiteral("commit"),
                        QStringLiteral("-q"), QStringLiteral("-m"), QStringLiteral("Engine")})
        && runGit(dir, {QStringLiteral("tag"), version});
}

} // namespace

// Every folder here is temporary. The folders without a checkout have no .git/index, so validate never starts git.
// The checkout tests start git on a repository made for the test, and skip when git is not installed.
class InstallValidatorTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();

    void validateMissingFolder();
    void validateEmptyFolder();
    void validateFolderWithFiles();
    void validateFolderWithOnlyHiddenGit();
    void validateGitMissingFromPath();
    void validateGitThatFails();
    void validateCompleteCheckout();
    void describeEachFailure();
    void describeJoinsReasonsOnePerLine();

private:
    QTemporaryDir m_home;
    QByteArray m_savedHome;
    bool m_hadHome = false;
    QByteArray m_savedUserProfile;
    bool m_hadUserProfile = false;
};

// HOME points at a temporary folder for the whole run, so git never reads the user's configuration.
void InstallValidatorTest::initTestCase()
{
    QVERIFY(m_home.isValid());
    m_hadHome = qEnvironmentVariableIsSet("HOME");
    m_savedHome = qgetenv("HOME");
    m_hadUserProfile = qEnvironmentVariableIsSet("USERPROFILE");
    m_savedUserProfile = qgetenv("USERPROFILE");
    qputenv("HOME", m_home.path().toLocal8Bit());
    qputenv("USERPROFILE", m_home.path().toLocal8Bit());
}

void InstallValidatorTest::cleanupTestCase()
{
    if (m_hadHome)
        qputenv("HOME", m_savedHome);
    else
        qunsetenv("HOME");

    if (m_hadUserProfile)
        qputenv("USERPROFILE", m_savedUserProfile);
    else
        qunsetenv("USERPROFILE");
}

// The folder is not created, so the folder check fails, and the checks inside it find nothing.
void InstallValidatorTest::validateMissingFolder()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());

    const QList<Failure> expected = {Failure::PathMissing, Failure::NoGitCheckout, Failure::NoGenerateOutput,
                                     Failure::NoBinary};
    QCOMPARE(InstallValidator::validate(root.filePath("missing"), QStringLiteral("1.10")), expected);
}

// An empty folder is not a missing one, so FolderEmpty stands in for PathMissing.
void InstallValidatorTest::validateEmptyFolder()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    QVERIFY(QDir().mkpath(root.filePath("empty")));

    const QList<Failure> expected = {Failure::FolderEmpty, Failure::NoGitCheckout, Failure::NoGenerateOutput,
                                     Failure::NoBinary};
    QCOMPARE(InstallValidator::validate(root.filePath("empty"), QStringLiteral("1.10")), expected);
}

void InstallValidatorTest::validateFolderWithFiles()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    QVERIFY(QDir().mkpath(root.filePath("files")));
    QFile readme(root.filePath("files/readme.txt"));
    QVERIFY(readme.open(QIODevice::WriteOnly));
    readme.close();

    const QList<Failure> expected = {Failure::NoGitCheckout, Failure::NoGenerateOutput, Failure::NoBinary};
    QCOMPARE(InstallValidator::validate(root.filePath("files"), QStringLiteral("1.10")), expected);
}

// A hidden .git counts as content, so the folder is not empty. It still has no checkout.
void InstallValidatorTest::validateFolderWithOnlyHiddenGit()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    QVERIFY(QDir().mkpath(root.filePath("hidden/.git")));

    const QList<Failure> expected = {Failure::NoGitCheckout, Failure::NoGenerateOutput, Failure::NoBinary};
    QCOMPARE(InstallValidator::validate(root.filePath("hidden"), QStringLiteral("1.10")), expected);
}

// The folder has an index, so git is asked, and PATH holds no git. The folder may be a complete checkout, so the
// result is GitUnavailable, not NoGitCheckout, which would offer to clear it.
void InstallValidatorTest::validateGitMissingFromPath()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    QVERIFY(writeFile(root.filePath("engine/.git/index"), "index"));
    QVERIFY(QDir().mkpath(root.filePath("no-git")));

    const ScopedPath path(QFile::encodeName(root.filePath("no-git")));
    const QList<Failure> expected = {Failure::GitUnavailable, Failure::NoGenerateOutput, Failure::NoBinary};
    QCOMPARE(InstallValidator::validate(root.filePath("engine"), QStringLiteral("1.10")), expected);
    QVERIFY(!InstallValidator::hasCompleteCheckout(root.filePath("engine"), QStringLiteral("1.10")));
}

// A git that runs and exits with an error is a checkout that fails the check, so it is NoGitCheckout.
void InstallValidatorTest::validateGitThatFails()
{
#if defined(Q_OS_WIN)
    QSKIP("The fake git is a POSIX shell script");
#else
    QTemporaryDir root;
    QVERIFY(root.isValid());
    QVERIFY(writeFile(root.filePath("engine/.git/index"), "index"));
    QVERIFY(writeFile(root.filePath("bin/git"), "#!/bin/sh\nexit 1\n", true));

    const ScopedPath path(QFile::encodeName(root.filePath("bin")));
    const QList<Failure> expected = {Failure::NoGitCheckout, Failure::NoGenerateOutput, Failure::NoBinary};
    QCOMPARE(InstallValidator::validate(root.filePath("engine"), QStringLiteral("1.10")), expected);
#endif
}

// A real checkout of the tag, with its editor, passes every check.
void InstallValidatorTest::validateCompleteCheckout()
{
    if (QStandardPaths::findExecutable(QStringLiteral("git")).isEmpty())
        QSKIP("git is not installed");

    QTemporaryDir root;
    QVERIFY(root.isValid());
    const QString engine = root.filePath("engine");
    QVERIFY(QDir().mkpath(engine));
    QVERIFY(makeCheckout(engine, QStringLiteral("1.10"), true));

    QCOMPARE(InstallValidator::validate(engine, QStringLiteral("1.10")), QList<Failure>());
    QVERIFY(InstallValidator::hasCompleteCheckout(engine, QStringLiteral("1.10")));
}

void InstallValidatorTest::describeEachFailure()
{
    QCOMPARE(InstallValidator::describe(Failure::PathMissing), QStringLiteral("Folder not found"));
    QCOMPARE(InstallValidator::describe(Failure::FolderEmpty), QStringLiteral("Folder is empty"));
    QCOMPARE(InstallValidator::describe(Failure::NoGitCheckout),
             QStringLiteral("Git checkout is incomplete or not at this version"));
    QCOMPARE(InstallValidator::describe(Failure::GitUnavailable),
             QStringLiteral("Git is not available, so the checkout could not be checked"));
    QCOMPARE(InstallValidator::describe(Failure::NoGenerateOutput),
             QStringLiteral("Generated project files are missing"));
    QCOMPARE(InstallValidator::describe(Failure::NoBinary),
             QStringLiteral("Release editor is missing or not executable"));
}

void InstallValidatorTest::describeJoinsReasonsOnePerLine()
{
    QVERIFY(InstallValidator::describe(QList<Failure>()).isEmpty());

    const QList<Failure> failures = {Failure::FolderEmpty, Failure::NoBinary};
    QCOMPARE(InstallValidator::describe(failures),
             InstallValidator::describe(Failure::FolderEmpty) + QLatin1Char('\n') + InstallValidator::describe(Failure::NoBinary));
}

QTEST_APPLESS_MAIN(InstallValidatorTest)

#include "tst_installvalidator.moc"
