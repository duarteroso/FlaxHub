#include "installlayout.h"
#include "installrules.h"
#include "pathutils.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QTest>

using Failure = InstallValidator::Failure;

// One bit per validator check. A row of failures is a bit mask over these, in the order failuresFor reads them.
constexpr int pathMissing = 1 << 0;
constexpr int folderEmpty = 1 << 1;
constexpr int noGitCheckout = 1 << 2;
constexpr int gitUnavailable = 1 << 3;
constexpr int noGenerateOutput = 1 << 4;
constexpr int noBinary = 1 << 5;

// The failed checks named by a mask. Bit 0 is PathMissing, and so on in the order of the enum.
static QList<Failure> failuresFor(int bits)
{
    const QList<Failure> all = {Failure::PathMissing, Failure::FolderEmpty, Failure::NoGitCheckout,
                                Failure::GitUnavailable, Failure::NoGenerateOutput, Failure::NoBinary};
    QList<Failure> failures;
    for (int bit = 0; bit < all.size(); ++bit) {
        if (bits & (1 << bit))
            failures.append(all.at(bit));
    }

    return failures;
}

class InstallRulesTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();

    void safeToRemoveRefusesRelativeAndEmptyPaths();
    void safeToRemoveRefusesRoot();
    void safeToRemoveRefusesHome();
    void safeToRemoveRefusesFoldersAboveHome();
    void safeToRemoveRefusesSymlinkToHome();
    void safeToRemoveRefusesLinksAndFiles();
    void safeToRemoveRefusesMissingFolder();
    void safeToRemoveAllowsInstallFolder();
    void safeToRemoveAllowsDocumentsFolder();

    void fullPathRefusesRelativePaths();
    void expandHomeExpandsTilde();
    void collapseHomeShortensHomePrefix();
    void defaultInstallDirectoryIsFullPathInHome();

    void freeFolderAllowsMissingAndEmptyFolders();
    void freeFolderRefusesFoldersWithContent();

    void markerIsInsideTheFolder();
    void mayUninstallAllowsFolderTheHubMade();
    void mayUninstallAllowsCompleteCheckout();
    void mayUninstallRefusesOtherFolders();
    void mayUninstallRefusesHomeAndRelativePaths();

    void needsCloneAndClearForCommonFailures_data();
    void needsCloneAndClearForCommonFailures();
    void needsCloneAndClearForEveryFailureSet();

private:
    QTemporaryDir m_root;
    QString m_home;
    bool m_hadHome = false;
    QByteArray m_savedHome;
    bool m_hadUserProfile = false;
    QByteArray m_savedUserProfile;
};

// HOME points at a folder inside a temporary one for the whole run, so no rule ever sees the real home folder.
void InstallRulesTest::initTestCase()
{
    QVERIFY(m_root.isValid());

    m_hadHome = qEnvironmentVariableIsSet("HOME");
    m_savedHome = qgetenv("HOME");
    m_hadUserProfile = qEnvironmentVariableIsSet("USERPROFILE");
    m_savedUserProfile = qgetenv("USERPROFILE");

    m_home = QDir(m_root.path()).filePath("home");
    QVERIFY(QDir().mkpath(m_home));
    qputenv("HOME", m_home.toLocal8Bit());
    // Qt takes the home folder from USERPROFILE on Windows.
    qputenv("USERPROFILE", m_home.toLocal8Bit());
    QCOMPARE(QDir::cleanPath(QDir::homePath()), QDir::cleanPath(m_home));

    QVERIFY(QDir().mkpath(QDir(m_home).filePath("FlaxHub/Engines/1.10")));
    QVERIFY(QDir().mkpath(QDir(m_home).filePath("Documents")));
}

void InstallRulesTest::cleanupTestCase()
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

void InstallRulesTest::safeToRemoveRefusesRelativeAndEmptyPaths()
{
    QVERIFY(!InstallRules::isSafeToRemove(QString()));
    QVERIFY(!InstallRules::isSafeToRemove(QStringLiteral("FlaxHub/Engines/1.10")));
}

// QDir::rootPath() is "/" on Unix and a drive root on Windows, so the case is a real root on each system.
void InstallRulesTest::safeToRemoveRefusesRoot()
{
    QVERIFY(!InstallRules::isSafeToRemove(QDir::rootPath()));
}

void InstallRulesTest::safeToRemoveRefusesHome()
{
    QVERIFY(!InstallRules::isSafeToRemove(m_home));
    QVERIFY(!InstallRules::isSafeToRemove(m_home + QLatin1Char('/')));
}

// Each of these holds home, so removing one would take home with it.
void InstallRulesTest::safeToRemoveRefusesFoldersAboveHome()
{
    QVERIFY(!InstallRules::isSafeToRemove(m_home + QStringLiteral("/..")));
    QVERIFY(!InstallRules::isSafeToRemove(QFileInfo(m_home).absolutePath()));
}

void InstallRulesTest::safeToRemoveRefusesSymlinkToHome()
{
    const QString link = m_root.filePath("home-link");
    if (!QFile::link(m_home, link))
        QSKIP("Could not create a symbolic link");

    QVERIFY(!InstallRules::isSafeToRemove(link));
}

// A link is refused even when it points to a folder that is not home, since a delete through it would empty that folder.
// A file is refused too, since an install is a folder.
void InstallRulesTest::safeToRemoveRefusesLinksAndFiles()
{
    const QString folder = m_root.filePath("linked-target");
    QVERIFY(QDir().mkpath(folder));
    QVERIFY(InstallRules::isSafeToRemove(folder));

    const QString link = m_root.filePath("link-to-target");
    if (!QFile::link(folder, link))
        QSKIP("Could not create a symbolic link");
    QVERIFY(!InstallRules::isSafeToRemove(link));
    // A trailing separator makes QFileInfo follow the link, so it is refused as well.
    QVERIFY(!InstallRules::isSafeToRemove(link + QLatin1Char('/')));

    const QString file = m_root.filePath("plain.txt");
    QFile plain(file);
    QVERIFY(plain.open(QIODevice::WriteOnly));
    plain.close();
    QVERIFY(!InstallRules::isSafeToRemove(file));
}

// A path that does not resolve is refused, so there is nothing to remove.
void InstallRulesTest::safeToRemoveRefusesMissingFolder()
{
    QVERIFY(!InstallRules::isSafeToRemove(QDir(m_home).filePath("Missing")));
}

void InstallRulesTest::safeToRemoveAllowsInstallFolder()
{
    QVERIFY(InstallRules::isSafeToRemove(QDir(m_home).filePath("FlaxHub/Engines/1.10")));
}

// isSafeToRemove refuses roots, home, its ancestors, links and files. A folder like this one passes it, and mayUninstall,
// which also needs the Hub's marker or a complete checkout, refuses it (see mayUninstallRefusesOtherFolders).
void InstallRulesTest::safeToRemoveAllowsDocumentsFolder()
{
    QVERIFY(InstallRules::isSafeToRemove(QDir(m_home).filePath("Documents")));
}

void InstallRulesTest::fullPathRefusesRelativePaths()
{
    QVERIFY(InstallRules::isFullPath(m_home));
    QVERIFY(!InstallRules::isFullPath(QStringLiteral("FlaxHub/Engines/1.10")));
    QVERIFY(!InstallRules::isFullPath(QStringLiteral("./engine")));
    QVERIFY(!InstallRules::isFullPath(QString()));
}

// A path from the settings may start with "~", which expandHome makes the home folder. Outside Windows a backslash is an
// ordinary file name character, so "~\" is left as it is there.
void InstallRulesTest::expandHomeExpandsTilde()
{
    QCOMPARE(QDir::cleanPath(expandHome(QStringLiteral("~"))), QDir::cleanPath(m_home));
    QCOMPARE(QDir::cleanPath(expandHome(QStringLiteral("~/FlaxHub/Engines"))),
             QDir::cleanPath(QDir(m_home).filePath(QStringLiteral("FlaxHub/Engines"))));
#if !defined(Q_OS_WIN)
    QCOMPARE(expandHome(QStringLiteral("~\\FlaxHub")), QStringLiteral("~\\FlaxHub"));
#endif
}

// The home folder is shown as "~", but only for the home folder and the paths inside it, at a folder boundary. Going
// back with expandHome gives the full path again.
void InstallRulesTest::collapseHomeShortensHomePrefix()
{
    QCOMPARE(collapseHome(m_home), QStringLiteral("~"));
    QCOMPARE(collapseHome(m_home + QLatin1Char('/')), QStringLiteral("~"));
    QCOMPARE(collapseHome(QDir(m_home).filePath(QStringLiteral("FlaxHub/Engines"))),
             QStringLiteral("~/FlaxHub/Engines"));

    // A folder whose name only starts with the home folder's name is a different folder.
    QCOMPARE(collapseHome(m_home + QStringLiteral("2/FlaxHub")), m_home + QStringLiteral("2/FlaxHub"));
    QCOMPARE(collapseHome(m_root.filePath("other")), m_root.filePath("other"));
    QCOMPARE(collapseHome(QStringLiteral("FlaxHub/Engines")), QStringLiteral("FlaxHub/Engines"));

    const QString engines = QDir(m_home).filePath(QStringLiteral("FlaxHub/Engines"));
    QCOMPARE(QDir::cleanPath(expandHome(collapseHome(engines))), QDir::cleanPath(engines));
}

// A fresh install takes its default folder from the home folder, so the Installs list shows it as a full path.
void InstallRulesTest::defaultInstallDirectoryIsFullPathInHome()
{
    QCOMPARE(QDir::cleanPath(InstallLayout::defaultInstallDirectory()),
             QDir::cleanPath(QDir(m_home).filePath(QStringLiteral("FlaxHub/Engines"))));
    QVERIFY(InstallRules::isFullPath(InstallLayout::defaultInstallDirectory()));
}

void InstallRulesTest::freeFolderAllowsMissingAndEmptyFolders()
{
    QVERIFY(InstallRules::isFreeFolder(m_root.filePath("missing")));
    QVERIFY(QDir().mkpath(m_root.filePath("empty")));
    QVERIFY(InstallRules::isFreeFolder(m_root.filePath("empty")));
}

void InstallRulesTest::freeFolderRefusesFoldersWithContent()
{
    // A hidden entry counts as content, so a folder that holds only .git is not free.
    QVERIFY(QDir().mkpath(m_root.filePath("hidden-git/.git")));
    QVERIFY(!InstallRules::isFreeFolder(m_root.filePath("hidden-git")));

    QVERIFY(QDir().mkpath(m_root.filePath("with-file")));
    QFile readme(m_root.filePath("with-file/readme.txt"));
    QVERIFY(readme.open(QIODevice::WriteOnly));
    readme.close();
    QVERIFY(!InstallRules::isFreeFolder(m_root.filePath("with-file")));

    // A file is not a folder, so it is not free either.
    QVERIFY(!InstallRules::isFreeFolder(m_root.filePath("with-file/readme.txt")));
}

// The marker is a file inside the folder, so the folder must exist before the marker is written, and a folder that holds
// the marker is no longer free.
void InstallRulesTest::markerIsInsideTheFolder()
{
    const QString folder = QDir(m_root.path()).filePath("marked/engine");
    const QString marker = QDir(folder).filePath(".flaxhub-install");
    QCOMPARE(QDir::cleanPath(InstallRules::markerPath(folder)), QDir::cleanPath(marker));

    QVERIFY(!InstallRules::hasMarker(folder));
    QVERIFY(!InstallRules::writeMarker(folder));
    QVERIFY(QDir().mkpath(folder));
    QVERIFY(InstallRules::isFreeFolder(folder));

    QVERIFY(InstallRules::writeMarker(folder));
    QVERIFY(InstallRules::hasMarker(folder));
    QVERIFY(!InstallRules::isFreeFolder(folder));

    QVERIFY(QDir(folder).removeRecursively());
    QVERIFY(!InstallRules::hasMarker(folder));
}

void InstallRulesTest::mayUninstallAllowsFolderTheHubMade()
{
    const QString folder = QDir(m_root.path()).filePath("hub/engine");
    QVERIFY(QDir().mkpath(folder));
    QFile partial(QDir(folder).filePath("partial.txt"));
    QVERIFY(partial.open(QIODevice::WriteOnly));
    partial.close();

    // An interrupted clone has files and no checkout, but the marker is enough.
    QVERIFY(!InstallRules::mayUninstall(folder, false));
    QVERIFY(InstallRules::writeMarker(folder));
    QVERIFY(InstallRules::mayUninstall(folder, false));
}

// A complete checkout is deleted without a marker, even when the Hub did not make it. The rule takes the result of the
// checkout check, so here it is given as true.
void InstallRulesTest::mayUninstallAllowsCompleteCheckout()
{
    const QString folder = QDir(m_root.path()).filePath("checkout/engine");
    QVERIFY(QDir().mkpath(folder));
    QFile readme(QDir(folder).filePath("readme.txt"));
    QVERIFY(readme.open(QIODevice::WriteOnly));
    readme.close();

    QVERIFY(!InstallRules::hasMarker(folder));
    QVERIFY(InstallRules::mayUninstall(folder, true));
    QVERIFY(!InstallRules::mayUninstall(folder, false));
}

// Folders the Hub did not make and that are not complete checkouts are kept by an Installed or Failed entry, whether they
// hold files or not. An Invalid entry's folder is deleted under isSafeToRemove alone, so it does not use mayUninstall.
void InstallRulesTest::mayUninstallRefusesOtherFolders()
{
    const QString documents = QDir(m_home).filePath("Documents/Notes");
    QVERIFY(QDir().mkpath(documents));
    QFile notes(QDir(documents).filePath("notes.txt"));
    QVERIFY(notes.open(QIODevice::WriteOnly));
    notes.close();
    QVERIFY(!InstallRules::mayUninstall(documents, false));

    // An empty folder that the Hub did not make is kept too.
    const QString empty = QDir(m_root.path()).filePath("unmarked-empty");
    QVERIFY(QDir().mkpath(empty));
    QVERIFY(!InstallRules::mayUninstall(empty, false));
}

// The Phase 1 guards still apply to a marker or a checkout: home, its ancestors and roots are never removed, and a
// relative path is never removed.
void InstallRulesTest::mayUninstallRefusesHomeAndRelativePaths()
{
    QVERIFY(InstallRules::writeMarker(m_home));
    QVERIFY(!InstallRules::mayUninstall(m_home, true));
    QVERIFY(!InstallRules::mayUninstall(QStringLiteral("/"), true));
    QVERIFY(!InstallRules::mayUninstall(QStringLiteral("FlaxHub/Engines/1.10"), true));
}

void InstallRulesTest::needsCloneAndClearForCommonFailures_data()
{
    QTest::addColumn<int>("failures");
    QTest::addColumn<bool>("clone");
    QTest::addColumn<bool>("clear");

    // The failure sets that validate reports for the folders a user can end up with.
    QTest::newRow("valid install") << 0 << false << false;
    QTest::newRow("missing folder") << (pathMissing | noGitCheckout | noGenerateOutput | noBinary) << true << false;
    QTest::newRow("empty folder") << (folderEmpty | noGitCheckout | noGenerateOutput | noBinary) << true << false;
    QTest::newRow("folder with files, no checkout") << (noGitCheckout | noGenerateOutput | noBinary) << true << true;
    QTest::newRow("only the folder missing") << pathMissing << true << false;
    QTest::newRow("only the checkout missing") << noGitCheckout << true << true;
    QTest::newRow("checkout without generated files") << (noGenerateOutput | noBinary) << false << false;
    QTest::newRow("checkout without the editor") << noBinary << false << false;
    // A checkout that could not be checked because git did not run is never cleared, and not cloned either.
    QTest::newRow("git unavailable") << gitUnavailable << false << false;
    QTest::newRow("git unavailable, folder with files") << (gitUnavailable | noGenerateOutput | noBinary) << false << false;
}

void InstallRulesTest::needsCloneAndClearForCommonFailures()
{
    QFETCH(int, failures);
    QFETCH(bool, clone);
    QFETCH(bool, clear);

    QCOMPARE(InstallRules::needsClone(failuresFor(failures)), clone);
    QCOMPARE(InstallRules::needsClear(failuresFor(failures)), clear);
}

// Combinations of the checks beyond the single failures in the table above. Each is checked against the result a
// person reads off the rules: a clone is needed when the folder or its checkout is missing, and a clear only for a
// folder that has files but no checkout.
void InstallRulesTest::needsCloneAndClearForEveryFailureSet()
{
    struct Case {
        int bits;
        bool clone;
        bool clear;
    };
    const QList<Case> cases = {
        {pathMissing | noGitCheckout, true, false},
        {folderEmpty | noGitCheckout, true, false},
        {pathMissing | folderEmpty | noGitCheckout | noGenerateOutput | noBinary, true, false},
        {noGitCheckout | noGenerateOutput, true, true},
        {noGitCheckout | noBinary, true, true},
        {noGenerateOutput, false, false},
        {gitUnavailable | noBinary, false, false},
    };

    for (const Case &c : cases) {
        const QString label = QStringLiteral("failure bits %1").arg(c.bits);
        QVERIFY2(InstallRules::needsClone(failuresFor(c.bits)) == c.clone, qPrintable(label));
        QVERIFY2(InstallRules::needsClear(failuresFor(c.bits)) == c.clear, qPrintable(label));
    }

    // A folder that is cleared is always cloned after the clear.
    for (int bits = 0; bits < 64; ++bits) {
        if (InstallRules::needsClear(failuresFor(bits)))
            QVERIFY2(InstallRules::needsClone(failuresFor(bits)), qPrintable(QStringLiteral("failure bits %1").arg(bits)));
    }
}

QTEST_APPLESS_MAIN(InstallRulesTest)

#include "tst_installrules.moc"
