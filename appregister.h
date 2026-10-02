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

#ifndef APPREGISTER_H
#define APPREGISTER_H

/*
 * The QML types and metatypes this program exposes.
 *
 * Lifted out of main.cpp for the same reason the colour table was: anything
 * that is not the application could not reproduce its environment. A page
 * hosting a QQuickWidget loads QML that imports Vedder.vesc.utility, and
 * without these registrations the engine reports the module as not installed
 * and the scene never loads -- so a snapshot of such a page records an empty
 * view and looks perfectly fine.
 *
 * Safe to call once, before the QML engine is used.
 */
namespace VtApp {

void registerTypes();

}

#endif // APPREGISTER_H
