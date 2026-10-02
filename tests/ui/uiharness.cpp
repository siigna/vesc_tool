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

#include "uiharness.h"

#include <QtTest>

#include <QAbstractButton>
#include <QComboBox>
#include <QCoreApplication>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QGroupBox>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLabel>
#include <QSettings>
#include <QStandardPaths>
#include <QTabWidget>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QRegularExpression>
#include <QTimer>

#include "appstyle.h"
#include "utility.h"
#include "vescinterface.h"

namespace {

QTemporaryDir *g_dir = nullptr;

QStringList g_messages;
QtMessageHandler g_prevHandler = nullptr;

void captureHandler(QtMsgType type, const QMessageLogContext &ctx,
                    const QString &msg)
{
    /*
     * Debug messages are collected too, because the one that matters most is
     * logged at that level: Utility::getAppQColor reports an unknown colour
     * name with qDebug and then returns red. Filtering to warnings let a
     * deliberately removed colour pass the test.
     */
    if (type == QtDebugMsg || type == QtWarningMsg ||
            type == QtCriticalMsg || type == QtFatalMsg) {
        g_messages.append(msg);
    }

    // Still pass it on, so a real problem is visible in the test output too.
    if (g_prevHandler != nullptr) {
        g_prevHandler(type, ctx, msg);
    }
}

}

void UiHarness::installMessageCapture()
{
    /*
     * Must be called from inside a test, not from main(): QTest::qExec
     * installs its own message handler, which replaced an earlier one and made
     * this silently collect nothing. Chaining to whatever is current keeps
     * QTest's own reporting intact.
     */
    if (g_prevHandler == nullptr) {
        g_prevHandler = qInstallMessageHandler(captureHandler);
    }
}

void UiHarness::clearMessages()
{
    g_messages.clear();
}

QStringList UiHarness::messages()
{
    return g_messages;
}

QString UiHarness::pinEnvironment()
{
    g_dir = new QTemporaryDir();
    const QString root = g_dir->path();

    for (const QString &sub: {"config", "data", "cache", "run", "home"}) {
        QDir().mkpath(root + "/" + sub);
    }

    qputenv("XDG_CONFIG_HOME", (root + "/config").toLocal8Bit());
    qputenv("XDG_DATA_HOME",   (root + "/data").toLocal8Bit());
    qputenv("XDG_CACHE_HOME",  (root + "/cache").toLocal8Bit());
    qputenv("XDG_RUNTIME_DIR", (root + "/run").toLocal8Bit());
    qputenv("HOME",            (root + "/home").toLocal8Bit());

    /*
     * The decimal separator is part of the rendered text of every spin box, so
     * a developer on a comma locale would otherwise disagree with everybody
     * else on dozens of widgets. It also pins
     * QLocale::system().measurementSystem(), which is where
     * VescInterface's useImperialUnits default comes from.
     */
    qputenv("LC_ALL", "C");
    qputenv("LANG", "C");
    qputenv("LC_NUMERIC", "C");
    qputenv("TZ", "UTC");

    // Pinned rather than inherited: a developer may export these.
    qputenv("QT_SCALE_FACTOR", "1");
    qputenv("QT_AUTO_SCREEN_SCALE_FACTOR", "0");
    qputenv("QT_ENABLE_HIGHDPI_SCALING", "0");

    if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM")) {
        qputenv("QT_QPA_PLATFORM", "offscreen");
    }

    return root;
}

void UiHarness::seedSettings()
{
    QSettings set;
    set.setValue("darkMode", true);
    set.setValue("useImperialUnits", false);
    set.setValue("app_scale_factor", 1.0);

    /*
     * Only needed once a test constructs MainWindow, and then it is essential:
     * its startup checks open the StartupWizard modally when intro_done is
     * falsy, and with no one to dismiss it the suite hung until the timeout.
     */
    set.setValue("intro_done", true);
    set.setValue("introVersion", VT_INTRO_VERSION);
    set.sync();
}

VescInterface *UiHarness::makeVesc()
{
    VescInterface *vesc = new VescInterface;

    // The same suppression the headless CLI paths apply, so that nothing here
    // reaches the network or swaps firmware.
    vesc->setBlockFwSwap(true);
    vesc->setIgnoreCustomConfigs(true);
    vesc->setShowFwUpdateAvailable(false);
    vesc->setIgnoreTestVersion(true);

    vesc->fwConfig()->loadParamsXml(Utility::configPath("fw.xml"));
    Utility::configLoadLatest(vesc);

    return vesc;
}

void UiHarness::settle()
{
    // ParamTable::addParamSubgroup re-enables updates through a zero timer, so
    // the table is not fully built until the loop has turned over once.
    for (int i = 0; i < 3; i++) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
    }
}

void UiHarness::settleWithTimers(int ms)
{
    /*
     * Real time, unlike settle(): processEvents returns at once when the queue
     * is empty, so it advances no wall clock and a 50 ms timer never fires.
     */
    QTest::qWait(ms);
}

namespace {

/*
 * True for a widget that lives inside one of the parameter editors. Their
 * internals are implementation detail of ParamEdit*, not of the page.
 */
bool isInsideParamEditor(const QWidget *w)
{
    for (const QObject *p = w->parent(); p != nullptr; p = p->parent()) {
        if (QString(p->metaObject()->className()).startsWith("ParamEdit")) {
            return true;
        }
    }
    return false;
}

QString identifyingText(const QWidget *w)
{
    if (auto b = qobject_cast<const QAbstractButton*>(w)) {
        return b->text();
    }
    if (auto g = qobject_cast<const QGroupBox*>(w)) {
        return g->title();
    }
    if (auto l = qobject_cast<const QLabel*>(w)) {
        return l->text();
    }
    return QString();
}

/*
 * Text that would otherwise encode the machine or the moment: the version and
 * git commit baked into several labels, and anything carrying a timestamp.
 * Dropped rather than recorded, so a commit does not invalidate every baseline.
 */
bool isVolatile(const QString &text)
{
    static const QRegularExpression stamp(
                "\\d{4}-\\d{2}-\\d{2}|\\d+\\.\\d+\\.\\d+|"
                "\\d{2}:\\d{2}:\\d{2}");
    return text.contains(stamp) ||
           text.contains(QString::number(VT_VERSION, 'f', 2));
}

}

QJsonObject UiHarness::describe(QWidget *page, const QString &pageName)
{
    QJsonObject out;
    out["page"] = pageName;

    QJsonArray widgets;

    /*
     * Sorted by object name, not by tree order. Reparenting a control during a
     * redesign is not a regression; losing it is. Sorting means the baseline
     * only moves when the set of controls or their state changes.
     */
    QList<QWidget*> all = page->findChildren<QWidget*>();
    std::sort(all.begin(), all.end(), [](QWidget *a, QWidget *b) {
        if (a->objectName() == b->objectName()) {
            return QString(a->metaObject()->className()) <
                   QString(b->metaObject()->className());
        }
        return a->objectName() < b->objectName();
    });

    for (QWidget *w: all) {
        if (w->objectName().isEmpty()) {
            // Qt's own internal children (scroll bar viewports and the like).
            continue;
        }

        if (isInsideParamEditor(w) ||
                QString(w->metaObject()->className()).startsWith("ParamEdit")) {
            /*
             * The editors inside a ParamTable are skipped, and so is anything
             * they contain. They have no useful identity -- ConfigParams
             * creates them without an objectName, so Qt names each one after
             * its class and a page ends up with a dozen indistinguishable
             * "ParamEditDouble" entries that drown out everything else.
             *
             * What matters about them is recorded against the table instead:
             * the row count and the editor type per row, which does move when
             * a parameter is renamed, retyped or dropped.
             */
            continue;
        }

        QJsonObject o;
        o["name"] = w->objectName();
        o["class"] = QString(w->metaObject()->className());
        o["enabled"] = w->isEnabled();

        const QString text = identifyingText(w);
        if (!text.isEmpty() && !isVolatile(text)) {
            o["text"] = text;
        }

        if (auto c = qobject_cast<QComboBox*>(w)) {
            /*
             * A few combos are filled from the host rather than from the
             * program: these two list the serial ports this machine happens to
             * have, which put "ttyACM0" into a committed baseline and would
             * fail on any other machine -- or on this one with the board
             * unplugged.
             *
             * Recorded as a note rather than dropped, so the control is still
             * known to exist and the reason it has no contents is visible in
             * the baseline instead of being a silent omission.
             */
            static const QStringList hostFilled{
                "serialPortBox", "victronPortBox", "serialDeviceBox"};

            if (hostFilled.contains(w->objectName())) {
                o["items"] = "(filled from the host; not recorded)";
            } else {
                QJsonArray items;
                for (int i = 0; i < c->count(); i++) {
                    items.append(c->itemText(i));
                }
                o["items"] = items;
            }
        }

        /*
         * For a ParamTable the row count is the signal: a renamed or missing
         * parameter makes addParamRow return false and the row simply does not
         * appear, with no error anywhere.
         */
        if (auto t = qobject_cast<QTableWidget*>(w)) {
            o["rows"] = t->rowCount();

            /*
             * The editor class per row, in order. This is the part that fails
             * when a parameter is renamed out of existence: the row goes away
             * silently, and only the shape of this list shows it.
             */
            QJsonArray editors;
            for (int r = 0; r < t->rowCount(); r++) {
                QWidget *cw = t->cellWidget(r, 1);
                editors.append(cw ? QString(cw->metaObject()->className())
                                  : QString("(none)"));
            }
            o["editors"] = editors;
        }

        if (auto t = qobject_cast<QTabWidget*>(w)) {
            QJsonArray tabs;
            for (int i = 0; i < t->count(); i++) {
                tabs.append(t->tabText(i));
            }
            o["tabs"] = tabs;
        }

        widgets.append(o);
    }

    out["widgets"] = widgets;
    out["widget_count"] = widgets.size();
    return out;
}

bool UiHarness::matchesBaseline(const QJsonObject &snap, const QString &name,
                               QString *detail)
{
    const QString baseDir = QStringLiteral(TESTS_UI_DIR) + "/baseline";
    const QString path = baseDir + "/" + name + ".json";

    const QByteArray actual =
            QJsonDocument(snap).toJson(QJsonDocument::Indented);

    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        // A missing baseline is a failure, not a pass. Otherwise a renamed page
        // would quietly stop being checked.
        *detail = QString("no baseline at %1 -- run ./update-baselines.sh")
                  .arg(path);
        QDir().mkpath(QStringLiteral(TESTS_UI_DIR) + "/actual");
        QFile a(QStringLiteral(TESTS_UI_DIR) + "/actual/" + name + ".json");
        if (a.open(QIODevice::WriteOnly)) {
            a.write(actual);
        }
        return false;
    }

    const QByteArray expect = f.readAll();
    if (expect == actual) {
        return true;
    }

    QDir().mkpath(QStringLiteral(TESTS_UI_DIR) + "/actual");
    QFile a(QStringLiteral(TESTS_UI_DIR) + "/actual/" + name + ".json");
    if (a.open(QIODevice::WriteOnly)) {
        a.write(actual);
    }

    // Name the first difference rather than dumping both files.
    const QStringList e = QString::fromUtf8(expect).split('\n');
    const QStringList g = QString::fromUtf8(actual).split('\n');
    for (int i = 0; i < qMax(e.size(), g.size()); i++) {
        const QString le = i < e.size() ? e.at(i) : QString("(end)");
        const QString lg = i < g.size() ? g.at(i) : QString("(end)");
        if (le != lg) {
            *detail = QString("line %1:\n  baseline: %2\n  actual:   %3")
                      .arg(i + 1).arg(le.trimmed(), lg.trimmed());
            break;
        }
    }

    return false;
}
