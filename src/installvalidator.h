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
    NoGenerateOutput,
    NoBinary
};

// Runs every check on the install of version at path, in order: the folder, the git checkout, the generated files
// and the editor binary. Returns the failed checks, so an empty list means the install is valid.
QList<Failure> validate(const QString &path, const QString &version);

// The reason shown for one failed check.
QString describe(Failure failure);

// The reasons for the failed checks, one per line. Empty when no check failed.
QString describe(const QList<Failure> &failures);

} // namespace InstallValidator

#endif
