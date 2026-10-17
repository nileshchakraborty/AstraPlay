import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import QtQuick.Controls.Material

import io.github.nileshchakraborty.astraplay

import "controls" as C

Dialog {
    id: splashDialog
    property string version: ""
    property Item restoreFocusItem

    parent: Overlay.overlay
    x: Math.round((parent.width - width) / 2)
    y: Math.round((parent.height - height) / 2)
    width: Math.min(parent.width * 0.9, 560)
    modal: true
    dim: true
    closePolicy: Popup.CloseOnEscape
    Material.roundedScale: Material.MediumScale
    padding: 0
    topPadding: 0
    bottomPadding: 0
    leftPadding: 0
    rightPadding: 0

    Overlay.modal: Rectangle {
        color: Qt.rgba(0, 0, 0, 0.75)
    }

    background: Rectangle {
        color: "#111827"
        radius: 12
        border.color: Material.accent
        border.width: 2
        clip: true
    }

    Component.onCompleted: header.visible = false

    onOpened: {
        updateNowButton.forceActiveFocus(Qt.TabFocusReason);
    }

    Keys.onEscapePressed: splashDialog.close()

    contentItem: ColumnLayout {
        spacing: 0

        // Title Header Bar
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 52
            color: Qt.rgba(245/255, 158/255, 11/255, 0.12)

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 20
                anchors.rightMargin: 20
                spacing: 10

                Image {
                    Layout.preferredWidth: 24
                    Layout.preferredHeight: 24
                    source: "qrc:/icons/refresh-24px.svg"
                }

                Label {
                    Layout.fillWidth: true
                    font.pixelSize: 19
                    font.bold: true
                    color: Material.accent
                    text: qsTr("Update Available!")
                }
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.margins: 22
            spacing: 16

            Label {
                Layout.fillWidth: true
                text: qsTr("A new version of AstraPlay is available for download.")
                font.pixelSize: 15
                font.bold: true
                wrapMode: Text.WordWrap
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 64
                radius: 8
                color: Qt.rgba(255, 255, 255, 0.05)
                border.color: Qt.rgba(255, 255, 255, 0.1)
                border.width: 1

                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 12
                    spacing: 14

                    Rectangle {
                        Layout.preferredWidth: 40
                        Layout.preferredHeight: 40
                        radius: 20
                        color: Qt.rgba(245/255, 158/255, 11/255, 0.2)
                        border.color: Material.accent
                        border.width: 1

                        Label {
                            anchors.centerIn: parent
                            text: "★"
                            color: Material.accent
                            font.pixelSize: 18
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2

                        Label {
                            text: qsTr("Latest Release: %1").arg(splashDialog.version.length > 0 ? splashDialog.version : UpdateManager.latestVersion)
                            font.bold: true
                            font.pixelSize: 14
                            color: Material.accent
                        }

                        Label {
                            text: qsTr("Current version: v%1").arg(UpdateManager.currentVersion)
                            font.pixelSize: 12
                            opacity: 0.75
                        }
                    }
                }
            }

            // Status or auto-download progress
            ColumnLayout {
                Layout.fillWidth: true
                visible: UpdateManager.downloading || UpdateManager.readyToInstall
                spacing: 6

                ProgressBar {
                    Layout.fillWidth: true
                    value: UpdateManager.downloadProgress
                    Material.accent: "#F59E0B"
                }

                Label {
                    Layout.alignment: Qt.AlignHCenter
                    text: UpdateManager.readyToInstall
                        ? qsTr("Download complete! Ready to install.")
                        : qsTr("Downloading update in background... (%1%)").arg(Math.round(UpdateManager.downloadProgress * 100))
                    font.pixelSize: 12
                    color: Material.accent
                }
            }

            // Action Buttons
            RowLayout {
                Layout.fillWidth: true
                Layout.topMargin: 6
                spacing: 12

                // Update / Install button
                C.Button {
                    id: updateNowButton
                    Layout.fillWidth: true
                    Layout.preferredHeight: 44
                    text: (UpdateManager.readyToInstall || UpdateManager.downloadProgress >= 1.0)
                        ? qsTr("Install & Restart")
                        : qsTr("Update Now")
                    Material.background: Material.accent
                    flat: false

                    leftPadding: 40
                    rightPadding: 16

                    Image {
                        anchors {
                            left: parent.left
                            verticalCenter: parent.verticalCenter
                            leftMargin: 10
                        }
                        width: 22
                        height: 22
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
                    KeyNavigation.right: notesButton
                }

                // Release Notes button
                C.Button {
                    id: notesButton
                    Layout.preferredHeight: 44
                    text: qsTr("What's New")
                    flat: true
                    leftPadding: 38
                    rightPadding: 14

                    Image {
                        anchors {
                            left: parent.left
                            verticalCenter: parent.verticalCenter
                            leftMargin: 8
                        }
                        width: 20
                        height: 20
                        sourceSize: Qt.size(width, height)
                        source: root.controllerButton("square")
                    }

                    onClicked: {
                        splashDialog.close();
                        root.showUpdateDialog();
                    }

                    KeyNavigation.priority: KeyNavigation.BeforeItem
                    KeyNavigation.left: updateNowButton
                    KeyNavigation.right: remindLaterButton
                }

                // Remind Me Later button
                C.Button {
                    id: remindLaterButton
                    Layout.preferredHeight: 44
                    text: qsTr("Later")
                    flat: true
                    leftPadding: 36
                    rightPadding: 14

                    Image {
                        anchors {
                            left: parent.left
                            verticalCenter: parent.verticalCenter
                            leftMargin: 8
                        }
                        width: 20
                        height: 20
                        sourceSize: Qt.size(width, height)
                        source: root.controllerButton("circle")
                    }

                    onClicked: splashDialog.close()

                    KeyNavigation.priority: KeyNavigation.BeforeItem
                    KeyNavigation.left: notesButton
                }
            }
        }
    }
}
