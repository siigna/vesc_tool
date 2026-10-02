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

#ifndef UIHARNESS_H
#define UIHARNESS_H

#include <QJsonObject>
#include <QString>
#include <QWidget>

class VescInterface;

/*
 * Shared scaffolding for the widget tests.
 *
 * Two jobs. The first is making the process hermetic, which has to happen
 * before the QApplication exists -- see pinEnvironment(). The second is
 * turning a widget tree into something a diff can be taken of.
 *
 * No board, no display server, no network: pages are constructed directly and
 * their configuration comes from the parameter XML compiled into the binary.
 */
namespace UiHarness {

/*
 * Points every XDG root at a fresh temporary directory and pins the locale and
 * scaling. MUST be called before the QApplication is constructed, which is why
 * the suite writes its own main() instead of using QTEST_MAIN.
 *
 * XDG_DATA_HOME matters as much as XDG_CONFIG_HOME: Utility::configPath
 * registers $AppData/res_config.rcc over the compiled-in parameter XML if that
 * file exists, so a developer who has ever downloaded a config archive would
 * otherwise get different parameters than everybody else. That is not
 * hypothetical -- it is exactly what made the CLI refuse to connect earlier in
 * this work, reporting "No Supported Firmwares".
 *
 * Returns the temporary directory, which the caller should keep alive.
 */
QString pinEnvironment();

// Seeds the QSettings keys that must not come from the developer's machine.
void seedSettings();

/*
 * A VescInterface with the parameter XML loaded and the same suppression the
 * headless CLI paths apply, so nothing reaches the network. Never null: eight
 * pages dereference it in setVesc() without a guard.
 */
VescInterface *makeVesc();

/*
 * One page's widget tree as JSON: object name, class, enabled, visible, and
 * whatever text identifies a control. Row counts for ParamTable, because an
 * unknown or renamed parameter yields a silently empty table rather than an
 * error.
 */
QJsonObject describe(QWidget *page, const QString &pageName);

// Lets deferred layout work run. ParamTable finishes through a zero timer.
void settle();

/*
 * Waits real time, so constructor timers actually fire.
 *
 * settle() spins processEvents, which returns at once when the queue is empty
 * and therefore advances no wall-clock time -- a 50 ms timer never runs. That
 * matters for more than the tests: several pages ship controls enabled in
 * their .ui and only correct them when their timer first fires, so a snapshot
 * taken without waiting records a state the user never sees.
 */
void settleWithTimers(int ms = 120);

/*
 * Qt warnings collected since the last clearMessages(). Several failures in
 * this program are reported only as a warning and are otherwise invisible: a
 * missing icon draws nothing, and an unknown colour name comes back red. A
 * test that reads these turns both into real failures.
 */
void installMessageCapture();
void clearMessages();
QStringList messages();

/*
 * Compares against tests/ui/baseline/<name>.json. On a mismatch, writes
 * actual/<name>.json, reports the first differing path, and returns false.
 */
bool matchesBaseline(const QJsonObject &snap, const QString &name,
                     QString *detail);

}

#endif // UIHARNESS_H
