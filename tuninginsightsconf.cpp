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

#include "tuninginsightsconf.h"

namespace TuningInsightsConf {

QList<TuningInsights::ConfigValue> extract(ConfigParams *conf)
{
    QList<TuningInsights::ConfigValue> out;

    if (conf == nullptr) {
        return out;
    }

    for (const QString &name: conf->getParamOrder()) {
        ConfigParam *p = conf->getParam(name);

        if (p == nullptr || p->type == CFG_T_QSTRING
                || p->type == CFG_T_UNDEFINED) {
            continue;
        }

        TuningInsights::ConfigValue v;
        v.name = name;
        v.type = p->type;
        v.valDouble = p->valDouble;
        v.valInt = p->valInt;
        out.append(v);
    }

    return out;
}

}
