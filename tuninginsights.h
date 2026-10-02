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

#ifndef TUNINGINSIGHTS_H
#define TUNINGINSIGHTS_H

#include <QJsonObject>
#include <QList>
#include <QStringList>

#include "datatypes.h"

/*
 * Builds what gets sent for a tuning analysis, and nothing else.
 *
 * No network, no file access, no QApplication, and no ConfigParams. That is
 * deliberate: the decision about which fields leave the machine is the part of
 * this feature worth testing, and keeping it free of I/O is what makes it
 * testable without a board, a display or a provider.
 *
 * ConfigParams is deliberately not reachable from here. It pulls in the
 * widget editors and from there QtQuick, which would drag the whole
 * application into the test binary. Callers convert their configuration to
 * ConfigValue first -- see tuninginsightsconf.h, which is the only place that
 * touches both.
 */
class TuningInsights
{
public:
    /*
     * One configuration parameter, flattened. Enough to serialise, and
     * nothing that reaches a widget.
     */
    struct ConfigValue {
        QString name;

        /*
         * The human label and unit from the shipped parameter XML, so the
         * reader does not have to guess what foc_sl_erpm or l_abs_current_max
         * mean. Both are static metadata from the configuration definition,
         * not anything a user typed -- the rule that free text cannot reach a
         * payload still holds.
         */
        QString label;
        QString unit;

        CFG_T type = CFG_T_UNDEFINED;
        double valDouble = 0.0;
        int valInt = 0;

        /*
         * Note there is no string member, and that is the point rather than
         * an omission. CFG_T_QSTRING is the only configuration type that can
         * hold text somebody typed, so free text cannot reach a payload even
         * if a future case label tries -- it is a compile error, not a
         * filtered value.
         */
    };

    /*
     * Log columns permitted in a payload, as an ALLOWLIST.
     *
     * Not a blacklist of gnss_*. The RT log header has 50 columns and this
     * fork added six of them, so the next person to add a column is likelier
     * than not. An allowlist leaves anything new out until somebody decides
     * otherwise; a blacklist would ship it.
     *
     * gnss_lat, gnss_lon and gnss_alt are absent by that rule rather than by
     * a special case. A ride log with coordinates is a record of where
     * somebody was and when.
     */
    /*
     * The column name from one header field, for either log dialect. See the
     * definition: the Tool writes bare names, the package logger writes
     * "name:label:unit:..." descriptors.
     */
    static QString normalizeColumn(const QString &rawHeaderField);

    static QStringList allowedLogColumns();

    /*
     * The payload: configuration, a telemetry snapshot, and a log window.
     *
     * logRows are raw CSV rows matching logHeader. Columns outside the
     * allowlist are dropped, and rows are downsampled to maxRows keeping the
     * first and the last, so a long ride does not become an enormous request
     * and the start and end of it still appear.
     */
    static QJsonObject buildPayload(const QList<ConfigValue> &mcConf,
                                    const QList<ConfigValue> &appConf,
                                    const MC_VALUES &rt,
                                    const QStringList &logHeader,
                                    const QList<QStringList> &logRows,
                                    int maxRows,
                                    const QString &fwInfo);

    // The instruction sent with the payload.
    /*
     * The built-in instruction text, so the GUI can show it for editing and a
     * caller can tell whether it has been changed.
     */
    static QString defaultInstructions();

    /*
     * The instructions followed by the payload. An empty `instructions` uses
     * defaultInstructions(). The payload is always appended here, so replacing
     * the instructions cannot drop the data or substitute different data.
     */
    static QString buildPrompt(const QJsonObject &payload,
                               const QString &instructions = QString());

    // Row indices kept when downsampling n rows to at most maxRows.
    static QList<int> sampleIndices(int n, int maxRows);

private:
    static QJsonObject configToJson(const QList<ConfigValue> &conf);

    /*
     * name -> "Label (unit)" for the fields that have one, sent beside the
     * values so a parameter name does not have to be interpreted from the
     * identifier alone.
     */
    static QJsonObject configNotes(const QList<ConfigValue> &conf);
    static QJsonObject rtToJson(const MC_VALUES &rt);
};

#endif // TUNINGINSIGHTS_H
