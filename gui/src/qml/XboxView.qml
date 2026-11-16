import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Effects

import io.github.nileshchakraborty.astraplay

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
    property bool codeCopied: false

    // Navigation and Catalog properties
    property int currentSection: 0 // 0: Cloud Gaming, 1: My Consoles
    property string searchQuery: ""
    property string selectedCategory: "All"
    property var filteredGames: []
    readonly property var categoryList: ["All", "Action & adventure", "Shooter", "Role playing", "Sports", "Family & kids", "Racing & flying", "Strategy"]

    function formatUserCode(code) {
        if (!code || code.length === 0) return "";
        let clean = code.replace(/[^A-Za-z0-9]/g, "");
        if (clean.length === 8) {
            return clean.substring(0, 4) + " - " + clean.substring(4);
        }
        return code;
    }

    function applyFilter() {
        let titles = XboxAuth.cloudTitles;
        if (!titles || titles.length === 0) {
            filteredGames = [];
            return;
        }
        let query = searchQuery.trim().toLowerCase();
        let hasQuery = query.length > 0;
        let filterCat = selectedCategory !== "All";

        if (!hasQuery && !filterCat) {
            filteredGames = titles;
            return;
        }

        let res = [];
        for (let i = 0; i < titles.length; ++i) {
            let g = titles[i];
            if (filterCat) {
                let catMatch = false;
                if (g.categories) {
                    for (let c = 0; c < g.categories.length; ++c) {
                        if (g.categories[c] === selectedCategory) {
                            catMatch = true;
                            break;
                        }
                    }
                }
                if (!catMatch) continue;
            }

            if (hasQuery) {
                let name = g.name ? g.name.toLowerCase() : "";
                let dev = g.developer ? g.developer.toLowerCase() : "";
                let tid = g.titleId ? g.titleId.toLowerCase() : "";
                if (name.indexOf(query) === -1 && dev.indexOf(query) === -1 && tid.indexOf(query) === -1) {
                    continue;
                }
            }
            res.push(g);
        }
        filteredGames = res;
    }

    function refreshAll() {
        XboxAuth.fetchConsoles();
        XboxAuth.fetchCloudTitles();
    }

    function launchGame(titleId, gameName) {
        let mainComp = root;
        while (mainComp && !mainComp.showStreamView) {
            mainComp = mainComp.parent;
        }
        if (mainComp && mainComp.showStreamView) {
            mainComp.showStreamView();
        }
        AstraPlay.startXboxStream("", titleId, gameName);
    }

    // Clean blue background matching AstraPlay theme
    CleanBlueBackground {
        anchors.fill: parent
        z: -2
    }

    Connections {
        target: XboxAuth

        function onCloudTitlesUpdated() {
            root.applyFilter();
        }

        function onDeviceCodeReceived(code, uri, qr, expires) {
            root.isLoggingIn = true;
            root.userCode = code;
            root.verificationUri = uri;
            root.qrCodeUrl = qr;
            root.expiresIn = expires;
            root.loginStatusMessage = qsTr("Waiting for authentication in browser...");
            root.codeCopied = false;
        }

        function onAuthenticated(gamertag, uhs) {
            root.isLoggingIn = false;
            root.loginStatusMessage = "";
            root.codeCopied = false;
            root.refreshAll();
        }

        function onAuthFailed(errorMessage) {
            root.isLoggingIn = false;
            root.loginStatusMessage = errorMessage;
            root.codeCopied = false;
        }

        function onLoginCancelled() {
            root.isLoggingIn = false;
            root.userCode = "";
            root.verificationUri = "";
            root.codeCopied = false;
        }

        function onLoggedOut() {
            root.isLoggingIn = false;
            root.userCode = "";
            root.verificationUri = "";
            root.codeCopied = false;
        }
    }

    Component.onCompleted: {
        root.applyFilter();
        if (XboxAuth.isLoggedIn) {
            root.refreshAll();
        }
    }

    onVisibleChanged: {
        if (visible) {
            root.applyFilter();
            if (XboxAuth.isLoggedIn) {
                root.refreshAll();
            }
        }
    }

    // ----------------------------------------------------
    // HEADER BAR (When logged in)
    // ----------------------------------------------------
    Rectangle {
        id: xboxHeaderBar
        anchors {
            top: parent.top
            left: parent.left
            right: parent.right
        }
        height: 60
        color: Qt.rgba(10/255, 15/255, 26/255, 0.85)
        border.color: Qt.rgba(16/255, 124/255, 65/255, 0.4)
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
                color: Qt.rgba(16/255, 124/255, 65/255, 0.2)
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
                        color: "#4ADE80"
                    }

                    Text {
                        text: XboxAuth.gamertag.length > 0 ? XboxAuth.gamertag : qsTr("Xbox User")
                        color: "#FFFFFF"
                        font.pixelSize: 14
                        font.weight: Font.DemiBold
                    }
                }
            }

            // Sub-navigation Section Tabs: Cloud Gaming | My Consoles
            RowLayout {
                spacing: 8
                Layout.leftMargin: 12

                // Cloud Gaming Tab
                Rectangle {
                    Layout.preferredHeight: 36
                    Layout.preferredWidth: cloudTabLabel.implicitWidth + 24
                    radius: 8
                    color: root.currentSection === 0 ? "#107C10" : (cloudTabMouse.containsMouse ? Qt.rgba(255, 255, 255, 0.1) : "transparent")
                    border.color: root.currentSection === 0 ? "#4ADE80" : "transparent"
                    border.width: 1

                    Text {
                        id: cloudTabLabel
                        anchors.centerIn: parent
                        text: qsTr("Cloud Gaming") + (XboxAuth.cloudTitles.length > 0 ? (" (" + XboxAuth.cloudTitles.length + ")") : "")
                        color: root.currentSection === 0 ? "#FFFFFF" : Qt.rgba(255, 255, 255, 0.75)
                        font.pixelSize: 13
                        font.weight: root.currentSection === 0 ? Font.Bold : Font.Medium
                    }

                    MouseArea {
                        id: cloudTabMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.currentSection = 0
                    }
                }

                // Consoles Tab
                Rectangle {
                    Layout.preferredHeight: 36
                    Layout.preferredWidth: consoleTabLabel.implicitWidth + 24
                    radius: 8
                    color: root.currentSection === 1 ? "#107C10" : (consoleTabMouse.containsMouse ? Qt.rgba(255, 255, 255, 0.1) : "transparent")
                    border.color: root.currentSection === 1 ? "#4ADE80" : "transparent"
                    border.width: 1

                    Text {
                        id: consoleTabLabel
                        anchors.centerIn: parent
                        text: qsTr("My Consoles") + (XboxAuth.consoles.length > 0 ? (" (" + XboxAuth.consoles.length + ")") : "")
                        color: root.currentSection === 1 ? "#FFFFFF" : Qt.rgba(255, 255, 255, 0.75)
                        font.pixelSize: 13
                        font.weight: root.currentSection === 1 ? Font.Bold : Font.Medium
                    }

                    MouseArea {
                        id: consoleTabMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.currentSection = 1
                    }
                }
            }

            Item { Layout.fillWidth: true }

            // Refresh button
            Button {
                text: qsTr("Refresh")
                Layout.preferredHeight: 36
                Layout.preferredWidth: 90
                background: Rectangle {
                    radius: 8
                    color: parent.hovered ? Qt.rgba(255, 255, 255, 0.12) : Qt.rgba(255, 255, 255, 0.06)
                    border.color: parent.activeFocus ? "#4ADE80" : Qt.rgba(255, 255, 255, 0.2)
                    border.width: 1
                }
                contentItem: Text {
                    text: parent.text
                    color: "#FFFFFF"
                    font.pixelSize: 13
                    font.weight: Font.Medium
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                onClicked: root.refreshAll()
            }

            // Sign Out button
            Button {
                text: qsTr("Sign Out")
                Layout.preferredHeight: 36
                Layout.preferredWidth: 90
                background: Rectangle {
                    radius: 8
                    color: parent.hovered ? Qt.rgba(239, 68, 68, 0.2) : Qt.rgba(255, 255, 255, 0.06)
                    border.color: parent.hovered ? "#EF4444" : Qt.rgba(255, 255, 255, 0.2)
                    border.width: 1
                }
                contentItem: Text {
                    text: parent.text
                    color: parent.hovered ? "#EF4444" : "#FFFFFF"
                    font.pixelSize: 13
                    font.weight: Font.Medium
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                onClicked: XboxAuth.logout()
            }
        }
    }

    // ----------------------------------------------------
    // SECTION 0: XBOX CLOUD GAMING CATALOG VIEW
    // ----------------------------------------------------
    Item {
        id: cloudSectionContainer
        anchors {
            top: xboxHeaderBar.bottom
            left: parent.left
            right: parent.right
            bottom: parent.bottom
            margins: 20
        }
        visible: XboxAuth.isLoggedIn && root.currentSection === 0

        // Search and Filters Bar
        ColumnLayout {
            id: searchControlsCol
            anchors {
                top: parent.top
                left: parent.left
                right: parent.right
            }
            spacing: 12

            RowLayout {
                Layout.fillWidth: true
                spacing: 14

                // Search field
                TextField {
                    id: searchInput
                    Layout.fillWidth: true
                    Layout.preferredHeight: 44
                    placeholderText: qsTr("Search Xbox Cloud games (e.g. Cricket 24, Halo, Forza)...")
                    color: "#FFFFFF"
                    font.pixelSize: 14

                    background: Rectangle {
                        radius: 10
                        color: Qt.rgba(17/255, 24/255, 39/255, 0.85)
                        border.color: searchInput.activeFocus ? "#4ADE80" : Qt.rgba(255, 255, 255, 0.15)
                        border.width: 1.5
                    }

                    onTextChanged: {
                        root.searchQuery = text;
                        root.applyFilter();
                    }

                    // Clear button
                    Rectangle {
                        anchors {
                            right: parent.right
                            rightMargin: 10
                            verticalCenter: parent.verticalCenter
                        }
                        width: 24
                        height: 24
                        radius: 12
                        color: Qt.rgba(255, 255, 255, 0.15)
                        visible: searchInput.text.length > 0

                        Text {
                            anchors.centerIn: parent
                            text: "×"
                            color: "#FFFFFF"
                            font.pixelSize: 16
                            font.weight: Font.Bold
                        }

                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                searchInput.text = "";
                                root.searchQuery = "";
                                root.applyFilter();
                            }
                        }
                    }
                }

                // Sync indicator / Count badge
                Rectangle {
                    Layout.preferredHeight: 44
                    Layout.preferredWidth: countRow.implicitWidth + 24
                    radius: 10
                    color: Qt.rgba(17/255, 24/255, 39/255, 0.85)
                    border.color: Qt.rgba(16/255, 124/255, 65/255, 0.4)
                    border.width: 1

                    RowLayout {
                        id: countRow
                        anchors.centerIn: parent
                        spacing: 8

                        BusyIndicator {
                            Layout.preferredWidth: 18
                            Layout.preferredHeight: 18
                            running: XboxAuth.isFetchingTitles
                            visible: running
                        }

                        Text {
                            text: XboxAuth.isFetchingTitles ? qsTr("Syncing...") : qsTr("%1 games").arg(root.filteredGames.length)
                            color: "#4ADE80"
                            font.pixelSize: 13
                            font.weight: Font.DemiBold
                        }
                    }
                }
            }

            // Category filter chips
            Flickable {
                Layout.fillWidth: true
                Layout.preferredHeight: 34
                contentWidth: categoryRow.implicitWidth
                flickableDirection: Flickable.HorizontalFlick
                boundsBehavior: Flickable.StopAtBounds
                clip: true

                RowLayout {
                    id: categoryRow
                    spacing: 8

                    Repeater {
                        model: root.categoryList

                        Rectangle {
                            Layout.preferredHeight: 32
                            Layout.preferredWidth: chipText.implicitWidth + 20
                            radius: 16
                            color: root.selectedCategory === modelData ? "#107C10" : (chipMouse.containsMouse ? Qt.rgba(255, 255, 255, 0.12) : Qt.rgba(255, 255, 255, 0.05))
                            border.color: root.selectedCategory === modelData ? "#4ADE80" : Qt.rgba(255, 255, 255, 0.15)
                            border.width: 1

                            Text {
                                id: chipText
                                anchors.centerIn: parent
                                text: modelData
                                color: root.selectedCategory === modelData ? "#FFFFFF" : Qt.rgba(255, 255, 255, 0.75)
                                font.pixelSize: 12
                                font.weight: root.selectedCategory === modelData ? Font.Bold : Font.Normal
                            }

                            MouseArea {
                                id: chipMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    root.selectedCategory = modelData;
                                    root.applyFilter();
                                }
                            }
                        }
                    }
                }
            }
        }

        // Direct Title ID Launch Compact Bar
        Rectangle {
            id: directLaunchBar
            anchors {
                left: parent.left
                right: parent.right
                bottom: parent.bottom
            }
            height: 48
            radius: 10
            color: Qt.rgba(17/255, 24/255, 39/255, 0.8)
            border.color: Qt.rgba(255, 255, 255, 0.1)
            border.width: 1

            RowLayout {
                anchors {
                    fill: parent
                    leftMargin: 14
                    rightMargin: 14
                }
                spacing: 12

                Text {
                    text: qsTr("Direct Title ID:")
                    color: Qt.rgba(255, 255, 255, 0.6)
                    font.pixelSize: 12
                    font.weight: Font.Medium
                }

                TextField {
                    id: directTitleInput
                    Layout.fillWidth: true
                    Layout.preferredHeight: 34
                    placeholderText: qsTr("e.g. CRICKET26THEOFFICIALGAMEOFTHEASHES")
                    color: "#FFFFFF"
                    font.pixelSize: 12
                    background: Rectangle {
                        radius: 6
                        color: Qt.rgba(255, 255, 255, 0.06)
                        border.color: directTitleInput.activeFocus ? "#4ADE80" : Qt.rgba(255, 255, 255, 0.12)
                        border.width: 1
                    }
                }

                Button {
                    text: qsTr("Launch ID")
                    Layout.preferredHeight: 34
                    Layout.preferredWidth: 100
                    background: Rectangle {
                        radius: 6
                        color: parent.hovered ? "#169B14" : "#107C10"
                    }
                    contentItem: Text {
                        text: parent.text
                        color: "#FFFFFF"
                        font.pixelSize: 12
                        font.weight: Font.Bold
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    onClicked: {
                        let t = directTitleInput.text.trim();
                        if (t.length > 0) {
                            root.launchGame(t, t);
                        }
                    }
                }
            }
        }

        // Grid View of Xbox Cloud Games
        GridView {
            id: gamesGrid
            anchors {
                top: searchControlsCol.bottom
                topMargin: 14
                left: parent.left
                right: parent.right
                bottom: directLaunchBar.top
                bottomMargin: 14
            }
            cellWidth: 195
            cellHeight: 275
            clip: true
            focus: true
            boundsBehavior: Flickable.StopAtBounds

            ScrollBar.vertical: ScrollBar {
                width: 8
                policy: ScrollBar.AsNeeded
                background: Rectangle { color: Qt.rgba(255, 255, 255, 0.08); radius: 4 }
                contentItem: Rectangle { radius: 4; color: "#107C10"; opacity: 0.8 }
            }

            model: root.filteredGames

            delegate: XboxGameCard {
                width: gamesGrid.cellWidth - 15
                height: gamesGrid.cellHeight - 15
                gameData: modelData
                onStreamGame: (titleId, gameName) => root.launchGame(titleId, gameName)
            }
        }

        // Empty state message when filteredGames is empty
        Rectangle {
            anchors.centerIn: gamesGrid
            width: 380
            height: 140
            radius: 12
            color: Qt.rgba(17/255, 24/255, 39/255, 0.9)
            border.color: Qt.rgba(255, 255, 255, 0.1)
            border.width: 1
            visible: root.filteredGames.length === 0

            ColumnLayout {
                anchors.centerIn: parent
                spacing: 12

                Text {
                    Layout.alignment: Qt.AlignHCenter
                    text: root.searchQuery.length > 0 ? qsTr("No games match '%1'").arg(root.searchQuery) : qsTr("No games found in catalog")
                    color: "#FFFFFF"
                    font.pixelSize: 15
                    font.weight: Font.Medium
                }

                Button {
                    Layout.alignment: Qt.AlignHCenter
                    text: qsTr("Refresh Catalog")
                    Layout.preferredHeight: 36
                    Layout.preferredWidth: 140
                    background: Rectangle {
                        radius: 8
                        color: parent.hovered ? "#169B14" : "#107C10"
                    }
                    contentItem: Text {
                        text: parent.text
                        color: "#FFFFFF"
                        font.pixelSize: 13
                        font.weight: Font.Bold
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    onClicked: XboxAuth.fetchCloudTitles()
                }
            }
        }
    }

    // ----------------------------------------------------
    // SECTION 1: MY XBOX CONSOLES VIEW
    // ----------------------------------------------------
    ScrollView {
        id: consolesScrollView
        anchors {
            top: xboxHeaderBar.bottom
            left: parent.left
            right: parent.right
            bottom: parent.bottom
            margins: 20
        }
        visible: XboxAuth.isLoggedIn && root.currentSection === 1
        clip: true

        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
        ScrollBar.vertical: ScrollBar {
            width: 8
            policy: ScrollBar.AsNeeded
            background: Rectangle { color: Qt.rgba(255, 255, 255, 0.1); radius: 4 }
            contentItem: Rectangle { radius: 4; color: "#107C10"; opacity: 0.7 }
        }

        ColumnLayout {
            width: consolesScrollView.width - 20
            spacing: 24

            Text {
                text: qsTr("Discovered Xbox Consoles")
                color: "#4ADE80"
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

                        Rectangle {
                            width: 56
                            height: 56
                            radius: 10
                            color: Qt.rgba(16/255, 124/255, 65/255, 0.25)
                            border.color: "#107C10"
                            border.width: 1

                            Text {
                                anchors.centerIn: parent
                                text: "X"
                                color: "#4ADE80"
                                font.pixelSize: 28
                                font.weight: Font.Black
                            }
                        }

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
                                    color: modelData.powerState === "On" ? "#4ADE80" :
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

                        RowLayout {
                            spacing: 12

                            Button {
                                text: qsTr("Wake Up")
                                visible: modelData.powerState === "ConnectedStandby"
                                Layout.preferredHeight: 40
                                Layout.preferredWidth: 95
                                background: Rectangle {
                                    radius: 8
                                    color: parent.hovered ? Qt.rgba(245/255, 158/255, 11/255, 0.2) : Qt.rgba(255, 255, 255, 0.08)
                                    border.color: "#F59E0B"
                                    border.width: 1
                                }
                                contentItem: Text {
                                    text: parent.text
                                    color: "#F59E0B"
                                    font.pixelSize: 13
                                    font.weight: Font.DemiBold
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                }
                                onClicked: XboxAuth.wakeConsole(modelData.id)
                            }

                            Button {
                                text: qsTr("Turn Off")
                                visible: modelData.powerState === "On"
                                Layout.preferredHeight: 40
                                Layout.preferredWidth: 95
                                background: Rectangle {
                                    radius: 8
                                    color: parent.hovered ? Qt.rgba(239/255, 68/255, 68/255, 0.2) : Qt.rgba(255, 255, 255, 0.08)
                                    border.color: Qt.rgba(255, 255, 255, 0.25)
                                    border.width: 1
                                }
                                contentItem: Text {
                                    text: parent.text
                                    color: parent.hovered ? "#EF4444" : "#FFFFFF"
                                    font.pixelSize: 13
                                    font.weight: Font.DemiBold
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                }
                                onClicked: XboxAuth.turnOffConsole(modelData.id)
                            }

                            Button {
                                text: qsTr("Stream")
                                Layout.preferredHeight: 40
                                Layout.preferredWidth: 110
                                background: Rectangle {
                                    radius: 8
                                    color: parent.hovered ? "#169B14" : "#107C10"
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
        }
    }

    // ----------------------------------------------------
    // NOT LOGGED IN VIEW (Welcome Card or Device Code Dialog)
    // ----------------------------------------------------
    Rectangle {
        id: authCard
        anchors.centerIn: parent
        width: Math.min(500, parent.width - 40)
        implicitHeight: authContentCol.implicitHeight + 56
        height: implicitHeight
        radius: 16
        color: Qt.rgba(17/255, 24/255, 39/255, 0.96)
        border.color: "#107C10"
        border.width: 1.5
        visible: !XboxAuth.isLoggedIn

        ColumnLayout {
            id: authContentCol
            anchors {
                top: parent.top
                left: parent.left
                right: parent.right
                margins: 28
            }
            spacing: 20

            // State A: Welcome Card
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 18
                visible: !root.isLoggingIn

                Rectangle {
                    Layout.alignment: Qt.AlignHCenter
                    width: 72
                    height: 72
                    radius: 36
                    color: Qt.rgba(16/255, 124/255, 65/255, 0.25)
                    border.color: "#107C10"
                    border.width: 2

                    Text {
                        anchors.centerIn: parent
                        text: "X"
                        color: "#4ADE80"
                        font.pixelSize: 40
                        font.weight: Font.Black
                    }
                }

                Text {
                    Layout.alignment: Qt.AlignHCenter
                    text: qsTr("Xbox Streaming")
                    color: "#FFFFFF"
                    font.pixelSize: 26
                    font.weight: Font.Bold
                }

                Text {
                    Layout.alignment: Qt.AlignHCenter
                    Layout.fillWidth: true
                    text: qsTr("Stream games from your Xbox Series X|S, Xbox One, or Xbox Cloud Gaming directly to AstraPlay with low-latency WebRTC and native controller input.")
                    color: Qt.rgba(255, 255, 255, 0.75)
                    font.pixelSize: 14
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                    lineHeight: 1.3
                }

                Item { Layout.preferredHeight: 8 }

                Button {
                    Layout.alignment: Qt.AlignHCenter
                    Layout.preferredWidth: 260
                    Layout.preferredHeight: 48
                    background: Rectangle {
                        radius: 10
                        color: parent.hovered ? "#169B14" : "#107C10"
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
                spacing: 18
                visible: root.isLoggingIn

                Text {
                    Layout.alignment: Qt.AlignHCenter
                    text: qsTr("Device Code Login")
                    color: "#FFFFFF"
                    font.pixelSize: 24
                    font.weight: Font.Bold
                }

                Text {
                    Layout.alignment: Qt.AlignHCenter
                    Layout.fillWidth: true
                    text: qsTr("Scan the QR code or visit <font color='#4ADE80'><b>%1</b></font> and enter:").arg(
                        root.verificationUri.length > 0 ? root.verificationUri.replace(/^https?:\/\//, "") : "microsoft.com/link"
                    )
                    textFormat: Text.RichText
                    color: Qt.rgba(255, 255, 255, 0.8)
                    font.pixelSize: 14
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                }

                // Generously-sized Code Box with internal copy action
                Rectangle {
                    Layout.alignment: Qt.AlignHCenter
                    Layout.preferredWidth: Math.min(380, parent.width)
                    Layout.preferredHeight: 60
                    radius: 12
                    color: Qt.rgba(0, 0, 0, 0.5)
                    border.color: "#107C10"
                    border.width: 1.5

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 20
                        anchors.rightMargin: 12
                        spacing: 12

                        Text {
                            Layout.fillWidth: true
                            text: root.formatUserCode(root.userCode)
                            color: "#FFFFFF"
                            font.pixelSize: 22
                            font.weight: Font.Black
                            font.letterSpacing: 2
                            font.family: "Menlo, Monaco, 'Courier New', monospace"
                            verticalAlignment: Text.AlignVCenter
                        }

                        Button {
                            Layout.preferredWidth: 78
                            Layout.preferredHeight: 36
                            background: Rectangle {
                                radius: 8
                                color: root.codeCopied ? "#107C10" : (parent.hovered ? Qt.rgba(255, 255, 255, 0.18) : Qt.rgba(255, 255, 255, 0.08))
                                border.color: root.codeCopied ? "#4ADE80" : Qt.rgba(255, 255, 255, 0.3)
                                border.width: 1
                            }
                            contentItem: Text {
                                text: root.codeCopied ? qsTr("Copied!") : qsTr("Copy")
                                color: root.codeCopied ? "#FFFFFF" : Qt.rgba(255, 255, 255, 0.9)
                                font.pixelSize: 12
                                font.weight: Font.DemiBold
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }
                            onClicked: {
                                AstraPlay.setClipboardText(root.userCode);
                                root.codeCopied = true;
                                copiedResetTimer.restart();
                            }
                        }
                    }
                }

                Timer {
                    id: copiedResetTimer
                    interval: 2000
                    onTriggered: root.codeCopied = false
                }

                // Framed QR Code with rounded white padding and subtle Xbox border
                Rectangle {
                    Layout.alignment: Qt.AlignHCenter
                    width: 190
                    height: 190
                    radius: 14
                    color: "#FFFFFF"
                    border.color: "#107C10"
                    border.width: 2.5

                    Image {
                        anchors.centerIn: parent
                        width: 170
                        height: 170
                        source: root.verificationUri.length > 0 ? AstraPlay.getQRCodeDataUrl(root.verificationUri, 200) : ""
                        fillMode: Image.PreserveAspectFit
                        smooth: true
                    }
                }

                // Status row with spinner and crisp green text
                RowLayout {
                    Layout.alignment: Qt.AlignHCenter
                    spacing: 10

                    BusyIndicator {
                        Layout.preferredWidth: 22
                        Layout.preferredHeight: 22
                        running: root.isLoggingIn
                    }

                    Text {
                        text: root.loginStatusMessage.length > 0 ? root.loginStatusMessage : qsTr("Waiting for authentication approval...")
                        color: "#4ADE80"
                        font.pixelSize: 13
                        font.weight: Font.Medium
                    }
                }

                Item { Layout.preferredHeight: 4 }

                // Cancel button
                Button {
                    Layout.alignment: Qt.AlignHCenter
                    Layout.preferredWidth: 160
                    Layout.preferredHeight: 40
                    background: Rectangle {
                        radius: 8
                        color: parent.hovered ? Qt.rgba(255, 255, 255, 0.12) : Qt.rgba(255, 255, 255, 0.05)
                        border.color: Qt.rgba(255, 255, 255, 0.3)
                        border.width: 1
                    }
                    contentItem: Text {
                        text: qsTr("Cancel")
                        color: "#FFFFFF"
                        font.pixelSize: 14
                        font.weight: Font.Medium
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    onClicked: {
                        XboxAuth.cancelLogin();
                        root.isLoggingIn = false;
                    }
                }
            }
        }
    }
}
