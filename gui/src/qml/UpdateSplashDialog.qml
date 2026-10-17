import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import QtQuick.Controls.Material

import io.github.nileshchakraborty.astraplay

Dialog {
    id: splashDialog
    property string version: ""
    property Item restoreFocusItem

    parent: Overlay.overlay
    x: Math.round((root.width - width) / 2)
    y: Math.round((root.height - height) / 2)
    width: Math.min(root.width * 0.9, 520)
    modal: true
    dim: true
    closePolicy: Popup.CloseOnEscape
    Material.roundedScale: Material.MediumScale
    padding: 24

    Overlay.modal: Rectangle {
        color: Qt.rgba(0, 0, 0, 0.75)
    }

    background: Rectangle {
        color: Material.dialogColor
        radius: 12
        border.color: Material.accent
        border.width: 2
    }

    Component.onCompleted: {
        header.horizontalAlignment = Text.AlignHCenter;
        header.background = null;
    }

    onOpened: {
        updateNowButton.forceActiveFocus(Qt.TabFocusReason);
    }

    Keys.onEscapePressed: splashDialog.close()

    ColumnLayout {
        spacing: 18
        anchors.fill: parent

        // Title with icon
        RowLayout {
            Layout.alignment: Qt.AlignHCenter
            spacing: 10

            Image {
                Layout.preferredWidth: 26
                Layout.preferredHeight: 26
                sourceSize: Qt.size(26, 26)
                source: "qrc:/icons/refresh-24px.svg"
            }

            Label {
                text: qsTr("Update Available")
                font.pixelSize: 20
                font.bold: true
                color: Material.accent
            }
        }

        Label {
            Layout.fillWidth: true
            text: qsTr("A new version of AstraPlay is available to install.")
            font.pixelSize: 14
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
            color: "#ffffff"
        }

        // Version card
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 60
            radius: 8
            color: Qt.rgba(0, 0, 0, 0.3)
            border.color: Qt.rgba(245/255, 158/255, 11/255, 0.35)
            border.width: 1

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 16
                anchors.rightMargin: 16
                spacing: 12

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2

                    Label {
                        text: qsTr("New version: %1").arg(splashDialog.version.length > 0 ? splashDialog.version : UpdateManager.latestVersion)
                        font.bold: true
                        font.pixelSize: 14
                        color: Material.accent
                    }

                    Label {
                        text: qsTr("Current version: v%1").arg(UpdateManager.currentVersion)
                        font.pixelSize: 12
                        color: "#94a3b8"
                    }
                }
            }
        }

        // Background download progress bar if active
        ColumnLayout {
            Layout.fillWidth: true
            visible: UpdateManager.downloading || UpdateManager.readyToInstall
            spacing: 4

            ProgressBar {
                Layout.fillWidth: true
                value: UpdateManager.downloadProgress
                Material.accent: "#F59E0B"
            }

            Label {
                Layout.alignment: Qt.AlignHCenter
                text: UpdateManager.readyToInstall
                    ? qsTr("Ready to install.")
                    : qsTr("Downloading... %1%").arg(Math.round(UpdateManager.downloadProgress * 100))
                font.pixelSize: 12
                color: Material.accent
            }
        }

        // Action Buttons
        RowLayout {
            Layout.alignment: Qt.AlignHCenter
            Layout.topMargin: 6
            spacing: 12

            Button {
                id: updateNowButton
                text: (UpdateManager.readyToInstall || UpdateManager.downloadProgress >= 1.0)
                    ? qsTr("Install")
                    : qsTr("Update")
                Material.background: Material.accent
                flat: true
                leftPadding: 44
                rightPadding: 16
                Material.roundedScale: Material.SmallScale

                Image {
                    anchors {
                        left: parent.left
                        verticalCenter: parent.verticalCenter
                        leftMargin: 10
                    }
                    width: 24
                    height: 24
                    sourceSize: Qt.size(width, height)
                    source: root.controllerButton("cross")
                }

                onClicked: {
                    splashDialog.close();
                    if (UpdateManager.readyToInstall || UpdateManager.downloadProgress >= 1.0) {
                        UpdateManager.installAndRestart();
                    } else {
                        root.showUpdateDialog();
                        UpdateManager.startDownload();
                    }
                }

                KeyNavigation.priority: KeyNavigation.BeforeItem
                KeyNavigation.right: detailsButton
            }

            Button {
                id: detailsButton
                text: qsTr("Details")
                Material.background: Material.accent
                flat: true
                leftPadding: 44
                rightPadding: 16
                Material.roundedScale: Material.SmallScale

                Image {
                    anchors {
                        left: parent.left
                        verticalCenter: parent.verticalCenter
                        leftMargin: 10
                    }
                    width: 24
                    height: 24
                    sourceSize: Qt.size(width, height)
                    source: root.controllerButton("square")
                }

                onClicked: {
                    splashDialog.close();
                    root.showUpdateDialog();
                }

                KeyNavigation.priority: KeyNavigation.BeforeItem
                KeyNavigation.left: updateNowButton
                KeyNavigation.right: laterButton
            }

            Button {
                id: laterButton
                text: qsTr("Later")
                Material.background: Material.accent
                flat: true
                leftPadding: 44
                rightPadding: 16
                Material.roundedScale: Material.SmallScale

                Image {
                    anchors {
                        left: parent.left
                        verticalCenter: parent.verticalCenter
                        leftMargin: 10
                    }
                    width: 24
                    height: 24
                    sourceSize: Qt.size(width, height)
                    source: root.controllerButton("circle")
                }

                onClicked: splashDialog.close()

                KeyNavigation.priority: KeyNavigation.BeforeItem
                KeyNavigation.left: detailsButton
            }
        }
    }
}
