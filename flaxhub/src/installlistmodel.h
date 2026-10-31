#ifndef INSTALLLISTMODEL_H
#define INSTALLLISTMODEL_H

#include "enginecatalog.h"
#include "engineregistry.h"
#include "installfolderjobs.h"
#include "installsteps.h"
#include "installvalidator.h"

#include <QByteArray>
#include <QList>
#include <QString>
#include <QStringList>
#include <QtQml/qqmlregistration.h>

#include <memory>

class InstallTable;
class QSettings;

// The engines the Hub knows about: the release tags GitHub lists as Available, and the installs on disk. This model
// holds their state and saves the installs. The rows and their changes of state are in an InstallTable, saved by
// InstallStore. The clone, generate and build steps run through InstallSteps, the tags are fetched by an EngineCatalog,
// and the folders are checked and deleted by InstallFolderJobs on worker threads.
class InstallListModel : public EngineRegistry
{
    Q_OBJECT
    QML_ELEMENT

public:
    enum InstallState {
        Available,
        Cloning,
        Generating,
        Building,
        Installed,
        Failed,
        // A saved install whose folder no longer passes InstallValidator. It stays listed until the user fixes or removes it.
        Invalid,
        // The folder is being deleted. The entry leaves the list once the delete finishes.
        Removing,
        // The folder is being checked: at startup, after the last step, or before a Fix or a Locate. The row shows it until
        // the result arrives.
        Checking
    };
    Q_ENUM(InstallState)

    // The steps of an install, in order. An install records the step it is on, or the one that failed.
    enum InstallStep {
        CloneStep,
        GenerateStep,
        BuildStep
    };
    Q_ENUM(InstallStep)

    enum VersionFetchState {
        NotFetched,
        Fetching,
        Fetched,
        FetchFailed
    };
    Q_ENUM(VersionFetchState)
    Q_PROPERTY(VersionFetchState versionFetchState READ versionFetchState NOTIFY versionFetchStateChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)

    enum Roles {
        VersionRole = Qt::UserRole + 1,
        ChannelRole,
        StateRole,
        ProgressRole,
        PathRole,
        IsDefaultRole,
        LogRole,
        RecentRole,
        StepRole,
        StepProgressRole,
        InstallableRole,
        ProblemRole,
        RelocatableRole,
        FixableRole,
        AddableRole
    };

    // The installs are saved in the application's settings.
    explicit InstallListModel(QObject *parent = nullptr);
    // The installs are saved in settings, which must outlive the model. Tests give a temporary file here.
    InstallListModel(QSettings *settings, QObject *parent = nullptr);
    ~InstallListModel() override;

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    bool isInstalled(const QString &version) const override;

    // The versions that are Installed, newest first.
    QStringList installedVersions() const override;

    // Whether the version can be picked as a new install now. Installed, running and failed versions cannot, and neither
    // can any version older than the newest ones (see InstallTable::recentVersionCount). A failed one is retried from
    // the Installs view instead.
    Q_INVOKABLE bool canInstall(const QString &version) const;

    // The editor binary in an installed engine's folder. Empty unless the version is Installed.
    QString editorExecutable(const QString &version) const override;

    // The engine a project opens with when its file names none: the default install, else the newest one. Empty when
    // nothing is installed.
    QString preferredVersion() const override;

    VersionFetchState versionFetchState() const;

    // True while an install is cloning, generating or building. A removal and a folder check do not count: a delete
    // that is cut short is checked again on the next run, and a check writes nothing.
    bool busy() const;

    // True while a folder is being deleted. busy() leaves these out, but quitting still cuts a delete short.
    Q_INVOKABLE bool removing() const;

    // True while a folder check runs: at startup, after the last step, or before a Fix or a Locate. busy() leaves these
    // out, since a check writes nothing, but quitting still leaves one unfinished.
    Q_INVOKABLE bool checking() const;

    // Lists the FlaxEngine release tags as Available installs. Only the first successful fetch
    // is kept; a failed one is tried again on the next call.
    Q_INVOKABLE void fetchAvailableVersions();

    // Whether a clone can go into the folder. The folder must be a full path and missing or empty, so no files are
    // written over.
    Q_INVOKABLE bool canInstallTo(const QString &path) const;
    // The folder an install of version goes into under destination: the destination without surrounding whitespace or
    // trailing "/", then "/" and the version. Empty when the destination is empty. startInstall with an empty path
    // installs under InstallLayout::defaultInstallDirectory() instead.
    Q_INVOKABLE QString installPathFor(const QString &version, const QString &destination) const;
    // An empty path installs to the default location. Returns false when nothing started: the version cannot be
    // installed, the path is not a full path, or the folder is not free. Each refusal is reported through errorOccurred.
    Q_INVOKABLE bool startInstall(const QString &version, const QString &path);
    // Deletes an installed or failed engine's folder in the background, but only when the Hub made it or it is still a
    // complete checkout (see InstallRules::mayUninstall). Otherwise the entry becomes Invalid with the reason. An
    // Invalid entry is removed from the list by deleting its folder whatever it holds, since only FlaxHub creates
    // entries. Every delete still passes InstallRules::isSafeToRemove. If the folder is kept or the delete fails, the
    // Invalid entry stays listed with its problem, and errorOccurred says why. A folder that is already gone just
    // leaves the list.
    Q_INVOKABLE void uninstall(const QString &version);
    // Points an Invalid install whose folder was not found at the folder the user located. Only a folder with the FlaxHub
    // marker is accepted, so a folder FlaxHub did not create is refused. The folder is checked like one loaded at
    // startup, so the install is Installed again if it passes, and Invalid with the new reason if not. A marked folder
    // that is not a checkout of this engine is refused too. Each refusal is reported through errorOccurred, and the
    // install stays as it was. So is a path that is not a full path.
    Q_INVOKABLE void relocate(const QString &version, const QString &path);
    // Re-checks an Invalid install and runs the steps its failed checks need. A folder that needs clearing is only
    // cleared when clearConfirmed is set. Otherwise clearConfirmationRequired is emitted, and the view asks the user
    // and calls fix again. fixStarted is emitted once a step is running.
    Q_INVOKABLE void fix(const QString &version, bool clearConfirmed);
    Q_INVOKABLE void setDefault(const QString &version);

signals:
    void versionFetchStateChanged();
    void busyChanged();
    // The state of an install changed. Log and progress lines do not emit it, so a listener that only needs the
    // installed set can rely on it.
    void installStateChanged();
    // A Fix needs the user to confirm that the folder's files may be deleted. The view asks, then calls fix with
    // clearConfirmed set.
    void clearConfirmationRequired(const QString &version, const QString &path);
    // A Fix has started a step, so the progress dialog can show it.
    void fixStarted(const QString &version, const QString &path);
    // A user action failed, and the window tells the user why. The title and message are for display.
    void errorOccurred(const QString &title, const QString &message);

private:
    using Failure = InstallValidator::Failure;

    void initialize();
    void loadInstalls();
    void saveInstalls();
    void notifyStateChanged(int row);
    void updateBusy();
    void finishUninstall(const QString &version, const QString &path, InstallFolderJobs::RemoveResult result,
                         bool wasInvalid);
    void decideFix(const QString &version, const QList<Failure> &failures, bool clearConfirmed);

    void appendLog(const QString &version, const QString &text);
    void setStepProgress(const QString &version, double fraction);
    void finishStep(const QString &version);
    void finishClone(const QString &version);
    void beginGenerate(const QString &version);
    void finishGenerate(const QString &version);
    void beginBuild(const QString &version);
    void finishInstall(const QString &version);
    void failInstall(const QString &version, const QString &message);

    void setVersionFetchState(VersionFetchState state);
    void finishVersionFetch(const QStringList &versions);

    // The rows and their state. It needs this class's enums, so it is held by pointer.
    std::unique_ptr<InstallTable> m_table;
    // Owned only when the default constructor was used. m_settings points at it, or at the settings a test passed in.
    std::unique_ptr<QSettings> m_ownedSettings;
    QSettings *m_settings = nullptr;
    EngineCatalog m_catalog;
    InstallSteps m_steps;
    InstallFolderJobs m_jobs;
    VersionFetchState m_versionFetchState = NotFetched;
    bool m_busy = false;
    // The last save of the installs failed, so the next failure is not reported again.
    bool m_saveFailed = false;
};

#endif
