#ifndef INSTALLVALIDATOR_H
#define INSTALLVALIDATOR_H

#include <QList>
#include <QString>

namespace InstallValidator {

// A check that failed. Each check has its own reason, and validate reports every check that fails.
enum class Failure {
    PathMissing,
    FolderEmpty,
    NoGitCheckout,
    // Git did not run, so the checkout could not be checked. The folder may be complete, so this is never a reason to
    // clear it.
    GitUnavailable,
    NoGenerateOutput,
    NoBinary
};

// Runs every check on the install of version at path, in order: the folder, the git checkout, the generated files
// and the editor binary. Returns the failed checks, so an empty list means the install is valid.
QList<Failure> validate(const QString &path, const QString &version);

// Whether the folder is a complete checkout of the tag of version, which is the git check validate makes. False when
// git cannot run, so a caller cannot rely on the folder being a checkout.
bool hasCompleteCheckout(const QString &path, const QString &version);

// Whether the editor binary of the install at path exists and can be started. It is a file check only, so it is cheap
// enough for the GUI thread, unlike validate, which runs git.
bool hasReleaseEditor(const QString &path);

// The reason shown for one failed check.
QString describe(Failure failure);

// The reasons for the failed checks, one per line. Empty when no check failed.
QString describe(const QList<Failure> &failures);

} // namespace InstallValidator

#endif
