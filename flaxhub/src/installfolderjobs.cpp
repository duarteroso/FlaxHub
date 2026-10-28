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

QString removeInstallFolder(const QString &path, const QString &version)
{
    if (!QFileInfo::exists(path))
        return {};

    if (!InstallRules::mayUninstall(path, InstallValidator::hasCompleteCheckout(path, version))) {
        return QStringLiteral("The folder was not made by FlaxHub and is not a complete FlaxEngine checkout, "
                              "so it was not deleted.");
    }
    if (!QDir(path).removeRecursively())
        return QStringLiteral("Could not delete the folder");

    return {};
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

void InstallFolderJobs::removeInstall(const QString &version, const QString &path,
                                      const std::function<void(const QString &)> &onRemoved)
{
    auto *watcher = new QFutureWatcher<QString>(this);
    connect(watcher, &QFutureWatcher<QString>::finished, this, [watcher, onRemoved]() {
        watcher->deleteLater();
        onRemoved(watcher->result());
    });
    watcher->setFuture(QtConcurrent::run([path, version]() {
        return removeInstallFolder(path, version);
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
