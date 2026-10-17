import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import QtQuick.Controls.Material

import io.github.nileshchakraborty.astraplay

import "controls" as C

Dialog {
    id: updateDialog
    parent: Overlay.overlay
    x: Math.round((root.width - width) / 2)
    y: Math.round((root.height - height) / 2)
    width: Math.min(root.width * 0.9, 620)
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
        color: Material.dialogColor
        radius: 12
        border.color: Material.accent
        border.width: 2
        clip: true
    }

    Component.onCompleted: {
        header.visible = false;
        header.background = null;
    }

    onOpened: {
        if (UpdateManager.updateAvailable)
            updateNowButton.forceActiveFocus(Qt.TabFocusReason);
        else if (checkAgainButton.visible)
            checkAgainButton.forceActiveFocus(Qt.TabFocusReason);
        else
            closeButton.forceActiveFocus(Qt.TabFocusReason);
    }

    Keys.onEscapePressed: {
        if (UpdateManager.downloading)
            UpdateManager.cancelDownload();
        updateDialog.close();
    }

    contentItem: ColumnLayout {
        id: dialogContentLayout
        spacing: 0

        // Custom Header Title Bar matching app styling
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 52
            color: Qt.rgba(245/255, 158/255, 11/255, 0.1)

            Label {
                anchors.fill: parent
                anchors.leftMargin: 20
                anchors.rightMargin: 20
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                font.pixelSize: 20
                font.bold: true
                color: Material.accent
                text: UpdateManager.updateAvailable ? qsTr("Software Update Available") : qsTr("Software Update")
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.margins: 20
            spacing: 16

            // Header / Version info
            RowLayout {
                spacing: 14
                Layout.fillWidth: true

                Rectangle {
                    Layout.preferredWidth: 46
                    Layout.preferredHeight: 46
                    radius: 23
                    color: Qt.rgba(245/255, 158/255, 11/255, 0.15)
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
                    spacing: 3
                    Layout.fillWidth: true

                    Label {
                        text: {
                            if (UpdateManager.checking)
                                return qsTr("Checking for updates...");
                            if (UpdateManager.readyToInstall || UpdateManager.downloadProgress >= 1.0)
                                return qsTr("Version %1 downloaded and ready to install!").arg(UpdateManager.latestVersion);
                            if (UpdateManager.updateAvailable)
                                return qsTr("AstraPlay %1 is available!").arg(UpdateManager.latestVersion);
                            return UpdateManager.statusMessage.length > 0 ? UpdateManager.statusMessage : qsTr("AstraPlay is up to date (v%1).").arg(UpdateManager.currentVersion);
                        }
                        font.bold: true
                        font.pixelSize: 16
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                    }

                    Label {
                        text: {
                            if (UpdateManager.failed)
                                return qsTr("Status: %1").arg(UpdateManager.lastError);
                            if (UpdateManager.updateAvailable)
                                return qsTr("Installed version: v%1  •  Latest version: %2").arg(UpdateManager.currentVersion).arg(UpdateManager.latestVersion);
                            return qsTr("Current version: v%1").arg(UpdateManager.currentVersion);
                        }
                        font.pixelSize: 12
                        color: UpdateManager.failed ? "#F44336" : Material.accent
                        opacity: UpdateManager.failed ? 1.0 : 0.8
                    }
                }
            }

            // Release notes box (when update available)
            Rectangle {
                id: releaseNotesBox
                Layout.fillWidth: true
                Layout.preferredHeight: Math.min(Math.max(dialogContentLayout.height * 0.35, 140), 220)
                color: Qt.rgba(255, 255, 255, 0.04)
                radius: 8
                border.color: Qt.rgba(255, 255, 255, 0.1)
                border.width: 1
                visible: UpdateManager.updateAvailable && UpdateManager.releaseNotes.length > 0

                ScrollView {
                    id: releaseNotesScrollView
                    anchors.fill: parent
                    anchors.margins: 10
                    clip: true

                    TextArea {
                        id: releaseNotesArea
                        readOnly: true
                        text: UpdateManager.releaseNotes
                        textFormat: Text.MarkdownText
                        wrapMode: Text.WordWrap
                        color: "#E5E7EB"
                        font.pixelSize: 13
                        background: null
                        selectByMouse: true

                        KeyNavigation.priority: KeyNavigation.BeforeItem
                        KeyNavigation.down: updateNowButton.visible ? updateNowButton : closeButton
                    }
                }
            }

            // Download progress bar & metrics
            ColumnLayout {
                Layout.fillWidth: true
                visible: UpdateManager.downloading || (UpdateManager.downloadProgress > 0 && UpdateManager.downloadProgress < 1.0)
                spacing: 6

                ProgressBar {
                    Layout.fillWidth: true
                    value: UpdateManager.downloadProgress
                    Material.accent: "#F59E0B"
                }

                RowLayout {
                    Layout.fillWidth: true

                    Label {
                        text: qsTr("Downloading: %1%").arg(Math.round(UpdateManager.downloadProgress * 100))
                        font.pixelSize: 12
                        font.bold: true
                        color: Material.accent
                    }

                    Item { Layout.fillWidth: true }

                    Label {
                        text: UpdateManager.downloadSizeString + (UpdateManager.downloadSpeed.length > 0 ? "  (" + UpdateManager.downloadSpeed + ")" : "")
                        font.pixelSize: 12
                        opacity: 0.85
                        color: "white"
                    }
                }
            }

            // Error notice banner
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 38
                radius: 6
                color: Qt.rgba(244/255, 67/255, 54/255, 0.15)
                border.color: "#F44336"
                border.width: 1
                visible: UpdateManager.failed

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 12
                    anchors.rightMargin: 12
                    spacing: 8

                    Label {
                        Layout.fillWidth: true
                        text: UpdateManager.lastError.length > 0 ? UpdateManager.lastError : qsTr("Download encountered an error. Please try again.")
                        color: "#F44336"
                        font.pixelSize: 12
                        elide: Text.ElideRight
                    }
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
                    visible: !UpdateManager.packageUpdatesManaged && !UpdateManager.updateAvailable && !UpdateManager.checking
                    onClicked: UpdateManager.checkForUpdates(true)

                    KeyNavigation.priority: KeyNavigation.BeforeItem
                    KeyNavigation.right: closeButton
                    KeyNavigation.up: releaseNotesBox.visible ? releaseNotesArea : checkAgainButton
                }

                // View on GitHub release page
                C.Button {
                    id: releasePageButton
                    text: qsTr("Release Page")
                    visible: UpdateManager.updateAvailable && UpdateManager.releaseUrl.length > 0
                    onClicked: Qt.openUrlExternally(UpdateManager.releaseUrl)

                    KeyNavigation.priority: KeyNavigation.BeforeItem
                    KeyNavigation.right: updateNowButton
                    KeyNavigation.up: releaseNotesBox.visible ? releaseNotesArea : releasePageButton
                }

                // Update / Download / Install / Retry button
                C.Button {
                    id: updateNowButton
                    text: {
                        if (UpdateManager.readyToInstall || UpdateManager.downloadProgress >= 1.0)
                            return qsTr("Install & Restart");
                        if (UpdateManager.downloading)
                            return qsTr("Downloading...");
                        if (UpdateManager.failed)
                            return qsTr("Retry Download");
                        return qsTr("Update Now");
                    }
                    visible: UpdateManager.updateAvailable
                    enabled: !UpdateManager.downloading
                    Material.background: Material.accent
                    flat: false

                    leftPadding: 42
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
                        if (UpdateManager.readyToInstall || UpdateManager.downloadProgress >= 1.0) {
                            UpdateManager.installAndRestart();
                        } else if (UpdateManager.failed) {
                            UpdateManager.retryDownload();
                        } else {
                            UpdateManager.startDownload();
                        }
                    }

                    KeyNavigation.priority: KeyNavigation.BeforeItem
                    KeyNavigation.left: releasePageButton.visible ? releasePageButton : (checkAgainButton.visible ? checkAgainButton : updateNowButton)
                    KeyNavigation.right: closeButton
                    KeyNavigation.up: releaseNotesBox.visible ? releaseNotesArea : updateNowButton
                }

                // Close / Cancel button
                C.Button {
                    id: closeButton
                    text: UpdateManager.downloading ? qsTr("Cancel") : qsTr("Close")
                    flat: true
                    leftPadding: 36
                    rightPadding: 16

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

                    onClicked: {
                        if (UpdateManager.downloading)
                            UpdateManager.cancelDownload();
                        updateDialog.close();
                    }

                    KeyNavigation.priority: KeyNavigation.BeforeItem
                    KeyNavigation.left: updateNowButton.visible ? updateNowButton : (checkAgainButton.visible ? checkAgainButton : closeButton)
                    KeyNavigation.up: releaseNotesBox.visible ? releaseNotesArea : closeButton
                }
            }
        }
    }

    Connections {
        target: UpdateManager
        function onDownloadFinished(filePath) {
            updateNowButton.forceActiveFocus(Qt.TabFocusReason);
        }
        function onFailedChanged() {
            if (UpdateManager.failed)
                updateNowButton.forceActiveFocus(Qt.TabFocusReason);
        }
    }
}
