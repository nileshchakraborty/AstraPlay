import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

Item {
    id: root

    property var gameData: null
    property bool isCurrentItem: GridView.isCurrentItem || false
    property bool hasFocus: isCurrentItem && GridView.view && GridView.view.activeFocus
    readonly property bool isHovered: cardMouseArea.containsMouse

    signal streamGame(string titleId, string gameName)

    width: 180
    height: 260

    Rectangle {
        id: cardBg
        anchors.fill: parent
        radius: 12
        color: "#111827"
        clip: true

        border.color: root.hasFocus ? "#4ADE80" : (root.isHovered ? "#107C10" : Qt.rgba(255, 255, 255, 0.12))
        border.width: root.hasFocus ? 2.5 : (root.isHovered ? 1.5 : 1)

        scale: (root.isHovered || root.hasFocus) ? 1.03 : 1.0
        Behavior on scale { NumberAnimation { duration: 160; easing.type: Easing.OutCubic } }
        Behavior on border.color { ColorAnimation { duration: 160 } }

        // Placeholder background while loading image
        Rectangle {
            anchors.fill: parent
            color: "#0B0F19"
            radius: 12

            Text {
                anchors.centerIn: parent
                text: "XBOX"
                color: Qt.rgba(255, 255, 255, 0.12)
                font.pixelSize: 22
                font.weight: Font.Black
                font.letterSpacing: 2
            }
        }

        // Cover Art Image
        Image {
            id: coverImage
            anchors.fill: parent
            source: (root.gameData && root.gameData.boxArt) ? root.gameData.boxArt : ""
            fillMode: Image.PreserveAspectCrop
            asynchronous: true
            smooth: true
            mipmap: true
            visible: status === Image.Ready
            opacity: status === Image.Ready ? 1.0 : 0.0
            Behavior on opacity { NumberAnimation { duration: 200 } }
        }

        // Cloud Badge
        Rectangle {
            anchors {
                top: parent.top
                left: parent.left
                margins: 10
            }
            height: 22
            width: badgeRow.implicitWidth + 14
            radius: 6
            color: Qt.rgba(16/255, 124/255, 65/255, 0.92)
            border.color: "#4ADE80"
            border.width: 1
            z: 2

            RowLayout {
                id: badgeRow
                anchors.centerIn: parent
                spacing: 4

                Rectangle {
                    width: 6
                    height: 6
                    radius: 3
                    color: "#4ADE80"
                }

                Text {
                    text: "CLOUD"
                    color: "#FFFFFF"
                    font.pixelSize: 10
                    font.weight: Font.Bold
                    font.letterSpacing: 0.5
                }
            }
        }

        // Bottom Gradient Overlay for Title and Details
        Rectangle {
            id: infoOverlay
            anchors {
                left: parent.left
                right: parent.right
                bottom: parent.bottom
            }
            height: (root.isHovered || root.hasFocus) ? 104 : 76
            Behavior on height { NumberAnimation { duration: 160; easing.type: Easing.OutCubic } }
            z: 3

            gradient: Gradient {
                GradientStop { position: 0.0; color: "transparent" }
                GradientStop { position: 0.35; color: Qt.rgba(10/255, 15/255, 26/255, 0.85) }
                GradientStop { position: 1.0; color: Qt.rgba(7/255, 10/255, 18/255, 0.98) }
            }

            ColumnLayout {
                anchors {
                    fill: parent
                    leftMargin: 10
                    rightMargin: 10
                    bottomMargin: 8
                    topMargin: 16
                }
                spacing: 2

                Text {
                    id: titleText
                    Layout.fillWidth: true
                    text: (root.gameData && root.gameData.name) ? root.gameData.name : qsTr("Unknown Game")
                    color: "#FFFFFF"
                    font.pixelSize: 13
                    font.weight: Font.DemiBold
                    elide: Text.ElideRight
                    maximumLineCount: 2
                    wrapMode: Text.Wrap
                }

                Text {
                    id: subText
                    Layout.fillWidth: true
                    text: {
                        if (!root.gameData) return "";
                        if (root.gameData.categories && root.gameData.categories.length > 0)
                            return root.gameData.categories[0];
                        if (root.gameData.developer)
                            return root.gameData.developer;
                        return "";
                    }
                    color: "#4ADE80"
                    font.pixelSize: 11
                    elide: Text.ElideRight
                    visible: text.length > 0
                }

                Item { Layout.fillHeight: true }

                // Stream Button revealed on hover or focus
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 28
                    radius: 6
                    color: streamMouseArea.containsMouse ? "#169B14" : "#107C10"
                    border.color: "#4ADE80"
                    border.width: 1
                    visible: root.isHovered || root.hasFocus
                    opacity: visible ? 1.0 : 0.0
                    Behavior on opacity { NumberAnimation { duration: 150 } }

                    RowLayout {
                        anchors.centerIn: parent
                        spacing: 6

                        Text {
                            text: "▶"
                            color: "#FFFFFF"
                            font.pixelSize: 10
                        }

                        Text {
                            text: qsTr("Stream")
                            color: "#FFFFFF"
                            font.pixelSize: 12
                            font.weight: Font.Bold
                        }
                    }

                    MouseArea {
                        id: streamMouseArea
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            if (root.gameData) {
                                let launchId = root.gameData.titleId || root.gameData.productId;
                                if (launchId) {
                                    root.streamGame(launchId, root.gameData.name || qsTr("Xbox Game"));
                                }
                            }
                        }
                    }
                }
            }
        }

        // Entire Card Click Handler
        MouseArea {
            id: cardMouseArea
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            z: 1
            onClicked: {
                if (root.gameData) {
                    let launchId = root.gameData.titleId || root.gameData.productId;
                    if (launchId) {
                        root.streamGame(launchId, root.gameData.name || qsTr("Xbox Game"));
                    }
                }
            }
        }
    }

    Keys.onReturnPressed: {
        if (gameData) {
            let launchId = gameData.titleId || gameData.productId;
            if (launchId) {
                streamGame(launchId, gameData.name || qsTr("Xbox Game"));
            }
        }
    }
    Keys.onEnterPressed: Keys.onReturnPressed(event)
}
