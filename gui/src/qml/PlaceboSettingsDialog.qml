import QtCore
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Dialogs

import io.github.nileshchakraborty.astraplay

import "controls" as C

DialogView {
    id: dialog
    property string options_url: ""
    title: qsTr("Placebo Renderer Settings")
    header: qsTr("* Defaults in () to right of value or marked with (Default)")
    buttonVisible: false
    Keys.onPressed: (event) => {
        if (event.modifiers)
            return;
        switch (event.key) {
        case Qt.Key_PageUp:
            bar.decrementCurrentIndex();
            event.accepted = true;
            break;
        case Qt.Key_PageDown:
            bar.incrementCurrentIndex();
            event.accepted = true;
            break;
        }
    }

    Item {
        TabBar {
            id: bar
            anchors {
                top: parent.top
                left: parent.left
                right: parent.right
                topMargin: 5
            }

            TabButton {
                text: qsTr("Config")
                focusPolicy: Qt.NoFocus
            }

            TabButton {
                text: qsTr("Scaling")
                focusPolicy: Qt.NoFocus
            }

            TabButton {
                text: qsTr("Debanding")
                focusPolicy: Qt.NoFocus
            }

            TabButton {
                text: qsTr("Sigmoidization")
                focusPolicy: Qt.NoFocus
            }

            TabButton {
                text: qsTr("Color")
                focusPolicy: Qt.NoFocus
            }

            TabButton {
                text: qsTr("Peak Detection")
                focusPolicy: Qt.NoFocus
            }

            TabButton {
                text: qsTr("Color Mapping")
                focusPolicy: Qt.NoFocus
            }
        }

        StackLayout {
            anchors {
                top: bar.bottom
                left: parent.left
                right: parent.right
                bottom: parent.bottom
            }
            currentIndex: bar.currentIndex
            onCurrentIndexChanged: nextItemInFocusChain().forceActiveFocus(Qt.TabFocusReason)

            Item {
                // Config
                GridLayout {
                    anchors {
                        top: parent.top
                        horizontalCenter: parent.horizontalCenter
                        topMargin: 20
                    }
                    columns: 2
                    rowSpacing: 10
                    columnSpacing: 20

                    Label {
                        text: qsTr("Libplacebo Renderer Options Details")
                        visible: !options_url
                    }

                    C.Button {
                        id: openwebpage
                        text: qsTr("Open Documentation")
                        onClicked: {
                            options_url = AstraPlay.openPlaceboOptionsLink()
                        }
                        visible: !options_url
                    }

                    Label {
                        text: qsTr("Open Documentation in Web Browser with copied URL")
                        visible: options_url
                    }

                    TextField {
                        id: openurl
                        echoMode: AstraPlay.settings.streamerMode ? TextInput.Password : TextInput.Normal
                        text: options_url
                        visible: false
                        Layout.preferredWidth: 400
                    }

                    C.Button {
                        id: copyUrl
                        text: qsTr("Click to Re-Copy URL")
                        onClicked: {
                            openurl.selectAll()
                            openurl.copy()
                        }
                        visible: options_url
                    }

                    Label {
                        text: qsTr("Export Render Settings to File")
                    }

                    C.Button {
                        id: exportButton
                        text: qsTr("Export")
                        onClicked: {
                            AstraPlay.settings.exportPlaceboSettings();
                        }
                        Material.roundedScale: Material.SmallScale
                        KeyNavigation.down: importButton
                        KeyNavigation.right: importButton
                    }

                    Label {
                        text: qsTr("Import Render Settings from File")
                    }

                    C.Button {
                        id: importButton
                        lastInFocusChain: true
                        text: qsTr("Import")
                        onClicked: {
                            AstraPlay.settings.importPlaceboSettings();
                        }
                        Material.roundedScale: Material.SmallScale
                        KeyNavigation.up: exportButton
                        KeyNavigation.left: exportButton
                    }
                }
            }

            Item {
                // Scaling
                GridLayout {
                    anchors {
                        top: parent.top
                        horizontalCenter: parent.horizontalCenter
                        topMargin: 20
                    }
                    columns: 3
                    rowSpacing: 10
                    columnSpacing: 20

                    Label {
                        Layout.alignment: Qt.AlignRight
                        text: qsTr("Upscaler:")
                    }

                    C.ComboBox {
                        Layout.preferredWidth: 400
                        firstInFocusChain: true
                        model: [qsTr("Custom"), qsTr("Nearest"), qsTr("Bilinear"), qsTr("Oversample"), qsTr("Bicubic"), qsTr("Gaussian"), qsTr("Catmull Rom"), qsTr("Lanczos"), qsTr("EwaLanczos"), qsTr("EwaLanczosSharp"), qsTr("EwaLanczosSharpest")]
                        currentIndex: AstraPlay.settings.placeboUpscaler
                        onActivated: index => AstraPlay.settings.placeboUpscaler = index
                    }

                    Label {
                        Layout.alignment: Qt.AlignRight
                        text: qsTr("(EwaLanczosSharp)")
                    }

                    Label {
                        Layout.alignment: Qt.AlignRight
                        text: qsTr("Plane Upscaler:")
                    }

                    C.ComboBox {
                        Layout.preferredWidth: 400
                        firstInFocusChain: true
                        model: [qsTr("Chosen Upscaler"), qsTr("Nearest"), qsTr("Bilinear"), qsTr("Oversample"), qsTr("Bicubic"), qsTr("Gaussian"), qsTr("Catmull Rom"), qsTr("Lanczos"), qsTr("EwaLanczos"), qsTr("EwaLanczosSharp"), qsTr("EwaLanczosSharpest")]
                        currentIndex: AstraPlay.settings.placeboPlaneUpscaler
                        onActivated: index => AstraPlay.settings.placeboPlaneUpscaler = index
                    }

                    Label {
                        Layout.alignment: Qt.AlignRight
                        text: qsTr("(Chosen Upscaler)")
                    }

                    Label {
                        Layout.alignment: Qt.AlignRight
                        text: qsTr("Downscaler:")
                    }

                    C.ComboBox {
                        Layout.preferredWidth: 400
                        firstInFocusChain: true
                        model: [qsTr("Custom"), qsTr("Box"), qsTr("Hermite"), qsTr("Bilinear"), qsTr("Bicubic"), qsTr("Gaussian"), qsTr("Catmull Rom"), qsTr("Mitchell"), qsTr("Lanczos")]
                        currentIndex: AstraPlay.settings.placeboDownscaler
                        onActivated: index => AstraPlay.settings.placeboDownscaler = index
                    }

                    Label {
                        Layout.alignment: Qt.AlignRight
                        text: qsTr("(Hermite)")
                    }

                    Label {
                        Layout.alignment: Qt.AlignRight
                        text: qsTr("Plane Downscaler:")
                    }

                    C.ComboBox {
                        Layout.preferredWidth: 400
                        firstInFocusChain: true
                        model: [qsTr("Chosen Downscaler"), qsTr("Box"), qsTr("Hermite"), qsTr("Bilinear"), qsTr("Bicubic"), qsTr("Gaussian"), qsTr("Catmull Rom"), qsTr("Mitchell"), qsTr("Lanczos")]
                        currentIndex: AstraPlay.settings.placeboPlaneDownscaler
                        onActivated: index => AstraPlay.settings.placeboPlaneDownscaler = index
                    }

                    Label {
                        Layout.alignment: Qt.AlignRight
                        text: qsTr("(Chosen Downscaler)")
                    }

                    Label {
                        Layout.alignment: Qt.AlignRight
                        text: qsTr("Frame Mixer:")
                    }

                    C.ComboBox {
                        Layout.preferredWidth: 400
                        model: [qsTr("None"), qsTr("Oversample"), qsTr("Hermite"), qsTr("Linear"), qsTr("Cubic")]
                        currentIndex: AstraPlay.settings.placeboFrameMixer
                        onActivated: index => AstraPlay.settings.placeboFrameMixer = index
                    }

                    Label {
                        Layout.alignment: Qt.AlignRight
                        text: qsTr("(Oversample)")
                    }

                    Label {
                        Layout.alignment: Qt.AlignRight
                        text: qsTr("Anti-ringing Strength:")
                    }

                    C.Slider {
                        Layout.preferredWidth: 250
                        from: 0.00
                        to: 1.00
                        stepSize: 0.01
                        value: AstraPlay.settings.placeboAntiringingStrength
                        onMoved: AstraPlay.settings.placeboAntiringingStrength = value

                        Label {
                            anchors {
                                left: parent.right
                                verticalCenter: parent.verticalCenter
                                leftMargin: 10
                            }
                            text: qsTr(parent.value.toFixed(2))
                        }
                    }

                    Label {
                        Layout.alignment: Qt.AlignRight
                        text: qsTr("(0.00)")
                    }
                }
            }

            Item {
                // Debanding
                GridLayout {
                    anchors {
                        top: parent.top
                        horizontalCenter: parent.horizontalCenter
                        topMargin: 20
                    }
                    columns: 3
                    rowSpacing: 10
                    columnSpacing: 20

                    Label {
                        Layout.alignment: Qt.AlignRight
                        text: qsTr("Deband Enabled:")
                    }

                    C.CheckBox {
                        text: qsTr("Enable Debanding")
                        checked: AstraPlay.settings.placeboDebandEnabled
                        onToggled: AstraPlay.settings.placeboDebandEnabled = checked
                    }


                    Label {
                        Layout.alignment: Qt.AlignRight
                        text: qsTr("(Checked)")
                    }

                    Label {
                        Layout.alignment: Qt.AlignRight
                        text: qsTr("Deband Preset:")
                        visible: AstraPlay.settings.placeboDebandEnabled
                    }

                    C.ComboBox {
                        Layout.preferredWidth: 400
                        model: [qsTr("Custom"), qsTr("Default")]
                        currentIndex: AstraPlay.settings.placeboDebandPreset
                        onActivated: index => AstraPlay.settings.placeboDebandPreset = index
                        visible: AstraPlay.settings.placeboDebandEnabled
                    }

                    Label {
                        Layout.alignment: Qt.AlignRight
                        text: qsTr("(Custom)")
                        visible: AstraPlay.settings.placeboDebandEnabled
                    }

                    Label {
                        Layout.alignment: Qt.AlignRight
                        text: qsTr("Deband Iterations:")
                        visible: AstraPlay.settings.placeboDebandEnabled && (AstraPlay.settings.placeboDebandPreset == 0)
                    }

                    C.Slider {
                        Layout.preferredWidth: 250
                        from: 0
                        to: 16
                        stepSize: 1
                        value: AstraPlay.settings.placeboDebandIterations
                        onMoved: AstraPlay.settings.placeboDebandIterations = value
                        visible: AstraPlay.settings.placeboDebandEnabled && (AstraPlay.settings.placeboDebandPreset == 0)

                        Label {
                            anchors {
                                left: parent.right
                                verticalCenter: parent.verticalCenter
                                leftMargin: 10
                            }
                            text: qsTr("%1").arg(parent.value)
                        }
                    }

                    Label {
                        Layout.alignment: Qt.AlignRight
                        text: qsTr("(1)")
                        visible: AstraPlay.settings.placeboDebandEnabled && (AstraPlay.settings.placeboDebandPreset == 0)
                    }

                    Label {
                        Layout.alignment: Qt.AlignRight
                        text: qsTr("Deband Threshold:")
                        visible: AstraPlay.settings.placeboDebandEnabled && (AstraPlay.settings.placeboDebandPreset == 0)
                    }

                    C.Slider {
                        Layout.preferredWidth: 250
                        from: 0.0
                        to: 1000.0
                        stepSize: 0.1
                        value: AstraPlay.settings.placeboDebandThreshold
                        onMoved: AstraPlay.settings.placeboDebandThreshold = value
                        visible: AstraPlay.settings.placeboDebandEnabled && (AstraPlay.settings.placeboDebandPreset == 0)

                        Label {
                            anchors {
                                left: parent.right
                                verticalCenter: parent.verticalCenter
                                leftMargin: 10
                            }
                            text: qsTr(parent.value.toFixed(1))
                        }
                    }

                    Label {
                        Layout.alignment: Qt.AlignRight
                        text: qsTr("(3.0)")
                        visible: AstraPlay.settings.placeboDebandEnabled && (AstraPlay.settings.placeboDebandPreset == 0)
                    }

                    Label {
                        Layout.alignment: Qt.AlignRight
                        text: qsTr("Deband Radius:")
                        visible: AstraPlay.settings.placeboDebandEnabled && (AstraPlay.settings.placeboDebandPreset == 0)
                    }

                    C.Slider {
                        Layout.preferredWidth: 250
                        from: 0.0
                        to: 1000.0
                        stepSize: 0.1
                        value: AstraPlay.settings.placeboDebandRadius
                        onMoved: AstraPlay.settings.placeboDebandRadius = value
                        visible: AstraPlay.settings.placeboDebandEnabled && (AstraPlay.settings.placeboDebandPreset == 0)

                        Label {
                            anchors {
                                left: parent.right
                                verticalCenter: parent.verticalCenter
                                leftMargin: 10
                            }
                            text: qsTr(parent.value.toFixed(1))
                        }
                    }

                    Label {
                        Layout.alignment: Qt.AlignRight
                        text: qsTr("(16.0)")
                        visible: AstraPlay.settings.placeboDebandEnabled && (AstraPlay.settings.placeboDebandPreset == 0)
                    }

                    Label {
                        Layout.alignment: Qt.AlignRight
                        text: qsTr("Deband Grain:")
                        visible: AstraPlay.settings.placeboDebandEnabled && (AstraPlay.settings.placeboDebandPreset == 0)
                    }

                    C.Slider {
                        Layout.preferredWidth: 250
                        from: 0.0
                        to: 1000.0
                        stepSize: 0.1
                        value: AstraPlay.settings.placeboDebandGrain
                        onMoved: AstraPlay.settings.placeboDebandGrain = value
                        visible: AstraPlay.settings.placeboDebandEnabled && (AstraPlay.settings.placeboDebandPreset == 0)

                        Label {
                            anchors {
                                left: parent.right
                                verticalCenter: parent.verticalCenter
                                leftMargin: 10
                            }
                            text: qsTr(parent.value.toFixed(1))
                        }
                    }

                    Label {
                        Layout.alignment: Qt.AlignRight
                        text: qsTr("(4.0)")
                        visible: AstraPlay.settings.placeboDebandEnabled && (AstraPlay.settings.placeboDebandPreset == 0)
                    }
                }
            }

            Item {
                // Sigmoidization
                GridLayout {
                    anchors {
                        top: parent.top
                        horizontalCenter: parent.horizontalCenter
                        topMargin: 20
                    }
                    columns: 3
                    rowSpacing: 10
                    columnSpacing: 20

                    Label {
                        Layout.alignment: Qt.AlignRight
                        text: qsTr("Sigmoidization Enabled:")
                    }

                    C.CheckBox {
                        text: qsTr("Enable Sigmoidization")
                        checked: AstraPlay.settings.placeboSigmoidEnabled
                        onToggled: AstraPlay.settings.placeboSigmoidEnabled = checked
                    }

                    Label {
                        Layout.alignment: Qt.AlignRight
                        text: qsTr("(Checked)")
                    }

                    Label {
                        Layout.alignment: Qt.AlignRight
                        text: qsTr("Sigmoid Preset:")
                        visible: AstraPlay.settings.placeboSigmoidEnabled
                    }

                    C.ComboBox {
                        Layout.preferredWidth: 400
                        model: [qsTr("Custom"), qsTr("Default")]
                        currentIndex: AstraPlay.settings.placeboSigmoidPreset
                        onActivated: index => AstraPlay.settings.placeboSigmoidPreset = index
                        visible: AstraPlay.settings.placeboSigmoidEnabled
                    }

                    Label {
                        Layout.alignment: Qt.AlignRight
                        text: qsTr("(Custom)")
                        visible: AstraPlay.settings.placeboSigmoidEnabled
                    }

                    Label {
                        Layout.alignment: Qt.AlignRight
                        text: qsTr("Sigmoid Center:")
                        visible: AstraPlay.settings.placeboSigmoidEnabled && (AstraPlay.settings.placeboSigmoidPreset == 0)
                    }

                    C.Slider {
                        Layout.preferredWidth: 250
                        from: 0.00
                        to: 1.00
                        stepSize: 0.01
                        value: AstraPlay.settings.placeboSigmoidCenter
                        onMoved: AstraPlay.settings.placeboSigmoidCenter = value
                        visible: AstraPlay.settings.placeboSigmoidEnabled && (AstraPlay.settings.placeboSigmoidPreset == 0)

                        Label {
                            anchors {
                                left: parent.right
                                verticalCenter: parent.verticalCenter
                                leftMargin: 10
                            }
                            text: qsTr(parent.value.toFixed(2))
                        }
                    }

                    Label {
                        Layout.alignment: Qt.AlignRight
                        text: qsTr("(0.75)")
                        visible: AstraPlay.settings.placeboSigmoidEnabled && (AstraPlay.settings.placeboSigmoidPreset == 0)
                    }

                    Label {
                        Layout.alignment: Qt.AlignRight
                        text: qsTr("Sigmoid Slope:")
                        visible: AstraPlay.settings.placeboSigmoidEnabled && (AstraPlay.settings.placeboSigmoidPreset == 0)
                    }

                    C.Slider {
                        Layout.preferredWidth: 250
                        from: 1.0
                        to: 20.0
                        stepSize: 0.1
                        value: AstraPlay.settings.placeboSigmoidSlope
                        onMoved: AstraPlay.settings.placeboSigmoidSlope = value
                        visible: AstraPlay.settings.placeboSigmoidEnabled && (AstraPlay.settings.placeboSigmoidPreset == 0)

                        Label {
                            anchors {
                                left: parent.right
                                verticalCenter: parent.verticalCenter
                                leftMargin: 10
                            }
                            text: qsTr(parent.value.toFixed(1))
                        }
                    }

                    Label {
                        Layout.alignment: Qt.AlignRight
                        text: qsTr("(6.5)")
                        visible: AstraPlay.settings.placeboSigmoidEnabled && (AstraPlay.settings.placeboSigmoidPreset == 0)
                    }
                }
            }

            Item {
                // Color
                GridLayout {
                    anchors {
                        top: parent.top
                        horizontalCenter: parent.horizontalCenter
                        topMargin: 20
                    }
                    columns: 3
                    rowSpacing: 10
                    columnSpacing: 20

                    Label {
                        Layout.alignment: Qt.AlignRight
                        text: qsTr("Color Adjustment Enabled:")
                    }

                    C.CheckBox {
                        text: qsTr("Enable Color Adjustment")
                        checked: AstraPlay.settings.placeboColorAdjustmentEnabled
                        onToggled: AstraPlay.settings.placeboColorAdjustmentEnabled = checked
                    }

                    Label {
                        Layout.alignment: Qt.AlignRight
                        text: qsTr("(Checked)")
                    }

                    Label {
                        Layout.alignment: Qt.AlignRight
                        text: qsTr("Color Adjustment Preset:")
                        visible: AstraPlay.settings.placeboColorAdjustmentEnabled
                    }

                    C.ComboBox {
                        Layout.preferredWidth: 400
                        model: [qsTr("Custom"), qsTr("Neutral")]
                        currentIndex: AstraPlay.settings.placeboColorAdjustmentPreset
                        onActivated: index => AstraPlay.settings.placeboColorAdjustmentPreset = index
                        visible: AstraPlay.settings.placeboColorAdjustmentEnabled
                    }

                    Label {
                        Layout.alignment: Qt.AlignRight
                        text: qsTr("(Custom)")
                        visible: AstraPlay.settings.placeboColorAdjustmentEnabled
                    }

                    Label {
                        Layout.alignment: Qt.AlignRight
                        text: qsTr("Brightness:")
                        visible: AstraPlay.settings.placeboColorAdjustmentEnabled && (AstraPlay.settings.placeboColorAdjustmentPreset == 0)
                    }

                    C.Slider {
                        Layout.preferredWidth: 250
                        from: -1.00
                        to: 1.00
                        stepSize: 0.01
                        value: AstraPlay.settings.placeboColorAdjustmentBrightness
                        onMoved: AstraPlay.settings.placeboColorAdjustmentBrightness = value
                        visible: AstraPlay.settings.placeboColorAdjustmentEnabled && (AstraPlay.settings.placeboColorAdjustmentPreset == 0)

                        Label {
                            anchors {
                                left: parent.right
                                verticalCenter: parent.verticalCenter
                                leftMargin: 10
                            }
                            text: qsTr(parent.value.toFixed(2))
                        }
                    }

                    Label {
                        Layout.alignment: Qt.AlignRight
                        text: qsTr("(0.00)")
                        visible: AstraPlay.settings.placeboColorAdjustmentEnabled && (AstraPlay.settings.placeboColorAdjustmentPreset == 0)
                    }

                    Label {
                        Layout.alignment: Qt.AlignRight
                        text: qsTr("Contrast:")
                        visible: AstraPlay.settings.placeboColorAdjustmentEnabled && (AstraPlay.settings.placeboColorAdjustmentPreset == 0)
                    }

                    C.Slider {
                        Layout.preferredWidth: 250
                        from: 0.0
                        to: 100.0
                        stepSize: 0.1
                        value: AstraPlay.settings.placeboColorAdjustmentContrast
                        onMoved: AstraPlay.settings.placeboColorAdjustmentContrast = value
                        visible: AstraPlay.settings.placeboColorAdjustmentEnabled && (AstraPlay.settings.placeboColorAdjustmentPreset == 0)

                        Label {
                            anchors {
                                left: parent.right
                                verticalCenter: parent.verticalCenter
                                leftMargin: 10
                            }
                            text: qsTr(parent.value.toFixed(1))
                        }
                    }

                    Label {
                        Layout.alignment: Qt.AlignRight
                        text: qsTr("(1.0)")
                        visible: AstraPlay.settings.placeboColorAdjustmentEnabled && (AstraPlay.settings.placeboColorAdjustmentPreset == 0)
                    }

                    Label {
                        Layout.alignment: Qt.AlignRight
                        text: qsTr("Saturation:")
                        visible: AstraPlay.settings.placeboColorAdjustmentEnabled && (AstraPlay.settings.placeboColorAdjustmentPreset == 0)
                    }

                    C.Slider {
                        Layout.preferredWidth: 250
                        from: 0.0
                        to: 100.0
                        stepSize: 0.1
                        value: AstraPlay.settings.placeboColorAdjustmentSaturation
                        onMoved: AstraPlay.settings.placeboColorAdjustmentSaturation = value
                        visible: AstraPlay.settings.placeboColorAdjustmentEnabled && (AstraPlay.settings.placeboColorAdjustmentPreset == 0)

                        Label {
                            anchors {
                                left: parent.right
                                verticalCenter: parent.verticalCenter
                                leftMargin: 10
                            }
                            text: qsTr(parent.value.toFixed(1))
                        }
                    }

                    Label {
                        Layout.alignment: Qt.AlignRight
                        text: qsTr("(1.0)")
                        visible: AstraPlay.settings.placeboColorAdjustmentEnabled && (AstraPlay.settings.placeboColorAdjustmentPreset == 0)
                    }

                    Label {
                        Layout.alignment: Qt.AlignRight
                        text: qsTr("Hue:")
                        visible: AstraPlay.settings.placeboColorAdjustmentEnabled && (AstraPlay.settings.placeboColorAdjustmentPreset == 0)
                    }

                    C.Slider {
                        Layout.preferredWidth: 250
                        from: 0.00
                        to: 6.28
                        stepSize: 0.01
                        value: AstraPlay.settings.placeboColorAdjustmentHue
                        onMoved: AstraPlay.settings.placeboColorAdjustmentHue = value
                        visible: AstraPlay.settings.placeboColorAdjustmentEnabled && (AstraPlay.settings.placeboColorAdjustmentPreset == 0)

                        Label {
                            anchors {
                                left: parent.right
                                verticalCenter: parent.verticalCenter
                                leftMargin: 10
                            }
                            text: qsTr(parent.value.toFixed(2))
                        }
                    }

                    Label {
                        Layout.alignment: Qt.AlignRight
                        text: qsTr("(0.00)")
                        visible: AstraPlay.settings.placeboColorAdjustmentEnabled && (AstraPlay.settings.placeboColorAdjustmentPreset == 0)
                    }

                    Label {
                        Layout.alignment: Qt.AlignRight
                        text: qsTr("Gamma:")
                        visible: AstraPlay.settings.placeboColorAdjustmentEnabled && (AstraPlay.settings.placeboColorAdjustmentPreset == 0)
                    }

                    C.Slider {
                        Layout.preferredWidth: 250
                        from: 0.0
                        to: 100.0
                        stepSize: 0.1
                        value: AstraPlay.settings.placeboColorAdjustmentGamma
                        onMoved: AstraPlay.settings.placeboColorAdjustmentGamma = value
                        visible: AstraPlay.settings.placeboColorAdjustmentEnabled && (AstraPlay.settings.placeboColorAdjustmentPreset == 0)

                        Label {
                            anchors {
                                left: parent.right
                                verticalCenter: parent.verticalCenter
                                leftMargin: 10
                            }
                            text: qsTr(parent.value.toFixed(1))
                        }
                    }

                    Label {
                        Layout.alignment: Qt.AlignRight
                        text: qsTr("(1.0)")
                        visible: AstraPlay.settings.placeboColorAdjustmentEnabled && (AstraPlay.settings.placeboColorAdjustmentPreset == 0)
                    }

                    Label {
                        Layout.alignment: Qt.AlignRight
                        text: qsTr("Temperature:")
                        visible: AstraPlay.settings.placeboColorAdjustmentEnabled && (AstraPlay.settings.placeboColorAdjustmentPreset == 0)
                    }

                    C.Slider {
                        Layout.preferredWidth: 250
                        from: -1.143
                        to: 5.286
                        stepSize: 0.001
                        value: AstraPlay.settings.placeboColorAdjustmentTemperature
                        onMoved: AstraPlay.settings.placeboColorAdjustmentTemperature = value
                        visible: AstraPlay.settings.placeboColorAdjustmentEnabled && (AstraPlay.settings.placeboColorAdjustmentPreset == 0)

                        Label {
                            anchors {
                                left: parent.right
                                verticalCenter: parent.verticalCenter
                                leftMargin: 10
                            }
                            text: qsTr(parent.value.toFixed(3))
                        }
                    }

                    Label {
                        Layout.alignment: Qt.AlignRight
                        text: qsTr("(0.000)")
                        visible: AstraPlay.settings.placeboColorAdjustmentEnabled && (AstraPlay.settings.placeboColorAdjustmentPreset == 0)
                    }
                }
            }

            Item {
                // Peak Detection
                GridLayout {
                    anchors {
                        top: parent.top
                        horizontalCenter: parent.horizontalCenter
                        topMargin: 20
                    }
                    columns: 3
                    rowSpacing: 10
                    columnSpacing: 20

                    Label {
                        Layout.alignment: Qt.AlignRight
                        text: qsTr("HDR Peak Detection Enabled:")
                    }

                    C.CheckBox {
                        text: qsTr("Enable HDR Peak Detection")
                        checked: AstraPlay.settings.placeboPeakDetectionEnabled
                        onToggled: AstraPlay.settings.placeboPeakDetectionEnabled = checked
                    }

                    Label {
                        Layout.alignment: Qt.AlignRight
                        text: qsTr("(Checked)")
                    }

                    Label {
                        Layout.alignment: Qt.AlignRight
                        text: qsTr("Peak Detection Preset:")
                        visible: AstraPlay.settings.placeboPeakDetectionEnabled
                    }

                    C.ComboBox {
                        Layout.preferredWidth: 400
                        model: [qsTr("Custom"), qsTr("Default"), qsTr("High Quality")]
                        currentIndex: AstraPlay.settings.placeboPeakDetectionPreset
                        onActivated: index => AstraPlay.settings.placeboPeakDetectionPreset = index
                        visible: AstraPlay.settings.placeboPeakDetectionEnabled
                    }

                    Label {
                        Layout.alignment: Qt.AlignRight
                        text: qsTr("(High Quality)")
                        visible: AstraPlay.settings.placeboPeakDetectionEnabled
                    }

                    Label {
                        Layout.alignment: Qt.AlignRight
                        text: qsTr("Peak Smoothing Period:")
                        visible: AstraPlay.settings.placeboPeakDetectionEnabled && (AstraPlay.settings.placeboPeakDetectionPreset == 0)
                    }

                    C.Slider {
                        Layout.preferredWidth: 250
                        from: 0.0
                        to: 1000.0
                        stepSize: 0.1
                        value: AstraPlay.settings.placeboPeakDetectionPeakSmoothingPeriod
                        onMoved: AstraPlay.settings.placeboPeakDetectionPeakSmoothingPeriod = value
                        visible: AstraPlay.settings.placeboPeakDetectionEnabled && (AstraPlay.settings.placeboPeakDetectionPreset == 0)

                        Label {
                            anchors {
                                left: parent.right
                                verticalCenter: parent.verticalCenter
                                leftMargin: 10
                            }
                            text: qsTr(parent.value.toFixed(1))
                        }
                    }

                    Label {
                        Layout.alignment: Qt.AlignRight
                        text: qsTr("(20.0)")
                        visible: AstraPlay.settings.placeboPeakDetectionEnabled && (AstraPlay.settings.placeboPeakDetectionPreset == 0)
                    }

                    Label {
                        Layout.alignment: Qt.AlignRight
                        text: qsTr("Scene Threshold Low:")
                        visible: AstraPlay.settings.placeboPeakDetectionEnabled && (AstraPlay.settings.placeboPeakDetectionPreset == 0)
                    }

                    C.Slider {
                        Layout.preferredWidth: 250
                        from: 0.0
                        to: 100.0
                        stepSize: 0.1
                        value: AstraPlay.settings.placeboPeakDetectionSceneThresholdLow
                        onMoved: AstraPlay.settings.placeboPeakDetectionSceneThresholdLow = value
                        visible: AstraPlay.settings.placeboPeakDetectionEnabled && (AstraPlay.settings.placeboPeakDetectionPreset == 0)

                        Label {
                            anchors {
                                left: parent.right
                                verticalCenter: parent.verticalCenter
                                leftMargin: 10
                            }
                            text: qsTr(parent.value.toFixed(1))
                        }
                    }

                    Label {
                        Layout.alignment: Qt.AlignRight
                        text: qsTr("(1.0)")
                        visible: AstraPlay.settings.placeboPeakDetectionEnabled && (AstraPlay.settings.placeboPeakDetectionPreset == 0)
                    }

                    Label {
                        Layout.alignment: Qt.AlignRight
                        text: qsTr("Scene Threshold High:")
                        visible: AstraPlay.settings.placeboPeakDetectionEnabled && (AstraPlay.settings.placeboPeakDetectionPreset == 0)
                    }

                    C.Slider {
                        Layout.preferredWidth: 250
                        from: 0.0
                        to: 100.0
                        stepSize: 0.1
                        value: AstraPlay.settings.placeboPeakDetectionSceneThresholdHigh
                        onMoved: AstraPlay.settings.placeboPeakDetectionSceneThresholdHigh = value
                        visible: AstraPlay.settings.placeboPeakDetectionEnabled && (AstraPlay.settings.placeboPeakDetectionPreset == 0)

                        Label {
                            anchors {
                                left: parent.right
                                verticalCenter: parent.verticalCenter
                                leftMargin: 10
                            }
                            text: qsTr(parent.value.toFixed(1))
                        }
                    }

                    Label {
                        Layout.alignment: Qt.AlignRight
                        text: qsTr("(3.0)")
                        visible: AstraPlay.settings.placeboPeakDetectionEnabled && (AstraPlay.settings.placeboPeakDetectionPreset == 0)
                    }

                    Label {
                        Layout.alignment: Qt.AlignRight
                        text: qsTr("Peak Percentile:")
                        visible: AstraPlay.settings.placeboPeakDetectionEnabled && (AstraPlay.settings.placeboPeakDetectionPreset == 0)
                    }

                    C.Slider {
                        Layout.preferredWidth: 250
                        from: 0.0
                        to: 100.0
                        stepSize: 0.001
                        value: AstraPlay.settings.placeboPeakDetectionPeakPercentile
                        onMoved: AstraPlay.settings.placeboPeakDetectionPeakPercentile = value
                        visible: AstraPlay.settings.placeboPeakDetectionEnabled && (AstraPlay.settings.placeboPeakDetectionPreset == 0)

                        Label {
                            anchors {
                                left: parent.right
                                verticalCenter: parent.verticalCenter
                                leftMargin: 10
                            }
                            text: qsTr(parent.value.toFixed(3))
                        }
                    }

                    Label {
                        Layout.alignment: Qt.AlignRight
                        text: qsTr("(99.995)")
                        visible: AstraPlay.settings.placeboPeakDetectionEnabled && (AstraPlay.settings.placeboPeakDetectionPreset == 0)
                    }

                    Label {
                        Layout.alignment: Qt.AlignRight
                        text: qsTr("Black Cutoff:")
                        visible: AstraPlay.settings.placeboPeakDetectionEnabled && (AstraPlay.settings.placeboPeakDetectionPreset == 0)
                    }

                    C.Slider {
                        Layout.preferredWidth: 250
                        from: 0.0
                        to: 100.0
                        stepSize: 0.1
                        value: AstraPlay.settings.placeboPeakDetectionBlackCutoff
                        onMoved: AstraPlay.settings.placeboPeakDetectionBlackCutoff = value
                        visible: AstraPlay.settings.placeboPeakDetectionEnabled && (AstraPlay.settings.placeboPeakDetectionPreset == 0)

                        Label {
                            anchors {
                                left: parent.right
                                verticalCenter: parent.verticalCenter
                                leftMargin: 10
                            }
                            text: qsTr(parent.value.toFixed(1))
                        }
                    }

                    Label {
                        Layout.alignment: Qt.AlignRight
                        text: qsTr("(1.0)")
                        visible: AstraPlay.settings.placeboPeakDetectionEnabled && (AstraPlay.settings.placeboPeakDetectionPreset == 0)
                    }

                    Label {
                    Layout.alignment: Qt.AlignRight
                    text: qsTr("Allow Delayed Peak:")
                    visible: AstraPlay.settings.placeboPeakDetectionEnabled && (AstraPlay.settings.placeboPeakDetectionPreset == 0)
                    }   

                    C.CheckBox {
                        text: qsTr("Enable 1-Frame Delayed Peak")
                        checked: AstraPlay.settings.placeboPeakDetectionAllowDelayedPeak
                        onToggled: AstraPlay.settings.placeboPeakDetectionAllowDelayedPeak = checked
                        visible: AstraPlay.settings.placeboPeakDetectionEnabled && (AstraPlay.settings.placeboPeakDetectionPreset == 0)
                    }

                    Label {
                        Layout.alignment: Qt.AlignRight
                        text: qsTr("(Unchecked)")
                        visible: AstraPlay.settings.placeboPeakDetectionEnabled && (AstraPlay.settings.placeboPeakDetectionPreset == 0)
                    }
                }
            }

            Item {
                // Color Mapping
                GridLayout {
                    anchors {
                        top: parent.top
                        horizontalCenter: parent.horizontalCenter
                        topMargin: 20
                    }
                    columns: 3
                    rowSpacing: 10
                    columnSpacing: 20

                    Label {
                        Layout.alignment: Qt.AlignRight
                        text: qsTr("Color Mapping Enabled:")
                    }

                    C.CheckBox {
                        text: qsTr("Use Color Mapping Settings")
                        checked: AstraPlay.settings.placeboColorMappingEnabled
                        onToggled: AstraPlay.settings.placeboColorMappingEnabled = checked
                    }

                    Label {
                        Layout.alignment: Qt.AlignRight
                        text: qsTr("(Checked)")
                    }

                    Label {
                        Layout.alignment: Qt.AlignRight
                        text: qsTr("Color Mapping Preset:")
                        visible: AstraPlay.settings.placeboColorMappingEnabled
                    }

                    C.ComboBox {
                        Layout.preferredWidth: 400
                        model: [qsTr("Custom"), qsTr("Default"), qsTr("High Quality")]
                        currentIndex: AstraPlay.settings.placeboColorMappingPreset
                        onActivated: index => AstraPlay.settings.placeboColorMappingPreset = index
                        visible: AstraPlay.settings.placeboColorMappingEnabled
                    }

                    Label {
                        Layout.alignment: Qt.AlignRight
                        text: qsTr("(High Quality)")
                        visible: AstraPlay.settings.placeboColorMappingEnabled
                    }

                    Label {
                        Layout.alignment: Qt.AlignRight
                        text: qsTr("Color Mapping Settings:")
                        visible: AstraPlay.settings.placeboColorMappingEnabled && AstraPlay.settings.placeboColorMappingPreset == 0
                    }

                    C.Button {
                        text: qsTr("Open")
                        onClicked: root.showPlaceboColorMappingDialog()
                        visible: AstraPlay.settings.placeboColorMappingEnabled && AstraPlay.settings.placeboColorMappingPreset == 0
                        Material.roundedScale: Material.SmallScale
                    }
                }
            }
        }
    }
}
