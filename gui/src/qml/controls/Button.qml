import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Effects

Button {
    id: control
    property bool firstInFocusChain: false
    property bool lastInFocusChain: false
    property bool sendOutput: false

    Material.background: visualFocus ? Material.accent : undefined
    
    // Enhanced futuristic styling
    background: Rectangle {
        radius: 8
        color: control.visualFocus ? "#F59E0B" : Qt.rgba(245/255, 158/255, 11/255, 0.1)
        border.color: control.visualFocus ? "transparent" : "#F59E0B"
        border.width: control.visualFocus ? 0 : 1
        
        Behavior on color { ColorAnimation { duration: 200 } }
        Behavior on border.color { ColorAnimation { duration: 200 } }
        
        // Glow effect for focused buttons
        Rectangle {
            anchors.fill: parent
            radius: parent.radius
            color: "transparent"
            border.color: "#F59E0B"
            border.width: 2
            opacity: control.visualFocus ? 0.6 : 0
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
    
    contentItem: Text {
        text: control.text
        font: control.font
        color: control.visualFocus ? "#000000" : "#F59E0B"
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        Behavior on color { ColorAnimation { duration: 200 } }
    }

    Component.onDestruction: {
        if (visualFocus) {
            let item = nextItemInFocusChain();
            if (item)
                item.forceActiveFocus(Qt.TabFocusReason);
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
        case Qt.Key_Return:
            if (visualFocus) {
                clicked();
            }
            event.accepted = true;
            break;
        }
    }
}
