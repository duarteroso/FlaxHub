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

QTEST_APPLESS_MAIN(FileManagerTest)

#include "tst_filemanager.moc"
