/*
	Copyright 2016 - 2023 Benjamin Vedder	benjamin@vedder.se
	Copyright Jeffrey M. Friesen
	Copyright r3n33
	Copyright 2026 Stephen Bouche

	The palette, the font registration and the stylesheet in this file were
	moved here out of main.cpp, where git records them as 47 lines from
	Jeffrey M. Friesen, 19 from Benjamin Vedder and 16 from r3n33. What is
	new here is the split into initIdentity/initColors/registerFonts/
	applyStyle so that a test binary can apply the same style the application
	does, and the fix to the Roboto-Bold filename.

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

#include "appstyle.h"
#include "utility.h"

#include <QFont>
#include <QFontDatabase>
#include <QPalette>
#include <QPixmapCache>
#include <QProxyStyle>
#include <QStyle>

namespace {

// Disables focus drawing for all widgets
class Style_tweaks : public QProxyStyle
{
public:
    using QProxyStyle::QProxyStyle;
    void drawPrimitive(PrimitiveElement element, const QStyleOption *option,
                       QPainter *painter, const QWidget *widget) const
    {
        if (element == QStyle::PE_FrameFocusRect) return;

        QProxyStyle::drawPrimitive(element, option, painter, widget);
    }
};

}

void VtAppStyle::initIdentity()
{
    QCoreApplication::setOrganizationName("VESC");
    QCoreApplication::setOrganizationDomain("vesc-project.com");
    QCoreApplication::setApplicationName("VESC Tool");
}

void VtAppStyle::initColors(bool isDark)
{
    /*
     * setDarkMode is called here rather than by the caller so the flag and
     * the colour table can never disagree: Utility::getThemePath keys icon
     * lookups off the flag, so a mismatch would serve one theme's icons on
     * the other theme's palette.
     */
    Utility::setDarkMode(isDark);
    QPixmapCache::setCacheLimit(256000);

    if (isDark) {
        qputenv("QT_QUICK_CONTROLS_CONF", ":/qtquickcontrols2_dark.conf");

        Utility::setAppQColor("lightestBackground", QColor(80,80,80));
        Utility::setAppQColor("lightBackground", QColor(72,72,72));
        Utility::setAppQColor("normalBackground", QColor(48,48,48));
        Utility::setAppQColor("darkBackground", QColor(39,39,39));
        Utility::setAppQColor("plotBackground", QColor(39,39,39));
        Utility::setAppQColor("normalText", QColor(180,180,180));
        Utility::setAppQColor("lightText", QColor(215,215,215));
        Utility::setAppQColor("disabledText", QColor(127,127,127));
        Utility::setAppQColor("lightAccent", QColor(0,161,221));
        Utility::setAppQColor("tertiary1",QColor(229, 207, 51));
        Utility::setAppQColor("tertiary2",QColor(51, 180, 229));
        Utility::setAppQColor("tertiary3",QColor(136, 51, 229));
        Utility::setAppQColor("midAccent", QColor(0,107,153));
        Utility::setAppQColor("darkAccent", QColor(0,75,107));
        Utility::setAppQColor("pink", QColor(219,98,139));
        Utility::setAppQColor("red", QColor(200,52,52));
        Utility::setAppQColor("orange", QColor(206,125,44));
        Utility::setAppQColor("yellow", QColor(210,210,127));
        Utility::setAppQColor("green", QColor(127,200,127));
        Utility::setAppQColor("cyan",QColor(79,203,203));
        Utility::setAppQColor("blue", QColor(77,127,196));
        Utility::setAppQColor("magenta", QColor(157,127,210));
        Utility::setAppQColor("white", QColor(255,255,255));
        Utility::setAppQColor("black", QColor(0,0,0));
        Utility::setAppQColor("brightHighlightActive", QColor(224,89,37));
        Utility::setAppQColor("brightHighlightInactive", QColor(224,89,37));
        Utility::setAppQColor("vescGreenDark", QColor(37,86,56));
        Utility::setAppQColor("vescGreenMedium", QColor(35,104,61));
        Utility::setAppQColor("vescBlue", QColor(0,160,227));
        Utility::setAppQColor("vescBlueDark", QColor(0,106,150));
    } else {
        qputenv("QT_QUICK_CONTROLS_CONF", ":/qtquickcontrols2.conf");

        Utility::setAppQColor("lightestBackground", QColor(200,200,200));
        Utility::setAppQColor("lightBackground", QColor(225,225,225));
        Utility::setAppQColor("normalBackground", QColor(240,240,240));
        Utility::setAppQColor("darkBackground", QColor(255,255,255));
        Utility::setAppQColor("plotBackground", QColor(250,250,250));
        Utility::setAppQColor("normalText", QColor(60,20,60));
        Utility::setAppQColor("lightText", QColor(33,33,33));
        Utility::setAppQColor("disabledText", QColor(110,110,110));
        Utility::setAppQColor("lightAccent", QColor(0,114,178));
        Utility::setAppQColor("tertiary1",QColor(229, 207, 51));
        Utility::setAppQColor("tertiary2",QColor(51, 180, 229));
        Utility::setAppQColor("tertiary3",QColor(136, 51, 229));
        Utility::setAppQColor("midAccent", QColor(0,107,153));
        Utility::setAppQColor("darkAccent", QColor(0,155,222));
        Utility::setAppQColor("pink", QColor(219,98,139));
        Utility::setAppQColor("red", QColor(200,52,52));
        Utility::setAppQColor("orange", QColor(206,125,44));
        Utility::setAppQColor("yellow", QColor(210,210,127));
        Utility::setAppQColor("green", QColor(127,200,127));
        Utility::setAppQColor("cyan",QColor(79,203,203));
        Utility::setAppQColor("blue", QColor(77,127,196));
        Utility::setAppQColor("magenta", QColor(157,127,210));
        Utility::setAppQColor("white", QColor(255,255,255));
        Utility::setAppQColor("black", QColor(0,0,0));
        Utility::setAppQColor("brightHighlightActive", QColor(242,118,72));
        Utility::setAppQColor("brightHighlightInactive", QColor(242,118,72));
        Utility::setAppQColor("vescGreenDark", QColor(14,135,59));
        Utility::setAppQColor("vescGreenMedium", QColor(24,166,77));
        Utility::setAppQColor("vescBlue", QColor(0,160,227));
        Utility::setAppQColor("vescBlueDark", QColor(0,106,150));
    }
}

void VtAppStyle::registerFonts()
{
    QFontDatabase::addApplicationFont("://res/fonts/DejaVuSans.ttf");
    QFontDatabase::addApplicationFont("://res/fonts/DejaVuSans-Bold.ttf");
    QFontDatabase::addApplicationFont("://res/fonts/DejaVuSans-BoldOblique.ttf");
    QFontDatabase::addApplicationFont("://res/fonts/DejaVuSans-Oblique.ttf");
    QFontDatabase::addApplicationFont("://res/fonts/DejaVuSansMono.ttf");
    QFontDatabase::addApplicationFont("://res/fonts/DejaVuSansMono-Bold.ttf");
    QFontDatabase::addApplicationFont("://res/fonts/DejaVuSansMono-BoldOblique.ttf");
    QFontDatabase::addApplicationFont("://res/fonts/DejaVuSansMono-Oblique.ttf");

    QFontDatabase::addApplicationFont("://res/fonts/Roboto/Roboto-Regular.ttf");
    QFontDatabase::addApplicationFont("://res/fonts/Roboto/Roboto-Medium.ttf");
    QFontDatabase::addApplicationFont("://res/fonts/Roboto/Roboto-Bold.ttf");
    QFontDatabase::addApplicationFont("://res/fonts/Roboto/Roboto-BoldItalic.ttf");
    QFontDatabase::addApplicationFont("://res/fonts/Roboto/Roboto-Italic.ttf");
    QFontDatabase::addApplicationFont("://res/fonts/Roboto/RobotoMono-VariableFont_wght.ttf");

    QFontDatabase::addApplicationFont("://res/fonts/Exan-Regular.ttf");

    qApp->setFont(QFont("Roboto", 12));
}

void VtAppStyle::applyStyle(QApplication *a, bool isDark)
{
    qApp->setStyleSheet("QListView::item::selected {background: qlineargradient(x1: 1.0, y1: 0.0, x2: 0, y2: 0, stop: 0 " +
                        Utility::getAppHexColor("lightAccent") +
                        ", stop: 0.4 " + Utility::getAppHexColor("darkAccent") + ");" +
                        " border: none;} ");
    QStyle *myStyle = new Style_tweaks("Fusion");
    a->setStyle(myStyle);

    if (isDark) {
        QPalette darkPalette;
        //QPalette::Inactive
        darkPalette.setColor(QPalette::Window,Utility::getAppQColor("darkBackground"));
        darkPalette.setColor(QPalette::WindowText,Utility::getAppQColor("lightText"));
        darkPalette.setColor(QPalette::Disabled,QPalette::WindowText,Utility::getAppQColor("disabledText"));
        darkPalette.setColor(QPalette::Base,Utility::getAppQColor("normalBackground"));
        darkPalette.setColor(QPalette::AlternateBase,Utility::getAppQColor("lightBackground"));
        darkPalette.setColor(QPalette::ToolTipBase,Utility::getAppQColor("lightestBackground"));
        darkPalette.setColor(QPalette::ToolTipText,Utility::getAppQColor("lightText"));
        darkPalette.setColor(QPalette::Text,Utility::getAppQColor("lightText"));
        darkPalette.setColor(QPalette::Disabled,QPalette::Text,Utility::getAppQColor("disabledText"));
        darkPalette.setColor(QPalette::Dark,QColor(35,35,35));
        darkPalette.setColor(QPalette::Shadow,QColor(20,20,20));
        darkPalette.setColor(QPalette::Button,Utility::getAppQColor("normalBackground"));
        darkPalette.setColor(QPalette::ButtonText,Utility::getAppQColor("lightText"));
        darkPalette.setColor(QPalette::Disabled,QPalette::ButtonText,Utility::getAppQColor("disabledText"));
        darkPalette.setColor(QPalette::Disabled,QPalette::Highlight,Utility::getAppQColor("lightestBackground"));
        darkPalette.setColor(QPalette::HighlightedText,Utility::getAppQColor("white"));
        darkPalette.setColor(QPalette::Inactive,QPalette::Highlight,Utility::getAppQColor("midAccent"));
        darkPalette.setColor(QPalette::Active,QPalette::Highlight,Utility::getAppQColor("darkAccent"));
        darkPalette.setColor(QPalette::Disabled,QPalette::HighlightedText,Utility::getAppQColor("disabledText"));
        darkPalette.setColor(QPalette::Link, QColor(150,150,255));
        darkPalette.setColor(QPalette::LinkVisited, QColor(220,150,255));
        qApp->setPalette(darkPalette);
        qApp->setStyleSheet(
                    "QTabBar::tab:selected, QTabBar::tab:hover {"
                    "    background: #3d3d3d;"
                    "    color: #eeeeee;"
                    "}"
                    "QTabBar::tab:!selected {"
                    "    background: #272727;"
                    "    color: #a5a5a5;"
                    "}"
                    );
    } else {
        QPalette lightPalette = qApp->style()->standardPalette();
        lightPalette.setColor(QPalette::Inactive,QPalette::Highlight,Utility::getAppQColor("darkAccent"));
        lightPalette.setColor(QPalette::Active,QPalette::Highlight,Utility::getAppQColor("midAccent"));
        qApp->setPalette(lightPalette);
        qApp->setStyleSheet("");
    }
}
