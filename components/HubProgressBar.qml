import QtQuick
import QtQuick.Controls.Basic
import FlaxHub

// The hub's progress bar. The owner sets `value` and `indeterminate`, and the bar's visibility.
ProgressBar {
    // The style fills the bar with palette.dark and draws the track with palette.midlight. Both are set on the bar
    // only, because palette.dark also outlines popups and palette.midlight is the pressed-item color.
    palette.dark: Theme.accent
    palette.midlight: Theme.border
    from: 0
    to: 1
}
