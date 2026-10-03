/*
    Copyright 2021 Benjamin Vedder	benjamin@vedder.se

    This file is part of VESC Tool.

    VESC Tool is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    VESC Tool is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
    */

import QtQuick 2.7
import QtQuick.Controls 2.2
import QtQuick.Layouts 1.3
import Qt.labs.settings 1.0 as QSettings
import Qt.labs.platform 1.1

import Vedder.vesc.vescinterface 1.0
import Vedder.vesc.utility 1.0

Item {
    implicitHeight: grid.implicitHeight
    property var dialogParent: ApplicationWindow.overlay

    // Android reaches the log file by a different route entirely; see the
    // Choose folder button below.
    readonly property bool isAndroid: Qt.platform.os === "android"

    // Display name of the granted folder, kept current by the signal below
    // rather than re-read on every paint.
    property string logFolderName: isAndroid ? Utility.logDirectoryName() : ""

    Connections {
        target: Utility
        function onLogDirectoryChanged(name) {
            logFolderName = name
        }
    }

    GridLayout {
        id: grid

        anchors.fill: parent
        anchors.topMargin: -5
        anchors.bottomMargin: -5

        clip: false
        visible: true
        rowSpacing: -10
        columnSpacing: 5
        rows: 3
        columns: 3

        Button {
            text: "Help"
            Layout.fillWidth: true

            onClicked: {
                VescIf.emitMessageDialog(
                            mInfoConf.getLongName("help_rt_logging"),
                            mInfoConf.getDescription("help_rt_logging"),
                            true, true)
            }
        }

        /*
         * On Android the log goes to a folder the user grants once through
         * the system picker, and there is no path to type or reset: see
         * Utility::pickLogDirectory. Writing into Documents with
         * WRITE_EXTERNAL_STORAGE stopped working at API 30, and the
         * permission request was a stub returning true, so the old controls
         * offered a choice that could not work.
         *
         * Everywhere else the path field and its Reset and Browse buttons
         * are unchanged.
         */
        Button {
            text: isAndroid ? "Choose folder" : "Reset"
            Layout.fillWidth: true
            Layout.columnSpan: isAndroid ? 2 : 1
            focusPolicy: Qt.NoFocus

            onClicked: {
                if (isAndroid) {
                    Utility.pickLogDirectory()
                } else {
                    rtLogFileText.text = StandardPaths.writableLocation(StandardPaths.DocumentsLocation) + "/logs"
                }
            }
        }

        Button {
            text: "Browse"
            Layout.fillWidth: true
            focusPolicy: Qt.NoFocus
            visible: !isAndroid

            onClicked: {
                if (Utility.requestFilePermission()) {
                    logFilePicker.folder = rtLogFileText.text
                    logFilePicker.enabled = true
                    logFilePicker.visible = true
//                    folderDialog.close()
//                    folderDialog.open()
                } else {
                    VescIf.emitMessageDialog(
                                "File Permissions",
                                "Unable to request file system permission.",
                                false, false)
                }
            }

            FolderDialog {
                id: folderDialog
                title: "Please choose a folder"
                onAccepted: {
                    rtLogFileText.text = folderDialog.folder
                    close()
                    parent.forceActiveFocus()
                }
                onRejected: {
                    close()
                    parent.forceActiveFocus()
                }
            }

            DirectoryPicker {
                id: logFilePicker
                anchors.fill: parent
                showDotAndDotDot: true
                visible: false
                enabled: false
                parent: dialogParent

                onDirSelected: {
                    rtLogFileText.text = fileName
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.columnSpan: 3
            Layout.topMargin: 6
            Layout.bottomMargin: 6
            height: rtLogFileText.implicitHeight + 14
            border.width: 2
            border.color: Utility.getAppHexColor("disabledText")
            color: Utility.getAppHexColor("normalBackground")
            radius: 3

            TextInput {
                color: Utility.getAppHexColor("lightText")
                id: rtLogFileText
                anchors.fill: parent
                anchors.margins: 7
                font.pointSize: 12
                text: StandardPaths.writableLocation(StandardPaths.DocumentsLocation) + "/logs"
                clip: true

                // Not editable on Android: the folder is whatever the user
                // granted, and a typed path cannot be written there.
                readOnly: isAndroid

                QSettings.Settings {
                    property alias path_rt_log: rtLogFileText.text
                }
            }

            // Covers the path field on Android, because what is stored for
            // that platform is a tree URI and showing one to someone is
            // useless.
            Text {
                anchors.fill: parent
                anchors.margins: 7
                font.pointSize: 12
                verticalAlignment: Text.AlignVCenter
                elide: Text.ElideMiddle
                visible: isAndroid
                color: Utility.getAppHexColor(
                           logFolderName.length > 0 ? "lightText" : "disabledText")
                text: logFolderName.length > 0
                      ? ("Logging to: " + logFolderName)
                      : "No folder chosen yet"
            }
        }

        CheckBox {
            id: rtLogEnBox
            text: "Enable RT Data Logging"
            Layout.fillWidth: true
            Layout.columnSpan: 3
            focusPolicy: Qt.NoFocus

            onClicked: {
                if (checked && !VescIf.isPortConnected()) {
                    VescIf.emitMessageDialog("Log Data",
                                             "You must be connected to the VESC in order to log data.",
                                             false, false)
                    checked = false
                    return
                }

                if (checked) {
                    if (isAndroid
                            ? VescIf.openRtLogFileSaf()
                            : VescIf.openRtLogFile(rtLogFileText.text)) {
                        VescIf.emitStatusMessage("Logging Started", true)
                        Utility.startGnssForegroundService()
                        VescIf.setWakeLock(true)

                        VescIf.emitMessageDialog("Logging Started",
                                                 "Motor data together with your location is now being logged to:\n\n" +
                                                 VescIf.rtLogFilePath() + "\n\n" +
                                                 "You can connect your device to a computer and transfer the file to it for " +
                                                 "analysis in the desktop version of ESCargot Tool (under Data Analysis->Log Analysis).",
                                                 true, false)
                    } else {
                        VescIf.emitStatusMessage("Logging Failed", false)
                    }
                } else {
                    VescIf.closeRtLogFile()
                    VescIf.emitStatusMessage("Log Saved", true)
                    Utility.stopGnssForegroundService()

                    if (!VescIf.useWakeLock()) {
                        VescIf.setWakeLock(false)
                    }
                }
            }

            Timer {
                repeat: true
                running: true
                interval: 500

                onTriggered: {
                    if (rtLogEnBox.checked && !VescIf.isRtLogOpen()) {
                        Utility.stopGnssForegroundService()

                        if (!VescIf.useWakeLock()) {
                            VescIf.setWakeLock(false)
                        }
                    }

                    rtLogEnBox.checked = VescIf.isRtLogOpen()
                }
            }
        }
    }
}
