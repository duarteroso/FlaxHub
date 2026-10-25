#ifndef PROJECTLISTMODEL_H
#define PROJECTLISTMODEL_H

#include <QAbstractListModel>
#include <QByteArray>
#include <QDateTime>
#include <QList>
#include <QPointer>
#include <QSet>
#include <QString>
#include <QtQml/qqmlregistration.h>

#include <memory>

class EngineRegistry;
class QSettings;

class ProjectListModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(EngineRegistry *installs READ installs WRITE setInstalls NOTIFY installsChanged)
    Q_PROPERTY(bool creating READ creating NOTIFY creatingChanged)

public:
    enum Roles {
        NameRole = Qt::UserRole + 1,
        PathRole,
        EngineVersionRole,
        LastOpenedRole,
        PinnedRole,
        EngineInstalledRole
    };

    // The projects are saved in the application's settings.
    explicit ProjectListModel(QObject *parent = nullptr);
    // The projects are saved in settings, which must outlive the model. Tests give a temporary file here.
    ProjectListModel(QSettings *settings, QObject *parent = nullptr);
    ~ProjectListModel() override;

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;
    QHash<int, QByteArray> roleNames() const override;

    EngineRegistry *installs() const;
    void setInstalls(EngineRegistry *installs);

    Q_INVOKABLE void remove(const QString &path);

    // Pins or unpins the project at path. The views keep pinned projects in their own group, so the project moves there.
    Q_INVOKABLE void setPinned(const QString &path, bool pinned);

    // Starts the project's engine editor on the project and marks the project as opened now. editorArguments is the
    // saved setting. It is split at white space outside quotes and passed after the project, and the quotes stay in the
    // arguments, since the engine reads them. A failure is reported through errorOccurred: the project file is missing,
    // the engine is not installed or the editor does not start.
    Q_INVOKABLE void open(const QString &path, const QString &editorArguments);

    bool creating() const;

    // The name the editor gives a project made from this folder name: its letters and digits only.
    Q_INVOKABLE QString sanitizedName(const QString &folderName) const;

    // Creates location/folderName with the engine's editor and adds it to the list once its .flaxproj exists.
    // Only one project is created at a time. A failure is reported through errorOccurred.
    Q_INVOKABLE void create(const QString &folderName, const QString &location, const QString &engineVersion);

    // Adds the project in folder, which must hold a .flaxproj file. Its files are left as they are. The engine is the
    // one the project file names, or the default install when it names none. A failure is reported through errorOccurred.
    Q_INVOKABLE void addExisting(const QString &folder);

signals:
    void installsChanged();
    void creatingChanged();
    void created(const QString &path);
    // A user action failed, and the window tells the user why. The title and message are for display.
    void errorOccurred(const QString &title, const QString &message);

private:
    struct Project {
        QString name;
        QString path;
        QString engineVersion;
        QDateTime lastOpened;
        bool pinned = false;
    };

    void loadProjects();
    void saveProjects();
    void insertProject(const Project &project);
    int rowFor(const QString &path) const;
    void refreshInstalled();
    void setCreating(bool creating);
    void finishCreate(const Project &project);

    QList<Project> m_projects;
    // Owned only when the default constructor was used. m_settings points at it, or at the settings a test passed in.
    std::unique_ptr<QSettings> m_ownedSettings;
    QSettings *m_settings = nullptr;
    // The engines that were installed when EngineInstalledRole was last emitted.
    QSet<QString> m_installedVersions;
    QPointer<EngineRegistry> m_installs;
    bool m_creating = false;
};

#endif
