import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import QtQuick.Controls.Material

import io.github.nileshchakraborty.astraplay

// Responsive controller overlay - shows how to open stream menu across handheld resolutions
Item {
    id: root

    signal dismissed()
    property bool active: false

    visible: active
    enabled: active

    // Dark backdrop
    Rectangle {
        anchors.fill: parent
        color: "#000000"
        opacity: 0.85
    }

    // Responsive container centered in screen with vertical offset for dismiss button clearance
    Item {
        id: container
        anchors.centerIn: parent
        anchors.verticalCenterOffset: -Math.round(parent.height * 0.04)
        width: Math.min(parent.width * 0.92, 960)
        height: Math.min(parent.height * 0.74, 520)

        // L1 Button & Label (Top Left)
        Rectangle {
            id: l1Button
            anchors {
                left: parent.left
                leftMargin: Math.round(container.width * 0.08)
                top: parent.top
                topMargin: 24
            }
            width: Math.min(100, container.width * 0.12)
            height: Math.min(60, container.height * 0.13)
            color: "#1a1a1a"
            border.color: Material.accent
            border.width: 3
            radius: 8

            Label {
                anchors.centerIn: parent
                text: "L1"
                font.pixelSize: Math.round(parent.height * 0.46)
                font.bold: true
                color: Material.accent
            }
        }

        Label {
            anchors {
                bottom: l1Button.top
                bottomMargin: 3
                horizontalCenter: l1Button.horizontalCenter
            }
            text: "(bumper)"
            font.pixelSize: 13
            color: "#888888"
            horizontalAlignment: Text.AlignHCenter
        }

        // R1 Button & Label (Top Right)
        Rectangle {
            id: r1Button
            anchors {
                right: parent.right
                rightMargin: Math.round(container.width * 0.08)
                top: parent.top
                topMargin: 24
            }
            width: Math.min(100, container.width * 0.12)
            height: Math.min(60, container.height * 0.13)
            color: "#1a1a1a"
            border.color: Material.accent
            border.width: 3
            radius: 8

            Label {
                anchors.centerIn: parent
                text: "R1"
                font.pixelSize: Math.round(parent.height * 0.46)
                font.bold: true
                color: Material.accent
            }
        }

        Label {
            anchors {
                bottom: r1Button.top
                bottomMargin: 3
                horizontalCenter: r1Button.horizontalCenter
            }
            text: "(bumper)"
            font.pixelSize: 13
            color: "#888888"
            horizontalAlignment: Text.AlignHCenter
        }

        // Center instruction box
        Rectangle {
            id: instructionBox
            anchors.centerIn: parent
            width: Math.min(400, container.width * 0.44)
            height: Math.min(200, container.height * 0.40)
            color: "#1a1a1a"
            border.color: Material.accent
            border.width: 2
            radius: 15

            ColumnLayout {
                anchors.centerIn: parent
                spacing: Math.round(instructionBox.height * 0.06)

                Label {
                    Layout.alignment: Qt.AlignHCenter
                    text: qsTr("Press L1 + R1")
                    font.pixelSize: Math.max(18, Math.round(instructionBox.height * 0.14))
                    font.bold: true
                    color: "#ffffff"
                }

                Label {
                    Layout.alignment: Qt.AlignHCenter
                    text: qsTr("and L3 + R3")
                    font.pixelSize: Math.max(18, Math.round(instructionBox.height * 0.14))
                    font.bold: true
                    color: "#ffffff"
                }

                Rectangle {
                    Layout.alignment: Qt.AlignHCenter
                    width: Math.round(instructionBox.width * 0.25)
                    height: 2
                    color: Material.accent
                    opacity: 0.5
                }

                Label {
                    Layout.alignment: Qt.AlignHCenter
                    text: qsTr("To open Stream Menu")
                    font.pixelSize: Math.max(14, Math.round(instructionBox.height * 0.10))
                    font.bold: true
                    color: Material.accent
                }
            }
        }

        // L3 Button (Stick Click - Left)
        Rectangle {
            id: l3Button
            anchors {
                horizontalCenter: l1Button.horizontalCenter
                verticalCenter: instructionBox.verticalCenter
            }
            width: Math.min(90, container.height * 0.18)
            height: width
            radius: width / 2
            color: "#1a1a1a"
            border.color: Material.accent
            border.width: 3

            Label {
                anchors.centerIn: parent
                text: "L3"
                font.pixelSize: Math.round(parent.height * 0.3)
                font.bold: true
                color: Material.accent
            }
        }

        Label {
            anchors {
                top: l3Button.bottom
                topMargin: 5
                horizontalCenter: l3Button.horizontalCenter
            }
            text: "(click stick)"
            font.pixelSize: 13
            color: "#888888"
            horizontalAlignment: Text.AlignHCenter
        }

        // R3 Button (Stick Click - Right)
        Rectangle {
            id: r3Button
            anchors {
                horizontalCenter: r1Button.horizontalCenter
                verticalCenter: instructionBox.verticalCenter
            }
            width: Math.min(90, container.height * 0.18)
            height: width
            radius: width / 2
            color: "#1a1a1a"
            border.color: Material.accent
            border.width: 3

            Label {
                anchors.centerIn: parent
                text: "R3"
                font.pixelSize: Math.round(parent.height * 0.3)
                font.bold: true
                color: Material.accent
            }
        }

        Label {
            anchors {
                top: r3Button.bottom
                topMargin: 5
                horizontalCenter: r3Button.horizontalCenter
            }
            text: "(click stick)"
            font.pixelSize: 13
            color: "#888888"
            horizontalAlignment: Text.AlignHCenter
        }

        // Angled lines connecting L1/R1 to center box (single canvas with dynamic repaint)
        Canvas {
            id: lineCanvas
            anchors.fill: parent
            onPaint: {
                var ctx = getContext("2d");
                ctx.reset();
                ctx.strokeStyle = Material.accent;
                ctx.lineWidth = 3;

                // L1 line - from bottom center of L1 to top left of instruction box
                ctx.beginPath();
                ctx.moveTo(l1Button.x + l1Button.width / 2, l1Button.y + l1Button.height);
                ctx.lineTo(instructionBox.x, instructionBox.y);
                ctx.stroke();

                // R1 line - from bottom center of R1 to top right of instruction box
                ctx.beginPath();
                ctx.moveTo(r1Button.x + r1Button.width / 2, r1Button.y + r1Button.height);
                ctx.lineTo(instructionBox.x + instructionBox.width, instructionBox.y);
                ctx.stroke();
            }

            Connections {
                target: container
                function onWidthChanged() { lineCanvas.requestPaint(); }
                function onHeightChanged() { lineCanvas.requestPaint(); }
            }
        }

        // Left stick to center horizontal line
        Rectangle {
            x: l3Button.x + l3Button.width
            y: Math.round(l3Button.y + l3Button.height / 2 - 1.5)
            width: Math.max(0, instructionBox.x - (l3Button.x + l3Button.width))
            height: 3
            color: Material.accent
        }

        // Right stick to center horizontal line
        Rectangle {
            x: instructionBox.x + instructionBox.width
            y: Math.round(r3Button.y + r3Button.height / 2 - 1.5)
            width: Math.max(0, r3Button.x - (instructionBox.x + instructionBox.width))
            height: 3
            color: Material.accent
        }
    }

    // Dismiss button at bottom with safe spacing
    Button {
        id: dismissButton
        anchors {
            bottom: parent.bottom
            horizontalCenter: parent.horizontalCenter
            bottomMargin: Math.max(16, Math.round(parent.height * 0.04))
        }
        width: Math.min(240, parent.width * 0.5)
        height: 50
        text: qsTr("Dismiss (B)")
        font.pixelSize: 18
        font.bold: true
        Material.background: Material.accent
        Material.roundedScale: Material.MediumScale

        onClicked: root.dismiss()
        Keys.onReturnPressed: root.dismiss()
        Keys.onEscapePressed: root.dismiss()

        Component.onCompleted: {
            if (root.active)
                forceActiveFocus()
        }
    }

    Keys.onEscapePressed: dismiss()

    function dismiss() {
        active = false;
        dismissed();
    }

    onActiveChanged: {
        if (active)
            dismissButton.forceActiveFocus()
    }

    opacity: active ? 1.0 : 0.0
    Behavior on opacity {
        NumberAnimation { duration: 300 }
    }
}
