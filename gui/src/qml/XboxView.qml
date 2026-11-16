import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Effects

import io.github.nileshchakraborty.astraplay

import "controls" as C

Pane {
    id: root
    padding: 0

    property var mainTabBar: null
    property var settingsButton: null
    property var showConfirmDialogFunc: null

    property bool isLoggingIn: false
    property string userCode: ""
    property string verificationUri: ""
    property string qrCodeUrl: ""
    property int expiresIn: 0
    property string loginStatusMessage: ""

    // Clean blue background matching AstraPlay theme
    CleanBlueBackground {
        anchors.fill: parent
        z: -2
    }

    Connections {
        target: XboxAuth

        function onDeviceCodeReceived(code, uri, qr, expires) {
            root.isLoggingIn = true;
            root.userCode = code;
            root.verificationUri = uri;
            root.qrCodeUrl = qr;
            root.expiresIn = expires;
            root.loginStatusMessage = qsTr("Waiting for authentication approval...");
        }

        function onAuthenticated(gamertag, uhs) {
            root.isLoggingIn = false;
            root.loginStatusMessage = "";
            XboxAuth.fetchConsoles();
        }

        function onAuthFailed(errorMessage) {
            root.isLoggingIn = false;
            root.loginStatusMessage = errorMessage;
        }

        function onLoggedOut() {
            root.isLoggingIn = false;
            root.userCode = "";
            root.verificationUri = "";
        }
    }

    Component.onCompleted: {
        if (XboxAuth.isLoggedIn) {
            XboxAuth.fetchConsoles();
        }
    }

    onVisibleChanged: {
        if (visible && XboxAuth.isLoggedIn) {
            XboxAuth.fetchConsoles();
        }
    }

    // Header bar (when logged in)
    Rectangle {
        id: xboxHeaderBar
        anchors {
            top: parent.top
            left: parent.left
            right: parent.right
        }
        height: 60
        color: Qt.rgba(10/255, 15/255, 26/255, 0.85)
        border.color: Qt.rgba(16/255, 124/255, 65/255, 0.3)
        border.width: 1
        visible: XboxAuth.isLoggedIn
        z: 10

        RowLayout {
            anchors {
                fill: parent
                leftMargin: 24
                rightMargin: 24
            }
            spacing: 16

            // Xbox profile pill
            Rectangle {
                Layout.preferredHeight: 38
                Layout.preferredWidth: profileRow.implicitWidth + 24
                radius: 19
                color: Qt.rgba(16/255, 124/255, 65/255, 0.15)
                border.color: "#107C10"
                border.width: 1

                RowLayout {
                    id: profileRow
                    anchors.centerIn: parent
                    spacing: 8

                    Rectangle {
                        width: 12
                        height: 12
                        radius: 6
                        color: "#107C10"
                    }

                    Text {
                        text: XboxAuth.gamertag.length > 0 ? XboxAuth.gamertag : qsTr("Xbox User")
                        color: "#FFFFFF"
                        font.pixelSize: 14
                        font.weight: Font.DemiBold
                    }
                }
            }

            Item { Layout.fillWidth: true }

            // Refresh Consoles button
            C.Button {
                text: qsTr("Refresh")
                Layout.preferredHeight: 36
                onClicked: XboxAuth.fetchConsoles()
            }

            // Sign Out button
            C.Button {
                text: qsTr("Sign Out")
                Layout.preferredHeight: 36
                onClicked: XboxAuth.logout()
            }
        }
    }

    // Scrollable container for logged-in view
    ScrollView {
        id: loggedInScrollView
        anchors {
            top: xboxHeaderBar.bottom
            left: parent.left
            right: parent.right
            bottom: parent.bottom
            margins: 20
        }
        visible: XboxAuth.isLoggedIn
        clip: true

        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
        ScrollBar.vertical: ScrollBar {
            width: 8
            policy: ScrollBar.AsNeeded
            background: Rectangle { color: Qt.rgba(255, 255, 255, 0.1); radius: 4 }
            contentItem: Rectangle { radius: 4; color: "#107C10"; opacity: 0.7 }
        }

        ColumnLayout {
            width: loggedInScrollView.width - 20
            spacing: 24

            // Section 1: My Consoles (xHome)
            Text {
                text: qsTr("My Xbox Consoles")
                color: "#107C10"
                font.pixelSize: 20
                font.weight: Font.Bold
            }

            // Consoles list
            Repeater {
                model: XboxAuth.consoles

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 110
                    radius: 12
                    color: Qt.rgba(17/255, 24/255, 39/255, 0.85)
                    border.color: Qt.rgba(16/255, 124/255, 65/255, 0.4)
                    border.width: 1

                    RowLayout {
                        anchors {
                            fill: parent
                            margins: 16
                        }
                        spacing: 20

                        // Console icon placeholder
                        Rectangle {
                            width: 56
                            height: 56
                            radius: 10
                            color: Qt.rgba(16/255, 124/255, 65/255, 0.2)
                            border.color: "#107C10"
                            border.width: 1

                            Text {
                                anchors.centerIn: parent
                                text: "X"
                                color: "#107C10"
                                font.pixelSize: 28
                                font.weight: Font.Black
                            }
                        }

                        // Info column
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 4

                            Text {
                                text: modelData.name || qsTr("Xbox Console")
                                color: "#FFFFFF"
                                font.pixelSize: 18
                                font.weight: Font.DemiBold
                            }

                            RowLayout {
                                spacing: 8

                                Rectangle {
                                    width: 8
                                    height: 8
                                    radius: 4
                                    color: modelData.powerState === "On" ? "#107C10" :
                                           (modelData.powerState === "ConnectedStandby" ? "#F59E0B" : "#6B7280")
                                }

                                Text {
                                    text: modelData.powerState === "On" ? qsTr("Online") :
                                          (modelData.powerState === "ConnectedStandby" ? qsTr("Standby") : qsTr("Offline"))
                                    color: Qt.rgba(255, 255, 255, 0.7)
                                    font.pixelSize: 13
                                }

                                Text {
                                    text: "• " + (modelData.consoleType || qsTr("Xbox Series X|S"))
                                    color: Qt.rgba(255, 255, 255, 0.5)
                                    font.pixelSize: 13
                                }
                            }
                        }

                        // Action buttons
                        RowLayout {
                            spacing: 12

                            // Wake up button (if in standby)
                            C.Button {
                                text: qsTr("Wake Up")
                                visible: modelData.powerState === "ConnectedStandby"
                                Layout.preferredHeight: 40
                                onClicked: XboxAuth.wakeConsole(modelData.id)
                            }

                            // Turn Off button (if On)
                            C.Button {
                                text: qsTr("Turn Off")
                                visible: modelData.powerState === "On"
                                Layout.preferredHeight: 40
                                onClicked: XboxAuth.turnOffConsole(modelData.id)
                            }

                            // Stream button
                            Button {
                                text: qsTr("Stream")
                                Layout.preferredHeight: 40
                                Layout.preferredWidth: 110
                                background: Rectangle {
                                    radius: 8
                                    color: "#107C10"
                                }
                                contentItem: Text {
                                    text: parent.text
                                    color: "#FFFFFF"
                                    font.pixelSize: 14
                                    font.weight: Font.Bold
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                }
                                onClicked: {
                                    AstraPlay.startXboxStream(modelData.id, "", modelData.name || qsTr("Xbox Console"));
                                }
                            }
                        }
                    }
                }
            }

            // Empty state for consoles
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 120
                radius: 12
                color: Qt.rgba(17/255, 24/255, 39/255, 0.6)
                border.color: Qt.rgba(255, 255, 255, 0.1)
                border.width: 1
                visible: XboxAuth.consoles.length === 0

                ColumnLayout {
                    anchors.centerIn: parent
                    spacing: 8

                    Text {
                        text: qsTr("No Xbox consoles discovered")
                        color: "#FFFFFF"
                        font.pixelSize: 16
                        font.weight: Font.Medium
                        Layout.alignment: Qt.AlignHCenter
                    }

                    Text {
                        text: qsTr("Ensure 'Remote features' are enabled on your Xbox (Settings > Devices & connections > Remote features).")
                        color: Qt.rgba(255, 255, 255, 0.6)
                        font.pixelSize: 13
                        Layout.alignment: Qt.AlignHCenter
                    }
                }
            }

            Item { Layout.preferredHeight: 12 }

            // Section 2: Cloud Gaming (xCloud)
            Text {
                text: qsTr("Xbox Cloud Gaming (xCloud)")
                color: "#107C10"
                font.pixelSize: 20
                font.weight: Font.Bold
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 140
                radius: 12
                color: Qt.rgba(17/255, 24/255, 39/255, 0.85)
                border.color: Qt.rgba(16/255, 124/255, 65/255, 0.4)
                border.width: 1

                ColumnLayout {
                    anchors {
                        fill: parent
                        margins: 16
                    }
                    spacing: 12

                    Text {
                        text: qsTr("Direct Cloud Game Launch")
                        color: "#FFFFFF"
                        font.pixelSize: 16
                        font.weight: Font.DemiBold
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 12

                        TextField {
                            id: cloudTitleInput
                            Layout.fillWidth: true
                            Layout.preferredHeight: 42
                            placeholderText: qsTr("Enter Xbox Title ID (or game title, e.g. Cricket 24)...")
                            color: "#FFFFFF"
                            background: Rectangle {
                                radius: 8
                                color: Qt.rgba(255, 255, 255, 0.08)
                                border.color: cloudTitleInput.activeFocus ? "#107C10" : Qt.rgba(255, 255, 255, 0.15)
                                border.width: 1
                            }
                        }

                        Button {
                            text: qsTr("Stream Game")
                            Layout.preferredHeight: 42
                            Layout.preferredWidth: 140
                            background: Rectangle {
                                radius: 8
                                color: "#107C10"
                            }
                            contentItem: Text {
                                text: parent.text
                                color: "#FFFFFF"
                                font.pixelSize: 14
                                font.weight: Font.Bold
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }
                            onClicked: {
                                let title = cloudTitleInput.text.trim();
                                if (title.length > 0) {
                                    AstraPlay.startXboxStream("", title, title);
                                }
                            }
                        }
                    }

                    Text {
                        text: qsTr("Streams games directly from Xbox Cloud Gaming servers using WebRTC.")
                        color: Qt.rgba(255, 255, 255, 0.5)
                        font.pixelSize: 12
                    }
                }
            }
        }
    }

    // Not logged in view: Welcome card OR Device Code Login card
    Rectangle {
        anchors.centerIn: parent
        width: Math.min(540, parent.width - 40)
        height: root.isLoggingIn ? 480 : 320
        radius: 16
        color: Qt.rgba(17/255, 24/255, 39/255, 0.95)
        border.color: "#107C10"
        border.width: 1.5
        visible: !XboxAuth.isLoggedIn

        ColumnLayout {
            anchors {
                fill: parent
                margins: 32
            }
            spacing: 16

            // State A: Welcome card
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 16
                visible: !root.isLoggingIn

                Rectangle {
                    Layout.alignment: Qt.AlignHCenter
                    width: 64
                    height: 64
                    radius: 32
                    color: Qt.rgba(16/255, 124/255, 65/255, 0.25)
                    border.color: "#107C10"
                    border.width: 2

                    Text {
                        anchors.centerIn: parent
                        text: "X"
                        color: "#107C10"
                        font.pixelSize: 36
                        font.weight: Font.Black
                    }
                }

                Text {
                    Layout.alignment: Qt.AlignHCenter
                    text: qsTr("Xbox Streaming")
                    color: "#FFFFFF"
                    font.pixelSize: 24
                    font.weight: Font.Bold
                }

                Text {
                    Layout.alignment: Qt.AlignHCenter
                    Layout.fillWidth: true
                    text: qsTr("Stream games from your Xbox Series X|S, Xbox One, or Xbox Cloud Gaming directly to AstraPlay with low-latency WebRTC and native controller input.")
                    color: Qt.rgba(255, 255, 255, 0.7)
                    font.pixelSize: 14
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                }

                Item { Layout.preferredHeight: 8 }

                Button {
                    Layout.alignment: Qt.AlignHCenter
                    Layout.preferredWidth: 260
                    Layout.preferredHeight: 46
                    background: Rectangle {
                        radius: 10
                        color: "#107C10"
                    }
                    contentItem: Text {
                        text: qsTr("Sign In with Xbox")
                        color: "#FFFFFF"
                        font.pixelSize: 15
                        font.weight: Font.Bold
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    onClicked: {
                        XboxAuth.startDeviceCodeLogin();
                    }
                }
            }

            // State B: Device Code Login in progress
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 14
                visible: root.isLoggingIn

                Text {
                    Layout.alignment: Qt.AlignHCenter
                    text: qsTr("Device Code Login")
                    color: "#FFFFFF"
                    font.pixelSize: 22
                    font.weight: Font.Bold
                }

                Text {
                    Layout.alignment: Qt.AlignHCenter
                    Layout.fillWidth: true
                    text: qsTr("Scan the QR code or go to %1 and enter the code:").arg(root.verificationUri.length > 0 ? root.verificationUri : "microsoft.com/link")
                    color: Qt.rgba(255, 255, 255, 0.7)
                    font.pixelSize: 13
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                }

                // Code box
                Rectangle {
                    Layout.alignment: Qt.AlignHCenter
                    Layout.preferredWidth: 260
                    Layout.preferredHeight: 52
                    radius: 8
                    color: Qt.rgba(0, 0, 0, 0.4)
                    border.color: "#107C10"
                    border.width: 1

                    RowLayout {
                        anchors.centerIn: parent
                        spacing: 12

                        Text {
                            text: root.userCode
                            color: "#FFFFFF"
                            font.pixelSize: 24
                            font.weight: Font.Black
                            font.letterSpacing: 4
                            font.family: "Monospace"
                        }

                        C.Button {
                            text: qsTr("Copy")
                            Layout.preferredHeight: 32
                            onClicked: AstraPlay.setClipboardText(root.userCode)
                        }
                    }
                }

                // QR Code
                Image {
                    Layout.alignment: Qt.AlignHCenter
                    Layout.preferredWidth: 160
                    Layout.preferredHeight: 160
                    source: root.verificationUri.length > 0 ? AstraPlay.getQRCodeDataUrl(root.verificationUri, 180) : ""
                    fillMode: Image.PreserveAspectFit
                }

                Text {
                    Layout.alignment: Qt.AlignHCenter
                    text: root.loginStatusMessage
                    color: "#107C10"
                    font.pixelSize: 13
                }

                C.Button {
                    Layout.alignment: Qt.AlignHCenter
                    Layout.preferredWidth: 160
                    Layout.preferredHeight: 36
                    text: qsTr("Cancel")
                    onClicked: {
                        XboxAuth.cancelLogin();
                        root.isLoggingIn = false;
                    }
                }
            }
        }
    }
}
