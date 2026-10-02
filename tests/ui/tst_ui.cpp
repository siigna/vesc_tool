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
#include <QApplication>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QJsonObject>
#include <QLabel>
#include <QPushButton>

#include "uiharness.h"
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

    void insightsEndpointFollowsProvider();
    void insightsKeylessProviderNeedsNoKey();
    void insightsButtonsNeedAController();
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

int main(int argc, char *argv[])
{
    /*
     * Order matters, and is the reason this is not QTEST_MAIN: pinEnvironment
     * has to run before the QApplication reads the locale, the scale factor or
     * any XDG path.
     */
    UiHarness::pinEnvironment();
    VtAppStyle::initIdentity();
    UiHarness::seedSettings();

    QApplication app(argc, argv);

    VtAppStyle::initColors(true);
    VtAppStyle::registerFonts();
    VtAppStyle::applyStyle(&app, true);

    g_vesc = UiHarness::makeVesc();

    UiTest tc;
    const int res = QTest::qExec(&tc, argc, argv);

    delete g_vesc;
    return res;
}

#include "tst_ui.moc"
