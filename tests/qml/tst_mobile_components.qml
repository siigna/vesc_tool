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
import QtTest 1.2

/*
 * Every mobile component instantiates, and does so without the engine
 * complaining.
 *
 * Compiling (tst_mobile_load.qml) and instantiating are different failures.
 * A component compiles with a binding loop in it and loads with one too; what
 * the loop costs is that Qt breaks it by refusing to re-evaluate, so a size
 * or a position keeps whatever value the aborted pass left behind. Nothing
 * crashes and nothing is logged where anyone looks, which is why three of
 * these had been in the tree for years.
 *
 * The zero-warning assertion is the point of the file. It found, and the
 * fixes beside it closed:
 *
 *   SetupWizardFoc.qml  TabButton.width / TabBar.implicitWidth loops
 *   SetupWizardIMU.qml  Connections with an implicitly defined onFoo, which
 *                       is an error rather than a warning in Qt 6
 *   StartPage.qml       Dialog.implicitWidth loop, from FwUpdate.qml
 *                       anchoring itself to its parent
 */
TestCase {
    name: "MobileComponents"
    when: windowShown
    width: 480
    height: 800

    /*
     * The scope the components are written against. See MobileScope.qml --
     * without it, six of them warn about an undefined parent, an undefined
     * ApplicationWindow.overlay, or main.qml's mainSwipeView id, none of
     * which is a defect in the component.
     */
    MobileScope {
        id: scope
    }

    // Not instantiable in this build; the reason is recorded in
    // tst_mobile_load.qml, which asserts it both ways.
    readonly property var notInstantiable: ["Vesc3DView.qml"]

    function test_instantiatesWithoutWarnings_data() {
        var out = []
        var files = QmlProbe.mobileFiles()

        for (var i = 0; i < files.length; i++) {
            if (notInstantiable.indexOf(files[i]) >= 0) {
                continue
            }

            out.push({ tag: files[i], file: files[i] })
        }

        return out
    }

    function test_instantiatesWithoutWarnings(row) {
        QmlProbe.clearWarnings()

        var made = scope.make(row.file)
        compare(made.error, "", row.file + ": " + made.error)

        // Bindings and layout passes are not all synchronous; a binding loop
        // in particular is reported when the second pass runs.
        wait(10)

        var w = QmlProbe.warnings()
        compare(w.length, 0, row.file + " warned: " + w.join(" | "))

        made.object.destroy()
    }

    /*
     * StartPage's firmware dialog gives its contentItem the whole content
     * area.
     *
     * Pins the fix rather than the bug. That contentItem used to carry
     * `anchors.fill: parent` plus a topMargin of the header height and a
     * bottomMargin of 50, all three of which a Dialog already does for it --
     * and which together closed the implicitWidth loop. With the anchors
     * removed the margins would have eaten the content area instead of
     * protecting it, so this checks the content really does start below the
     * header and end above the footer, with nothing left over.
     */
    function test_firmwareDialogFillsItsContentArea() {
        var made = scope.make("StartPage.qml", { width: 480, height: 800 })
        compare(made.error, "", made.error)

        var page = made.object
        var dlg = QmlProbe.findChild(page, "firmwareDialog")
        verify(dlg !== null, "no child named firmwareDialog")

        dlg.open()
        wait(50)

        verify(dlg.contentItem !== null, "dialog has no contentItem")

        /*
         * Dialog's own account of the space it has, minus what the header
         * and footer take out of it. availableHeight is the padded box, and
         * header and footer sit inside it -- so this is the identity a
         * correctly laid out contentItem satisfies, and it is also what the
         * removed topMargin and bottomMargin were each double-counting.
         */
        compare(dlg.contentItem.width, dlg.availableWidth,
                "content is not as wide as the content area: " +
                dlg.contentItem.width + " vs " + dlg.availableWidth)

        var taken = dlg.header.height + dlg.footer.height

        compare(dlg.contentItem.height, dlg.availableHeight - taken,
                "content does not fill the space between header and footer: " +
                dlg.contentItem.height + " vs " +
                (dlg.availableHeight - taken))

        verify(taken > 0, "neither header nor footer has any height")
        verify(dlg.contentItem.height > 0, "content area collapsed to nothing")

        dlg.close()
        page.destroy()
    }

    /*
     * DoubleSpinBox is the one mobile component with arithmetic in it rather
     * than layout: Controls' SpinBox is integer-only, so this wraps it with a
     * scale factor of 10^decimals and converts in both directions. Every
     * numeric motor parameter on a phone is edited through it.
     */
    function test_doubleSpinBoxScalesByDecimals() {
        var made = scope.make("DoubleSpinBox.qml", {
            decimals: 2,
            realFrom: 0.0,
            realTo: 10.0,
            realStepSize: 0.5,
            realValue: 3.25
        })
        compare(made.error, "", made.error)

        var sb = made.object

        // The round trip through the integer SpinBox keeps two decimals.
        compare(sb.realValue, 3.25, "realValue did not survive the scaling")

        sb.realValue = 7.5
        compare(sb.realValue, 7.5, "realValue did not take a new value")

        sb.destroy()
    }

    function test_doubleSpinBoxClampsToRange() {
        var made = scope.make("DoubleSpinBox.qml", {
            decimals: 1,
            realFrom: -5.0,
            realTo: 5.0,
            realStepSize: 1.0,
            realValue: 0.0
        })
        compare(made.error, "", made.error)

        var sb = made.object

        /*
         * The inner SpinBox clamps to from/to, and its onValueChanged writes
         * the clamped value back out -- so an out-of-range assignment comes
         * back at the limit rather than being kept. A parameter editor that
         * did not do this would send a motor configuration value outside the
         * range the XML declares.
         */
        sb.realValue = 50.0
        wait(10)
        compare(sb.realValue, 5.0, "a value above realTo was not clamped")

        sb.realValue = -50.0
        wait(10)
        compare(sb.realValue, -5.0, "a value below realFrom was not clamped")

        sb.destroy()
    }
}
