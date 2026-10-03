/*
	Copyright 2026 Stephen Bouche

	This file is part of ESCargot Tool.

	ESCargot Tool is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	ESCargot Tool is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

import QtQuick 2.10
import QtTest 1.2

/*
 * Every mobile QML file compiles, and the set of them is what the resource
 * says it is.
 *
 * This is the cheapest test that would have caught something real. A missing
 * or misspelled import, a typo in a property name on a declared type, a
 * renamed C++ type -- none of these are build errors, because QML is compiled
 * when it is loaded. The mobile application loads main.qml and nothing else
 * until the user navigates, so a broken ConfigPageCustom.qml ships.
 */
TestCase {
    name: "MobileLoad"

    /*
     * Files that cannot compile in this build, with the reason, asserted
     * exactly rather than skipped.
     *
     * Vesc3DView.qml imports Qt3D.Core/Render/Input/Extras and
     * QtQuick.Scene3D. qt3d is not among the Qt modules the package depends
     * on -- checked against the wrapper the nix build produces, which carries
     * qtquickcontrols, qtquickcontrols2, qtgraphicaleffects, qtpositioning,
     * qtgamepad, qtconnectivity, qtsvg and qtwayland, and no qt3d. So the
     * file ships inside mobile/qml.qrc and can never load.
     *
     * Nothing references it either: the only Vesc3DView in the program is
     * widgets/vesc3dview.cpp, a QOpenGLWidget, which is a different class
     * that the three desktop IMU pages use. The QML one is dead.
     *
     * Left in place rather than deleted because it is upstream's file and
     * this is a fork; recorded here so the state is a decision and not an
     * oversight. Either add qt3d to the package or drop the file -- and
     * whichever happens, this test fails until the list is updated.
     */
    readonly property var knownUnloadable: ({
        "Vesc3DView.qml": "imports Qt3D.*, and qt3d is not a dependency of this build"
    })

    // The file list comes from the resource, not from here, so a newly added
    // QML file is covered without this file changing.
    function test_everyFileCompiles_data() {
        var out = []
        var files = QmlProbe.mobileFiles()

        for (var i = 0; i < files.length; i++) {
            out.push({ tag: files[i], file: files[i] })
        }

        return out
    }

    function test_everyFileCompiles(row) {
        var expectedBad = knownUnloadable.hasOwnProperty(row.file)
        var c = Qt.createComponent("qrc:/mobile/" + row.file)

        // createComponent is synchronous for a qrc: URL, but the status is
        // checked rather than assumed: an asynchronous Loading would compare
        // unequal and say so instead of passing silently.
        verify(c.status !== Component.Loading,
               row.file + " did not compile synchronously")

        if (expectedBad) {
            /*
             * Asserted in both directions. A file on the list that starts
             * compiling is as much a reason to fail as one that stops: it
             * means the reason recorded above is no longer true and the entry
             * has to go.
             */
            compare(c.status, Component.Error,
                    row.file + " now compiles; remove it from knownUnloadable (" +
                    knownUnloadable[row.file] + ")")
        } else {
            compare(c.status, Component.Ready,
                    row.file + ": " + c.errorString())
        }

        c.destroy()
    }

    // A QML file present on disk but absent from qml.qrc is not in the
    // application at all, and nothing else notices. Guards the list this
    // suite derives everything else from.
    function test_resourceListIsNotEmpty() {
        var files = QmlProbe.mobileFiles()

        verify(files.length >= 56,
               "expected at least 56 mobile QML files, found " + files.length)
        verify(files.indexOf("main.qml") >= 0, "main.qml missing from qml.qrc")
    }

    // Every name on the exception list is a file that actually exists. A
    // renamed or deleted file would otherwise leave a stale entry behind,
    // quietly exempting nothing.
    function test_exceptionListIsCurrent() {
        var files = QmlProbe.mobileFiles()

        for (var name in knownUnloadable) {
            verify(files.indexOf(name) >= 0,
                   "knownUnloadable names " + name + ", which is not in qml.qrc")
        }
    }
}
