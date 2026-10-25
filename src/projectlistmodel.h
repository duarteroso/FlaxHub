#ifndef PROJECTLISTMODEL_H
#define PROJECTLISTMODEL_H

#include <QAbstractListModel>
#include <QByteArray>
#include <QDateTime>
#include <QList>
#include <QString>
#include <QtQml/qqmlregistration.h>

class InstallListModel;

class ProjectListModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(InstallListModel *installs READ installs WRITE setInstalls NOTIFY installsChanged)
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

    explicit ProjectListModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;
    QHash<int, QByteArray> roleNames() const override;

    InstallListModel *installs() const;
    void setInstalls(InstallListModel *installs);

    Q_INVOKABLE void remove(const QString &path);

    // Pins or unpins the project at path. The views keep pinned projects in their own group, so the project moves there.
    Q_INVOKABLE void setPinned(const QString &path, bool pinned);

    // Starts the project's engine editor on the project and marks the project as opened now. Returns false when the
    // engine is not installed or the editor does not start.
    Q_INVOKABLE bool open(const QString &path);

    bool creating() const;

    // The name the editor gives a project made from this folder name: its letters and digits only.
    Q_INVOKABLE QString sanitizedName(const QString &folderName) const;

    // Creates location/folderName with the engine's editor and adds it to the list once its .flaxproj exists.
    // Only one project is created at a time.
    Q_INVOKABLE void create(const QString &folderName, const QString &location, const QString &engineVersion);

    // Adds the project in folder, which must hold a .flaxproj file. Its files are left as they are. The engine is the
    // one the project file names, or the default install when it names none.
    Q_INVOKABLE void addExisting(const QString &folder);

signals:
    void installsChanged();
    void creatingChanged();
    void created(const QString &path);
    void createFailed(const QString &message);
    void addFailed(const QString &message);

private:
    struct Project {
        QString name;
        QString path;
        QString engineVersion;
        QDateTime lastOpened;
        bool pinned;
    };

    void loadProjects();
    void saveProjects() const;
    void insertProject(const Project &project);
    int rowFor(const QString &path) const;
    void refreshInstalled();
    void setCreating(bool creating);
    void finishCreate(const Project &project);
    void failCreate(const QString &projectDir, bool existed, const QString &message);

    QList<Project> m_projects;
    InstallListModel *m_installs = nullptr;
    bool m_creating = false;
    // Everything the editor printed during the create, for the reason in a failure message.
    QByteArray m_createOutput;
};

#endif
