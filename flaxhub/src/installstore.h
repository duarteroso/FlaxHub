#ifndef INSTALLSTORE_H
#define INSTALLSTORE_H

#include "installtable.h"

#include <QList>
#include <QString>

class QSettings;

// The installs saved between runs, in the settings group "Installs": the version, the folder and the default flag of
// each one. The model decides when to load and save, and what a failed save means for the user.
namespace InstallStore {

struct SavedInstall {
    QString version;
    QString path;
    bool isDefault = false;
};

// The installs that the last run saved, in the order they were saved.
QList<SavedInstall> load(QSettings &settings);

// Saves every install except the ones still available, since those have no folder. Returns false when the settings
// could not be written.
bool save(QSettings &settings, const QList<InstallTable::Install> &installs);

} // namespace InstallStore

#endif
