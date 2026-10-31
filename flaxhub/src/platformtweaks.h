#pragma once

// Platform settings that must be applied before QGuiApplication is created.
namespace PlatformTweaks {

// macOS adds "Enter Full Screen" to the View menu by itself. The Hub's window has a fixed size, so the item is turned off.
// Does nothing on other platforms.
void disableFullScreenMenuItem();

}
