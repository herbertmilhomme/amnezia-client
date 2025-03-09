import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import PageEnum 1.0
import Style 1.0

import "./"
import "../Controls2"
import "../Config"

PageType {
    id: root

    BackButtonType {
        id: backButton
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.topMargin: 20
    }

    FlickableType {
        id: fl
        anchors.top: backButton.bottom
        anchors.bottom: parent.bottom
        contentHeight: content.height

        ColumnLayout {
            id: content
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right

            HeaderTypeWithSwitcher {
                Layout.fillWidth: true
                Layout.leftMargin: 16
                Layout.rightMargin: 16

                headerText: qsTr("Kill Switch")
                descriptionText: qsTr("Enable to ensure network traffic goes through a secure VPN tunnel, preventing accidental exposure of your IP and DNS queries if the connection drops")

                showSwitcher: true
                switcher {
                    checked: SettingsController.isKillSwitchEnabled
                    enabled: !ConnectionController.isConnected
                }
                switcherFunction: function(checked) {
                    if (!ConnectionController.isConnected) {
                        SettingsController.isKillSwitchEnabled = checked
                    } else {
                        PageController.showNotificationMessage(qsTr("Cannot change killSwitch settings during active connection"))
                        switcher.checked = SettingsController.isKillSwitchEnabled
                    }
                }
            }

            VerticalRadioButton {
                id: softKillSwitch
                Layout.fillWidth: true
                Layout.topMargin: 32
                Layout.leftMargin: 16
                Layout.rightMargin: 16

                enabled: SettingsController.isKillSwitchEnabled && !ConnectionController.isConnected
                checked: !SettingsController.strictKillSwitchEnabled

                text: qsTr("Soft Kill Switch")
                descriptionText: qsTr("Internet connection is blocked if VPN connection drops accidentally")

                onClicked: {
                    SettingsController.strictKillSwitchEnabled = false
                }
            }

            DividerType {}

            VerticalRadioButton {
                id: strictKillSwitch
                Layout.fillWidth: true
                Layout.leftMargin: 16
                Layout.rightMargin: 16

                enabled: SettingsController.isKillSwitchEnabled && !ConnectionController.isConnected
                checked: SettingsController.strictKillSwitchEnabled

                text: qsTr("Strict Kill Switch")
                descriptionText: qsTr("Internet connection is blocked even if VPN was turned off manually or not started")

                onClicked: {
                    SettingsController.strictKillSwitchEnabled = true
                }
            }

            DividerType {}
            
            LabelWithButtonType {
                Layout.topMargin: 32
                Layout.fillWidth: true

                enabled: true
                text: qsTr("Kill Switch Exceptions")
                descriptionText: qsTr("IP addresses that will remain accessible even when Kill Switch is activated")
                rightImageSource: "qrc:/images/controls/chevron-right.svg"

                clickedFunction: function() {
                    PageController.goToPage(PageEnum.PageSettingsKillSwitchExceptions)
                }
            }
        }
    }
} 