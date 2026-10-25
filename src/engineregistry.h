#ifndef ENGINEREGISTRY_H
#define ENGINEREGISTRY_H

#include <QAbstractListModel>
#include <QString>
#include <QStringList>
#include <QtQml/qqmlregistration.h>

// The installed engines, as the projects see them. ProjectListModel reads the installs through this, so it does not
// depend on how they are installed. It is a list model because the installs view lists the same rows, and a QObject
// can have only one QObject base. QML cannot create one, but a property of this type takes an InstallListModel.
class EngineRegistry : public QAbstractListModel
{
    Q_OBJECT
    QML_ANONYMOUS

public:
    explicit EngineRegistry(QObject *parent = nullptr)
        : QAbstractListModel(parent)
    {
        // Rows that come or go can change the installed set as well.
        connect(this, &QAbstractItemModel::rowsInserted, this, &EngineRegistry::installedEnginesChanged);
        connect(this, &QAbstractItemModel::rowsRemoved, this, &EngineRegistry::installedEnginesChanged);
        connect(this, &QAbstractItemModel::modelReset, this, &EngineRegistry::installedEnginesChanged);
    }

    virtual bool isInstalled(const QString &version) const = 0;
    // The versions that are installed, newest first.
    virtual QStringList installedVersions() const = 0;
    // The editor binary of an installed engine. Empty unless the version is installed.
    virtual QString editorExecutable(const QString &version) const = 0;
    // The engine a project opens with when its file names none. Empty when nothing is installed.
    virtual QString preferredVersion() const = 0;

signals:
    // The installed set may have changed. It can fire when the set stays the same, so a listener compares.
    void installedEnginesChanged();
};

#endif
