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

/*
 * Widget tests. No board, no display server, no network.
 *
 * Three things are checked, and they fail for different reasons on purpose:
 *
 *   structure   -- the set of controls on each page, against a committed JSON
 *                  baseline. Catches a control renamed, removed or left
 *                  disabled, which is what breaks backwards compatibility when
 *                  the UI is reworked.
 *   round-trip  -- a parameter editor and its ConfigParams still agree, which
 *                  is the guarantee that a redesign has not changed what gets
 *                  written to a controller.
 *   interaction -- pressing a control does what it should, driven through
 *                  widget pointers rather than screen coordinates.
 *
 * Why this file has its own main(): the environment must be pinned before the
 * QApplication reads the locale, the scale factor or any XDG path, and
 * QTEST_MAIN constructs the QApplication first.
 */

#include <QtTest>
#include <QVector>
#include <QApplication>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QJsonObject>
#include <QGroupBox>
#include <QPlainTextEdit>
#include <QTabWidget>
#include <QTextBrowser>
#include <QHash>
#include <QPixmapCache>
#include <QTemporaryDir>
#include <QTextStream>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QFile>
#include <QLabel>
#include <QPushButton>

#include "mainwindow.h"
#include "widgets/pagelistitem.h"
#include <QListWidget>
#include <QStackedWidget>
#include "uiharness.h"
#include "utility.h"
#include "tuninginsights.h"
#include "appstyle.h"
#include "configparams.h"
#include "vescinterface.h"
#include "widgets/paramtable.h"
#include "widgets/parameditdouble.h"

#include "pages/pageapppas.h"
#include "pages/pageappadc.h"
#include "pages/pageappgeneral.h"
#include "pages/pageappppm.h"
#include "pages/pageappuart.h"
#include "pages/pageappnrf.h"
#include "pages/pageappnunchuk.h"
#include "pages/pageappsettings.h"
#include "pages/pagebldc.h"
#include "pages/pagedc.h"
#include "pages/pagefoc.h"
#include "pages/pagegpd.h"
#include "pages/pagecontrollers.h"
#include "pages/pagemotor.h"
#include "pages/pagemotorinfo.h"
#include "pages/pagemotorsettings.h"
#include "pages/pagedataanalysis.h"
#include "pages/pageterminal.h"
#include "pages/pagedebugprint.h"
#include "pages/pagetuninginsights.h"
#include "pages/pagebms.h"
#include "pages/pagecananalyzer.h"
#include "pages/pagesetupcalculators.h"
#include "pages/pagertdata.h"
#include "pages/pagesampleddata.h"

enum PageId {
    Page_PageAppPas,
    Page_PageAppAdc,
    Page_PageAppGeneral,
    Page_PageAppPpm,
    Page_PageAppUart,
    Page_PageAppNrf,
    Page_PageAppNunchuk,
    Page_PageAppSettings,
    Page_PageBldc,
    Page_PageDc,
    Page_PageFoc,
    Page_PageGPD,
    Page_PageControllers,
    Page_PageMotor,
    Page_PageMotorInfo,
    Page_PageMotorSettings,
    Page_PageDataAnalysis,
    Page_PageTerminal,
    Page_PageDebugPrint,
    Page_PageTuningInsights,
    Page_PageBms,
    Page_PageCanAnalyzer,
    Page_PageSetupCalculators,
    Page_PageRtData,
    Page_PageSampledData
};

static VescInterface *g_vesc = nullptr;

static QWidget *makePage(int id, VescInterface *vesc)
{
    switch (PageId(id)) {
    case Page_PageAppPas: { auto p = new PageAppPas(); p->setVesc(vesc); return p; }
    case Page_PageAppAdc: { auto p = new PageAppAdc(); p->setVesc(vesc); return p; }
    case Page_PageAppGeneral: { auto p = new PageAppGeneral(); p->setVesc(vesc); return p; }
    case Page_PageAppPpm: { auto p = new PageAppPpm(); p->setVesc(vesc); return p; }
    case Page_PageAppUart: { auto p = new PageAppUart(); p->setVesc(vesc); return p; }
    case Page_PageAppNrf: { auto p = new PageAppNrf(); p->setVesc(vesc); return p; }
    case Page_PageAppNunchuk: { auto p = new PageAppNunchuk(); p->setVesc(vesc); return p; }
    case Page_PageAppSettings: { auto p = new PageAppSettings(); p->setVesc(vesc); return p; }
    case Page_PageBldc: { auto p = new PageBldc(); p->setVesc(vesc); return p; }
    case Page_PageDc: { auto p = new PageDc(); p->setVesc(vesc); return p; }
    case Page_PageFoc: { auto p = new PageFoc(); p->setVesc(vesc); return p; }
    case Page_PageGPD: { auto p = new PageGPD(); p->setVesc(vesc); return p; }
    case Page_PageControllers: { auto p = new PageControllers(); p->setVesc(vesc); return p; }
    case Page_PageMotor: { auto p = new PageMotor(); p->setVesc(vesc); return p; }
    case Page_PageMotorInfo: { auto p = new PageMotorInfo(); p->setVesc(vesc); return p; }
    case Page_PageMotorSettings: { auto p = new PageMotorSettings(); p->setVesc(vesc); return p; }
    case Page_PageDataAnalysis: { auto p = new PageDataAnalysis(); p->setVesc(vesc); return p; }
    case Page_PageTerminal: { auto p = new PageTerminal(); p->setVesc(vesc); return p; }
    case Page_PageDebugPrint: { auto p = new PageDebugPrint(); return p; }
    case Page_PageTuningInsights: { auto p = new PageTuningInsights(); p->setVesc(vesc); return p; }
    case Page_PageBms: { auto p = new PageBms(); p->setVesc(vesc); return p; }
    case Page_PageCanAnalyzer: { auto p = new PageCanAnalyzer(); p->setVesc(vesc); return p; }
    case Page_PageSetupCalculators: { auto p = new PageSetupCalculators(); p->setVesc(vesc); return p; }
    case Page_PageRtData: { auto p = new PageRtData(); p->setVesc(vesc); return p; }
    case Page_PageSampledData: { auto p = new PageSampledData(); p->setVesc(vesc); return p; }
    }
    return nullptr;
}

class UiTest: public QObject
{
    Q_OBJECT

private slots:
    void structureMatchesBaseline_data();
    void structureMatchesBaseline();

    void paramPagesAreNotEmpty();
    void paramEditorKeepsItsValue();
    void editorWritesThroughToConfig();
    void configChangeReachesEditor();

    void noMissingIconsOrColours();
    void brandingIsOurs();

    void insightsPreviewShowsTheAnswerTab();
    void insightsInstructionsAreEditable();
    void insightsSavesWhatWasSent();
    void insightsSavedConfigLoadsBackIn();

    void insightsEndpointFollowsProvider();
    void insightsKeylessProviderNeedsNoKey();
    void insightsButtonsNeedAController();

    /*
     * Last on purpose. These build the real MainWindow, whose timer runs the
     * startup checks -- which end in Utility::checkVersion, a live network
     * request. Nothing turns the event loop after them, so that timer never
     * fires and the suite stays offline.
     */
    void mainWindowNavAndStackStayInStep();
    void mainWindowOpensPagesByName();
    void mainWindowHidesWhatNeedsAConnection();
};

void UiTest::structureMatchesBaseline_data()
{
    QTest::addColumn<QString>("name");
    QTest::addColumn<int>("id");

    QTest::newRow("PageAppPas") << QString("PageAppPas") << int(Page_PageAppPas);
    QTest::newRow("PageAppAdc") << QString("PageAppAdc") << int(Page_PageAppAdc);
    QTest::newRow("PageAppGeneral") << QString("PageAppGeneral") << int(Page_PageAppGeneral);
    QTest::newRow("PageAppPpm") << QString("PageAppPpm") << int(Page_PageAppPpm);
    QTest::newRow("PageAppUart") << QString("PageAppUart") << int(Page_PageAppUart);
    QTest::newRow("PageAppNrf") << QString("PageAppNrf") << int(Page_PageAppNrf);
    QTest::newRow("PageAppNunchuk") << QString("PageAppNunchuk") << int(Page_PageAppNunchuk);
    QTest::newRow("PageAppSettings") << QString("PageAppSettings") << int(Page_PageAppSettings);
    QTest::newRow("PageBldc") << QString("PageBldc") << int(Page_PageBldc);
    QTest::newRow("PageDc") << QString("PageDc") << int(Page_PageDc);
    QTest::newRow("PageFoc") << QString("PageFoc") << int(Page_PageFoc);
    QTest::newRow("PageGPD") << QString("PageGPD") << int(Page_PageGPD);
    QTest::newRow("PageControllers") << QString("PageControllers") << int(Page_PageControllers);
    QTest::newRow("PageMotor") << QString("PageMotor") << int(Page_PageMotor);
    QTest::newRow("PageMotorInfo") << QString("PageMotorInfo") << int(Page_PageMotorInfo);
    QTest::newRow("PageMotorSettings") << QString("PageMotorSettings") << int(Page_PageMotorSettings);
    QTest::newRow("PageDataAnalysis") << QString("PageDataAnalysis") << int(Page_PageDataAnalysis);
    QTest::newRow("PageTerminal") << QString("PageTerminal") << int(Page_PageTerminal);
    QTest::newRow("PageDebugPrint") << QString("PageDebugPrint") << int(Page_PageDebugPrint);
    QTest::newRow("PageTuningInsights") << QString("PageTuningInsights") << int(Page_PageTuningInsights);
    QTest::newRow("PageBms") << QString("PageBms") << int(Page_PageBms);
    QTest::newRow("PageCanAnalyzer") << QString("PageCanAnalyzer") << int(Page_PageCanAnalyzer);
    QTest::newRow("PageSetupCalculators") << QString("PageSetupCalculators") << int(Page_PageSetupCalculators);
    QTest::newRow("PageRtData") << QString("PageRtData") << int(Page_PageRtData);
    QTest::newRow("PageSampledData") << QString("PageSampledData") << int(Page_PageSampledData);
}

void UiTest::structureMatchesBaseline()
{
    QFETCH(QString, name);
    QFETCH(int, id);

    QScopedPointer<QWidget> page(makePage(id, g_vesc));
    QVERIFY2(!page.isNull(), qPrintable(name));

    UiHarness::settle();

    const QJsonObject snap = UiHarness::describe(page.data(), name);

    /*
     * A page with no named widgets would match an empty baseline and read as a
     * pass, so the snapshot has to be non-trivial before it is compared.
     */
    QVERIFY2(snap["widget_count"].toInt() > 0,
             qPrintable(name + " produced no named widgets"));

    QString detail;
    const bool ok = UiHarness::matchesBaseline(snap, name, &detail);
    QVERIFY2(ok, qPrintable(name + ": " + detail));
}

void UiTest::paramPagesAreNotEmpty()
{
    /*
     * A ParamTable's row count is the only evidence that the parameter XML
     * loaded and that the page's subgroup names still match it. A renamed
     * parameter makes ParamTable::addParamRow return false and the row simply
     * does not appear, with nothing reported anywhere -- so "the page
     * rendered" is not enough.
     */
    struct { int id; const char *name; } pages[] = {
        { Page_PageFoc, "PageFoc" },
        { Page_PageAppPas, "PageAppPas" },
        { Page_PageAppGeneral, "PageAppGeneral" },
        { Page_PageBldc, "PageBldc" },
    };

    for (auto &p: pages) {
        QScopedPointer<QWidget> page(makePage(p.id, g_vesc));
        UiHarness::settle();

        int total = 0;
        for (ParamTable *t: page->findChildren<ParamTable*>()) {
            total += t->rowCount();
        }

        QVERIFY2(total > 0,
                 qPrintable(QString("%1 has no parameter rows: either the "
                                    "config did not load or a subgroup was "
                                    "renamed").arg(p.name)));
    }
}

void UiTest::paramEditorKeepsItsValue()
{
    /*
     * The editors bind to ConfigParams by name and write back through
     * updateParamDouble, so a value typed into the editor has to survive the
     * trip out to the config and back.
     */
    QScopedPointer<QWidget> page(makePage(Page_PageFoc, g_vesc));
    UiHarness::settle();

    ParamEditDouble *ed = page->findChild<ParamEditDouble*>();
    QVERIFY2(ed, "PageFoc has no double editor; the page or the config changed");

    QDoubleSpinBox *box = ed->findChild<QDoubleSpinBox*>();
    QVERIFY(box);

    const double before = box->value();
    QVERIFY2(box->maximum() > before, "no headroom to move the value");

    const double target = qMin(before + box->singleStep(), box->maximum());
    box->setValue(target);
    UiHarness::settle();

    QVERIFY2(qAbs(box->value() - target) < 1e-9,
             "the editor did not keep the value it was given");
    QVERIFY2(qAbs(box->value() - before) > 1e-12,
             "the value did not actually change, so this proves nothing");
}

void UiTest::insightsEndpointFollowsProvider()
{
    QScopedPointer<QWidget> page(makePage(Page_PageTuningInsights, g_vesc));
    UiHarness::settle();

    QComboBox *provider = page->findChild<QComboBox*>("providerBox");
    QLabel *endpoint = page->findChild<QLabel*>("endpointLabel");
    QVERIFY(provider);
    QVERIFY(endpoint);

    // The default has to be the one that keeps the data on this machine.
    QCOMPARE(provider->currentData().toString(), QString("ollama"));
    QVERIFY(endpoint->text().contains("localhost"));
    QVERIFY(endpoint->text().contains("nothing leaves it"));

    const int ix = provider->findData(QString("openai"));
    QVERIFY(ix >= 0);
    provider->setCurrentIndex(ix);
    UiHarness::settle();

    QVERIFY2(endpoint->text().contains("api.openai.com"),
             qPrintable(endpoint->text()));
    QVERIFY2(!endpoint->text().contains("nothing leaves it"),
             "a remote endpoint must not claim the data stays local");
}

void UiTest::insightsKeylessProviderNeedsNoKey()
{
    QScopedPointer<QWidget> page(makePage(Page_PageTuningInsights, g_vesc));
    UiHarness::settle();

    QComboBox *provider = page->findChild<QComboBox*>("providerBox");
    QWidget *keyGroup = page->findChild<QWidget*>("keyGroup");
    QVERIFY(provider);
    QVERIFY(keyGroup);

    // ollama is selected by default and needs no key at all.
    QVERIFY2(!keyGroup->isEnabled(),
             "a provider that needs no key must not invite one");

    const int ix = provider->findData(QString("anthropic"));
    QVERIFY(ix >= 0);
    provider->setCurrentIndex(ix);
    UiHarness::settle();

    QVERIFY2(keyGroup->isEnabled(), "a keyed provider must allow a key");
}

void UiTest::insightsButtonsNeedAController()
{
    QScopedPointer<QWidget> page(makePage(Page_PageTuningInsights, g_vesc));
    UiHarness::settle();

    QPushButton *analyse = page->findChild<QPushButton*>("analyseButton");
    QPushButton *preview = page->findChild<QPushButton*>("previewButton");
    QVERIFY(analyse);
    QVERIFY(preview);

    /*
     * Nothing is connected here, so neither button should be pressable: the
     * configuration and the telemetry both come off the board.
     */
    QVERIFY2(!g_vesc->isPortConnected(), "the test must not be connected");
    QVERIFY2(!analyse->isEnabled(), "Analyse was enabled with no controller");
    QVERIFY2(!preview->isEnabled(), "Preview was enabled with no controller");
}


/*
 * Every double-valued parameter and its value. Used to find which parameter an
 * editor actually wrote to, without needing the private name it is bound to.
 */
static QHash<QString, double> doubleSnapshot(ConfigParams *conf)
{
    QHash<QString, double> out;

    for (const QString &n: conf->getParamOrder()) {
        ConfigParam *p = conf->getParam(n);

        if (p != nullptr && p->type == CFG_T_DOUBLE) {
            out.insert(n, conf->getParamDouble(n));
        }
    }

    return out;
}

void UiTest::editorWritesThroughToConfig()
{
    /*
     * The test that matters here, and the one that was missing: driving an
     * editor has to change the backing ConfigParams, because that is what gets
     * written to a controller.
     *
     * The previous version of this only read the spin box's own value back,
     * which proved QDoubleSpinBox works and nothing else -- deleting both
     * updateParamDouble calls in parameditdouble.cpp left the whole suite
     * green.
     */
    QScopedPointer<QWidget> page(makePage(Page_PageAppPas, g_vesc));
    UiHarness::settle();

    ConfigParams *conf = g_vesc->appConfig();
    const QHash<QString, double> before = doubleSnapshot(conf);
    QVERIFY2(!before.isEmpty(), "no double parameters to drive");

    ParamEditDouble *ed = page->findChild<ParamEditDouble*>();
    QVERIFY(ed);
    QDoubleSpinBox *box = ed->findChild<QDoubleSpinBox*>();
    QVERIFY(box);

    const double target = qMin(box->value() + box->singleStep(),
                               box->maximum());
    QVERIFY2(qAbs(target - box->value()) > 1e-12, "no headroom to move");

    box->setValue(target);
    UiHarness::settle();

    const QHash<QString, double> after = doubleSnapshot(conf);
    QStringList changed;

    for (auto it = after.constBegin(); it != after.constEnd(); ++it) {
        if (qAbs(it.value() - before.value(it.key())) > 1e-9) {
            changed << it.key();
        }
    }

    QCOMPARE(changed.size(), 1);
    QCOMPARE(changed.first(), ed->name());
    QVERIFY2(qAbs(conf->getParamDouble(changed.first()) - target) < 1e-6,
             qPrintable(QString("%1 is %2, the editor was set to %3")
                        .arg(changed.first())
                        .arg(conf->getParamDouble(changed.first()))
                        .arg(target)));
}

void UiTest::configChangeReachesEditor()
{
    // The other direction: a value set on the config moves the widget.
    QScopedPointer<QWidget> page(makePage(Page_PageAppPas, g_vesc));
    UiHarness::settle();

    ConfigParams *conf = g_vesc->appConfig();

    /*
     * Asked of the editor rather than guessed. The first version of this
     * matched a parameter to an editor by equal value and picked
     * app_ppm_conf.ramp_time_neg, which is not on this page at all --
     * ParamEditDouble::name() is the parameter it is actually bound to.
     */
    ParamEditDouble *ed = nullptr;
    QDoubleSpinBox *box = nullptr;

    for (ParamEditDouble *cand: page->findChildren<ParamEditDouble*>()) {
        QDoubleSpinBox *b = cand->findChild<QDoubleSpinBox*>();

        if (b != nullptr && !cand->name().isEmpty() &&
                b->maximum() > conf->getParamDouble(cand->name()) + 1e-6) {
            ed = cand;
            box = b;
            break;
        }
    }

    QVERIFY2(ed != nullptr, "no editor on the page has headroom to move");

    const QString name = ed->name();
    const double before = box->value();
    const double target = qMin(before + box->singleStep(), box->maximum());

    conf->updateParamDouble(name, target, nullptr);
    UiHarness::settle();

    QVERIFY2(qAbs(box->value() - target) < 1e-6,
             qPrintable(QString("setting %1 to %2 on the config left the "
                                "editor at %3")
                        .arg(name).arg(target).arg(box->value())));
    QVERIFY2(qAbs(box->value() - before) > 1e-12, "the value did not move");
}

void UiTest::noMissingIconsOrColours()
{
    /*
     * Two failures this program reports only as a warning, and which are
     * otherwise invisible: Utility::getIcon draws nothing when a file is
     * missing (a light-theme variant nobody added looks fine in dark mode),
     * and Utility::getAppQColor returns red for a name it does not know.
     *
     * Constructing every page and reading the warnings turns both into
     * failures. Note that icons are cached process-wide, so a miss is only
     * reported on the first page that asks for it -- which is why this
     * constructs all of them rather than testing one.
     */
    UiHarness::installMessageCapture();
    UiHarness::clearMessages();

    /*
     * Icons are cached process-wide, so a miss is only reported the first time
     * one is asked for -- and the structure test has already constructed every
     * page by now. Clearing the cache makes this test see them again rather
     * than depending on which test ran first.
     */
    QPixmapCache::clear();

    for (int id = 0; id <= int(Page_PageSampledData); id++) {
        QScopedPointer<QWidget> page(makePage(id, g_vesc));
        UiHarness::settle();
    }

    QStringList bad;
    for (const QString &m: UiHarness::messages()) {
        if (m.contains("icon not found") ||
                m.contains("not found in standard colors")) {
            bad << m;
        }
    }

    QVERIFY2(bad.isEmpty(), qPrintable("\n  " + bad.join("\n  ")));
}

void UiTest::brandingIsOurs()
{
    /*
     * The fork must not present the upstream project's marks as its own. This
     * is mechanical to check and was not: the welcome heading still said
     * "VESC® Tool" after the rebrand, and it took a screenshot to notice.
     */
    for (const QString &path: {":/res/logo.png", ":/res/icon.svg",
                               ":/res/+theme_light/logo.png",
                               ":/res/+theme_light/icon.svg"}) {
        QPixmap pm(path);
        QVERIFY2(!pm.isNull(), qPrintable(path + " does not load"));
    }

    const QString about = Utility::aboutText();

    // Our name, our copyright.
    QVERIFY(about.contains("ESCargot Tool"));
    QVERIFY(about.contains("Stephen Bouche"));

    // The upstream mark acknowledged, not claimed.
    QVERIFY2(!about.contains("<b>VESC"),
             "the about box must not lead with the upstream product name");
    QVERIFY(about.contains("not affiliated with or endorsed"));

    // CC BY-SA requires the credit to travel with the binary, not just the repo.
    QVERIFY2(about.contains("Geierunited"), "placeholder logo attribution is missing");
    QVERIFY2(about.contains("CC BY-SA 3.0"), "placeholder logo licence is missing");

    // And no page may call this program by the upstream name.
    for (int id = 0; id <= int(Page_PageSampledData); id++) {
        QScopedPointer<QWidget> page(makePage(id, g_vesc));
        UiHarness::settle();

        for (QLabel *l: page->findChildren<QLabel*>()) {
            QVERIFY2(!l->text().contains("VESC® Tool") &&
                     !l->text().contains("VESC&reg; Tool"),
                     qPrintable(QString("a label still says VESC(R) Tool: %1")
                                .arg(l->text().left(80))));
        }
    }
}

/*
 * MainWindow builds all ~40 pages in one 200-line function and keeps the
 * navigation list and the stacked widget in step by convention only -- nothing
 * in the code enforces it. These construct the real window.
 *
 * Deliberately without turning the event loop: MainWindow's startup checks run
 * from its timer and end in Utility::checkVersion, which makes a live network
 * request. Not processing events keeps the suite offline, and the pages are all
 * built in the constructor anyway.
 */
/*
 * A log in the package logger's shape, written to a temporary file so the test
 * does not depend on anybody's ride data. The gnss columns are present on
 * purpose: what gets saved has to be the filtered log, not the source.
 */
static QString writeSampleLog(const QString &dir)
{
    const QString path = dir + "/sample.csv";
    QFile f(path);

    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return QString();
    }

    QTextStream ts(&f);
    ts << "Input Voltage:Input Voltage:V:2:0:0;RPM:RPM::2:0:0;"
          "kmh_vesc:Speed ESC:km/h:2:0:0;"
          "gnss_lat:gnss_lat::2:0:0;gnss_lon:gnss_lon::2:0:0\n";

    for (int i = 0; i < 20; i++) {
        ts << 80.0 - i * 0.1 << ";" << i * 100 << ";" << i * 1.5
           << ";57.70887;11.97456\n";
    }

    return path;
}

void UiTest::insightsSavesWhatWasSent()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    const QString log = writeSampleLog(tmp.path());
    QVERIFY(!log.isEmpty());

    QScopedPointer<QWidget> p(makePage(Page_PageTuningInsights, g_vesc));
    PageTuningInsights *page = qobject_cast<PageTuningInsights*>(p.data());
    QVERIFY(page);
    UiHarness::settle();

    page->applyCliDefaults(QString(), QString(), QString(), log, false);
    UiHarness::settle();

    // Build the payload, which is what the saves are taken from.
    QPushButton *preview = page->findChild<QPushButton*>("previewButton");
    QVERIFY(preview);
    preview->setEnabled(true);
    QTest::mouseClick(preview, Qt::LeftButton);
    UiHarness::settle();

    const QString out = tmp.path() + "/sent-log.csv";
    QString err;
    QVERIFY2(page->saveSentLogTo(out, &err), qPrintable(err));

    QFile f(out);
    QVERIFY(f.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString csv = QString::fromUtf8(f.readAll());

    /*
     * The point of saving "the log that was sent" rather than copying the
     * source file: the location columns were filtered out before sending, so
     * they must be absent here too.
     */
    QVERIFY2(!csv.contains("gnss"), "the saved log still carries gnss columns");
    QVERIFY2(!csv.contains("57.70887"), "the saved log still carries coordinates");

    // and the permitted columns did survive, so this is not passing by emptiness
    QVERIFY2(csv.contains("Input Voltage"), qPrintable(csv.left(200)));
    QVERIFY2(csv.contains("kmh_vesc"), qPrintable(csv.left(200)));

    // The package logger's shape: ';' separated, "name:label:unit:..." header.
    const QStringList lines = csv.split('\n', QString::SkipEmptyParts);
    QVERIFY(lines.size() > 1);
    QVERIFY2(lines.first().contains(';'), "header is not ';' separated");
    QVERIFY2(lines.first().contains("Speed ESC"),
             "header lost the label the logger puts in it");
    QCOMPARE(lines.at(1).count(';'), lines.first().count(';'));
}

void UiTest::insightsSavedConfigLoadsBackIn()
{
    /*
     * "Standard re-uploadable config" is a claim, so it is checked: the saved
     * XML has to load back through the same ConfigParams that writes it, and
     * carry a value that survives the trip.
     */
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    QScopedPointer<QWidget> p(makePage(Page_PageTuningInsights, g_vesc));
    PageTuningInsights *page = qobject_cast<PageTuningInsights*>(p.data());
    QVERIFY(page);
    UiHarness::settle();

    QPushButton *preview = page->findChild<QPushButton*>("previewButton");
    QVERIFY(preview);
    preview->setEnabled(true);
    QTest::mouseClick(preview, Qt::LeftButton);
    UiHarness::settle();

    QString err;
    QVERIFY2(page->saveBundleTo(tmp.path(), &err), qPrintable(err));

    for (const QString &name: {"payload.json", "mcconf.xml", "appconf.xml"}) {
        QVERIFY2(QFile::exists(tmp.path() + "/" + name),
                 qPrintable(name + QString(" was not written")));
    }

    // The payload has to be JSON, since that is what was sent.
    QFile pf(tmp.path() + "/payload.json");
    QVERIFY(pf.open(QIODevice::ReadOnly));
    QJsonParseError pe;
    const QJsonDocument doc = QJsonDocument::fromJson(pf.readAll(), &pe);
    QCOMPARE(pe.error, QJsonParseError::NoError);
    QVERIFY(doc.object().contains("mcconf"));

    // And the configuration has to come back in.
    const double before = g_vesc->mcConfig()->getParamDouble("l_current_max");

    /*
     * A second interface, so the definitions are loaded exactly the way the
     * application loads them. An earlier version of this passed
     * configPath("parameters_mcconf.xml") -- a path with no version directory
     * in it -- so nothing was defined, loadXml set nothing, and the value came
     * back as 0.
     */
    QScopedPointer<VescInterface> other(UiHarness::makeVesc());
    QVERIFY2(other->mcConfig()->loadXml(tmp.path() + "/mcconf.xml",
                                        "MCConfiguration"),
             "the saved motor configuration did not load back in");
    QVERIFY2(qAbs(other->mcConfig()->getParamDouble("l_current_max") - before) < 1e-6,
             qPrintable(QString("l_current_max came back as %1, was %2")
                        .arg(other->mcConfig()->getParamDouble("l_current_max"))
                        .arg(before)));
}

/*
 * One window, shared by the tests below and deliberately never destroyed.
 *
 * Constructing and destroying three of them in one process segfaulted after
 * the third: MainWindow's teardown drops widgets through deleteLater, and with
 * no event loop turning (which is what keeps the startup checks, and their
 * live network request, from running) those deletions queue up and are then
 * run against objects whose owners have gone. Leaking one window avoids the
 * teardown entirely and is honest about what is being tested, which is the
 * window as built, not its destructor.
 */
static MainWindow *sharedWindow()
{
    static MainWindow *w = nullptr;

    if (w == nullptr) {
        w = new MainWindow;
    }

    return w;
}

static QListWidget *navOf(MainWindow *w)
{
    return w->findChild<QListWidget*>("pageList");
}

void UiTest::mainWindowNavAndStackStayInStep()
{
    MainWindow *w = sharedWindow();

    QListWidget *nav = navOf(w);
    QStackedWidget *stack = w->findChild<QStackedWidget*>("pageWidget");
    QVERIFY(nav);
    QVERIFY(stack);

    QVERIFY2(nav->count() > 30,
             qPrintable(QString("only %1 navigation rows").arg(nav->count())));

    /*
     * on_pageList_currentRowChanged does setCurrentIndex(currentRow), so a row
     * and its page must share an index. If they ever drift, every page below
     * the drift opens the wrong screen.
     */
    QCOMPARE(nav->count(), stack->count());

    // Every row must carry a PageListItem, since that is what names it.
    for (int i = 0; i < nav->count(); i++) {
        PageListItem *item =
                qobject_cast<PageListItem*>(nav->itemWidget(nav->item(i)));
        QVERIFY2(item != nullptr,
                 qPrintable(QString("row %1 has no PageListItem").arg(i)));
        QVERIFY2(!item->name().isEmpty(),
                 qPrintable(QString("row %1 has no name").arg(i)));
    }
}

void UiTest::mainWindowOpensPagesByName()
{
    MainWindow *w = sharedWindow();

    QListWidget *nav = navOf(w);
    QStackedWidget *stack = w->findChild<QStackedWidget*>("pageWidget");
    QVERIFY(nav);
    QVERIFY(stack);

    /*
     * openPage is what --showPage uses, and it is the only reproducible way to
     * reach a page: driving the navigation list by mouse coordinates is not,
     * because the scroll does not always land on the same row.
     */
    w->openPage("Tuning Insights");
    QVERIFY2(qobject_cast<PageTuningInsights*>(stack->currentWidget()) != nullptr,
             qPrintable(QString("Tuning Insights opened %1")
                        .arg(stack->currentWidget() == nullptr ? "nothing"
                             : stack->currentWidget()->metaObject()->className())));

    w->openPage("FOC");
    QVERIFY2(qobject_cast<PageFoc*>(stack->currentWidget()) != nullptr,
             "FOC did not open PageFoc");

    // A name that does not exist must not move the selection.
    const int before = nav->currentRow();
    w->openPage("No Such Page");
    QCOMPARE(nav->currentRow(), before);
}

void UiTest::mainWindowHidesWhatNeedsAConnection()
{
    MainWindow *w = sharedWindow();
    QListWidget *nav = navOf(w);
    QVERIFY(nav);

    /*
     * The custom configuration pages are registered hidden and only revealed
     * for hardware that reports having them. Unconnected, they must not be in
     * the list -- their page still exists in the stack, which is why the
     * count check above compares against the stack and not against what is
     * visible.
     */
    int hiddenConfigRows = 0;

    for (int i = 0; i < nav->count(); i++) {
        PageListItem *item =
                qobject_cast<PageListItem*>(nav->itemWidget(nav->item(i)));

        if (item != nullptr && item->name().startsWith("Config")) {
            QVERIFY2(nav->item(i)->isHidden(),
                     qPrintable(QString("%1 is visible with no board")
                                .arg(item->name())));
            hiddenConfigRows++;
        }
    }

    QVERIFY2(hiddenConfigRows > 0,
             "found no custom-config rows at all; the registration changed");
}

void UiTest::insightsPreviewShowsTheAnswerTab()
{
    QScopedPointer<QWidget> page(makePage(Page_PageTuningInsights, g_vesc));
    UiHarness::settle();

    QTabWidget *tabs = page->findChild<QTabWidget*>("tabs");
    QWidget *answerTab = page->findChild<QWidget*>("answerTab");
    QPushButton *preview = page->findChild<QPushButton*>("previewButton");
    QVERIFY(tabs);
    QVERIFY(answerTab);
    QVERIFY(preview);

    QCOMPARE(tabs->currentIndex(), 0);

    /*
     * Not connected, so this reports why rather than producing a payload --
     * and the result still belongs on the answer tab. Leaving the user on
     * Setup after pressing a button looks like nothing happened.
     */
    preview->setEnabled(true);
    QTest::mouseClick(preview, Qt::LeftButton);
    UiHarness::settle();

    QCOMPARE(tabs->currentWidget(), answerTab);

    QTextBrowser *view = page->findChild<QTextBrowser*>("answerView");
    QVERIFY(view);
    QVERIFY2(!view->toPlainText().trimmed().isEmpty(),
             "the answer tab was shown with nothing in it");
}

void UiTest::insightsInstructionsAreEditable()
{
    /*
     * The prompt decides what advice you get about your own hardware, so it
     * has to be visible and changeable rather than a constant in the source.
     */
    QScopedPointer<QWidget> p(makePage(Page_PageTuningInsights, g_vesc));
    PageTuningInsights *page = qobject_cast<PageTuningInsights*>(p.data());
    QVERIFY(page);
    UiHarness::settle();

    QPlainTextEdit *edit = page->findChild<QPlainTextEdit*>("promptEdit");
    QGroupBox *group = page->findChild<QGroupBox*>("promptGroup");
    QVERIFY(edit);
    QVERIFY(group);

    // Shown by default, holding the real text, and not in force until ticked.
    QCOMPARE(edit->toPlainText(), TuningInsights::defaultInstructions());
    QVERIFY(!group->isChecked());

    page->setInstructions("Only comment on PAS.");
    UiHarness::settle();

    QVERIFY2(group->isChecked(), "a supplied prompt must be in force");
    QCOMPARE(edit->toPlainText(), QString("Only comment on PAS."));

    page->setInstructions(QString());
    UiHarness::settle();

    QVERIFY2(!group->isChecked(), "an empty prompt must restore the default");
    QCOMPARE(edit->toPlainText(), TuningInsights::defaultInstructions());
}

int main(int argc, char *argv[])
{
    /*
     * --light runs with the light palette instead of the dark one.
     *
     * Not the structure snapshots: those record names, classes and text, none
     * of which the theme changes, so a second set of baselines would be a copy
     * of the first. What the theme does change is which files get loaded --
     * Utility::getThemePath sends every icon lookup to res/+theme_light -- and
     * which colour names exist, since the two palettes are independent literal
     * lists in appstyle.cpp and can drift apart. So the light run executes the
     * checks that notice a missing icon or an unknown colour.
     *
     * The flag is stripped before qExec, which rejects options it does not
     * know.
     */
    bool light = false;
    QVector<char*> args;

    for (int i = 0; i < argc; i++) {
        if (qstrcmp(argv[i], "--light") == 0) {
            light = true;
        } else {
            args.append(argv[i]);
        }
    }

    int realArgc = args.size();
    char **realArgv = args.data();

    /*
     * Order matters, and is the reason this is not QTEST_MAIN: pinEnvironment
     * has to run before the QApplication reads the locale, the scale factor or
     * any XDG path.
     */
    UiHarness::pinEnvironment();
    VtAppStyle::initIdentity();
    UiHarness::seedSettings();

    QApplication app(realArgc, realArgv);

    VtAppStyle::initColors(!light);
    VtAppStyle::registerFonts();
    VtAppStyle::applyStyle(&app, !light);

    g_vesc = UiHarness::makeVesc();

    UiTest tc;
    int res = 0;

    if (light) {
        // Only the checks whose outcome the theme can change.
        QStringList only;
        only << QString::fromLocal8Bit(realArgv[0])
             << "noMissingIconsOrColours" << "brandingIsOurs";
        res = QTest::qExec(&tc, only);
    } else {
        res = QTest::qExec(&tc, realArgc, realArgv);
    }

    delete g_vesc;
    return res;
}

#include "tst_ui.moc"
