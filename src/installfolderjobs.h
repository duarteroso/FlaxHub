#ifndef INSTALLFOLDERJOBS_H
#define INSTALLFOLDERJOBS_H

#include "installvalidator.h"

#include <QList>
#include <QObject>
#include <QString>

#include <functional>

// The folder work an install needs that can take a while: the checks, which run git, and the deletes, since a cloned
// engine has thousands of files. Each job runs on a worker thread and reads only the path and the version it is given,
// so it does not touch the model. Its callback runs on the GUI thread once the job is done.
class InstallFolderJobs : public QObject
{
    Q_OBJECT

public:
    using Failure = InstallValidator::Failure;

    explicit InstallFolderJobs(QObject *parent = nullptr);

    // Runs InstallValidator on the folder. onChecked gets the failed checks. checking() is true until then.
    void check(const QString &version, const QString &path, const std::function<void(const QList<Failure> &)> &onChecked);

    // Deletes an install's folder, but only when InstallRules::mayUninstall allows it, and its marker goes with it.
    // onRemoved gets an empty string once the folder is gone, else the reason it was kept.
    void removeInstall(const QString &version, const QString &path, const std::function<void(const QString &)> &onRemoved);

    // Clears a folder for a Fix. The user has confirmed the clear, so only the path guards apply. onCleared gets whether
    // the folder is gone. A folder that is already gone counts as removed.
    void clear(const QString &path, const std::function<void(bool)> &onCleared);

    // True while a check that has been started has not returned yet.
    bool checking() const;

private:
    int m_pendingChecks = 0;
};

#endif
