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
import QtQuick.Controls 2.10

import Vedder.vesc.commands 1.0
import Vedder.vesc.configparams 1.0

/*
 * The environment mobile/main.qml provides to everything below it.
 *
 * Not a mock. The mobile components are written against main.qml's scope and
 * cannot be instantiated without it -- three different ways, all of which
 * showed up the first time this suite created them bare:
 *
 *   StartPage.qml      reads `mainSwipeView`, which is an id in main.qml.
 *                      An id lives in its document's context, and a component
 *                      main.qml creates inherits that context, so the
 *                      reference resolves there and nowhere else.
 *   DetectBldc.qml     and the three other Detect* pages default
 *                      `dialogParent` to `ApplicationWindow.overlay`, an
 *                      attached property that is undefined unless there is a
 *                      real ApplicationWindow above.
 *   ParamList.qml      and ParamListScroll.qml read `parent.width`, so a null
 *                      creation parent is not a case they have to handle.
 *
 * So this is an ApplicationWindow carrying main.qml's own properties and ids,
 * and `make()` creates into its context. Anything a component needs that is
 * missing here is a thing main.qml supplies, and belongs here rather than in
 * a test.
 */
ApplicationWindow {
    id: appWindow

    width: 480
    height: 800

    /*
     * Shown, because ApplicationWindow.overlay -- which the four Detect*
     * pages use as their default dialogParent -- does not exist until the
     * window is set up. Harmless offscreen, which is how this suite runs.
     */
    visible: true

    // main.qml's own initialisers, which is what a device with no notch
    // reports.
    property int notchLeft: 0
    property int notchRight: 0
    property int notchBot: 0
    property int notchTop: 0
    property bool mainIsHorizontal: appWindow.width > appWindow.height

    property Commands mCommands: VescIf.commands()
    property ConfigParams mMcConf: VescIf.mcConfig()
    property ConfigParams mAppConf: VescIf.appConfig()
    property ConfigParams mInfoConf: VescIf.infoConfig()
    property bool connected: false
    property bool fwReadCorrectly: false

    SwipeView {
        id: mainSwipeView
        anchors.fill: parent
    }

    /*
     * Creates a mobile component in this document's context.
     *
     * Returns { error, object }: a compile failure comes back as a non-empty
     * error rather than a thrown exception, so a caller can report which file
     * and why.
     */
    function make(file, props) {
        var c = Qt.createComponent("qrc:/mobile/" + file)

        if (c.status !== Component.Ready) {
            return { error: c.errorString(), object: null }
        }

        var o = c.createObject(mainSwipeView, props === undefined ? {} : props)

        if (o === null) {
            return { error: "createObject returned null", object: null }
        }

        return { error: "", object: o }
    }
}
