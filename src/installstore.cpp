#include "installstore.h"

#include "settingsarray.h"

#include <QSettings>

namespace {

constexpr const char *installsGroup = "Installs";

} // namespace

QList<InstallStore::SavedInstall> InstallStore::load(QSettings &settings)
{
    QList<SavedInstall> installs;
    SettingsArray::read(settings, installsGroup, [&installs](QSettings &entry) {
        SavedInstall install;
        install.version = entry.value("version").toString();
        install.path = entry.value("path").toString();
        install.isDefault = entry.value("isDefault").toBool();
        installs.append(install);
    });

    return installs;
}

bool InstallStore::save(QSettings &settings, const QList<InstallTable::Install> &installs)
{
    return SettingsArray::write(settings, installsGroup, installs, [](QSettings &entry, const InstallTable::Install &install) {
        if (install.state == InstallListModel::Available)
            return false;

        entry.setValue("version", install.version);
        entry.setValue("path", install.path);
        entry.setValue("isDefault", install.isDefault);
        return true;
    });
}
