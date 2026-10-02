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

#ifndef TUNINGCLIENT_H
#define TUNINGCLIENT_H

#include <QObject>
#include <QString>

#include "insightsprovider.h"

/*
 * Sends a prompt to a provider and returns the text, or an error.
 *
 * Knows nothing about which vendor it is talking to; that is the provider's
 * job. Nothing here is specific to any one of them, which is the point.
 */
class TuningClient: public QObject
{
    Q_OBJECT

public:
    explicit TuningClient(QObject *parent = nullptr);

    /*
     * The key for a provider, looked up in this order:
     *
     *   1. the provider's own environment variable
     *   2. ANTHROPIC_AUTH_TOKEN, for the anthropic kind only, because that is
     *      what a local Claude Code setup already exports
     *   3. QSettings, "insights/<id>/key"
     *
     * A provider that needs no key returns empty without reading anything --
     * not an error, and the reason a local model works with nothing
     * configured.
     *
     * The value is never logged, never put in a payload, and never shown in a
     * preview. Callers must not print it either.
     */
    static QString resolveKey(const InsightsProvider::Config &cfg);

    /*
     * Reports why a request cannot be made yet, or empty if it can. Separate
     * from send() so the GUI can disable a button and the CLI can fail before
     * assembling a payload.
     */
    static QString preflight(const InsightsProvider::Config &cfg);

    // Blocking. Returns the text, or empty with err set.
    QString send(InsightsProvider *provider, const QString &prompt,
                 int maxTokens, int timeoutMs, QString *err);

private:
    static QString settingsKeyFor(const InsightsProvider::Config &cfg);
};

#endif // TUNINGCLIENT_H
