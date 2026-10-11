import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Effects

Slider {
    id: control
    property bool firstInFocusChain: false
    property bool lastInFocusChain: false
    property bool sendOutput: false
    
    // Enhanced futuristic styling
    background: Rectangle {
        x: control.leftPadding
        y: control.topPadding + control.availableHeight / 2 - height / 2
        implicitWidth: 200
        implicitHeight: 4
        width: control.availableWidth
        height: implicitHeight
        radius: 2
        color: Qt.rgba(255, 255, 255, 0.2)
        
        Rectangle {
            width: control.visualPosition * parent.width
            height: parent.height
            color: "#F59E0B"
            radius: parent.radius
            
            Rectangle {
                anchors.fill: parent
                radius: parent.radius
                color: "#F59E0B"
                opacity: 0.5
                
                layer.enabled: true
                layer.effect: MultiEffect {
                    blurEnabled: true
                    blurMax: 8
                    blur: 0.4
                }
            }
        }
    }
    
    handle: Rectangle {
        x: control.leftPadding + control.visualPosition * (control.availableWidth - width)
        y: control.topPadding + control.availableHeight / 2 - height / 2
        implicitWidth: 20
        implicitHeight: 20
        radius: 10
        color: control.pressed ? "#ffffff" : "#F59E0B"
        border.color: control.visualFocus ? "#ffffff" : "transparent"
        border.width: control.visualFocus ? 2 : 0
        
        Behavior on color { ColorAnimation { duration: 200 } }
        Behavior on border.color { ColorAnimation { duration: 200 } }
        
        // Glow effect for focused handle
        Rectangle {
            anchors.fill: parent
            radius: parent.radius
            color: "transparent"
            border.color: "#F59E0B"
            border.width: 2
            opacity: control.visualFocus ? 0.7 : 0
            visible: opacity > 0
            
            layer.enabled: control.visualFocus
            layer.effect: MultiEffect {
                blurEnabled: true
                blurMax: 12
                blur: 0.8
            }
            
            Behavior on opacity { NumberAnimation { duration: 200 } }
        }
    }

    Keys.onPressed: (event) => {
        switch (event.key) {
        case Qt.Key_Up:
            if (!firstInFocusChain) {
                let item = nextItemInFocusChain(false);
                if (item)
                    item.forceActiveFocus(Qt.TabFocusReason);
                if(!sendOutput)
                    event.accepted = true;
            }
            break;
        case Qt.Key_Down:
            if (!lastInFocusChain) {
                let item = nextItemInFocusChain();
                if (item)
                    item.forceActiveFocus(Qt.TabFocusReason);
                if(!sendOutput)
                    event.accepted = true;
            }
            break;
        }
    }
}
