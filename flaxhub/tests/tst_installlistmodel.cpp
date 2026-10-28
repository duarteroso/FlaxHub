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

    void relocateSavesTheNewFolder();
    void relocateRefusesFolderWithoutMarker();
    void relocateRefusesCompleteCheckoutWithoutMarker();
    void relocateRefusesEmptyAndMissingFolder();
    void relocateRefusesMarkedFolderThatIsNotACheckout();
    void checkingIsTrueWhileChecksRun();
    void refusesRelativePaths();
    void interruptedInstallIsKeptAcrossRestart();
    void failedFixIsKeptAndRetryNeedsFullPath();
    void uninstallRemovesLocatedCheckout();
    void uninstallKeepsFolderThatIsNoLongerACheckout();
    void uninstallRemovesMarkedFolderOfInvalidInstall();
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

// A folder the user locates is saved, so the next run starts with it. The folder is a checkout FlaxHub made, marker included.
void InstallListModelTest::relocateSavesTheNewFolder()
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
    {
        QSettings settings(file, QSettings::IniFormat);
        writeInstalls(settings, {{QStringLiteral("1.10"), root.filePath("missing-1.10"), false}});
    }

    {
        QSettings settings(file, QSettings::IniFormat);
        InstallListModel model(&settings);
        QTRY_VERIFY_WITH_TIMEOUT(checksFinished(model), 10000);

        model.relocate(QStringLiteral("1.10"), engine);
        QTRY_COMPARE_WITH_TIMEOUT(stateOf(model, QStringLiteral("1.10")), InstallListModel::Installed, 10000);
        QCOMPARE(model.data(model.index(0), InstallListModel::PathRole).toString(), engine);
    }

    QSettings settings(file, QSettings::IniFormat);
    InstallListModel reopened(&settings);
    QTRY_VERIFY_WITH_TIMEOUT(checksFinished(reopened), 10000);
    QCOMPARE(reopened.rowCount(), 1);
    QCOMPARE(reopened.data(reopened.index(0), InstallListModel::PathRole).toString(), engine);
}

// A folder that FlaxHub did not create is refused even when it holds files. The refusal is immediate, so the install is
// never Checking, and it is left as it was.
void InstallListModelTest::relocateRefusesFolderWithoutMarker()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    QVERIFY(QDir().mkpath(root.filePath("with-files")));
    QFile readme(root.filePath("with-files/readme.txt"));
    QVERIFY(readme.open(QIODevice::WriteOnly));
    readme.close();

    const QString file = root.filePath("installs.ini");
    {
        QSettings settings(file, QSettings::IniFormat);
        writeInstalls(settings, {{QStringLiteral("1.10"), root.filePath("missing-1.10"), false}});
    }

    QSettings settings(file, QSettings::IniFormat);
    InstallListModel model(&settings);
    QTRY_VERIFY_WITH_TIMEOUT(checksFinished(model), 10000);

    QSignalSpy errors(&model, &InstallListModel::errorOccurred);
    model.relocate(QStringLiteral("1.10"), root.filePath("with-files"));
    QCOMPARE(errors.count(), 1);
    QCOMPARE(errors.first().at(0).toString(), QStringLiteral("Could not locate folder"));
    QVERIFY(errors.first().at(1).toString().contains(QStringLiteral("not created by FlaxHub")));

    QCOMPARE(stateOf(model, QStringLiteral("1.10")), InstallListModel::Invalid);
    QCOMPARE(model.data(model.index(0), InstallListModel::PathRole).toString(), root.filePath("missing-1.10"));
    QVERIFY(model.data(model.index(0), InstallListModel::RelocatableRole).toBool());
}

// A complete checkout of this engine is refused when FlaxHub did not create it, since Locate trusts only the marker.
void InstallListModelTest::relocateRefusesCompleteCheckoutWithoutMarker()
{
    if (!checkoutTestsSupported())
        QSKIP("Needs git and a POSIX shell");

    QTemporaryDir root;
    QVERIFY(root.isValid());
    const QString engine = root.filePath("engine");
    QVERIFY(QDir().mkpath(engine));
    QVERIFY(makeCheckout(engine, "#!/bin/sh\n", true));

    const QString file = root.filePath("installs.ini");
    {
        QSettings settings(file, QSettings::IniFormat);
        writeInstalls(settings, {{QStringLiteral("1.10"), root.filePath("missing-1.10"), false}});
    }

    QSettings settings(file, QSettings::IniFormat);
    InstallListModel model(&settings);
    QTRY_VERIFY_WITH_TIMEOUT(checksFinished(model), 10000);

    QSignalSpy errors(&model, &InstallListModel::errorOccurred);
    model.relocate(QStringLiteral("1.10"), engine);
    QCOMPARE(errors.count(), 1);
    QVERIFY(errors.first().at(1).toString().contains(QStringLiteral("not created by FlaxHub")));

    QCOMPARE(stateOf(model, QStringLiteral("1.10")), InstallListModel::Invalid);
    QCOMPARE(model.data(model.index(0), InstallListModel::PathRole).toString(), root.filePath("missing-1.10"));
}

// An empty folder and a missing folder are refused too. Neither can hold the marker, so each is one refusal.
void InstallListModelTest::relocateRefusesEmptyAndMissingFolder()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    QVERIFY(QDir().mkpath(root.filePath("empty")));

    const QString file = root.filePath("installs.ini");
    {
        QSettings settings(file, QSettings::IniFormat);
        writeInstalls(settings, {{QStringLiteral("1.10"), root.filePath("missing-1.10"), false}});
    }

    QSettings settings(file, QSettings::IniFormat);
    InstallListModel model(&settings);
    QTRY_VERIFY_WITH_TIMEOUT(checksFinished(model), 10000);

    QSignalSpy errors(&model, &InstallListModel::errorOccurred);
    model.relocate(QStringLiteral("1.10"), root.filePath("empty"));
    QCOMPARE(errors.count(), 1);
    QVERIFY(errors.last().at(1).toString().contains(QStringLiteral("not created by FlaxHub")));

    model.relocate(QStringLiteral("1.10"), root.filePath("never-made"));
    QCOMPARE(errors.count(), 2);
    QVERIFY(errors.last().at(1).toString().contains(QStringLiteral("not created by FlaxHub")));

    QCOMPARE(stateOf(model, QStringLiteral("1.10")), InstallListModel::Invalid);
    QCOMPARE(model.data(model.index(0), InstallListModel::PathRole).toString(), root.filePath("missing-1.10"));
}

// A folder with the marker that is not a checkout of this engine is refused, since Fix would clear it. The check runs
// before the refusal, so the notice arrives after the install has been checked again. No git is needed.
void InstallListModelTest::relocateRefusesMarkedFolderThatIsNotACheckout()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const QString marked = root.filePath("marked");
    QVERIFY(QDir().mkpath(marked));
    QVERIFY(InstallRules::writeMarker(marked));

    const QString file = root.filePath("installs.ini");
    {
        QSettings settings(file, QSettings::IniFormat);
        writeInstalls(settings, {{QStringLiteral("1.10"), root.filePath("missing-1.10"), false}});
    }

    QSettings settings(file, QSettings::IniFormat);
    InstallListModel model(&settings);
    QTRY_VERIFY_WITH_TIMEOUT(checksFinished(model), 10000);

    QSignalSpy errors(&model, &InstallListModel::errorOccurred);
    model.relocate(QStringLiteral("1.10"), marked);
    QTRY_COMPARE_WITH_TIMEOUT(errors.count(), 1, 10000);
    QCOMPARE(errors.first().at(0).toString(), QStringLiteral("Could not locate folder"));
    QVERIFY(errors.first().at(1).toString().contains(QStringLiteral("was created by FlaxHub but is not")));

    QCOMPARE(stateOf(model, QStringLiteral("1.10")), InstallListModel::Invalid);
    QCOMPARE(model.data(model.index(0), InstallListModel::PathRole).toString(), root.filePath("missing-1.10"));
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

// Relative paths are refused by the Locate and Add install paths, so no folder is chosen relative to the working folder.
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
    model.relocate(QStringLiteral("1.10"), QStringLiteral("relative/engine"));
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

// A located checkout is Installed, and Uninstall deletes it through the marker that Locate requires.
void InstallListModelTest::uninstallRemovesLocatedCheckout()
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
    writeInstalls(settings, {{QStringLiteral("1.10"), root.filePath("missing-1.10"), false}});
    InstallListModel model(&settings);
    QTRY_VERIFY_WITH_TIMEOUT(checksFinished(model), 10000);

    model.relocate(QStringLiteral("1.10"), engine);
    QTRY_COMPARE_WITH_TIMEOUT(stateOf(model, QStringLiteral("1.10")), InstallListModel::Installed, 10000);

    model.uninstall(QStringLiteral("1.10"));
    QTRY_COMPARE_WITH_TIMEOUT(stateOf(model, QStringLiteral("1.10")), InstallListModel::Available, 10000);
    QVERIFY(!QFileInfo::exists(engine));
}

// An installed folder that stops being a checkout is not deleted: the folder may hold files the Hub does not own. The
// entry becomes Invalid with the reason, and the folder stays.
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
}

// An Invalid install whose folder has the marker is FlaxHub's own folder, so Uninstall deletes it. The folder holds the
// marker and one file, and is not a checkout, so no git is needed.
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
