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
#include <QTcpServer>
#include <QTcpSocket>
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


/*
 * A one-shot HTTP server on a loopback port. Not a mock of the client: a real
 * socket, so the request is actually serialised, sent, and parsed back. It
 * captures the bytes it received so the test can assert on what went out --
 * including that the key did not.
 */
class StubServer: public QObject
{
    Q_OBJECT

public:
    StubServer(const QByteArray &body, int status = 200)
    {
        mBody = body;
        mStatus = status;

        connect(&mServer, &QTcpServer::newConnection, this, [this]() {
            QTcpSocket *sock = mServer.nextPendingConnection();

            connect(sock, &QTcpSocket::readyRead, this, [this, sock]() {
                mRequest.append(sock->readAll());

                /*
                 * Wait for the whole request, not just the headers. Replying
                 * as soon as the blank line arrives races the body: the first
                 * version of this stub did exactly that and the assertion on
                 * the model name failed against a request that had not
                 * finished arriving.
                 */
                const int hdrEnd = mRequest.indexOf("\r\n\r\n");
                if (hdrEnd < 0) {
                    return;
                }

                const QByteArray hdrs = mRequest.left(hdrEnd).toLower();
                int want = 0;
                const int clPos = hdrs.indexOf("content-length:");
                if (clPos >= 0) {
                    want = hdrs.mid(clPos + 15,
                                    hdrs.indexOf("\r\n", clPos) - clPos - 15)
                            .trimmed().toInt();
                }

                if (mRequest.size() - (hdrEnd + 4) < want) {
                    return;
                }

                const QByteArray reason = mStatus == 200 ? "OK" : "Bad Request";
                QByteArray resp = "HTTP/1.1 " + QByteArray::number(mStatus)
                        + " " + reason + "\r\n"
                        "Content-Type: application/json\r\n"
                        "Content-Length: " + QByteArray::number(mBody.size())
                        + "\r\n"
                        "Connection: close\r\n\r\n" + mBody;

                sock->write(resp);
                sock->flush();
                sock->disconnectFromHost();
            });
        });

        mServer.listen(QHostAddress::LocalHost, 0);
    }

    quint16 port() const { return mServer.serverPort(); }
    QByteArray request() const { return mRequest; }

private:
    QTcpServer mServer;
    QByteArray mBody;
    QByteArray mRequest;
    int mStatus;
};

class TuningTest: public QObject
{
    Q_OBJECT

private slots:
    void locationNeverLeaves();
    void allowlistNotBlacklist();
    void packageLoggerDialect();
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
    void cutOffReplyBlamesTheCapNotTheDialect();
    void truncatedAnswerSaysSo();
    void reasoningIsSentOnlyWhenAsked();
    void promptOverrideCannotDropTheData();

    void transportReachesAStubServer();
    void transportReportsAServerError();
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
    /*
     * The whole gnss_ family, under either dialect's spelling. The Tool's log
     * has eight (gnss_posTime, gnss_lat, gnss_lon, gnss_alt, gnss_gVel,
     * gnss_vVel, gnss_hAcc, gnss_vAcc) and the package logger spells two of
     * its own (gnss_h_acc, gnss_h_vel), which is the reason none of them are
     * enumerated as exclusions anywhere.
     */
    QCOMPARE(allowed.filter(QRegularExpression("^gnss_")).size(), 0);
    QVERIFY(!allowed.contains("gnss_hAcc"));
    QVERIFY(!allowed.contains("gnss_h_vel"));
    QVERIFY(!allowed.contains("gnss_posTime"));

    /*
     * 50 of the Tool log's 61 columns plus 32 of the package logger's 38.
     * A count, so that a column added without a thought about what it
     * discloses shows up here as a failure.
     */
    QCOMPARE(allowed.size(), 82);
}

void TuningTest::packageLoggerDialect()
{
    /*
     * The logs on hand are the package logger's, whose header fields are
     * "name:label:unit:dec:..." descriptors rather than bare names. Before
     * normalizeColumn existed, every field failed the allowlist and the
     * payload silently carried no log at all -- safe, and useless.
     */
    QCOMPARE(TuningInsights::normalizeColumn("Input Voltage:Input Voltage:V:2:0:0"),
             QString("Input Voltage"));
    QCOMPARE(TuningInsights::normalizeColumn("gnss_lat:gnss_lat::2:0:0"),
             QString("gnss_lat"));
    // A bare name has no colon, so the Tool's own dialect passes through.
    QCOMPARE(TuningInsights::normalizeColumn("input_voltage"), QString("input_voltage"));

    const QStringList header = QStringList()
            << "Input Voltage:Input Voltage:V:2:0:0"
            << "RPM:RPM::2:0:0"
            << "kmh_vesc:Speed ESC:km/h:2:0:0"
            << "t_day:Time:s:3:0:1"
            << "gnss_lat:gnss_lat::2:0:0"
            << "gnss_lon:gnss_lon::2:0:0"
            << "gnss_alt:gnss_alt:m:2:0:0"
            << "gnss_h_acc:gnss_h_acc:m:2:0:0"
            << "gnss_h_vel:gnss_h_vel:m/s:2:0:0";

    QList<QStringList> rows;
    rows << (QStringList() << "50.2" << "1200" << "24.5" << "3600"
                           << "57.70887" << "11.97456" << "12.0" << "3.5" << "6.8");

    MC_VALUES rt;
    const QJsonObject payload = TuningInsights::buildPayload(
                QList<ConfigValue>(), QList<ConfigValue>(), rt, header, rows, 10, "test");
    const QString json = QString::fromUtf8(QJsonDocument(payload).toJson());

    // The log arrived, under normalized names rather than raw descriptors.
    QVERIFY(json.contains("Input Voltage"));
    QVERIFY(json.contains("kmh_vesc"));

    /*
     * The label and unit from the descriptor are now sent as well, in
     * log.column_fields, because "cnt_ah" and "iq" mean nothing on their own.
     * They are static metadata from the logger's own header, not anything a
     * user typed. The column names themselves stay normalized.
     */
    const QJsonObject notes =
            payload["log"].toObject()["column_fields"].toObject();
    QCOMPARE(notes["kmh_vesc"].toString(), QString("Speed ESC (km/h)"));
    QVERIFY(!notes.contains("gnss_lat"));

    /*
     * And no label may reintroduce location. t_day_pos is excluded from the
     * allowlist precisely because its label is "Time GNSS", which would put
     * that word back into a payload that is otherwise free of it.
     */
    QVERIFY(!json.contains("gnss"));
    QVERIFY(!json.contains("GNSS"));

    // and no position, including the two spellings only this dialect uses
    QVERIFY(!json.contains("gnss_lat"));
    QVERIFY(!json.contains("gnss_h_acc"));
    QVERIFY(!json.contains("gnss_h_vel"));
    QVERIFY(!json.contains("57.70887"));
    QVERIFY(!json.contains("11.97456"));

    QCOMPARE(payload["log"].toObject()["columns_dropped"].toInt(), 5);
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

void TuningTest::cutOffReplyBlamesTheCapNotTheDialect()
{
    /*
     * A reasoning model spends the answer's token budget thinking first, and
     * on a payload of this size it can spend all of it: measured with
     * claude-sonnet-5 and 45 kB, 5000 completion tokens went to reasoning and
     * the content came back empty, finish_reason "length".
     *
     * The parser used to call that a dialect mismatch, which sent the reader
     * to check their base URL when the fix was to raise a number.
     */
    InsightsProvider::Config cfg;
    cfg.kind = InsightsProvider::KindOpenAiCompatible;
    cfg.baseUrl = "http://localhost:11434";
    cfg.model = "test";
    QScopedPointer<InsightsProvider> prov(InsightsProvider::create(cfg));

    const QByteArray reasoned =
            "{\"choices\":[{\"finish_reason\":\"length\","
            "\"message\":{\"content\":\"\"}}],"
            "\"usage\":{\"completion_tokens\":700,"
            "\"completion_tokens_details\":{\"reasoning_tokens\":700}}}";

    QString err;
    QVERIFY(prov->parseReply(reasoned, &err).isEmpty());
    QVERIFY2(err.contains("700"), qPrintable(err));
    QVERIFY2(err.contains("reasoning"), qPrintable(err));
    QVERIFY2(err.contains("--insightsMaxTokens"), qPrintable(err));
    QVERIFY2(!err.contains("dialect"),
             qPrintable("a cut-off answer must not be reported as a dialect "
                        "mismatch: " + err));

    // Cut off with no reasoning reported: still the cap, just without a figure.
    const QByteArray plain =
            "{\"choices\":[{\"finish_reason\":\"length\","
            "\"message\":{\"content\":\"\"}}]}";
    err.clear();
    QVERIFY(prov->parseReply(plain, &err).isEmpty());
    QVERIFY2(err.contains("--insightsMaxTokens"), qPrintable(err));

    /*
     * An empty answer that was NOT cut off is the case where blaming the
     * dialect is right, so that message has to survive.
     */
    const QByteArray stopped =
            "{\"choices\":[{\"finish_reason\":\"stop\","
            "\"message\":{\"content\":\"\"}}]}";
    err.clear();
    QVERIFY(prov->parseReply(stopped, &err).isEmpty());
    QVERIFY2(err.contains("dialect"), qPrintable(err));
}

void TuningTest::truncatedAnswerSaysSo()
{
    // Advice that stops mid-sentence must not look like advice that finished.
    InsightsProvider::Config cfg;
    cfg.kind = InsightsProvider::KindOpenAiCompatible;
    cfg.baseUrl = "http://localhost:11434";
    QScopedPointer<InsightsProvider> prov(InsightsProvider::create(cfg));

    const QByteArray cut =
            "{\"choices\":[{\"finish_reason\":\"length\","
            "\"message\":{\"content\":\"Lower l_current_max to\"}}]}";

    QString err;
    const QString out = prov->parseReply(cut, &err);

    QVERIFY2(err.isEmpty(), qPrintable(err));
    QVERIFY(out.startsWith("Lower l_current_max to"));
    QVERIFY2(out.contains("cut off"), qPrintable(out));
}

void TuningTest::reasoningIsSentOnlyWhenAsked()
{
    /*
     * The field is OpenRouter's, and a strict server can reject a request
     * carrying something it does not know, so the default wire format must be
     * exactly what it was before the control existed.
     */
    InsightsProvider::Config cfg;
    cfg.kind = InsightsProvider::KindOpenAiCompatible;
    cfg.baseUrl = "http://localhost:11434";
    cfg.model = "test";

    {
        QScopedPointer<InsightsProvider> prov(InsightsProvider::create(cfg));
        const QJsonObject body = QJsonDocument::fromJson(
                    prov->body("hello", 100)).object();
        QVERIFY2(!body.contains("reasoning"),
                 "a request must carry no reasoning field unless asked");
    }

    cfg.reasoning = "off";
    {
        QScopedPointer<InsightsProvider> prov(InsightsProvider::create(cfg));
        const QJsonObject r = QJsonDocument::fromJson(
                    prov->body("hello", 100)).object()["reasoning"].toObject();
        QCOMPARE(r["enabled"].toBool(), false);
    }

    cfg.reasoning = "low";
    {
        QScopedPointer<InsightsProvider> prov(InsightsProvider::create(cfg));
        const QJsonObject r = QJsonDocument::fromJson(
                    prov->body("hello", 100)).object()["reasoning"].toObject();
        QCOMPARE(r["effort"].toString(), QString("low"));
    }

    cfg.reasoning = "400";
    {
        QScopedPointer<InsightsProvider> prov(InsightsProvider::create(cfg));
        const QJsonObject r = QJsonDocument::fromJson(
                    prov->body("hello", 100)).object()["reasoning"].toObject();
        QCOMPARE(r["max_tokens"].toInt(), 400);
    }

    // The Anthropic dialect does not take this field at all.
    cfg.kind = InsightsProvider::KindAnthropic;
    cfg.baseUrl = "https://api.anthropic.com";
    cfg.reasoning = "low";
    {
        QScopedPointer<InsightsProvider> prov(InsightsProvider::create(cfg));
        const QJsonObject body = QJsonDocument::fromJson(
                    prov->body("hello", 100)).object();
        QVERIFY2(!body.contains("reasoning"),
                 "the anthropic dialect must not carry an openai-only field");
    }
}

void TuningTest::promptOverrideCannotDropTheData()
{
    /*
     * The instructions are replaceable, the data is not: buildPrompt appends
     * the payload itself, so a replaced prompt cannot omit it or substitute
     * something else.
     */
    MC_VALUES rt;
    const QJsonObject payload = TuningInsights::buildPayload(
                QList<ConfigValue>(), QList<ConfigValue>(), rt,
                testHeader(), testRows(10), 5, "test");

    const QString custom = TuningInsights::buildPrompt(payload, "Only PAS.");

    QVERIFY(custom.contains("Only PAS."));
    QVERIFY2(!custom.contains("Give concrete tuning observations"),
             "the default instructions must be replaced, not appended to");
    QVERIFY2(custom.contains("Data:"), "the data section went missing");
    QVERIFY2(custom.contains("input_voltage"), "the payload went missing");

    // Empty means "use the built-in text", not "send no instructions".
    const QString fallback = TuningInsights::buildPrompt(payload, "   ");
    QVERIFY(fallback.contains("Give concrete tuning observations"));
    QVERIFY(fallback.contains("Data:"));

    // And the default is what --insightsPrintPrompt hands out.
    QVERIFY(TuningInsights::defaultInstructions().contains(
                "Give concrete tuning observations"));
    QVERIFY2(TuningInsights::defaultInstructions().contains("mcconf_fields"),
             "the instructions should point at the field notes in the payload");
}

void TuningTest::transportReachesAStubServer()
{
    /*
     * The whole send path over a real socket: request built, posted, reply
     * parsed. A local server is the honest way to test this -- it is also
     * exactly the shape of the ollama setup, which is the default provider.
     */
    StubServer stub("{\"choices\":[{\"message\":"
                    "{\"content\":\"Motor current looks conservative.\"}}]}");
    QVERIFY(stub.port() != 0);

    QString err;
    InsightsProvider::Config cfg = InsightsProvider::fromSpec(
                QString("openai:http://127.0.0.1:%1:stub-model").arg(stub.port()),
                &err);
    QVERIFY2(err.isEmpty(), qPrintable(err));

    QScopedPointer<InsightsProvider> prov(InsightsProvider::create(cfg));
    QVERIFY(!prov.isNull());

    TuningClient client;
    const QString reply = client.send(prov.data(), "a prompt", 256, 5000, &err);

    QVERIFY2(err.isEmpty(), qPrintable(err));
    QCOMPARE(reply, QString("Motor current looks conservative."));

    // It went where it said it would, in the dialect it said it would.
    const QByteArray req = stub.request();
    QVERIFY(req.startsWith("POST /v1/chat/completions"));
    QVERIFY2(req.contains("stub-model"), req.constData());
    QVERIFY(req.contains("a prompt"));

    /*
     * No key was set for this provider and none was invented. An empty
     * Authorization header would be as much of a bug as a wrong one.
     */
    QVERIFY(!req.contains("Authorization"));
}

void TuningTest::transportReportsAServerError()
{
    /*
     * A provider that answers 400 with an explanation. The explanation is
     * worth more than the status code, so the body is parsed even on an error
     * and the message reaches the user.
     */
    StubServer stub("{\"error\":{\"message\":\"model \\\"nope\\\" not found\"}}", 400);
    QVERIFY(stub.port() != 0);

    QString err;
    InsightsProvider::Config cfg = InsightsProvider::fromSpec(
                QString("openai:http://127.0.0.1:%1:nope").arg(stub.port()), &err);
    QVERIFY2(err.isEmpty(), qPrintable(err));

    QScopedPointer<InsightsProvider> prov(InsightsProvider::create(cfg));
    TuningClient client;
    const QString reply = client.send(prov.data(), "a prompt", 256, 5000, &err);

    QVERIFY(reply.isEmpty());
    QVERIFY(!err.isEmpty());
    QVERIFY2(err.contains("not found"), qPrintable(err));
}

QTEST_GUILESS_MAIN(TuningTest)
#include "tst_tuning.moc"
