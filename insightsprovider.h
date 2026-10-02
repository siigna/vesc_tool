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

#ifndef INSIGHTSPROVIDER_H
#define INSIGHTSPROVIDER_H

#include <QByteArray>
#include <QList>
#include <QNetworkRequest>
#include <QString>

/*
 * Where a tuning analysis is sent, as a seam rather than a vendor.
 *
 * Two dialects cover nearly the whole field: Anthropic's /v1/messages, and
 * the /v1/chat/completions shape that OpenAI, Ollama, llama.cpp's server, LM
 * Studio, vLLM, OpenRouter and others all speak. A provider is four values --
 * kind, base URL, model, and the name of the environment variable holding a
 * key -- none of them compiled in.
 *
 * A provider needing no key is a supported case, not an error. That is what
 * makes a model on localhost work, and a local model is a better answer to
 * "what leaves my machine" than any amount of redaction.
 */

class InsightsProvider
{
public:
    enum Kind {
        KindAnthropic,
        KindOpenAiCompatible,
    };

    struct Config {
        Kind kind = KindOpenAiCompatible;
        QString id;
        QString baseUrl;
        QString model;
        QString keyEnvVar;      // empty: this provider needs no key

        bool needsKey() const { return !keyEnvVar.isEmpty(); }
        bool isLocal() const;   // loopback host, so nothing leaves the machine
    };

    explicit InsightsProvider(const Config &cfg);
    virtual ~InsightsProvider() {}

    // The presets. None is privileged, and one of them needs no network.
    static QList<Config> presets();
    static Config presetByName(const QString &id, bool *found = nullptr);

    /*
     * Parses "kind:baseUrl:model", for --insightsProvider with something that
     * is not a preset. kind is "anthropic" or "openai".
     */
    static Config fromSpec(const QString &spec, QString *err);

    static InsightsProvider *create(const Config &cfg);

    const Config &config() const { return mCfg; }

    // The URL a request would go to, for showing before anything is sent.
    virtual QString endpoint() const = 0;

    virtual QNetworkRequest request(const QString &apiKey) const = 0;
    virtual QByteArray body(const QString &prompt, int maxTokens) const = 0;

    /*
     * Pulls the reply text out, or sets err. An error object from the
     * provider, a truncated body and a body of the wrong shape all have to
     * come back as errors rather than as empty output -- an empty analysis
     * that looks like a successful one is worse than a failure.
     */
    virtual QString parseReply(const QByteArray &in, QString *err) const = 0;

protected:
    Config mCfg;
};

class AnthropicProvider: public InsightsProvider
{
public:
    explicit AnthropicProvider(const Config &cfg): InsightsProvider(cfg) {}
    QString endpoint() const override;
    QNetworkRequest request(const QString &apiKey) const override;
    QByteArray body(const QString &prompt, int maxTokens) const override;
    QString parseReply(const QByteArray &in, QString *err) const override;
};

class OpenAiCompatProvider: public InsightsProvider
{
public:
    explicit OpenAiCompatProvider(const Config &cfg): InsightsProvider(cfg) {}
    QString endpoint() const override;
    QNetworkRequest request(const QString &apiKey) const override;
    QByteArray body(const QString &prompt, int maxTokens) const override;
    QString parseReply(const QByteArray &in, QString *err) const override;
};

#endif // INSIGHTSPROVIDER_H
