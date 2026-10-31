#ifndef INSTALLRULES_H
#define INSTALLRULES_H

#include "installvalidator.h"

#include <QList>
#include <QString>

// The rules for an install's folder, kept apart from InstallListModel. They take only paths, version strings and
// validator results, so tests can check them without a model, a settings file or a process.
namespace InstallRules {

// An install folder is given as a full path. A relative one would resolve against the working folder of the app, which
// is not a folder the user chose. The path is not expanded, so "~" must be expanded before this is called.
bool isFullPath(const QString &path);

// True for a folder that exists and has nothing in it. Hidden and system entries count, so a folder that holds only a
// .git directory is not empty.
bool isEmptyFolder(const QString &path);

// A clone can go only into a folder that is missing or empty. Any other folder already holds files that the clone
// would not own, so nothing is written into it.
bool isFreeFolder(const QString &path);

// The path comes from the saved settings, so it is checked before anything is deleted. The path is resolved first,
// so ".." and links in its parents cannot lead to a folder other than the one named. A filesystem root, the home folder
// and any folder that holds the home folder are never removed. The path itself must be a folder and not a link, since
// a delete through a link empties the folder it points to.
bool isSafeToRemove(const QString &path);

// The marker the Hub writes into an engine folder once its clone has made the folder: a file named .flaxhub-install,
// so the folder records that FlaxHub created it. git refuses to clone into a folder that already has files, so the
// marker waits for the clone. A clone that was cut short has no marker, so Uninstall of a Failed entry keeps it unless
// it is a complete checkout. An Invalid entry is deleted either way.
QString markerPath(const QString &path);
// Writes the marker into the folder at path, which must exist. Returns false when it cannot be written.
bool writeMarker(const QString &path);
bool hasMarker(const QString &path);

// Uninstall deletes an Installed or Failed entry's folder only when isSafeToRemove allows it and either the Hub made it
// (it has a marker) or it is a complete checkout (completeCheckout, as InstallValidator::hasCompleteCheckout reports).
// Any other folder is kept, even one the user chose, since it may hold files the Hub does not own. An Invalid entry is
// not checked this way: its folder is deleted under isSafeToRemove alone (see InstallFolderJobs::removeInstall).
bool mayUninstall(const QString &path, bool completeCheckout);

// A clone needs a folder that is missing or empty. A folder that is not a complete checkout is cleared first (see
// needsClear), so a clone is needed for either. A folder whose checkout could not be checked (GitUnavailable) is neither
// cloned nor cleared, since it may be complete.
bool needsClone(const QList<InstallValidator::Failure> &failures);

// Clearing deletes the files in a folder that has some but is not a complete checkout, so Fix asks before it.
bool needsClear(const QList<InstallValidator::Failure> &failures);

} // namespace InstallRules

#endif
