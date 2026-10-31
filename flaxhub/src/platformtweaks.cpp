#include "platformtweaks.h"

#include <QtGlobal>

#ifndef Q_OS_MACOS
void PlatformTweaks::disableFullScreenMenuItem() {}
#endif
