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

#ifndef TUNINGINSIGHTSCONF_H
#define TUNINGINSIGHTSCONF_H

#include "configparams.h"
#include "tuninginsights.h"

/*
 * The one place that knows about both ConfigParams and the payload.
 *
 * Kept apart from tuninginsights.cpp on purpose: configparams.h reaches the
 * widget editors and from there QtQuick, so anything including it drags the
 * application into whatever links it. The payload layer stays free of that,
 * and the tests link it without a display.
 */
namespace TuningInsightsConf {

/*
 * Flattens a configuration, in getParamOrder() order.
 *
 * Enumerated rather than listed, so a parameter added upstream is included
 * without anyone remembering to -- the opposite of the log allowlist, because
 * a configuration parameter is a number somebody set in this tool, not
 * something recorded about where they went.
 *
 * CFG_T_QSTRING is dropped here. It is the only type that can hold free text.
 */
QList<TuningInsights::ConfigValue> extract(ConfigParams *conf);

}

#endif // TUNINGINSIGHTSCONF_H
