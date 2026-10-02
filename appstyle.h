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

#ifndef APPSTYLE_H
#define APPSTYLE_H

#include <QApplication>

/*
 * The application's visual identity: the named colour table, the bundled
 * fonts, and the style and palette.
 *
 * This lived inside main.cpp, which meant anything that was not the
 * application could not reproduce the way the application looks. That matters
 * most for the colour table: Utility::getAppQColor returns Qt::red for a name
 * it does not know and logs a line per miss, and a page constructor makes
 * dozens of lookups (Utility::setPlotColors alone makes about thirty per
 * plot). A test binary that skipped this would render red-on-red and bury its
 * own output in warnings.
 *
 * Call order is initColors, then registerFonts once a QApplication exists,
 * then applyStyle.
 */
namespace VtAppStyle {

/*
 * The organisation and application names. QSettings resolves through these,
 * so every page's default-constructed QSettings depends on them being set
 * before it runs.
 */
void initIdentity();

// The named colour table, Utility::setDarkMode, and the pixmap cache limit.
void initColors(bool isDark);

// The bundled DejaVu/Roboto/Exan faces, and the default application font.
void registerFonts();

// The list stylesheet, the Fusion-derived style, and the palette.
void applyStyle(QApplication *a, bool isDark);

}

#endif // APPSTYLE_H
