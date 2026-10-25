#ifndef SETTINGSARRAY_H
#define SETTINGSARRAY_H

#include <QSettings>

// The models save their lists as a QSettings array under one group. These two own the array pattern, so each model
// only says what one entry holds.
namespace SettingsArray {

// Calls readEntry(settings) once per saved entry, with the array index already set.
template<typename ReadEntry>
void read(QSettings &settings, const char *group, ReadEntry readEntry)
{
    const int count = settings.beginReadArray(group);
    for (int i = 0; i < count; ++i) {
        settings.setArrayIndex(i);
        readEntry(settings);
    }
    settings.endArray();
}

// Replaces the group with the items for which writeEntry(settings, item) returns true, and writes them in order.
// An item that returns false is skipped. Returns false when the settings could not be written.
template<typename Items, typename WriteEntry>
bool write(QSettings &settings, const char *group, const Items &items, WriteEntry writeEntry)
{
    settings.remove(group);
    settings.beginWriteArray(group);
    int saved = 0;
    for (const auto &item : items) {
        settings.setArrayIndex(saved);
        if (writeEntry(settings, item))
            ++saved;
    }
    settings.endArray();
    settings.sync();
    return settings.status() == QSettings::NoError;
}

} // namespace SettingsArray

#endif
