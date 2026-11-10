import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

ApplicationWindow {
    id: win
    width: 1040
    height: 840
    minimumWidth: 760
    minimumHeight: 740
    visible: true
    title: "miniMover"
    color: "#101216"
    Material.theme: Material.Dark
    Material.accent: theme.accent
    Material.roundedScale: theme.radius
    property var s: backend.state
    property bool typing: activeFocusItem instanceof TextInput || activeFocusItem instanceof TextEdit
    property string pickerError: ""
    property string pendingAction: ""
    property var pendingArguments: ({})
    property bool controlsEnabled: !!s.connected && !s.busy && !s.printing

    function confirm(action, args, message) {
        pendingAction = action
        pendingArguments = args
        confirmation.text = message
        confirmation.open()
    }
    onClosing: function(close) {
        if (s.busy) {
            close.accepted = false
            blocked.open()
        } else if (s.printing) {
            close.accepted = false
            confirm("quit", {}, qsTr("Close monitoring? The printer will continue printing."))
        }
    }
    Shortcut { sequence: "?"; context: Qt.ApplicationShortcut; enabled: !win.typing; onActivated: help.open() }
    Shortcut { sequence: "Q"; context: Qt.ApplicationShortcut; enabled: !win.typing; onActivated: win.close() }
    Shortcut { sequence: "Space"; context: Qt.ApplicationShortcut; enabled: !win.typing && !win.s.busy; onActivated: backend.command("status") }
    Shortcut { sequence: "Ctrl+R"; context: Qt.ApplicationShortcut; enabled: !!win.s.connected && !win.s.busy; onActivated: backend.command("status") }
    Shortcut { sequence: "Ctrl+O"; context: Qt.ApplicationShortcut; onActivated: { tabs.currentIndex = 0; filePicker.pick("print") } }

    Connections {
        target: filePicker
        function onChosen(purpose, path) {
            win.pickerError = ""
            if (purpose === "print") printPath.text = path
            else if (purpose === "input") inputPath.text = path
            else outputPath.text = path
        }
        function onError(message) { win.pickerError = message }
    }

    Item {
        anchors.fill: parent
        LayoutMirroring.enabled: Qt.application.layoutDirection === Qt.RightToLeft
        LayoutMirroring.childrenInherit: true
        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 28
            spacing: 16
            RowLayout {
                ColumnLayout {
                    spacing: 2
                    Label { text: "miniMover"; font.pixelSize: 30; font.bold: true }
                    Label { text: qsTr("XYZ da Vinci · printer control"); color: "#a1a7b3" }
                }
                Item { Layout.fillWidth: true }
                Label { text: win.s.connected ? qsTr("Connected") : qsTr("Offline"); color: win.s.connected ? "#7ed6a2" : "#a1a7b3" }
                Button { text: qsTr("Shortcuts"); onClicked: help.open() }
            }
            Rectangle {
                Layout.fillWidth: true
                implicitHeight: connectionRow.implicitHeight + 24
                color: "#1b1e25"
                radius: theme.radius
                RowLayout {
                    id: connectionRow
                    anchors.fill: parent
                    anchors.margins: 12
                    ComboBox { id: transport; model: [qsTr("USB serial"), qsTr("Wi-Fi (experimental)")]; enabled: !win.s.busy && !win.s.connected; Layout.preferredWidth: 210 }
                    TextField { id: device; objectName: "device"; placeholderText: transport.currentIndex === 0 ? qsTr("/dev/ttyACM0 (blank: auto-detect)") : qsTr("Printer IP address or hostname"); Layout.fillWidth: true; enabled: !win.s.busy && !win.s.connected; selectByMouse: true }
                    Button {
                        text: win.s.connected ? qsTr("Disconnect") : qsTr("Connect")
                        enabled: !win.s.busy
                        onClicked: {
                            if (win.s.connected) {
                                if (win.s.printing) win.confirm("disconnect", {}, qsTr("Disconnect monitoring? The printer will continue printing."))
                                else backend.command("disconnect")
                            } else backend.command("connect", {device: device.text, network: transport.currentIndex === 1})
                        }
                    }
                }
            }
            Rectangle {
                Layout.fillWidth: true
                implicitHeight: 136
                radius: theme.radius
                color: "#1b1e25"
                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 18
                    RowLayout {
                        Label { text: win.s.model || qsTr("No printer selected"); font.bold: true; font.pixelSize: 17; Layout.fillWidth: true }
                        Button { text: qsTr("Refresh"); enabled: !!win.s.connected && !win.s.busy; onClicked: backend.command("status") }
                    }
                    RowLayout {
                        spacing: 30
                        Label { text: win.s.connected ? (win.s.printerStatus || qsTr("Ready")) : qsTr("Connect over USB to read printer status"); Layout.fillWidth: true; wrapMode: Text.Wrap }
                        Label { text: qsTr("Nozzle: %1 / %2 °C").arg(win.s.temperature ?? "—").arg(win.s.targetTemperature ?? "—"); color: "#a1a7b3" }
                        Label {
                            text: {
                                let lang = translator.language
                                return qsTr("Filament: %1 m").arg(win.s.filament === undefined ? "—" : Number(win.s.filament).toLocaleString(Qt.locale(), 'f', 1))
                            }
                            color: "#a1a7b3"
                        }
                    }
                    ProgressBar { Layout.fillWidth: true; from: 0; to: 100; value: win.s.printProgress || 0; visible: !!win.s.printing }
                }
            }
            TabBar {
                id: tabs
                objectName: "tabs"
                Layout.fillWidth: true
                TabButton { text: qsTr("Print") }
                TabButton { text: qsTr("Convert files") }
                TabButton { text: qsTr("Printer controls") }
            }
            StackLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                currentIndex: tabs.currentIndex
                ColumnLayout {
                    spacing: 16
                    Label { text: qsTr("Send a print file"); font.pixelSize: 22; font.bold: true }
                    Label { text: qsTr("G-code is converted for the connected printer before upload. Clear the bed before starting."); wrapMode: Text.Wrap; Layout.fillWidth: true; color: "#a1a7b3" }
                    RowLayout {
                        TextField { id: printPath; objectName: "printPath"; placeholderText: qsTr("Path to .gcode or .3w file"); Layout.fillWidth: true; selectByMouse: true; enabled: !win.s.busy }
                        Button { text: qsTr("Browse…"); enabled: !win.s.busy; onClicked: filePicker.pick("print") }
                    }
                    Button {
                        text: qsTr("Upload and print")
                        highlighted: true
                        enabled: win.controlsEnabled && printPath.text.length > 0
                        onClicked: win.confirm("print", {input: printPath.text}, qsTr("Start printing? Confirm the bed is clear and the selected file is safe for this printer."))
                    }
                    RowLayout {
                        Button { text: qsTr("Pause"); enabled: !!win.s.printing && !win.s.busy; onClicked: backend.command("pause") }
                        Button { text: qsTr("Resume"); enabled: !!win.s.printing && !win.s.busy; onClicked: backend.command("resume") }
                        Button { text: qsTr("Cancel print"); enabled: !!win.s.printing && !win.s.busy; onClicked: win.confirm("cancel", {}, qsTr("Cancel the current print? This cannot be undone.")) }
                    }
                    Item { Layout.fillHeight: true }
                }
                ColumnLayout {
                    spacing: 12
                    Label { text: qsTr("Convert without a printer"); font.pixelSize: 22; font.bold: true }
                    Label { text: qsTr("Convert .3w to G-code, or G-code to .3w for the selected model. Originals are never overwritten."); Layout.fillWidth: true; wrapMode: Text.Wrap; color: "#a1a7b3" }
                    RowLayout {
                        TextField { id: inputPath; objectName: "inputPath"; placeholderText: qsTr("Input file"); Layout.fillWidth: true; selectByMouse: true; enabled: !win.s.busy }
                        Button { text: qsTr("Browse…"); enabled: !win.s.busy; onClicked: filePicker.pick("input") }
                    }
                    RowLayout {
                        TextField { id: outputPath; objectName: "outputPath"; placeholderText: qsTr("New output file path"); Layout.fillWidth: true; selectByMouse: true; enabled: !win.s.busy }
                        Button { text: qsTr("Choose…"); enabled: !win.s.busy; onClicked: filePicker.pick("output", true) }
                    }
                    ComboBox { id: model; objectName: "model"; model: backend.models; Layout.fillWidth: true; enabled: !win.s.busy }
                    Button { text: qsTr("Convert"); objectName: "convertButton"; highlighted: true; enabled: !win.s.busy && inputPath.text.length > 0 && outputPath.text.length > 0; onClicked: backend.command("convert", {input: inputPath.text, output: outputPath.text, model: model.currentIndex}) }
                    Item { Layout.fillHeight: true }
                }
                ColumnLayout {
                    spacing: 12
                    Label { text: qsTr("Maintenance and movement"); font.pixelSize: 22; font.bold: true }
                    Label { text: qsTr("Keep the build area clear. Maintenance controls require a printer without an LCD tuning menu."); Layout.fillWidth: true; wrapMode: Text.Wrap; color: "#a1a7b3" }
                    Flow {
                        Layout.fillWidth: true
                        spacing: 10
                        Repeater {
                            model: [{label: qsTr("Home"), action: "home"}, {label: qsTr("Load filament"), action: "load"}, {label: qsTr("Unload filament"), action: "unload"}, {label: qsTr("Calibrate bed"), action: "calibrate"}, {label: qsTr("Clean nozzle"), action: "clean"}]
                            Button { required property var modelData; text: modelData.label; enabled: win.controlsEnabled && !win.s.hasMenu; onClicked: win.confirm(modelData.action, {}, qsTr("Run this maintenance operation? The printer may move or heat its nozzle.")) }
                        }
                    }
                    RowLayout {
                        ComboBox { id: axis; model: ["X", "Y", "Z"]; enabled: win.controlsEnabled && !win.s.hasMenu }
                        SpinBox { id: distance; from: 1; to: 10; value: 1; enabled: win.controlsEnabled && !win.s.hasMenu }
                        Label { text: "mm" }
                        Button { text: qsTr("Jog −"); enabled: win.controlsEnabled && !win.s.hasMenu; onClicked: backend.command("jog", {axis: axis.currentText.toLowerCase(), distance: -distance.value}) }
                        Button { text: qsTr("Jog +"); enabled: win.controlsEnabled && !win.s.hasMenu; onClicked: backend.command("jog", {axis: axis.currentText.toLowerCase(), distance: distance.value}) }
                    }
                    RowLayout {
                        Button { text: qsTr("Enable auto-level"); enabled: win.controlsEnabled && !win.s.hasMenu; onClicked: backend.command("level", {enabled: true}) }
                        Button { text: qsTr("Disable auto-level"); enabled: win.controlsEnabled && !win.s.hasMenu; onClicked: backend.command("level", {enabled: false}) }
                        Button { text: qsTr("Buzzer on"); enabled: win.controlsEnabled && !win.s.hasMenu; onClicked: backend.command("buzzer", {enabled: true}) }
                        Button { text: qsTr("Buzzer off"); enabled: win.controlsEnabled && !win.s.hasMenu; onClicked: backend.command("buzzer", {enabled: false}) }
                    }
                    Item { Layout.fillHeight: true }
                }
            }
            Frame {
                Layout.fillWidth: true
                visible: !!win.s.prompt
                ColumnLayout {
                    width: parent.width
                    Label {
                        Layout.fillWidth: true
                        wrapMode: Text.Wrap
                        text: win.s.prompt === "lower" ? qsTr("Lower the bed detector, then continue.") : win.s.prompt === "raise" ? qsTr("Raise the bed detector, then continue.") : win.s.prompt === "clean" ? qsTr("Clean the nozzle carefully, then continue.") : qsTr("Wait for filament to leave the nozzle, then continue.")
                    }
                    Button { text: qsTr("Continue"); onClicked: backend.command("continue") }
                }
            }
            ProgressBar { Layout.fillWidth: true; from: 0; to: 100; value: win.s.progress || 0; indeterminate: !!win.s.busy && !win.s.progress; visible: !!win.s.busy }
            Label { text: win.pickerError || win.s.message || ""; color: win.pickerError || win.s.error ? "#ff9393" : "#a1a7b3"; wrapMode: Text.Wrap; Layout.fillWidth: true }
            Label { text: win.s.printerError || ""; visible: text.length > 0; color: "#ff9393"; wrapMode: Text.Wrap; Layout.fillWidth: true }
        }
    }
    Dialog {
        id: confirmation
        property string text: ""
        anchors.centerIn: parent
        width: Math.min(520, win.width - 48)
        title: qsTr("Confirm operation")
        modal: true
        standardButtons: Dialog.Ok | Dialog.Cancel
        Label { text: confirmation.text; width: parent.width; wrapMode: Text.Wrap }
        onAccepted: {
            if (win.pendingAction === "quit") Qt.quit()
            else backend.command(win.pendingAction, win.pendingArguments)
        }
    }
    Dialog {
        id: blocked
        anchors.centerIn: parent
        title: qsTr("Operation in progress")
        modal: true
        standardButtons: Dialog.Ok
        Label { text: qsTr("Wait for the current operation to finish before closing.") }
    }
    Dialog {
        id: help
        anchors.centerIn: parent
        title: qsTr("Keyboard shortcuts")
        modal: true
        standardButtons: Dialog.Close
        Label { text: qsTr("Ctrl+O   Choose print file\nCtrl+R   Refresh printer status\nSpace    Refresh printer status\n?             Show shortcuts\nQ            Quit"); lineHeight: 1.7 }
    }
}
