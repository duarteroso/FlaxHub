#ifndef INSTALLLISTMODEL_H
#define INSTALLLISTMODEL_H

#include <QAbstractListModel>
#include <QByteArray>
#include <QList>
#include <QNetworkAccessManager>
#include <QString>
#include <QStringList>
#include <QtQml/qqmlregistration.h>

#include <functional>

class InstallListModel : public QAbstractListModel
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
        Removing
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
        FixableRole
    };

    explicit InstallListModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    bool isInstalled(const QString &version) const;

    // Whether the version can be installed now. Installed and running versions cannot.
    Q_INVOKABLE bool canInstall(const QString &version) const;

    // The editor binary in an installed engine's folder. Empty unless the version is Installed.
    Q_INVOKABLE QString editorExecutable(const QString &version) const;

    // The engine a project opens with when its file names none: the default install, else the newest one. Empty when
    // nothing is installed.
    Q_INVOKABLE QString preferredVersion() const;

    VersionFetchState versionFetchState() const;

    // True while an install is cloning, generating or building. A removal does not count, since a delete that
    // is cut short is checked again on the next run.
    bool busy() const;

    // Lists the FlaxEngine release tags as Available installs. Only the first successful fetch
    // is kept; a failed one is tried again on the next call.
    Q_INVOKABLE void fetchAvailableVersions();

    // An empty path installs to the default location.
    Q_INVOKABLE void startInstall(const QString &version, const QString &path);
    // Deletes an installed or failed engine's folder in the background. An Invalid entry is only removed from
    // the list, since its folder is missing or does not pass the check.
    Q_INVOKABLE void uninstall(const QString &version);
    // Points an Invalid install whose folder was not found at the folder the user located. The folder is checked
    // like one loaded at startup, so the install is Installed again if it passes, and Invalid with the new reason if not.
    Q_INVOKABLE void relocate(const QString &version, const QString &path);
    // Whether Fix would clear the folder first, which deletes its files. The view asks the user before that.
    Q_INVOKABLE bool fixClearsFolder(const QString &version) const;
    // Re-checks an Invalid install and runs the steps its failed checks need. A folder that needs clearing is only
    // cleared when clearConfirmed is set. Returns true when a step started, so the caller can show the progress.
    Q_INVOKABLE bool fix(const QString &version, bool clearConfirmed);
    Q_INVOKABLE void setDefault(const QString &version);

signals:
    void versionFetchStateChanged();
    void busyChanged();

private:
    struct Install {
        QString version;
        QString channel;
        InstallState state;
        double progress;
        QString path;
        bool isDefault;
        QString log;
        // stepProgress is 0..1 for a step that reports a fraction, and -1 for one that does not.
        InstallStep step;
        double stepProgress;
        // The last line of log is a progress line that the next output replaces.
        bool logLineIsProgress = false;
        // The reasons an Invalid install failed its checks, one per line. Empty for every other state.
        QString problem;
        // Set when an Invalid install's folder was not found. Only that case can be located again.
        bool pathMissing = false;
    };

    int rowFor(const QString &version) const;
    void loadInstalls();
    void saveInstalls() const;
    void cloneRepository(const QString &version, const QString &path);
    void runStep(const QString &version, const QString &program, const QStringList &arguments,
                 const QString &workingDirectory, const std::function<void()> &onSuccess);
    void appendOutput(const QString &version, const QByteArray &output);
    void finishClone(const QString &version);
    void beginGenerate(const QString &version);
    void finishGenerate(const QString &version);
    void beginBuild(const QString &version);
    void finishBuild(const QString &version);
    void finishInstall(const QString &version);
    void failInstall(const QString &version, const QString &message);
    void finishUninstall(const QString &version, bool removed);
    void resetToAvailable(int row);
    void updateBusy();

    void setVersionFetchState(VersionFetchState state);
    void requestTagPage(int page, const QStringList &tags);
    void finishVersionFetch(const QStringList &tags);

    QList<Install> m_installs;
    QNetworkAccessManager m_network;
    VersionFetchState m_versionFetchState = NotFetched;
    bool m_busy = false;
};

#endif
