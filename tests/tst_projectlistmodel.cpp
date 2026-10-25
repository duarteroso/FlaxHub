#include "englishcatalogue.h"
#include "installlayout.h"
#include "installlistmodel.h"
#include "projectlistmodel.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QSettings>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

namespace {

// Whether every check has returned. Until then an install row shows Checking.
bool checksFinished(const InstallListModel &model)
{
    for (int row = 0; row < model.rowCount(); ++row) {
        if (model.data(model.index(row), InstallListModel::StateRole).toInt() == InstallListModel::Checking)
            return false;
    }

    return true;
}

// Writes a project folder that holds a .flaxproj naming the engine version, and returns the folder's path. The path
// is empty when the file cannot be written.
QString makeProjectFolder(const QTemporaryDir &root, const QString &name, const QString &engineVersion)
{
    const QString folder = root.filePath(name);
    QDir().mkpath(folder);
    QFile project(folder + QStringLiteral("/MyGame.flaxproj"));
    if (!project.open(QIODevice::WriteOnly))
        return {};

    project.write(QStringLiteral("{\"MinEngineVersion\": \"%1\"}").arg(engineVersion).toUtf8());
    return folder;
}

// The fake editor records its arguments, one per line, in @ARGV@ and then acts as the editor does for -new. A project
// folder named Broken is made and the run fails with exit code 3, one named Silent exits without a project file, and any
// other folder gets its .flaxproj.
const char *const fakeEditorScript = R"SCRIPT(#!/bin/sh
printf '%s\n' "$@" > '@ARGV@.tmp' && mv '@ARGV@.tmp' '@ARGV@'
new=
dir=
for arg in "$@"; do
    case "$arg" in
        -new) new=1 ;;
        -project=*) dir=${arg#-project=} ;;
    esac
done
[ -n "$new" ] || exit 0
dir=${dir#\"}
dir=${dir%\"}
case "$(basename "$dir")" in
    Broken)
        mkdir -p "$dir"
        echo "Editor failure"
        exit 3
        ;;
    Silent) exit 0 ;;
esac
mkdir -p "$dir" && printf '{}' > "$dir/$(basename "$dir").flaxproj"
)SCRIPT";

// Whether the engine fixture can be made here. It needs git, which makes the checkout the install check reads, and the
// fake editor is a POSIX shell script, so Windows is left out.
bool canMakeFakeEngine()
{
#if defined(Q_OS_WIN)
    return false;
#else
    return !QStandardPaths::findExecutable(QStringLiteral("git")).isEmpty();
#endif
}

// Runs git in folder and returns whether it exited without an error.
bool runGit(const QString &folder, const QStringList &arguments)
{
    QProcess git;
    git.setWorkingDirectory(folder);
    git.start(QStringLiteral("git"), arguments);
    return git.waitForFinished(10000) && git.exitStatus() == QProcess::NormalExit && git.exitCode() == 0;
}

// Writes an engine folder that the install check passes for version: a git checkout tagged version, the generated files
// and the fake editor. Returns false when a step fails.
bool writeFakeEngine(const QString &folder, const QString &version, const QString &argvFile)
{
    const QString editorDirectory = InstallLayout::editorDirectoryRelativePath();
    const QStringList generated = {
        InstallLayout::buildToolRelativePath(),
        editorDirectory + QStringLiteral("/FlaxEditor.Build.json"),
        editorDirectory + QStringLiteral("/FlaxEngine.CSharp.dll"),
    };
    for (const QString &file : generated) {
        const QString path = QDir(folder).filePath(file);
        if (!QDir().mkpath(QFileInfo(path).absolutePath()))
            return false;
        QFile output(path);
        if (!output.open(QIODevice::WriteOnly))
            return false;
    }

    const QString editor = QDir(folder).filePath(InstallLayout::editorRelativePath());
    QString script = QString::fromLatin1(fakeEditorScript);
    script.replace(QStringLiteral("@ARGV@"), argvFile);
    {
        QFile file(editor);
        if (!file.open(QIODevice::WriteOnly) || file.write(script.toUtf8()) < 0)
            return false;
    }
    if (!QFile::setPermissions(editor, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner))
        return false;

    const QStringList identity = {
        QStringLiteral("-c"), QStringLiteral("user.name=FlaxHub"),
        QStringLiteral("-c"), QStringLiteral("user.email=test@example.invalid"),
        QStringLiteral("-c"), QStringLiteral("commit.gpgsign=false"),
    };
    return runGit(folder, {QStringLiteral("init"), QStringLiteral("-q")})
        && runGit(folder, {QStringLiteral("add"), QStringLiteral("-A")})
        && runGit(folder, identity + QStringList{QStringLiteral("commit"), QStringLiteral("-q"), QStringLiteral("-m"), QStringLiteral("Engine")})
        && runGit(folder, {QStringLiteral("tag"), version});
}

// Saves one install as the install list reads it, so the model under test loads it and checks it.
void saveInstall(const QString &file, const QString &version, const QString &path)
{
    QSettings settings(file, QSettings::IniFormat);
    settings.beginWriteArray(QStringLiteral("Installs"));
    settings.setArrayIndex(0);
    settings.setValue(QStringLiteral("version"), version);
    settings.setValue(QStringLiteral("path"), path);
    settings.setValue(QStringLiteral("isDefault"), true);
    settings.endArray();
}

// The arguments the fake editor recorded in file, one per line.
QStringList readArguments(const QString &file)
{
    QFile input(file);
    if (!input.open(QIODevice::ReadOnly))
        return {};

    return QString::fromUtf8(input.readAll()).split(QLatin1Char('\n'), Qt::SkipEmptyParts);
}

} // namespace

// The projects are saved in a temporary settings file, so the user's settings are not touched.
class ProjectListModelTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();

    void restoresAddedProjects();
    void lastOpenedRoundTrips();
    void installStateChangesDoNotRefreshProjects();
    void addExistingRejectsProjectAlreadyInList();
    void openKeepsQuotesInEditorArguments();
    void openKeepsApostrophesInWords();
    void createAddsProjectOnceAndRejectsDuplicates();
    void createRemovesFolderOfFailedRun();
    void createReportsEditorThatCannotStart();

private:
    QTemporaryDir m_home;
    QTranslator m_translator;
    QByteArray m_savedHome;
    bool m_hadHome = false;
    QByteArray m_savedUserProfile;
    bool m_hadUserProfile = false;
};

// HOME points at a temporary folder for the whole run, so git never reads the user's configuration.
void ProjectListModelTest::initTestCase()
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

void ProjectListModelTest::cleanupTestCase()
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

// A project added from disk, and its pin, come back after the model is made again from the same settings.
void ProjectListModelTest::restoresAddedProjects()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const QString folder = makeProjectFolder(root, QStringLiteral("Game Folder"), QStringLiteral("1.10"));
    QVERIFY(!folder.isEmpty());
    const QString canonical = QFileInfo(folder).canonicalFilePath();
    const QString file = root.filePath("projects.ini");

    {
        QSettings settings(file, QSettings::IniFormat);
        ProjectListModel model(&settings);
        model.addExisting(folder);
        QCOMPARE(model.rowCount(), 1);
        QCOMPARE(model.data(model.index(0), ProjectListModel::NameRole).toString(), QStringLiteral("MyGame"));
        QCOMPARE(model.data(model.index(0), ProjectListModel::EngineVersionRole).toString(), QStringLiteral("1.10"));

        model.setPinned(canonical, true);
    }

    QSettings settings(file, QSettings::IniFormat);
    ProjectListModel reopened(&settings);
    QCOMPARE(reopened.rowCount(), 1);
    QCOMPARE(reopened.data(reopened.index(0), ProjectListModel::PathRole).toString(), canonical);
    QCOMPARE(reopened.data(reopened.index(0), ProjectListModel::EngineVersionRole).toString(), QStringLiteral("1.10"));
    QVERIFY(reopened.data(reopened.index(0), ProjectListModel::PinnedRole).toBool());
}

// The lastOpened date that is saved comes back as the same valid date after the model is made again. This guards the
// saved format: the date is stored as a QDateTime, and an existing file must keep loading the same way.
void ProjectListModelTest::lastOpenedRoundTrips()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const QString folder = makeProjectFolder(root, QStringLiteral("Game"), QStringLiteral("1.10"));
    QVERIFY(!folder.isEmpty());
    const QString file = root.filePath("projects.ini");

    qint64 savedAt = 0;
    {
        QSettings settings(file, QSettings::IniFormat);
        ProjectListModel model(&settings);
        model.addExisting(folder);
        const QDateTime lastOpened = model.data(model.index(0), ProjectListModel::LastOpenedRole).toDateTime();
        QVERIFY(lastOpened.isValid());
        savedAt = lastOpened.toMSecsSinceEpoch();
    }

    QSettings settings(file, QSettings::IniFormat);
    ProjectListModel reopened(&settings);
    QCOMPARE(reopened.rowCount(), 1);
    const QDateTime restored = reopened.data(reopened.index(0), ProjectListModel::LastOpenedRole).toDateTime();
    QVERIFY(restored.isValid());
    QCOMPARE(restored.toMSecsSinceEpoch(), savedAt);
}

// Installs whose state changes without any engine becoming Installed must not refresh the projects. Here the installs
// are Invalid, so the installed set never changes.
void ProjectListModelTest::installStateChangesDoNotRefreshProjects()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const QString folder = makeProjectFolder(root, QStringLiteral("Game"), QStringLiteral("1.10"));
    QVERIFY(!folder.isEmpty());

    const QString installFile = root.filePath("installs.ini");
    {
        QSettings settings(installFile, QSettings::IniFormat);
        settings.beginWriteArray(QStringLiteral("Installs"));
        settings.setArrayIndex(0);
        settings.setValue(QStringLiteral("version"), QStringLiteral("1.10"));
        settings.setValue(QStringLiteral("path"), root.filePath("missing-1.10"));
        settings.setValue(QStringLiteral("isDefault"), false);
        settings.endArray();
    }

    QSettings installSettings(installFile, QSettings::IniFormat);
    InstallListModel installs(&installSettings);

    QSettings projectSettings(root.filePath("projects.ini"), QSettings::IniFormat);
    ProjectListModel projects(&projectSettings);
    projects.addExisting(folder);
    projects.setInstalls(&installs);

    int refreshes = 0;
    QObject::connect(&projects, &QAbstractItemModel::dataChanged, &projects, [&refreshes]() {
        ++refreshes;
    });

    // The checks change the installs' state, which the projects hear about, but no engine becomes Installed.
    QTRY_VERIFY_WITH_TIMEOUT(checksFinished(installs), 10000);
    QCOMPARE(refreshes, 0);
    QVERIFY(installs.installedVersions().isEmpty());
}

// A folder that is already in the list is refused under any spelling of its path: a trailing slash, dot segments, a
// symbolic link, and on Windows another letter case. The list keeps the one path it stored.
void ProjectListModelTest::addExistingRejectsProjectAlreadyInList()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const QString folder = makeProjectFolder(root, QStringLiteral("Game Folder"), QStringLiteral("1.10"));
    QVERIFY(!folder.isEmpty());

    QSettings settings(root.filePath("projects.ini"), QSettings::IniFormat);
    ProjectListModel model(&settings);
    QSignalSpy errors(&model, &ProjectListModel::errorOccurred);
    model.addExisting(folder);
    QCOMPARE(model.rowCount(), 1);
    const QString path = model.data(model.index(0), ProjectListModel::PathRole).toString();
    QCOMPARE(path, QFileInfo(folder).canonicalFilePath());

    QStringList spellings = {
        folder + QStringLiteral("/"),
        folder + QStringLiteral("/."),
        QDir(folder).filePath(QStringLiteral("../Game Folder")),
    };
#if defined(Q_OS_WIN)
    spellings.append(folder.toUpper());
#else
    QVERIFY(QFile(folder).link(root.filePath("link")));
    spellings.append(root.filePath("link"));
#endif
    for (const QString &spelling : spellings)
        model.addExisting(spelling);

    QCOMPARE(model.rowCount(), 1);
    QCOMPARE(errors.count(), spellings.size());
    QCOMPARE(errors.last().at(1).toString(), QStringLiteral("%1 is already in the list.").arg(path));
}

// The editor gets the saved arguments with their quotes, so the engine reads -title="My game" as one value. The engine
// joins its arguments with spaces (Main/MainUtil.h), so the line it parses is the one the user wrote after the project.
void ProjectListModelTest::openKeepsQuotesInEditorArguments()
{
    if (!canMakeFakeEngine())
        QSKIP("The fake engine needs git and a POSIX shell.");

    QTemporaryDir root;
    QVERIFY(root.isValid());
    const QString argvFile = root.filePath("argv.txt");
    const QString engine = root.filePath("engine");
    QVERIFY(writeFakeEngine(engine, QStringLiteral("1.10"), argvFile));
    saveInstall(root.filePath("installs.ini"), QStringLiteral("1.10"), engine);

    QSettings installSettings(root.filePath("installs.ini"), QSettings::IniFormat);
    InstallListModel installs(&installSettings);
    QTRY_VERIFY_WITH_TIMEOUT(installs.isInstalled(QStringLiteral("1.10")), 10000);

    const QString folder = makeProjectFolder(root, QStringLiteral("Game"), QStringLiteral("1.10"));
    QVERIFY(!folder.isEmpty());
    QSettings settings(root.filePath("projects.ini"), QSettings::IniFormat);
    ProjectListModel model(&settings);
    model.setInstalls(&installs);
    model.addExisting(folder);
    QCOMPARE(model.rowCount(), 1);
    const QString path = model.data(model.index(0), ProjectListModel::PathRole).toString();

    model.open(path, QStringLiteral("-title=\"My game\" -fullscreen"));
    QTRY_VERIFY_WITH_TIMEOUT(QFileInfo::exists(argvFile), 10000);
    const QStringList expected = {
        QStringLiteral("-project=\"%1/MyGame.flaxproj\"").arg(path),
        QStringLiteral("-title=\"My game\""),
        QStringLiteral("-fullscreen"),
    };
    QCOMPARE(readArguments(argvFile), expected);
}

// An apostrophe inside a word is an ordinary character, so the arguments after it still split at their spaces. A quoted
// value after "=" still keeps its words together, as -title='My game' does.
void ProjectListModelTest::openKeepsApostrophesInWords()
{
    if (!canMakeFakeEngine())
        QSKIP("The fake engine needs git and a POSIX shell.");

    QTemporaryDir root;
    QVERIFY(root.isValid());
    const QString argvFile = root.filePath("argv.txt");
    const QString engine = root.filePath("engine");
    QVERIFY(writeFakeEngine(engine, QStringLiteral("1.10"), argvFile));
    saveInstall(root.filePath("installs.ini"), QStringLiteral("1.10"), engine);

    QSettings installSettings(root.filePath("installs.ini"), QSettings::IniFormat);
    InstallListModel installs(&installSettings);
    QTRY_VERIFY_WITH_TIMEOUT(installs.isInstalled(QStringLiteral("1.10")), 10000);

    const QString folder = makeProjectFolder(root, QStringLiteral("Game"), QStringLiteral("1.10"));
    QVERIFY(!folder.isEmpty());
    QSettings settings(root.filePath("projects.ini"), QSettings::IniFormat);
    ProjectListModel model(&settings);
    model.setInstalls(&installs);
    model.addExisting(folder);
    QCOMPARE(model.rowCount(), 1);
    const QString path = model.data(model.index(0), ProjectListModel::PathRole).toString();

    model.open(path, QStringLiteral("-notes=don't -title='My game' -fullscreen"));
    QTRY_VERIFY_WITH_TIMEOUT(QFileInfo::exists(argvFile), 10000);
    const QStringList expected = {
        QStringLiteral("-project=\"%1/MyGame.flaxproj\"").arg(path),
        QStringLiteral("-notes=don't"),
        QStringLiteral("-title='My game'"),
        QStringLiteral("-fullscreen"),
    };
    QCOMPARE(readArguments(argvFile), expected);
}

// A created project joins the list once. A second create at the same place is refused, and so is one after the folder
// was deleted, since the list still holds the path.
void ProjectListModelTest::createAddsProjectOnceAndRejectsDuplicates()
{
    if (!canMakeFakeEngine())
        QSKIP("The fake engine needs git and a POSIX shell.");

    QTemporaryDir root;
    QVERIFY(root.isValid());
    const QString argvFile = root.filePath("argv.txt");
    const QString engine = root.filePath("engine");
    QVERIFY(writeFakeEngine(engine, QStringLiteral("1.10"), argvFile));
    saveInstall(root.filePath("installs.ini"), QStringLiteral("1.10"), engine);

    QSettings installSettings(root.filePath("installs.ini"), QSettings::IniFormat);
    InstallListModel installs(&installSettings);
    QTRY_VERIFY_WITH_TIMEOUT(installs.isInstalled(QStringLiteral("1.10")), 10000);

    QSettings settings(root.filePath("projects.ini"), QSettings::IniFormat);
    ProjectListModel model(&settings);
    model.setInstalls(&installs);
    QSignalSpy created(&model, &ProjectListModel::created);
    QSignalSpy errors(&model, &ProjectListModel::errorOccurred);

    const QString location = root.filePath("projects");
    model.create(QStringLiteral("MyGame"), location, QStringLiteral("1.10"));
    QVERIFY(created.wait(10000));
    QCOMPARE(int(created.count()), 1);
    QVERIFY(!model.creating());
    QCOMPARE(model.rowCount(), 1);
    const QString path = model.data(model.index(0), ProjectListModel::PathRole).toString();
    QCOMPARE(created.at(0).at(0).toString(), path);
    QVERIFY(QFileInfo::exists(path + QStringLiteral("/MyGame.flaxproj")));
    const QStringList expected = {
        QStringLiteral("-new"),
        QStringLiteral("-exit"),
        QStringLiteral("-headless"),
        QStringLiteral("-std"),
        QStringLiteral("-project=\"%1\"").arg(path),
    };
    QCOMPARE(readArguments(argvFile), expected);

    model.create(QStringLiteral("MyGame"), location, QStringLiteral("1.10"));
    QCOMPARE(int(errors.count()), 1);
    QCOMPARE(errors.at(0).at(0).toString(), QStringLiteral("Could not create project"));
    QCOMPARE(errors.at(0).at(1).toString(), QStringLiteral("%1 is already in the list.").arg(path));
    QCOMPARE(model.rowCount(), 1);

    QVERIFY(QDir(path).removeRecursively());
    model.create(QStringLiteral("MyGame"), location, QStringLiteral("1.10"));
    QCOMPARE(int(errors.count()), 2);
    QCOMPARE(model.rowCount(), 1);
}

// A run that fails removes the folder the editor made, so a retry is not blocked by it. A folder that was there before
// the run is kept. A run that writes no project file is reported as such.
void ProjectListModelTest::createRemovesFolderOfFailedRun()
{
    if (!canMakeFakeEngine())
        QSKIP("The fake engine needs git and a POSIX shell.");

    QTemporaryDir root;
    QVERIFY(root.isValid());
    const QString argvFile = root.filePath("argv.txt");
    const QString engine = root.filePath("engine");
    QVERIFY(writeFakeEngine(engine, QStringLiteral("1.10"), argvFile));
    saveInstall(root.filePath("installs.ini"), QStringLiteral("1.10"), engine);

    QSettings installSettings(root.filePath("installs.ini"), QSettings::IniFormat);
    InstallListModel installs(&installSettings);
    QTRY_VERIFY_WITH_TIMEOUT(installs.isInstalled(QStringLiteral("1.10")), 10000);

    QSettings settings(root.filePath("projects.ini"), QSettings::IniFormat);
    ProjectListModel model(&settings);
    model.setInstalls(&installs);
    QSignalSpy created(&model, &ProjectListModel::created);
    QSignalSpy errors(&model, &ProjectListModel::errorOccurred);

    const QString location = root.filePath("projects");
    const QString broken = location + QStringLiteral("/Broken");
    QVERIFY(QDir().mkpath(broken));
    model.create(QStringLiteral("Broken"), location, QStringLiteral("1.10"));
    QVERIFY(errors.wait(10000));
    QCOMPARE(errors.at(0).at(0).toString(), QStringLiteral("Could not create project"));
    QCOMPARE(errors.at(0).at(1).toString(), QStringLiteral("FlaxEditor exited with code 3. Editor failure"));
    QVERIFY(QFileInfo(broken).isDir());
    QVERIFY(!model.creating());

    // Now the editor makes the folder, so the failure removes it.
    QVERIFY(QDir(broken).removeRecursively());
    model.create(QStringLiteral("Broken"), location, QStringLiteral("1.10"));
    QVERIFY(errors.wait(10000));
    QVERIFY(!QFileInfo::exists(broken));

    model.create(QStringLiteral("Silent"), location, QStringLiteral("1.10"));
    QVERIFY(errors.wait(10000));
    QVERIFY(errors.last().at(1).toString().startsWith(QStringLiteral("FlaxEditor did not create ")));
    QCOMPARE(int(created.count()), 0);
    QCOMPARE(model.rowCount(), 0);
}

// An editor that cannot be started is reported with the reason the start gave, and no folder is left behind.
void ProjectListModelTest::createReportsEditorThatCannotStart()
{
    if (!canMakeFakeEngine())
        QSKIP("The fake engine needs git and a POSIX shell.");

    QTemporaryDir root;
    QVERIFY(root.isValid());
    const QString argvFile = root.filePath("argv.txt");
    const QString engine = root.filePath("engine");
    QVERIFY(writeFakeEngine(engine, QStringLiteral("1.10"), argvFile));
    saveInstall(root.filePath("installs.ini"), QStringLiteral("1.10"), engine);

    QSettings installSettings(root.filePath("installs.ini"), QSettings::IniFormat);
    InstallListModel installs(&installSettings);
    QTRY_VERIFY_WITH_TIMEOUT(installs.isInstalled(QStringLiteral("1.10")), 10000);

    // The install check has passed, so the editor is still listed. Taking its execute bit makes the start fail.
    const QString editor = installs.editorExecutable(QStringLiteral("1.10"));
    QVERIFY(!editor.isEmpty());
    QVERIFY(QFile::setPermissions(editor, QFile::ReadOwner | QFile::WriteOwner));

    QSettings settings(root.filePath("projects.ini"), QSettings::IniFormat);
    ProjectListModel model(&settings);
    model.setInstalls(&installs);
    QSignalSpy errors(&model, &ProjectListModel::errorOccurred);

    const QString location = root.filePath("projects");
    model.create(QStringLiteral("Game"), location, QStringLiteral("1.10"));
    QVERIFY(errors.wait(10000));
    QCOMPARE(errors.at(0).at(0).toString(), QStringLiteral("Could not create project"));
    QVERIFY(errors.at(0).at(1).toString().startsWith(QStringLiteral("Could not start FlaxEditor: ")));
    QVERIFY(!model.creating());
    QCOMPARE(model.rowCount(), 0);
    QVERIFY(!QFileInfo::exists(location + QStringLiteral("/Game")));
}

QTEST_MAIN(ProjectListModelTest)

#include "tst_projectlistmodel.moc"
