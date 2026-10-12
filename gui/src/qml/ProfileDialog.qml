import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import QtQuick.Controls.Material

import io.github.nileshchakraborty.astraplay

import "controls" as C

DialogView {
    id: profileDialog
    readonly property bool steamCloudAvailable: (typeof AstraPlay.clearSteamCloudData === "function")

    buttonText: {
        if(deleteBox.visible && deleteBox.checked)
            qsTr("Delete Profile")
        else if(profileName.visible)
            qsTr("Create Profile")
        else
            qsTr("Switch Profile")
    }
    buttonEnabled: {
        if(deleteBox.visible && deleteBox.checked)
            true
        else if(profileName.visible)
            profileName.text.trim();
        else
            !(profileComboBox.model[profileComboBox.currentIndex] == "default" && AstraPlay.settings.currentProfile == "") && profileComboBox.model[profileComboBox.currentIndex] != AstraPlay.settings.currentProfile
    }
    onAccepted: {
        if(deleteBox.visible && deleteBox.checked) {
            let profileToDelete = profileComboBox.model[profileComboBox.currentIndex]
            // Delete locally first
            AstraPlay.settings.deleteProfile(profileToDelete)
            // Then delete from Steam Cloud if available
            if (typeof AstraPlay.deleteProfileFromCloud === "function") {
                AstraPlay.deleteProfileFromCloud(profileToDelete)
            }
        }
        else if(profileName.visible) {
            AstraPlay.settings.currentProfile = profileName.text.trim()
            stack.pop()
            root.showToast(qsTr("Profile Created"), qsTr("Restart app for changes to take effect"), "#FF9800")
            return
        }
        else {
            let oldProfile = AstraPlay.settings.currentProfile
            let newProfile = profileComboBox.model[profileComboBox.currentIndex] == "default" ? "" : profileComboBox.model[profileComboBox.currentIndex]
            if (oldProfile !== newProfile) {
                AstraPlay.settings.currentProfile = newProfile
                stack.pop()
                root.showToast(qsTr("Profile Switched"), qsTr("Restart app for changes to take effect"), "#FF9800")
                return
            }
        }
        stack.pop()
    }

    Item {
        GridLayout {
            anchors {
                top: parent.top
                horizontalCenter: parent.horizontalCenter
                topMargin: 50
            }
            columns: 2
            rowSpacing: 10
            columnSpacing: 20

            Label {
                Layout.alignment: Qt.AlignRight
                text: qsTr("User Profile:")
            }

            C.ComboBox {
                id: profileComboBox
                Layout.preferredWidth: 400
                firstInFocusChain: false
                model: AstraPlay.settings.profiles
                currentIndex: Math.max(0, model.indexOf(AstraPlay.settings.currentProfile))
                lastInFocusChain: false
                
                // This handler is triggered when user selects an item (press A on item in popup)
                onActivated: (index) => {
                    currentIndex = index;
                    // Auto-focus the "Switch Profile" button for quick profile switching
                    if (headerButton && headerButton.visible && headerButton.enabled) {
                        headerButton.forceActiveFocus(Qt.TabFocusReason);
                    }
                }
                
                // Allow navigation up to dialog header button
                Keys.onPressed: (event) => {
                    // Don't intercept keys when popup is open - let ComboBox handle it
                    if (popup.visible)
                        return;
                    
                    if (event.key === Qt.Key_Up) {
                        if (headerButton && headerButton.visible && headerButton.enabled) {
                            headerButton.forceActiveFocus(Qt.TabFocusReason)
                            event.accepted = true
                        }
                    }
                }
            }

            Label {
                text: qsTr("New Profile Name")
                visible: profileComboBox.currentIndex == profileComboBox.model.indexOf("create new profile")
            }

            C.TextField {
                id: profileName
                visible: profileComboBox.currentIndex == profileComboBox.model.indexOf("create new profile")
                Layout.preferredWidth: 400
                lastInFocusChain: true
            }

            Label {
                text: qsTr("Delete selected profile")
                visible: profileComboBox.model[profileComboBox.currentIndex] != "default" && profileComboBox.model[profileComboBox.currentIndex] != "create new profile" && profileComboBox.model[profileComboBox.currentIndex] != AstraPlay.settings.currentProfile
            }

            C.CheckBox {
                id: deleteBox
                visible: profileComboBox.model[profileComboBox.currentIndex] != "default" && profileComboBox.model[profileComboBox.currentIndex] != "create new profile" && profileComboBox.model[profileComboBox.currentIndex] != AstraPlay.settings.currentProfile
            }

            Label {
                Layout.alignment: Qt.AlignRight
                Layout.topMargin: 20
                text: qsTr("Steam Cloud Sync:")
                visible: profileDialog.steamCloudAvailable
            }

            C.CheckBox {
                id: steamCloudSyncCheckbox
                Layout.topMargin: 20
                checked: AstraPlay.settings.steamCloudSync
                onToggled: AstraPlay.settings.steamCloudSync = checked
                visible: profileDialog.steamCloudAvailable
            }

            Item {
                Layout.columnSpan: 2
                Layout.preferredHeight: 10
            }

            Item {
                // Empty item to take up first column
            }

            C.Button {
                id: clearCloudDataButton
                Layout.preferredWidth: 400
                Layout.preferredHeight: 50
                text: qsTr("Clear Steam Cloud Data")
                Material.roundedScale: Material.SmallScale
                lastInFocusChain: true
                visible: profileDialog.steamCloudAvailable
                onClicked: {
                    root.showConfirmDialog(
                        qsTr("Clear Steam Cloud Data"),
                        qsTr("Are you sure you want to delete all AstraPlay configuration files from Steam Cloud?\n\nThis will permanently delete all synced profiles from the cloud.\n\nLocal files will not be affected."),
                        () => AstraPlay.clearSteamCloudData()
                    );
                }
            }
        }
    }
}