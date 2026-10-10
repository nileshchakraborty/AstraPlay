import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import QtQuick.Controls.Material

import org.streetpea.chiaking

import "controls" as C

Dialog {
    id: updateDialog
    title: UpdateManager.updateAvailable ? qsTr("Update Available") : qsTr("Software Update")
    parent: Overlay.overlay
    x: Math.round((parent.width - width) / 2)
    y: Math.round((parent.height - height) / 2)
    width: Math.min(600, parent.width - 40)
    modal: true
    Material.roundedScale: Material.MediumScale

    background: Rectangle {
        color: Material.dialogColor
        radius: 12
        border.color: Material.accent
        border.width: 2
    }

    onOpened: {
        if (UpdateManager.updateAvailable)
            updateNowButton.forceActiveFocus(Qt.TabFocusReason);
        else if (checkAgainButton.visible)
            checkAgainButton.forceActiveFocus(Qt.TabFocusReason);
        else
            closeButton.forceActiveFocus(Qt.TabFocusReason);
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 16

        // Header / Version info
        RowLayout {
            spacing: 12
            Layout.fillWidth: true

            Rectangle {
                width: 44
                height: 44
                radius: 22
                color: Qt.rgba(0, 212/255, 255/255, 0.15)
                border.color: Material.accent
                border.width: 1

                Image {
                    anchors.centerIn: parent
                    width: 24
                    height: 24
                    source: "qrc:/icons/refresh-24px.svg"
                }
            }

            ColumnLayout {
                spacing: 2
                Layout.fillWidth: true

                Label {
                    text: {
                        if (UpdateManager.checking)
                            return qsTr("Checking for updates...");
                        if (UpdateManager.updateAvailable)
                            return qsTr("New version %1 is available!").arg(UpdateManager.latestVersion);
                        return UpdateManager.statusMessage.length > 0 ? UpdateManager.statusMessage : qsTr("Pylux is up to date (v%1).").arg(UpdateManager.currentVersion);
                    }
                    font.bold: true
                    font.pixelSize: 16
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }

                Label {
                    text: qsTr("Status: %1").arg(UpdateManager.statusMessage)
                    font.pixelSize: 12
                    opacity: 0.75
                    visible: UpdateManager.updateAvailable
                }
            }
        }

        // Release notes box (when update available)
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 140
            color: Qt.rgba(255, 255, 255, 0.05)
            radius: 8
            border.color: Qt.rgba(255, 255, 255, 0.1)
            border.width: 1
            visible: UpdateManager.updateAvailable && UpdateManager.releaseNotes.length > 0

            ScrollView {
                anchors.fill: parent
                anchors.margins: 10
                clip: true

                TextArea {
                    readOnly: true
                    text: UpdateManager.releaseNotes
                    wrapMode: Text.WordWrap
                    color: "white"
                    font.pixelSize: 12
                    background: null
                }
            }
        }

        // Download progress bar
        ColumnLayout {
            Layout.fillWidth: true
            visible: UpdateManager.downloading || (UpdateManager.downloadProgress > 0 && UpdateManager.downloadProgress < 1.0)
            spacing: 6

            ProgressBar {
                Layout.fillWidth: true
                value: UpdateManager.downloadProgress
                Material.accent: "#00d4ff"
            }

            Label {
                text: qsTr("Downloading: %1%").arg(Math.round(UpdateManager.downloadProgress * 100))
                font.pixelSize: 12
                Layout.alignment: Qt.AlignHCenter
                opacity: 0.8
            }
        }

        // Action Buttons
        RowLayout {
            Layout.alignment: Qt.AlignRight
            spacing: 12

            // Check Again button
            C.Button {
                id: checkAgainButton
                text: qsTr("Check Again")
                visible: !UpdateManager.updateAvailable && !UpdateManager.checking
                onClicked: UpdateManager.checkForUpdates(true)
            }

            // View on GitHub button
            C.Button {
                text: qsTr("Release Page")
                visible: UpdateManager.updateAvailable && UpdateManager.releaseUrl.length > 0
                onClicked: Qt.openUrlExternally(UpdateManager.releaseUrl)
            }

            // Update / Download / Install button
            C.Button {
                id: updateNowButton
                text: {
                    if (UpdateManager.downloadProgress >= 1.0)
                        return qsTr("Install & Restart");
                    if (UpdateManager.downloading)
                        return qsTr("Downloading...");
                    return qsTr("Update Now");
                }
                visible: UpdateManager.updateAvailable
                enabled: !UpdateManager.downloading
                Material.background: Material.accent
                onClicked: {
                    if (UpdateManager.downloadProgress >= 1.0) {
                        UpdateManager.installAndRestart();
                    } else {
                        UpdateManager.startDownload();
                    }
                }
            }

            // Close / Cancel button
            C.Button {
                id: closeButton
                text: UpdateManager.downloading ? qsTr("Cancel") : qsTr("Close")
                onClicked: {
                    if (UpdateManager.downloading)
                        UpdateManager.cancelDownload();
                    updateDialog.close();
                }
            }
        }
    }

    Connections {
        target: UpdateManager
        function onDownloadFinished(filePath) {
            // Auto trigger install prompt or notify user
        }
    }
}
