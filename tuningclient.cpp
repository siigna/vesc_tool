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

#include "tuningclient.h"

#include <QElapsedTimer>
#include <QEventLoop>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QSettings>
#include <QTimer>

TuningClient::TuningClient(QObject *parent): QObject(parent)
{
}

QString TuningClient::settingsKeyFor(const InsightsProvider::Config &cfg)
{
    return QString("insights/%1/key").arg(cfg.id.isEmpty() ? "custom" : cfg.id);
}

QString TuningClient::resolveKey(const InsightsProvider::Config &cfg)
{
    if (!cfg.needsKey()) {
        return QString();
    }

    QByteArray env = qgetenv(cfg.keyEnvVar.toUtf8().constData());
    if (!env.isEmpty()) {
        return QString::fromUtf8(env);
    }

    /*
     * A local Claude Code setup exports ANTHROPIC_AUTH_TOKEN rather than
     * ANTHROPIC_API_KEY, so accept it for that provider. Only that one: the
     * name says what it is for, and reusing it elsewhere would send a token
     * to a host it was not issued for.
     */
    if (cfg.kind == InsightsProvider::KindAnthropic) {
        env = qgetenv("ANTHROPIC_AUTH_TOKEN");
        if (!env.isEmpty()) {
            return QString::fromUtf8(env);
        }
    }

    QSettings set;
    return set.value(settingsKeyFor(cfg), "").toString();
}

QString TuningClient::preflight(const InsightsProvider::Config &cfg)
{
    if (cfg.baseUrl.isEmpty()) {
        return "No provider is selected.";
    }
    if (cfg.model.isEmpty()) {
        return "No model is set for this provider.";
    }
    if (cfg.needsKey() && resolveKey(cfg).isEmpty()) {
        return QString("No API key. Set %1 in the environment, or save one in "
                       "Preferences.").arg(cfg.keyEnvVar);
    }
    return QString();
}

QString TuningClient::send(InsightsProvider *provider, const QString &prompt,
                           int maxTokens, int timeoutMs, QString *err)
{
    if (provider == nullptr) {
        if (err != nullptr) {
            *err = "No provider.";
        }
        return QString();
    }

    const QString pre = preflight(provider->config());
    if (!pre.isEmpty()) {
        if (err != nullptr) {
            *err = pre;
        }
        return QString();
    }

    QNetworkAccessManager manager;
    QNetworkRequest req = provider->request(resolveKey(provider->config()));
    const QByteArray payload = provider->body(prompt, maxTokens);

    /*
     * Progress on stderr, because this call blocks for as long as the provider
     * takes to write its answer -- tens of seconds is normal -- and a silent
     * terminal is indistinguishable from a hang. Never the key: only the
     * endpoint, the sizes and the elapsed time.
     */
    fprintf(stderr, "insights: POST %s, %.1f kB, model %s, up to %d tokens\n",
            provider->endpoint().toLocal8Bit().constData(),
            payload.size() / 1024.0,
            provider->config().model.toLocal8Bit().constData(),
            maxTokens);
    fflush(stderr);

    QElapsedTimer clock;
    clock.start();

    QNetworkReply *reply = manager.post(req, payload);

    // One line per second, so the wait is visibly progressing.
    QTimer tick;
    connect(&tick, &QTimer::timeout, [&clock]() {
        fprintf(stderr, "\rinsights: waiting on provider, %.0fs",
                clock.elapsed() / 1000.0);
        fflush(stderr);
    });
    tick.start(1000);

    /*
     * Blocking, matching how codeloader.cpp fetches the package archive. A
     * timer rather than waiting indefinitely: a provider that accepts the
     * connection and then says nothing would otherwise hang the tool, and a
     * local server that is not running is a normal thing to get wrong.
     */
    QEventLoop loop;
    QTimer timer;
    bool timedOut = false;

    timer.setSingleShot(true);
    connect(&timer, &QTimer::timeout, [&]() {
        timedOut = true;
        reply->abort();
        loop.quit();
    });
    connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);

    timer.start(timeoutMs);
    loop.exec();
    timer.stop();
    tick.stop();

    const qint64 elapsedMs = clock.elapsed();

    const QByteArray body = reply->readAll();

    fprintf(stderr, "\rinsights: %.1f kB reply in %.1fs%-20s\n",
            body.size() / 1024.0, elapsedMs / 1000.0, " ");
    fflush(stderr);

    const QNetworkReply::NetworkError netErr = reply->error();
    const int status = reply->attribute(
                QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QString netErrStr = reply->errorString();
    reply->deleteLater();

    if (timedOut) {
        if (err != nullptr) {
            *err = QString("No reply from %1 within %2 ms.")
                    .arg(provider->endpoint()).arg(timeoutMs);
        }
        return QString();
    }

    /*
     * The body is parsed even on an HTTP error, because providers put their
     * real explanation in it -- "model not found", "insufficient quota". The
     * transport error is only reported when the body has nothing to say.
     */
    QString parseErr;
    const QString text = provider->parseReply(body, &parseErr);

    if (!text.isEmpty()) {
        return text;
    }

    if (err != nullptr) {
        if (!parseErr.isEmpty()) {
            *err = status != 0
                    ? QString("%1 (HTTP %2)").arg(parseErr).arg(status)
                    : parseErr;
        } else if (netErr != QNetworkReply::NoError) {
            *err = QString("%1: %2").arg(provider->endpoint(), netErrStr);
        } else {
            *err = "Empty reply.";
        }
    }
    return QString();
}
