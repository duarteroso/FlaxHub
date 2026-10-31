#include "englishcatalogue.h"
#include "installlayout.h"
#include "installlistmodel.h"
#include "installrules.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QProcessEnvironment>
#include <QSettings>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

namespace {

struct SavedInstall {
    QString version;
    QString path;
    bool isDefault;
};

// Writes the installs the way InstallListModel saves them, so the model under test reads what it would have written.
void writeInstalls(QSettings &settings, const QList<SavedInstall> &installs)
{
    settings.beginWriteArray(QStringLiteral("Installs"));
    for (int i = 0; i < installs.size(); ++i) {
        settings.setArrayIndex(i);
        settings.setValue(QStringLiteral("version"), installs.at(i).version);
        settings.setValue(QStringLiteral("path"), installs.at(i).path);
        settings.setValue(QStringLiteral("isDefault"), installs.at(i).isDefault);
    }
    settings.endArray();
}

// Whether every check has returned. Until then a row shows Checking.
bool checksFinished(const InstallListModel &model)
{
    for (int row = 0; row < model.rowCount(); ++row) {
        if (model.data(model.index(row), InstallListModel::StateRole).toInt() == InstallListModel::Checking)
            return false;
    }

    return true;
}

// The row of the install of version, or -1 when there is none.
int rowOf(const InstallListModel &model, const QString &version)
{
    for (int row = 0; row < model.rowCount(); ++row) {
        if (model.data(model.index(row), InstallListModel::VersionRole).toString() == version)
            return row;
    }

    return -1;
}

int stateOf(const InstallListModel &model, const QString &version)
{
    const int row = rowOf(model, version);
    return row < 0 ? -1 : model.data(model.index(row), InstallListModel::StateRole).toInt();
}

// Writes a file, and makes it executable when asked.
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

// Makes dir a checkout of the engine at tag 1.10. Its build tool runs buildScript, and its editor is written only when
// withEditor is set, so a checkout without it fails the editor check and nothing else.
bool makeCheckout(const QString &dir, const QByteArray &buildScript, bool withEditor)
{
    const QString editorDirectory = QDir(dir).filePath(InstallLayout::editorDirectoryRelativePath());
    if (!writeFile(QDir(dir).filePath(InstallLayout::buildToolRelativePath()), buildScript, true))
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
        && runGit(dir, {QStringLiteral("tag"), QStringLiteral("1.10")});
}

// The checkout tests run git and POSIX shell scripts, so they are skipped where either is missing.
bool checkoutTestsSupported()
{
#if defined(Q_OS_WIN)
    return false;
#else
    return !QStandardPaths::findExecutable(QStringLiteral("git")).isEmpty();
#endif
}

// Sends GitHub's FlaxEngine remote to a local repository while it lives, so a clone never reaches the network. git reads
// the rewrite from the GIT_CONFIG_* variables in its environment, which the models' git processes inherit. Destroying it
// puts back the variables as they were.
class RedirectedRemote
{
public:
    explicit RedirectedRemote(const QString &repository)
    {
        const QString key = QStringLiteral("url.file://%1.insteadOf").arg(repository);
        const QByteArray values[kCount] = {"1", key.toUtf8(), "https://github.com/FlaxEngine/FlaxEngine.git"};
        for (int i = 0; i < kCount; ++i) {
            m_wasSet[i] = qEnvironmentVariableIsSet(kNames[i]);
            m_saved[i] = qgetenv(kNames[i]);
            qputenv(kNames[i], values[i]);
        }
    }

    ~RedirectedRemote()
    {
        for (int i = 0; i < kCount; ++i) {
            if (m_wasSet[i])
                qputenv(kNames[i], m_saved[i]);
            else
                qunsetenv(kNames[i]);
        }
    }

    RedirectedRemote(const RedirectedRemote &) = delete;
    RedirectedRemote &operator=(const RedirectedRemote &) = delete;

private:
    static constexpr int kCount = 3;
    static constexpr const char *kNames[kCount] = {"GIT_CONFIG_COUNT", "GIT_CONFIG_KEY_0", "GIT_CONFIG_VALUE_0"};
    bool m_wasSet[kCount] = {};
    QByteArray m_saved[kCount];
};

} // namespace

// The installs are saved in a temporary settings file, so the user's settings are not touched. The tests that need no
// engine folder use folders that are missing, empty or hold only a file, so each check fails without running git. The
// tests with a checkout make a git repository in a temporary folder, and their build script is a shell script.
class InstallListModelTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();

    void checkingIsTrueWhileChecksRun();
    void refusesRelativePaths();
    void interruptedInstallIsKeptAcrossRestart();
    void failedFixIsKeptAndRetryNeedsFullPath();
    void fixClonesMissingFolderAgain();
    void uninstallRemovesMarkedCheckout();
    void uninstallKeepsFolderThatIsNoLongerACheckout();
    void uninstallRemovesMarkedFolderOfInvalidInstall();
    void uninstallRemovesUnmarkedFolderOfInvalidInstall();
    void uninstallRemovesMissingFolderOfInvalidInstall();
    void uninstallRefusesUnsafeInvalidFolder();
    void fixAsksBeforeClearingFolderWithFiles();
    void logReplacesProgressLineAcrossChunks();

private:
    QTemporaryDir m_home;
    QTranslator m_translator;
    QByteArray m_savedHome;
    bool m_hadHome = false;
    QByteArray m_savedUserProfile;
    bool m_hadUserProfile = false;
};

// HOME points at a temporary folder for the whole run, so git never reads the user's configuration.
void InstallListModelTest::initTestCase()
{
    QVERIFY(m_home.isValid());
    QVERIFY(installEnglishCatalogue(m_translator));
    m_hadHome = qEnvironmentVariableIsSet("HOME");
    m_savedHome = qgetenv("HOME");
    m_hadUserProfile = qEnvironmentVariableIsSet("USERPROFILE");
    m_savedUserProfile = qgetenv("USERPROFILE");
    qputenv("HOME", m_home.path().toLocal8Bit());
    qputenv("USERPROFILE", m_home.path().toLocal8Bit());
}

void InstallListModelTest::cleanupTestCase()
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

// The checks run on worker threads and their results are delivered by the event loop, so right after the model is
// built they are still running.
void InstallListModelTest::checkingIsTrueWhileChecksRun()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());

    const QString file = root.filePath("installs.ini");
    {
        QSettings settings(file, QSettings::IniFormat);
        writeInstalls(settings, {{QStringLiteral("1.10"), root.filePath("missing-1.10"), false}});
    }

    QSettings settings(file, QSettings::IniFormat);
    InstallListModel model(&settings);
    QVERIFY(model.checking());
    QTRY_VERIFY_WITH_TIMEOUT(!model.checking(), 10000);
    QVERIFY(checksFinished(model));
}

// Relative paths are refused by the Add install path, so no folder is chosen relative to the working folder.
void InstallListModelTest::refusesRelativePaths()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());

    const QString file = root.filePath("installs.ini");
    {
        QSettings settings(file, QSettings::IniFormat);
        writeInstalls(settings, {{QStringLiteral("1.10"), root.filePath("missing-1.10"), false}});
    }

    QSettings settings(file, QSettings::IniFormat);
    InstallListModel model(&settings);
    QTRY_VERIFY_WITH_TIMEOUT(checksFinished(model), 10000);

    QSignalSpy errors(&model, &InstallListModel::errorOccurred);
    QVERIFY(!model.startInstall(QStringLiteral("1.10"), QStringLiteral("relative/engine")));
    QCOMPARE(errors.count(), 1);
    QCOMPARE(stateOf(model, QStringLiteral("1.10")), InstallListModel::Invalid);
    QCOMPARE(model.data(model.index(0), InstallListModel::PathRole).toString(), root.filePath("missing-1.10"));

    QVERIFY(!model.canInstallTo(QStringLiteral("relative/engine")));
    QVERIFY(!model.canInstallTo(QString()));
    QVERIFY(model.canInstallTo(root.filePath("free")));

    QCOMPARE(model.installPathFor(QStringLiteral("1.10"), QStringLiteral("  /opt/engines//  ")), QStringLiteral("/opt/engines/1.10"));
    QCOMPARE(model.installPathFor(QStringLiteral("1.10"), QStringLiteral("/")), QString());
    QCOMPARE(model.installPathFor(QStringLiteral("1.10"), QStringLiteral("   ")), QString());
}

// An install that is running when the app quits is saved with its path. The next run loads it and checks it, so it
// is Invalid while its folder is incomplete, and the user can fix it. The build here sleeps, so the quit comes mid-step.
void InstallListModelTest::interruptedInstallIsKeptAcrossRestart()
{
    if (!checkoutTestsSupported())
        QSKIP("Needs git and a POSIX shell");

    QTemporaryDir root;
    QVERIFY(root.isValid());
    const QString engine = root.filePath("engine");
    QVERIFY(QDir().mkpath(engine));
    QVERIFY(makeCheckout(engine, "#!/bin/sh\nsleep 30\n", false));

    const QString file = root.filePath("installs.ini");
    {
        QSettings settings(file, QSettings::IniFormat);
        writeInstalls(settings, {{QStringLiteral("1.10"), engine, false},
                                 {QStringLiteral("1.9"), root.filePath("missing-1.9"), false}});
    }

    {
        QSettings settings(file, QSettings::IniFormat);
        InstallListModel model(&settings);
        QTRY_VERIFY_WITH_TIMEOUT(checksFinished(model), 10000);
        QCOMPARE(stateOf(model, QStringLiteral("1.10")), InstallListModel::Invalid);

        // Without an editor the Fix runs the build step, which the script keeps running.
        model.fix(QStringLiteral("1.10"), false);
        QTRY_COMPARE_WITH_TIMEOUT(stateOf(model, QStringLiteral("1.10")), InstallListModel::Building, 10000);

        // Removing the other entry finishes while the build runs, and that save must keep the running entry.
        model.uninstall(QStringLiteral("1.9"));
        QTRY_COMPARE_WITH_TIMEOUT(stateOf(model, QStringLiteral("1.9")), InstallListModel::Available, 10000);
    }

    QSettings settings(file, QSettings::IniFormat);
    InstallListModel reopened(&settings);
    QCOMPARE(reopened.rowCount(), 1);
    QCOMPARE(reopened.data(reopened.index(0), InstallListModel::PathRole).toString(), engine);
    QTRY_VERIFY_WITH_TIMEOUT(checksFinished(reopened), 10000);
    QCOMPARE(stateOf(reopened, QStringLiteral("1.10")), InstallListModel::Invalid);
    QVERIFY(reopened.data(reopened.index(0), InstallListModel::ProblemRole).toString().contains(
        QStringLiteral("Release editor")));
}

// A failed step is kept too, so its folder is not left without an entry. Retrying a failed install takes the path the
// install already has, so a path the user typed for it must be a full path.
void InstallListModelTest::failedFixIsKeptAndRetryNeedsFullPath()
{
    if (!checkoutTestsSupported())
        QSKIP("Needs git and a POSIX shell");

    QTemporaryDir root;
    QVERIFY(root.isValid());
    const QString engine = root.filePath("engine");
    QVERIFY(QDir().mkpath(engine));
    QVERIFY(makeCheckout(engine, "#!/bin/sh\nexit 1\n", false));

    const QString file = root.filePath("installs.ini");
    {
        QSettings settings(file, QSettings::IniFormat);
        writeInstalls(settings, {{QStringLiteral("1.10"), engine, false}});
    }

    {
        QSettings settings(file, QSettings::IniFormat);
        InstallListModel model(&settings);
        QTRY_VERIFY_WITH_TIMEOUT(checksFinished(model), 10000);

        model.fix(QStringLiteral("1.10"), false);
        QTRY_COMPARE_WITH_TIMEOUT(stateOf(model, QStringLiteral("1.10")), InstallListModel::Failed, 10000);

        // A failed install is not offered as a new install, but its retry is still installable.
        QVERIFY(!model.canInstall(QStringLiteral("1.10")));
        QVERIFY(!model.data(model.index(0), InstallListModel::AddableRole).toBool());
        QVERIFY(model.data(model.index(0), InstallListModel::InstallableRole).toBool());

        // The retry is installable, so only the path check stops this start.
        QVERIFY(!model.startInstall(QStringLiteral("1.10"), QStringLiteral("relative/engine")));
    }

    QSettings settings(file, QSettings::IniFormat);
    InstallListModel reopened(&settings);
    QCOMPARE(reopened.rowCount(), 1);
    QCOMPARE(reopened.data(reopened.index(0), InstallListModel::PathRole).toString(), engine);
    QTRY_VERIFY_WITH_TIMEOUT(checksFinished(reopened), 10000);
    QCOMPARE(stateOf(reopened, QStringLiteral("1.10")), InstallListModel::Invalid);
}

// A missing folder is an Invalid install that Fix clones again. Its parent folder is missing too, and the clone creates
// it. The clone comes from a local repository that holds the same tag, and that repository has the generate script and
// the editor, so the fix ends Installed with the marker written.
void InstallListModelTest::fixClonesMissingFolderAgain()
{
    if (!checkoutTestsSupported())
        QSKIP("Needs git and a POSIX shell");

    QTemporaryDir root;
    QVERIFY(root.isValid());
    const QString remote = root.filePath("remote");
    QVERIFY(QDir().mkpath(remote));
    QVERIFY(writeFile(QDir(remote).filePath(InstallLayout::generateScriptName()), "#!/bin/sh\nexit 0\n", true));
    QVERIFY(makeCheckout(remote, "#!/bin/sh\n", true));
    const RedirectedRemote redirect(remote);

    const QString engine = root.filePath("engines/1.10");
    const QString file = root.filePath("installs.ini");
    QSettings settings(file, QSettings::IniFormat);
    writeInstalls(settings, {{QStringLiteral("1.10"), engine, false}});
    InstallListModel model(&settings);
    QTRY_VERIFY_WITH_TIMEOUT(checksFinished(model), 10000);

    // The folder is missing, so the install is Invalid, and Fix is offered for it.
    QCOMPARE(stateOf(model, QStringLiteral("1.10")), InstallListModel::Invalid);
    QVERIFY(model.data(model.index(0), InstallListModel::PathMissingRole).toBool());
    QVERIFY(model.data(model.index(0), InstallListModel::FixableRole).toBool());

    model.fix(QStringLiteral("1.10"), false);
    QTRY_COMPARE_WITH_TIMEOUT(stateOf(model, QStringLiteral("1.10")), InstallListModel::Installed, 30000);
    QVERIFY(!model.data(model.index(0), InstallListModel::PathMissingRole).toBool());
    QVERIFY(InstallRules::hasMarker(engine));
    QVERIFY(QFileInfo::exists(QDir(engine).filePath(InstallLayout::editorRelativePath())));
}

// A checkout that is saved with its marker is Installed when it is loaded, and Uninstall deletes it through the marker.
void InstallListModelTest::uninstallRemovesMarkedCheckout()
{
    if (!checkoutTestsSupported())
        QSKIP("Needs git and a POSIX shell");

    QTemporaryDir root;
    QVERIFY(root.isValid());
    const QString engine = root.filePath("engine");
    QVERIFY(QDir().mkpath(engine));
    QVERIFY(makeCheckout(engine, "#!/bin/sh\n", true));
    QVERIFY(InstallRules::writeMarker(engine));

    const QString file = root.filePath("installs.ini");
    QSettings settings(file, QSettings::IniFormat);
    writeInstalls(settings, {{QStringLiteral("1.10"), engine, false}});
    InstallListModel model(&settings);
    QTRY_COMPARE_WITH_TIMEOUT(stateOf(model, QStringLiteral("1.10")), InstallListModel::Installed, 10000);

    model.uninstall(QStringLiteral("1.10"));
    QTRY_COMPARE_WITH_TIMEOUT(stateOf(model, QStringLiteral("1.10")), InstallListModel::Available, 10000);
    QVERIFY(!QFileInfo::exists(engine));
}

// An installed folder that stops being a checkout is not deleted by its first Uninstall: the folder may hold files the Hub
// does not own. The entry becomes Invalid with the reason, and the folder stays. The second Uninstall deletes it, since
// an Invalid entry's folder is deleted whatever it holds.
void InstallListModelTest::uninstallKeepsFolderThatIsNoLongerACheckout()
{
    if (!checkoutTestsSupported())
        QSKIP("Needs git and a POSIX shell");

    QTemporaryDir root;
    QVERIFY(root.isValid());
    const QString engine = root.filePath("engine");
    QVERIFY(QDir().mkpath(engine));
    QVERIFY(makeCheckout(engine, "#!/bin/sh\n", true));

    const QString file = root.filePath("installs.ini");
    QSettings settings(file, QSettings::IniFormat);
    writeInstalls(settings, {{QStringLiteral("1.10"), engine, false}});
    InstallListModel model(&settings);
    QTRY_COMPARE_WITH_TIMEOUT(stateOf(model, QStringLiteral("1.10")), InstallListModel::Installed, 10000);

    QVERIFY(QFile::remove(QDir(engine).filePath(".git/index")));
    model.uninstall(QStringLiteral("1.10"));
    QTRY_COMPARE_WITH_TIMEOUT(stateOf(model, QStringLiteral("1.10")), InstallListModel::Invalid, 10000);
    QVERIFY(QFileInfo::exists(engine));
    QVERIFY(model.data(model.index(0), InstallListModel::ProblemRole).toString().contains(
        QStringLiteral("not a complete FlaxEngine checkout")));

    model.uninstall(QStringLiteral("1.10"));
    QTRY_COMPARE_WITH_TIMEOUT(stateOf(model, QStringLiteral("1.10")), InstallListModel::Available, 10000);
    QTRY_VERIFY_WITH_TIMEOUT(!QFileInfo::exists(engine), 10000);
}

// An Invalid install whose folder has the marker is deleted with its marker and its files. The folder holds the marker
// and one file, and is not a checkout, so no git is needed.
void InstallListModelTest::uninstallRemovesMarkedFolderOfInvalidInstall()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const QString folder = root.filePath("marked");
    QVERIFY(writeFile(QDir(folder).filePath("leftover.txt"), "x"));
    QVERIFY(InstallRules::writeMarker(folder));

    const QString file = root.filePath("installs.ini");
    QSettings settings(file, QSettings::IniFormat);
    writeInstalls(settings, {{QStringLiteral("1.10"), folder, false}});
    InstallListModel model(&settings);
    QTRY_COMPARE_WITH_TIMEOUT(stateOf(model, QStringLiteral("1.10")), InstallListModel::Invalid, 10000);

    model.uninstall(QStringLiteral("1.10"));
    QTRY_COMPARE_WITH_TIMEOUT(stateOf(model, QStringLiteral("1.10")), InstallListModel::Available, 10000);
    QTRY_VERIFY_WITH_TIMEOUT(!QFileInfo::exists(folder), 10000);
}

// An Invalid install is one FlaxHub listed, so Uninstall deletes its folder even without the marker. The folder holds one
// file and is not a checkout, so no git is needed. The entry goes back to Available, and no notice is shown.
void InstallListModelTest::uninstallRemovesUnmarkedFolderOfInvalidInstall()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const QString folder = root.filePath("unmarked");
    QVERIFY(writeFile(QDir(folder).filePath("leftover.txt"), "x"));

    const QString file = root.filePath("installs.ini");
    QSettings settings(file, QSettings::IniFormat);
    writeInstalls(settings, {{QStringLiteral("1.10"), folder, false}});
    InstallListModel model(&settings);
    QTRY_COMPARE_WITH_TIMEOUT(stateOf(model, QStringLiteral("1.10")), InstallListModel::Invalid, 10000);

    QSignalSpy errors(&model, &InstallListModel::errorOccurred);
    model.uninstall(QStringLiteral("1.10"));
    QTRY_COMPARE_WITH_TIMEOUT(stateOf(model, QStringLiteral("1.10")), InstallListModel::Available, 10000);
    QTRY_VERIFY_WITH_TIMEOUT(!QFileInfo::exists(folder), 10000);
    QCOMPARE(errors.count(), 0);
}

// An Invalid install whose folder is already gone is removed from the list without a notice. Nothing is created at its
// path, since there is no folder to delete.
void InstallListModelTest::uninstallRemovesMissingFolderOfInvalidInstall()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const QString missing = root.filePath("gone/1.10");

    const QString file = root.filePath("installs.ini");
    QSettings settings(file, QSettings::IniFormat);
    writeInstalls(settings, {{QStringLiteral("1.10"), missing, false}});
    InstallListModel model(&settings);
    QTRY_COMPARE_WITH_TIMEOUT(stateOf(model, QStringLiteral("1.10")), InstallListModel::Invalid, 10000);
    QVERIFY(model.data(model.index(0), InstallListModel::PathMissingRole).toBool());

    QSignalSpy errors(&model, &InstallListModel::errorOccurred);
    model.uninstall(QStringLiteral("1.10"));
    QTRY_COMPARE_WITH_TIMEOUT(stateOf(model, QStringLiteral("1.10")), InstallListModel::Available, 10000);
    QCOMPARE(errors.count(), 0);
    QVERIFY(!QFileInfo::exists(missing));
}

// The home folder is never deleted, even for an Invalid entry. isSafeToRemove refuses it, so the entry stays Invalid, a
// notice says the folder was not deleted, and the home folder is still there afterwards. The entry names the home folder
// the tests run with, which is a temporary folder.
void InstallListModelTest::uninstallRefusesUnsafeInvalidFolder()
{
    const QString home = m_home.path();
    QVERIFY(QFileInfo::exists(home));

    QTemporaryDir root;
    QVERIFY(root.isValid());
    const QString file = root.filePath("installs.ini");
    QSettings settings(file, QSettings::IniFormat);
    writeInstalls(settings, {{QStringLiteral("1.10"), home, false}});
    InstallListModel model(&settings);
    QTRY_COMPARE_WITH_TIMEOUT(stateOf(model, QStringLiteral("1.10")), InstallListModel::Invalid, 10000);

    QSignalSpy errors(&model, &InstallListModel::errorOccurred);
    model.uninstall(QStringLiteral("1.10"));
    QTRY_COMPARE_WITH_TIMEOUT(errors.count(), 1, 10000);
    QCOMPARE(errors.first().at(0).toString(), QStringLiteral("Could not remove engine"));
    QVERIFY(errors.first().at(1).toString().contains(QStringLiteral("stays in the list")));

    QCOMPARE(stateOf(model, QStringLiteral("1.10")), InstallListModel::Invalid);
    QCOMPARE(model.rowCount(), 1);
    QCOMPARE(model.data(model.index(0), InstallListModel::PathRole).toString(), home);
    QVERIFY(QFileInfo::exists(home));
}

// Fix on a folder that has files but is not a checkout asks first. Nothing is cleared until the user confirms.
void InstallListModelTest::fixAsksBeforeClearingFolderWithFiles()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const QString folder = root.filePath("with-files");
    QVERIFY(writeFile(QDir(folder).filePath("readme.txt"), "x"));

    const QString file = root.filePath("installs.ini");
    QSettings settings(file, QSettings::IniFormat);
    writeInstalls(settings, {{QStringLiteral("1.10"), folder, false}});
    InstallListModel model(&settings);
    QTRY_COMPARE_WITH_TIMEOUT(stateOf(model, QStringLiteral("1.10")), InstallListModel::Invalid, 10000);

    QSignalSpy confirmations(&model, &InstallListModel::clearConfirmationRequired);
    QSignalSpy started(&model, &InstallListModel::fixStarted);
    model.fix(QStringLiteral("1.10"), false);
    QTRY_COMPARE_WITH_TIMEOUT(confirmations.count(), 1, 10000);
    QCOMPARE(confirmations.first().at(0).toString(), QStringLiteral("1.10"));
    QCOMPARE(confirmations.first().at(1).toString(), folder);

    QCOMPARE(started.count(), 0);
    QCOMPARE(stateOf(model, QStringLiteral("1.10")), InstallListModel::Invalid);
    QVERIFY(QFileInfo::exists(QDir(folder).filePath("readme.txt")));
}

// Git redraws its progress line with a carriage return, and the next output replaces that line, even when the output
// arrives in a later chunk. The build script prints a progress line ending in a carriage return, waits so the next
// print is a separate chunk, then prints the line that replaces it.
void InstallListModelTest::logReplacesProgressLineAcrossChunks()
{
    if (!checkoutTestsSupported())
        QSKIP("Needs git and a POSIX shell");

    QTemporaryDir root;
    QVERIFY(root.isValid());
    const QString engine = root.filePath("engine");
    QVERIFY(QDir().mkpath(engine));
    QVERIFY(makeCheckout(engine,
                         "#!/bin/sh\nprintf 'before\\nProgress 10%%\\r'\nsleep 1\nprintf 'Progress 90%%\\ndone\\n'\nexit 1\n",
                         false));

    const QString file = root.filePath("installs.ini");
    QSettings settings(file, QSettings::IniFormat);
    writeInstalls(settings, {{QStringLiteral("1.10"), engine, false}});
    InstallListModel model(&settings);
    QTRY_VERIFY_WITH_TIMEOUT(checksFinished(model), 10000);

    model.fix(QStringLiteral("1.10"), false);
    QTRY_COMPARE_WITH_TIMEOUT(stateOf(model, QStringLiteral("1.10")), InstallListModel::Failed, 15000);

    const QString log = model.data(model.index(0), InstallListModel::LogRole).toString();
    QVERIFY2(log.contains(QStringLiteral("before\nProgress 90%\ndone\n")), qPrintable(log));
    QVERIFY2(!log.contains(QStringLiteral("Progress 10%")), qPrintable(log));
}

QTEST_MAIN(InstallListModelTest)

#include "tst_installlistmodel.moc"
