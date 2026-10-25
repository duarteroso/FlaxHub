#include "filemanager.h"

#include <QDir>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QTest>

class FileManagerTest : public QObject
{
    Q_OBJECT

private slots:
    void folderUrlOfEmptyPathIsAnExistingFolder();
    void folderUrlOfExistingFolderIsThatFolder();
    void folderUrlOfMissingFolderIsNearestExistingParent();
    void displayPathShowsHomeAsTilde();
};

namespace {

QString canonical(const QUrl &url)
{
    return QFileInfo(url.toLocalFile()).canonicalFilePath();
}

} // namespace

void FileManagerTest::folderUrlOfEmptyPathIsAnExistingFolder()
{
    FileManager manager;
    const QUrl url = manager.folderUrl(QString());
    QVERIFY(url.isLocalFile());
    QVERIFY(QFileInfo(url.toLocalFile()).isDir());
}

void FileManagerTest::folderUrlOfExistingFolderIsThatFolder()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());

    FileManager manager;
    QCOMPARE(canonical(manager.folderUrl(root.path())), QFileInfo(root.path()).canonicalFilePath());
}

// A folder that is missing opens at the nearest parent that exists, however deep the missing part is.
void FileManagerTest::folderUrlOfMissingFolderIsNearestExistingParent()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());

    FileManager manager;
    const QUrl url = manager.folderUrl(root.filePath("missing/deeper"));
    QVERIFY(url.isLocalFile());
    QCOMPARE(canonical(url), QFileInfo(root.path()).canonicalFilePath());
}

// The home folder is shown as "~" and a folder whose name only starts with it is left as it is. The test uses the real
// home folder, since it does not point HOME at a temporary one.
void FileManagerTest::displayPathShowsHomeAsTilde()
{
    FileManager manager;
    const QString home = QDir::homePath();
    QCOMPARE(manager.displayPath(home), QStringLiteral("~"));
    QCOMPARE(manager.displayPath(home + QStringLiteral("/FlaxHub")), QStringLiteral("~/FlaxHub"));
    QCOMPARE(manager.displayPath(home + QStringLiteral("2/FlaxHub")), home + QStringLiteral("2/FlaxHub"));
}

QTEST_APPLESS_MAIN(FileManagerTest)

#include "tst_filemanager.moc"
