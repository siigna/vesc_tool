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

#include "insightsprovider.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUrl>

bool InsightsProvider::Config::isLocal() const
{
    const QString host = QUrl(baseUrl).host();

    return host == "localhost" || host == "127.0.0.1" || host == "::1"
            || host.startsWith("127.");
}

InsightsProvider::InsightsProvider(const Config &cfg): mCfg(cfg)
{
}

QList<InsightsProvider::Config> InsightsProvider::presets()
{
    QList<Config> out;

    /*
     * Starting points, not recommendations. The two that need no key and no
     * network are listed alongside the hosted ones deliberately: for this
     * feature they are the only options where the data does not leave the
     * machine at all.
     */
    Config ollama;
    ollama.kind = KindOpenAiCompatible;
    ollama.id = "ollama";
    ollama.baseUrl = "http://localhost:11434";
    ollama.model = "llama3.1";
    out.append(ollama);

    Config local;
    local.kind = KindOpenAiCompatible;
    local.id = "local";
    local.baseUrl = "http://localhost:8080";
    local.model = "local-model";
    out.append(local);

    Config anthropic;
    anthropic.kind = KindAnthropic;
    anthropic.id = "anthropic";
    /*
     * ANTHROPIC_BASE_URL wins when it is set, because a setup that already
     * exports it is pointed somewhere on purpose -- a gateway, a proxy, or a
     * company endpoint -- and silently sending to api.anthropic.com instead
     * would route the data past whatever that is there for.
     */
    /*
     * ANTHROPIC_BASE_URL is honoured, but only when it actually points at
     * Anthropic. A setup here exported it pointing at openrouter.ai, which
     * speaks the OpenAI dialect: posting /v1/messages there returned HTTP 200
     * with a body this code could find no text in. The variable names a host,
     * not a protocol, so a host that is not Anthropic's is left to the
     * openrouter and openai presets, or to --insightsProvider, rather than
     * being addressed in a dialect it does not speak.
     */
    const QString envBase =
            QString::fromLocal8Bit(qgetenv("ANTHROPIC_BASE_URL")).trimmed();

    anthropic.baseUrl = QUrl(envBase).host().endsWith("anthropic.com")
            ? envBase
            : QString("https://api.anthropic.com");

    while (anthropic.baseUrl.endsWith("/")) {
        anthropic.baseUrl.chop(1);
    }

    anthropic.model = "claude-sonnet-5";
    anthropic.keyEnvVar = "ANTHROPIC_API_KEY";
    out.append(anthropic);

    Config openai;
    openai.kind = KindOpenAiCompatible;
    openai.id = "openai";
    openai.baseUrl = "https://api.openai.com";
    openai.model = "gpt-4o";
    openai.keyEnvVar = "OPENAI_API_KEY";
    out.append(openai);

    Config openrouter;
    openrouter.kind = KindOpenAiCompatible;
    openrouter.id = "openrouter";
    openrouter.baseUrl = "https://openrouter.ai/api";
    /*
     * Verified against openrouter's own model list rather than guessed.
     * anthropic/claude-opus-5 is a one-flag swap and took the same wall time
     * in testing, but it is a reasoning model and needs a token cap large
     * enough to cover its thinking as well as its answer.
     */
    openrouter.model = "anthropic/claude-sonnet-5";
    openrouter.keyEnvVar = "OPENROUTER_API_KEY";
    out.append(openrouter);

    return out;
}

InsightsProvider::Config InsightsProvider::presetByName(const QString &id,
                                                        bool *found)
{
    for (const Config &c: presets()) {
        if (c.id == id) {
            if (found != nullptr) {
                *found = true;
            }
            return c;
        }
    }

    if (found != nullptr) {
        *found = false;
    }
    return Config();
}

InsightsProvider::Config InsightsProvider::fromSpec(const QString &spec,
                                                    QString *err)
{
    Config out;
    bool found = false;

    out = presetByName(spec, &found);
    if (found) {
        return out;
    }

    /*
     * "kind:baseUrl:model". Split from the left twice only, because a URL
     * contains colons -- splitting on every colon turns
     * http://localhost:11434 into three pieces.
     */
    int firstColon = spec.indexOf(':');
    int lastColon = spec.lastIndexOf(':');

    if (firstColon < 0 || lastColon <= firstColon) {
        if (err != nullptr) {
            QStringList names;
            for (const Config &c: presets()) {
                names.append(c.id);
            }
            *err = QString("Unknown provider \"%1\". Use one of: %2, "
                           "or \"kind:baseUrl:model\" where kind is "
                           "anthropic or openai.")
                    .arg(spec, names.join(", "));
        }
        return Config();
    }

    const QString kind = spec.left(firstColon);
    out.baseUrl = spec.mid(firstColon + 1, lastColon - firstColon - 1);
    out.model = spec.mid(lastColon + 1);
    out.id = "custom";

    if (kind == "anthropic") {
        out.kind = KindAnthropic;
        out.keyEnvVar = "ANTHROPIC_API_KEY";
    } else if (kind == "openai") {
        out.kind = KindOpenAiCompatible;
        // No key assumed: a custom openai-compatible endpoint is usually
        // something local. Set INSIGHTS_API_KEY to send one.
        out.keyEnvVar = "";
    } else {
        if (err != nullptr) {
            *err = QString("Unknown provider kind \"%1\". "
                           "Use anthropic or openai.").arg(kind);
        }
        return Config();
    }

    if (out.baseUrl.isEmpty() || out.model.isEmpty()) {
        if (err != nullptr) {
            *err = "A custom provider needs \"kind:baseUrl:model\".";
        }
        return Config();
    }

    return out;
}

InsightsProvider *InsightsProvider::create(const Config &cfg)
{
    switch (cfg.kind) {
    case KindAnthropic:
        return new AnthropicProvider(cfg);
    case KindOpenAiCompatible:
    default:
        return new OpenAiCompatProvider(cfg);
    }
}

/* ------------------------------------------------------------- Anthropic -- */

QString AnthropicProvider::endpoint() const
{
    return mCfg.baseUrl + "/v1/messages";
}

QNetworkRequest AnthropicProvider::request(const QString &apiKey) const
{
    QNetworkRequest req{QUrl(endpoint())};

    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    req.setRawHeader("anthropic-version", "2023-06-01");
    if (!apiKey.isEmpty()) {
        req.setRawHeader("x-api-key", apiKey.toUtf8());
    }
    return req;
}

QByteArray AnthropicProvider::body(const QString &prompt, int maxTokens) const
{
    QJsonObject msg;
    msg["role"] = "user";
    msg["content"] = prompt;

    QJsonArray messages;
    messages.append(msg);

    QJsonObject root;
    root["model"] = mCfg.model;
    root["max_tokens"] = maxTokens;
    root["messages"] = messages;

    return QJsonDocument(root).toJson(QJsonDocument::Compact);
}

QString AnthropicProvider::parseReply(const QByteArray &in, QString *err) const
{
    QJsonParseError pe;
    const QJsonDocument doc = QJsonDocument::fromJson(in, &pe);

    if (pe.error != QJsonParseError::NoError) {
        if (err != nullptr) {
            *err = QString("Could not parse the reply: %1").arg(pe.errorString());
        }
        return QString();
    }

    const QJsonObject root = doc.object();

    if (root.contains("error")) {
        if (err != nullptr) {
            const QJsonObject e = root["error"].toObject();
            *err = QString("%1: %2")
                    .arg(e["type"].toString("error"),
                         e["message"].toString("no message"));
        }
        return QString();
    }

    const QJsonArray content = root["content"].toArray();
    QString out;

    for (const QJsonValue &v: content) {
        const QJsonObject block = v.toObject();
        if (block["type"].toString() == "text") {
            out += block["text"].toString();
        }
    }

    if (out.isEmpty() && err != nullptr) {
        *err = "The reply carried no text. If the endpoint is not the one this dialect expects, it can answer 200 in a shape this cannot read -- check the provider kind against the base URL.";
    }
    return out;
}

/* ------------------------------------------------- OpenAI-compatible ----- */

QString OpenAiCompatProvider::endpoint() const
{
    return mCfg.baseUrl + "/v1/chat/completions";
}

QNetworkRequest OpenAiCompatProvider::request(const QString &apiKey) const
{
    QNetworkRequest req{QUrl(endpoint())};

    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    if (!apiKey.isEmpty()) {
        req.setRawHeader("Authorization",
                         QByteArray("Bearer ") + apiKey.toUtf8());
    }
    return req;
}

QByteArray OpenAiCompatProvider::body(const QString &prompt,
                                      int maxTokens) const
{
    QJsonObject msg;
    msg["role"] = "user";
    msg["content"] = prompt;

    QJsonArray messages;
    messages.append(msg);

    QJsonObject root;
    root["model"] = mCfg.model;
    root["max_tokens"] = maxTokens;
    root["messages"] = messages;
    root["stream"] = false;

    /*
     * Only sent when asked for. An unrecognised field is a risk with a strict
     * server, and a local model has no thinking budget to configure, so the
     * default wire format is unchanged.
     */
    if (!mCfg.reasoning.isEmpty()) {
        QJsonObject r;
        bool isCount = false;
        const int count = mCfg.reasoning.toInt(&isCount);

        if (isCount) {
            r["max_tokens"] = count;
        } else if (mCfg.reasoning.compare("off", Qt::CaseInsensitive) == 0) {
            r["enabled"] = false;
        } else {
            r["effort"] = mCfg.reasoning.toLower();
        }

        root["reasoning"] = r;
    }

    return QJsonDocument(root).toJson(QJsonDocument::Compact);
}

QString OpenAiCompatProvider::parseReply(const QByteArray &in,
                                         QString *err) const
{
    QJsonParseError pe;
    const QJsonDocument doc = QJsonDocument::fromJson(in, &pe);

    if (pe.error != QJsonParseError::NoError) {
        if (err != nullptr) {
            *err = QString("Could not parse the reply: %1").arg(pe.errorString());
        }
        return QString();
    }

    const QJsonObject root = doc.object();

    if (root.contains("error")) {
        if (err != nullptr) {
            // Some servers put a string here, some an object.
            const QJsonValue e = root["error"];
            if (e.isObject()) {
                *err = e.toObject()["message"].toString("unknown error");
            } else {
                *err = e.toString("unknown error");
            }
        }
        return QString();
    }

    const QJsonArray choices = root["choices"].toArray();

    if (choices.isEmpty()) {
        if (err != nullptr) {
            *err = "The reply carried no choices.";
        }
        return QString();
    }

    const QJsonObject choice = choices.at(0).toObject();
    const QString out = choice["message"].toObject()["content"].toString();
    const QString finish = choice["finish_reason"].toString();

    if (out.isEmpty() && err != nullptr) {
        /*
         * An empty answer has two very different causes, and saying the wrong
         * one sends the reader to the wrong place.
         *
         * A reasoning model spends the same token budget on thinking first, so
         * too small a cap returns finish_reason "length" with every completion
         * token consumed and nothing left to say: measured with
         * anthropic/claude-opus-5 at a 700-token cap, 700 reasoning tokens and
         * an empty content field. That is not a dialect mismatch, which is
         * what this used to claim.
         */
        const QJsonObject usage = root["usage"].toObject();
        const int reasoning = usage["completion_tokens_details"].toObject()
                ["reasoning_tokens"].toInt();

        if (finish == "length") {
            if (reasoning > 0) {
                *err = QString("The answer was cut off before any text: all "
                               "%1 tokens went to the model's reasoning. Raise "
                               "the token cap (--insightsMaxTokens), which has "
                               "to cover the thinking as well as the answer.")
                        .arg(reasoning);
            } else {
                *err = "The answer was cut off before any text. Raise the "
                       "token cap (--insightsMaxTokens).";
            }
        } else {
            *err = QString("The reply carried no text (finish_reason \"%1\"). "
                           "If the endpoint is not the one this dialect "
                           "expects it can answer 200 in a shape this cannot "
                           "read -- check the provider kind against the base "
                           "URL.").arg(finish.isEmpty() ? "none" : finish);
        }
        return QString();
    }

    /*
     * Truncated but non-empty: worth saying, because advice that stops
     * mid-sentence should not look like advice that finished.
     */
    if (finish == "length") {
        return out + QStringLiteral("\n\n[cut off at the token cap; raise "
                                    "--insightsMaxTokens for the rest]");
    }

    return out;
}
