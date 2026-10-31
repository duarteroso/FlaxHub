#include "installfolderjobs.h"

#include "installrules.h"

#include <QDir>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QtConcurrentRun>

namespace {

bool removeFolder(const QString &path)
{
    if (!QFileInfo::exists(path))
        return true;

    return InstallRules::isSafeToRemove(path) && QDir(path).removeRecursively();
}

InstallFolderJobs::RemoveResult removeInstallFolder(const QString &path, const QString &version, bool invalidEntry)
{
    using RemoveResult = InstallFolderJobs::RemoveResult;

    if (!QFileInfo::exists(path))
        return RemoveResult::Removed;

    if (!InstallRules::isSafeToRemove(path))
        return RemoveResult::UnsafePath;

    // The checkout check runs git, so it is only made for an entry that needs it.
    if (!invalidEntry && !InstallRules::mayUninstall(path, InstallValidator::hasCompleteCheckout(path, version)))
        return RemoveResult::NotOwned;

    return QDir(path).removeRecursively() ? RemoveResult::Removed : RemoveResult::DeleteFailed;
}

} // namespace

InstallFolderJobs::InstallFolderJobs(QObject *parent)
    : QObject(parent)
{
}

void InstallFolderJobs::check(const QString &version, const QString &path,
                              const std::function<void(const QList<Failure> &)> &onChecked)
{
    ++m_pendingChecks;
    auto *watcher = new QFutureWatcher<QList<Failure>>(this);
    connect(watcher, &QFutureWatcher<QList<Failure>>::finished, this, [this, watcher, onChecked]() {
        watcher->deleteLater();
        --m_pendingChecks;
        onChecked(watcher->result());
    });
    watcher->setFuture(QtConcurrent::run([path, version]() {
        return InstallValidator::validate(path, version);
    }));
}

void InstallFolderJobs::removeInstall(const QString &version, const QString &path, bool invalidEntry,
                                      const std::function<void(RemoveResult)> &onRemoved)
{
    auto *watcher = new QFutureWatcher<RemoveResult>(this);
    connect(watcher, &QFutureWatcher<RemoveResult>::finished, this, [watcher, onRemoved]() {
        watcher->deleteLater();
        onRemoved(watcher->result());
    });
    watcher->setFuture(QtConcurrent::run([path, version, invalidEntry]() {
        return removeInstallFolder(path, version, invalidEntry);
    }));
}

void InstallFolderJobs::clear(const QString &path, const std::function<void(bool)> &onCleared)
{
    auto *watcher = new QFutureWatcher<bool>(this);
    connect(watcher, &QFutureWatcher<bool>::finished, this, [watcher, onCleared]() {
        watcher->deleteLater();
        onCleared(watcher->result());
    });
    watcher->setFuture(QtConcurrent::run([path]() {
        return removeFolder(path);
    }));
}

bool InstallFolderJobs::checking() const
{
    return m_pendingChecks > 0;
}
