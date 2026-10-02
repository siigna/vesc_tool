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
 * Tests for Tuning Insights. Offline: no network, no board, no display.
 *
 * The first test here is the one that matters. Everything else is ordinary
 * correctness; redaction is a promise about what leaves somebody's machine,
 * and it has to be checked rather than reviewed.
 */

#include <QtTest>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

#include "tuninginsights.h"
#include "insightsprovider.h"
#include "tuningclient.h"

using ConfigValue = TuningInsights::ConfigValue;

static ConfigValue cv(const QString &name, CFG_T type,
                      double d = 0.0, int i = 0)
{
    ConfigValue v;
    v.name = name;
    v.type = type;
    v.valDouble = d;
    v.valInt = i;
    return v;
}

class TuningTest: public QObject
{
    Q_OBJECT

private slots:
    void locationNeverLeaves();
    void allowlistNotBlacklist();
    void downsampleKeepsEnds();
    void downsampleHonoursCap();
    void configIsEnumeratedAndTyped();
    void configSkipsFreeText();
    void anthropicRequestShape();
    void openAiRequestShape();
    void bothDialectsParse();
    void badRepliesAreErrors();
    void localProviderNeedsNoKey();
    void keyNeverAppearsInPayload();
    void customSpecParsesUrlWithPort();
    void badSpecExplainsItself();
};

/* A header shaped like the real one, plus a column nobody has invented yet. */
static QStringList testHeader()
{
    return QStringList()
            << "ms_today" << "input_voltage" << "erpm" << "duty_cycle"
            << "gnss_lat" << "gnss_lon" << "gnss_alt"
            << "pas_rider_watts" << "home_address";
}

static QList<QStringList> testRows(int n)
{
    QList<QStringList> rows;

    for (int i = 0; i < n; i++) {
        rows.append(QStringList()
                    << QString::number(i)
                    << "48.1" << "3000" << "0.42"
                    << "57.70887" << "11.97456" << "12.0"
                    << "110" << "12 Secret Street");
    }
    return rows;
}

void TuningTest::locationNeverLeaves()
{
    MC_VALUES rt;
    const QJsonObject payload = TuningInsights::buildPayload(
                QList<ConfigValue>(), QList<ConfigValue>(), rt, testHeader(), testRows(50), 10, "test");
    const QString json = QString::fromUtf8(QJsonDocument(payload).toJson());

    // The coordinates, the column names, and the invented column.
    QVERIFY(!json.contains("gnss_lat"));
    QVERIFY(!json.contains("gnss_lon"));
    QVERIFY(!json.contains("gnss_alt"));
    QVERIFY(!json.contains("57.70887"));
    QVERIFY(!json.contains("11.97456"));
    QVERIFY(!json.contains("home_address"));
    QVERIFY(!json.contains("Secret Street"));

    // and the permitted ones did survive, so this is not passing by emptiness
    QVERIFY(json.contains("input_voltage"));
    QVERIFY(json.contains("pas_rider_watts"));
}

void TuningTest::allowlistNotBlacklist()
{
    /*
     * The guard against somebody "simplifying" this into a blacklist of
     * gnss_*: an unknown column must be excluded by default. If the
     * implementation ever asks "is this forbidden" instead of "is this
     * allowed", home_address passes and this fails.
     */
    const QStringList allowed = TuningInsights::allowedLogColumns();

    QVERIFY(!allowed.contains("home_address"));
    QVERIFY(!allowed.contains("gnss_lat"));
    QVERIFY(allowed.contains("erpm"));
    QCOMPARE(allowed.filter(QRegularExpression("^gnss_")).size(), 0);

    // 47 of the RT log's 50 columns; the three absent are the gnss ones.
    QCOMPARE(allowed.size(), 47);
}

void TuningTest::downsampleKeepsEnds()
{
    const QList<int> ix = TuningInsights::sampleIndices(10000, 200);

    QCOMPARE(ix.first(), 0);
    QCOMPARE(ix.last(), 9999);
    QVERIFY(ix.size() <= 200);
    // strictly increasing, no duplicates
    for (int i = 1; i < ix.size(); i++) {
        QVERIFY(ix.at(i) > ix.at(i - 1));
    }
}

void TuningTest::downsampleHonoursCap()
{
    QCOMPARE(TuningInsights::sampleIndices(5, 200).size(), 5);
    QVERIFY(TuningInsights::sampleIndices(10000, 50).size() <= 50);
    QCOMPARE(TuningInsights::sampleIndices(0, 10).size(), 0);
    QCOMPARE(TuningInsights::sampleIndices(100, 0).size(), 0);
    QCOMPARE(TuningInsights::sampleIndices(1, 200).size(), 1);

    MC_VALUES rt;
    const QJsonObject p = TuningInsights::buildPayload(
                QList<ConfigValue>(), QList<ConfigValue>(), rt, testHeader(), testRows(10000), 200, "");
    const QJsonObject log = p["log"].toObject();

    QCOMPARE(log["rows_total"].toInt(), 10000);
    QVERIFY(log["rows_sent"].toInt() <= 200);
    QCOMPARE(log["columns_dropped"].toInt(), 4);   // 3 gnss + home_address
}

void TuningTest::configIsEnumeratedAndTyped()
{
    const QList<ConfigValue> conf = QList<ConfigValue>()
            << cv("a_double", CFG_T_DOUBLE, 1.5)
            << cv("an_int", CFG_T_INT, 0.0, 7)
            << cv("a_bool", CFG_T_BOOL, 0.0, 1)
            << cv("an_enum", CFG_T_ENUM, 0.0, 3);

    MC_VALUES rt;
    const QJsonObject p = TuningInsights::buildPayload(
                conf, QList<ConfigValue>(), rt, QStringList(),
                QList<QStringList>(), 10, "");
    const QJsonObject mc = p["mcconf"].toObject();

    QCOMPARE(mc["a_double"].toDouble(), 1.5);
    QCOMPARE(mc["an_int"].toInt(), 7);
    QCOMPARE(mc["a_bool"].toBool(), true);
    QVERIFY(mc["a_bool"].isBool());
    QCOMPARE(mc["an_enum"].toInt(), 3);
}

void TuningTest::configSkipsFreeText()
{
    /*
     * Free text is excluded in three independent ways, and this checks the
     * weakest of them. ConfigValue has no string member at all, so putting a
     * QSTRING in a payload is a compile error rather than a filtered value;
     * extraction drops the type before it gets here; and the serialiser has
     * no case for it. Only the last is observable from a test.
     */
    const QList<ConfigValue> conf = QList<ConfigValue>()
            << cv("a_string", CFG_T_QSTRING)
            << cv("an_undefined", CFG_T_UNDEFINED)
            << cv("a_double", CFG_T_DOUBLE, 2.5);

    MC_VALUES rt;
    const QJsonObject p = TuningInsights::buildPayload(
                conf, QList<ConfigValue>(), rt, QStringList(),
                QList<QStringList>(), 10, "");
    const QJsonObject mc = p["mcconf"].toObject();

    QVERIFY(!mc.contains("a_string"));
    QVERIFY(!mc.contains("an_undefined"));
    QVERIFY(mc.contains("a_double"));
}

void TuningTest::anthropicRequestShape()
{
    InsightsProvider::Config cfg = InsightsProvider::presetByName("anthropic");
    QScopedPointer<InsightsProvider> p(InsightsProvider::create(cfg));

    QVERIFY(p->endpoint().endsWith("/v1/messages"));

    const QNetworkRequest req = p->request("test-key");
    QCOMPARE(req.rawHeader("x-api-key"), QByteArray("test-key"));
    QCOMPARE(req.rawHeader("anthropic-version"), QByteArray("2023-06-01"));
    QVERIFY(req.rawHeader("Authorization").isEmpty());

    const QJsonObject body = QJsonDocument::fromJson(
                p->body("hello", 1024)).object();
    QCOMPARE(body["model"].toString(), QString("claude-sonnet-5"));
    QCOMPARE(body["max_tokens"].toInt(), 1024);
    QCOMPARE(body["messages"].toArray().at(0).toObject()["content"].toString(),
             QString("hello"));
}

void TuningTest::openAiRequestShape()
{
    InsightsProvider::Config cfg = InsightsProvider::presetByName("openai");
    QScopedPointer<InsightsProvider> p(InsightsProvider::create(cfg));

    QVERIFY(p->endpoint().endsWith("/v1/chat/completions"));

    const QNetworkRequest req = p->request("test-key");
    QCOMPARE(req.rawHeader("Authorization"), QByteArray("Bearer test-key"));
    QVERIFY(req.rawHeader("x-api-key").isEmpty());
    QVERIFY(req.rawHeader("anthropic-version").isEmpty());
}

void TuningTest::bothDialectsParse()
{
    // Recorded shapes, both carrying the same text.
    const QByteArray anthropic =
            R"({"id":"msg_1","type":"message","role":"assistant",
                "content":[{"type":"text","text":"l_current_max looks high."}]})";
    const QByteArray openai =
            R"({"id":"chatcmpl-1","choices":[{"index":0,"message":
                {"role":"assistant","content":"l_current_max looks high."}}]})";

    QScopedPointer<InsightsProvider> a(InsightsProvider::create(
                InsightsProvider::presetByName("anthropic")));
    QScopedPointer<InsightsProvider> o(InsightsProvider::create(
                InsightsProvider::presetByName("openai")));

    QString err;
    QCOMPARE(a->parseReply(anthropic, &err), QString("l_current_max looks high."));
    QVERIFY(err.isEmpty());
    QCOMPARE(o->parseReply(openai, &err), QString("l_current_max looks high."));
    QVERIFY(err.isEmpty());
}

void TuningTest::badRepliesAreErrors()
{
    QScopedPointer<InsightsProvider> a(InsightsProvider::create(
                InsightsProvider::presetByName("anthropic")));
    QScopedPointer<InsightsProvider> o(InsightsProvider::create(
                InsightsProvider::presetByName("openai")));
    QString err;

    // truncated
    err.clear();
    QVERIFY(a->parseReply("{\"content\":[{\"type\":\"te", &err).isEmpty());
    QVERIFY(!err.isEmpty());

    // provider error object
    err.clear();
    QVERIFY(a->parseReply(
                R"({"error":{"type":"invalid_request_error","message":"bad model"}})",
                &err).isEmpty());
    QVERIFY(err.contains("bad model"));

    // openai-style error as an object, and as a bare string
    err.clear();
    QVERIFY(o->parseReply(R"({"error":{"message":"insufficient quota"}})",
                          &err).isEmpty());
    QVERIFY(err.contains("insufficient quota"));

    err.clear();
    QVERIFY(o->parseReply(R"({"error":"model not found"})", &err).isEmpty());
    QVERIFY(err.contains("model not found"));

    // right shape, no text: must be an error, not a blank analysis
    err.clear();
    QVERIFY(o->parseReply(R"({"choices":[]})", &err).isEmpty());
    QVERIFY(!err.isEmpty());
}

void TuningTest::localProviderNeedsNoKey()
{
    const InsightsProvider::Config ollama =
            InsightsProvider::presetByName("ollama");

    QVERIFY(!ollama.needsKey());
    QVERIFY(ollama.isLocal());
    // nothing missing, so a local run works with no configuration at all
    QVERIFY(TuningClient::preflight(ollama).isEmpty());
    QVERIFY(TuningClient::resolveKey(ollama).isEmpty());

    const InsightsProvider::Config anthropic =
            InsightsProvider::presetByName("anthropic");
    QVERIFY(anthropic.needsKey());
    QVERIFY(!anthropic.isLocal());
}

void TuningTest::keyNeverAppearsInPayload()
{
    qputenv("ANTHROPIC_API_KEY", "sk-ant-SHOULD-NOT-APPEAR");

    const InsightsProvider::Config cfg =
            InsightsProvider::presetByName("anthropic");
    QCOMPARE(TuningClient::resolveKey(cfg), QString("sk-ant-SHOULD-NOT-APPEAR"));
    QVERIFY(TuningClient::preflight(cfg).isEmpty());

    MC_VALUES rt;
    const QJsonObject p = TuningInsights::buildPayload(
                QList<ConfigValue>(), QList<ConfigValue>(), rt, testHeader(), testRows(5), 5, "test");
    const QString json = QString::fromUtf8(QJsonDocument(p).toJson());
    const QString prompt = TuningInsights::buildPrompt(p);

    QVERIFY(!json.contains("SHOULD-NOT-APPEAR"));
    QVERIFY(!prompt.contains("SHOULD-NOT-APPEAR"));

    qunsetenv("ANTHROPIC_API_KEY");
}

void TuningTest::customSpecParsesUrlWithPort()
{
    QString err;
    const InsightsProvider::Config c = InsightsProvider::fromSpec(
                "openai:http://localhost:11434:mistral", &err);

    QVERIFY2(err.isEmpty(), qPrintable(err));
    QCOMPARE(c.kind, InsightsProvider::KindOpenAiCompatible);
    QCOMPARE(c.baseUrl, QString("http://localhost:11434"));
    QCOMPARE(c.model, QString("mistral"));
    QVERIFY(c.isLocal());
    QVERIFY(!c.needsKey());
}

void TuningTest::badSpecExplainsItself()
{
    QString err;

    InsightsProvider::fromSpec("nonsense", &err);
    QVERIFY(!err.isEmpty());
    QVERIFY2(err.contains("ollama"), qPrintable(err));   // lists the presets

    err.clear();
    InsightsProvider::fromSpec("gemini:https://x:y", &err);
    QVERIFY(err.contains("anthropic"));                  // names the kinds
}

QTEST_GUILESS_MAIN(TuningTest)
#include "tst_tuning.moc"
