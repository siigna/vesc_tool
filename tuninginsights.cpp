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

#include "tuninginsights.h"

#include <QJsonArray>
#include <QJsonDocument>

QString TuningInsights::normalizeColumn(const QString &rawHeaderField)
{
    /*
     * Two log dialects reach this feature, and their header fields are not
     * shaped the same way.
     *
     * ESCargot Tool writes a bare name per field ("input_voltage"), one field
     * per ';'. The package logger on the Express writes a descriptor instead
     * ("Input Voltage:Input Voltage:V:2:0:0") whose first ':'-separated part
     * is the name. Taking the part before the first ':' gives the name in
     * both, since a bare name contains no colon.
     */
    return rawHeaderField.section(':', 0, 0).trimmed();
}

QStringList TuningInsights::allowedLogColumns()
{
    /*
     * The permitted column names, for both log dialects.
     *
     * Written out rather than derived, so adding a column to a log does not
     * silently add it here: a new field has to be put in this list on
     * purpose. That is the whole point of an allowlist, and the test in
     * tests/tuning fails if this becomes a blacklist.
     *
     * Every gnss_* field is absent, the whole family rather than the three
     * that spell out a position. The header ESCargot Tool writes carries
     * eight of them (gnss_posTime, gnss_lat, gnss_lon, gnss_alt, gnss_gVel,
     * gnss_vVel, gnss_hAcc, gnss_vAcc) and the package logger carries five
     * under different names (gnss_h_acc, gnss_h_vel). A list of the ones
     * worth dropping is a list that has to be right twice; the allowlist only
     * has to be right once. Ground speed is already in the payload as
     * speed_meters_per_sec and kmh_vesc.
     */
    static const QStringList allowed = QStringList()
        /* ESCargot Tool's own RT log: 46 of its 61 columns. */
        << "ms_today" << "input_voltage" << "temp_mos_max" << "temp_mos_1"
        << "temp_mos_2" << "temp_mos_3" << "temp_motor" << "current_motor"
        << "current_in" << "d_axis_current" << "q_axis_current" << "erpm"
        << "duty_cycle" << "amp_hours_used" << "amp_hours_charged" << "watt_hours_used"
        << "watt_hours_charged" << "tachometer" << "tachometer_abs" << "encoder_position"
        << "fault_code" << "vesc_id" << "d_axis_voltage" << "q_axis_voltage"
        << "ms_today_setup" << "amp_hours_setup" << "amp_hours_charged_setup" << "watt_hours_setup"
        << "watt_hours_charged_setup" << "battery_level" << "battery_wh_tot" << "current_in_setup"
        << "current_motor_setup" << "speed_meters_per_sec" << "tacho_meters" << "tacho_abs_meters"
        << "num_vescs" << "ms_today_imu" << "roll" << "pitch"
        << "yaw"
        /*
         * Orientation rates, not position: useful for balance and vibration
         * questions, and they say nothing about where the ride happened.
         */
        << "accX" << "accY" << "accZ" << "gyroX" << "gyroY" << "gyroZ"
        /* The six fields this fork added. */
        << "pas_cadence" << "pas_torque_nm" << "pas_rider_watts"
        << "pas_assist_watts" << "pas_output_rel" << "pas_flags"
        /*
         * The package logger's names, which overlap the above only in
         * roll/pitch/yaw. 33 of its 38 columns.
         */
        << "Input Voltage" << "Current" << "Current In" << "Duty"
        << "RPM" << "Temp Fet" << "Temp Motor" << "Batt"
        << "kmh_vesc" << "fault" << "trip_vesc" << "trip_vesc_abs"
        << "cnt_ah" << "cnt_wh" << "cnt_ah_chg" << "cnt_wh_chg"
        << "ADC1" << "ADC2" << "iq" << "id"
        << "vq" << "vd" << "iq-set" << "id-set"
        << "iq-target" << "id-target" << "Fault" << "Power Factor"
        << "t_day" << "t_day_pos"
        ;
    return allowed;
}

QList<int> TuningInsights::sampleIndices(int n, int maxRows)
{
    QList<int> out;

    if (n <= 0 || maxRows <= 0) {
        return out;
    }
    if (n <= maxRows) {
        for (int i = 0; i < n; i++) {
            out.append(i);
        }
        return out;
    }

    /*
     * Evenly spaced, with the first and last rows always kept. The ends of a
     * ride are where the interesting things are -- the first pedal stroke and
     * whatever happened just before it stopped -- and an even stride alone
     * drops the last one more often than not.
     */
    out.append(0);
    if (maxRows > 2) {
        double step = double(n - 1) / double(maxRows - 1);
        for (int i = 1; i < (maxRows - 1); i++) {
            int ix = int(step * double(i) + 0.5);
            if (ix > 0 && ix < (n - 1) && ix != out.last()) {
                out.append(ix);
            }
        }
    }
    out.append(n - 1);
    return out;
}

QJsonObject TuningInsights::configToJson(const QList<ConfigValue> &conf)
{
    QJsonObject out;

    /*
     * Whatever the caller extracted, serialised by type.
     *
     * CFG_T_QSTRING never arrives here: tuninginsightsconf.cpp drops it on
     * extraction, because it is the only type that can hold text somebody
     * typed and none of it helps a tuning question. The switch has no case for
     * it, so adding one would have to be deliberate.
     */
    for (const ConfigValue &p: conf) {
        switch (p.type) {
        case CFG_T_DOUBLE:
            out[p.name] = p.valDouble;
            break;
        case CFG_T_INT:
        case CFG_T_ENUM:
        case CFG_T_BITFIELD:
            out[p.name] = p.valInt;
            break;
        case CFG_T_BOOL:
            out[p.name] = (p.valInt != 0);
            break;
        case CFG_T_QSTRING:
        case CFG_T_UNDEFINED:
        default:
            break;
        }
    }

    return out;
}

QJsonObject TuningInsights::rtToJson(const MC_VALUES &rt)
{
    QJsonObject out;

    out["v_in"] = rt.v_in;
    out["temp_mos"] = rt.temp_mos;
    out["temp_motor"] = rt.temp_motor;
    out["current_motor"] = rt.current_motor;
    out["current_in"] = rt.current_in;
    out["id"] = rt.id;
    out["iq"] = rt.iq;
    out["duty_now"] = rt.duty_now;
    out["rpm"] = rt.rpm;
    out["amp_hours"] = rt.amp_hours;
    out["amp_hours_charged"] = rt.amp_hours_charged;
    out["watt_hours"] = rt.watt_hours;
    out["watt_hours_charged"] = rt.watt_hours_charged;
    out["tachometer"] = rt.tachometer;
    out["tachometer_abs"] = rt.tachometer_abs;
    out["position"] = rt.position;
    out["fault_str"] = rt.fault_str;

    return out;
}

QJsonObject TuningInsights::buildPayload(const QList<ConfigValue> &mcConf,
                                         const QList<ConfigValue> &appConf,
                                         const MC_VALUES &rt,
                                         const QStringList &logHeader,
                                         const QList<QStringList> &logRows,
                                         int maxRows,
                                         const QString &fwInfo)
{
    QJsonObject payload;

    if (!fwInfo.isEmpty()) {
        payload["firmware"] = fwInfo;
    }
    payload["mcconf"] = configToJson(mcConf);
    payload["appconf"] = configToJson(appConf);
    payload["realtime"] = rtToJson(rt);

    if (logHeader.isEmpty() || logRows.isEmpty()) {
        return payload;
    }

    // Which header positions survive the allowlist.
    const QStringList allowed = allowedLogColumns();
    QList<int> keep;
    QJsonArray outHeader;
    for (int i = 0; i < logHeader.size(); i++) {
        const QString name = normalizeColumn(logHeader.at(i));

        if (allowed.contains(name)) {
            keep.append(i);
            /*
             * The normalized name, not the raw field: the package logger's
             * descriptor would otherwise carry its label and unit along, and
             * the two dialects would not look alike to whatever reads this.
             */
            outHeader.append(name);
        }
    }

    if (keep.isEmpty()) {
        return payload;
    }

    QJsonArray outRows;
    for (int ix: sampleIndices(logRows.size(), maxRows)) {
        const QStringList &row = logRows.at(ix);
        QJsonArray outRow;

        for (int col: keep) {
            outRow.append(col < row.size() ? row.at(col) : QString());
        }
        outRows.append(outRow);
    }

    QJsonObject log;
    log["columns"] = outHeader;
    log["rows"] = outRows;
    log["rows_total"] = logRows.size();
    log["rows_sent"] = outRows.size();
    log["columns_dropped"] = logHeader.size() - keep.size();
    payload["log"] = log;

    return payload;
}

QString TuningInsights::buildPrompt(const QJsonObject &payload)
{
    QString out;

    out += "You are reviewing the configuration and telemetry of a brushless "
           "motor controller running ESCargot firmware, a fork of the VESC(R) "
           "firmware.\n\n";
    out += "Give concrete tuning observations. For each one, name the "
           "parameter, say what the current value is, what you would try, and "
           "why the data supports it. Say plainly when the data does not "
           "support a conclusion rather than guessing.\n\n";
    out += "Do not assume a vehicle type, rider weight or intended use that "
           "is not in the data. The person reading this will make the change "
           "by hand, so a wrong number is their problem to catch -- flag "
           "anything that could damage hardware or be unsafe to ride.\n\n";
    out += "Note the log has had its location columns removed before being "
           "sent, so do not ask about route, terrain or elevation.\n\n";
    out += "Data:\n";
    out += QString::fromUtf8(QJsonDocument(payload).toJson(QJsonDocument::Indented));

    return out;
}
