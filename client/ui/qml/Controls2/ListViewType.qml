import QtQuick
import QtQuick.Controls

ListView {
    id: root

    property bool isFocusable: true

    ScrollBar.vertical: ScrollBarType {}

    clip: true
    reuseItems: true
}
