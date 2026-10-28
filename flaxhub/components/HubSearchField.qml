import QtQuick
import FlaxHub

// The search box above a list. The placeholder is the name screen readers give it.
HubTextField {
    Accessible.name: placeholderText
}
