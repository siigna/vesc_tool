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

#include "appregister.h"

#include <QtQml>

#include "bleuart.h"
#include "bleuartdummy.h"
#include "codeloader.h"
#include "commands.h"
#include "configparams.h"
#include "esp32/esp32flash.h"
#include "mobile/fwhelper.h"
#include "mobile/logreader.h"
#include "mobile/logwriter.h"
#include "minimp3/qminimp3.h"
#include "systemcommandexecutor.h"
#include "tcphub.h"
#include "tcpserversimple.h"
#include "udpserversimple.h"
#include "utility.h"
#include "mobile/vesc3ditem.h"
#include "vescinterface.h"
#include "pages/pagemotorcomparison.h"   // MotorData

void VtApp::registerTypes()
{
#ifdef HAS_BLUETOOTH
    qmlRegisterType<BleUart>("Vedder.vesc.bleuart", 1, 0, "BleUart");
#else
    qmlRegisterType<BleUartDummy>("Vedder.vesc.bleuart", 1, 0, "BleUart");
#endif
    qmlRegisterType<Commands>("Vedder.vesc.commands", 1, 0, "Commands");
    qmlRegisterType<ConfigParams>("Vedder.vesc.configparams", 1, 0, "ConfigParams");
    qmlRegisterType<FwHelper>("Vedder.vesc.fwhelper", 1, 0, "FwHelper");
    qmlRegisterType<Esp32Flash>("Vedder.vesc.esp32flash", 1, 0, "Esp32Flash");
    qmlRegisterType<TcpServerSimple>("Vedder.vesc.tcpserversimple", 1, 0, "TcpServerSimple");
    qmlRegisterType<UdpServerSimple>("Vedder.vesc.udpserversimple", 1, 0, "UdpServerSimple");
    qmlRegisterType<Vesc3dItem>("Vedder.vesc.vesc3ditem", 1, 0, "Vesc3dItem");
    qmlRegisterType<LogWriter>("Vedder.vesc.logwriter", 1, 0, "LogWriter");
    qmlRegisterType<LogReader>("Vedder.vesc.logreader", 1, 0, "LogReader");
    qmlRegisterType<TcpHub>("Vedder.vesc.tcphub", 1, 0, "TcpHub");
    qmlRegisterType<CodeLoader>("Vedder.vesc.codeloader", 1, 0, "CodeLoader");
    qmlRegisterType<QMiniMp3>("Vedder.vesc.qminimp3", 1, 0, "QMiniMp3");
#ifdef Q_OS_LINUX
    qmlRegisterType<SystemCommandExecutor>("Vedder.vesc.syscmd", 1, 0, "SysCmd");
#endif

    qRegisterMetaType<VSerialInfo_t>();
    qRegisterMetaType<MCCONF_TEMP>();
    qRegisterMetaType<MC_VALUES>();
    qRegisterMetaType<BMS_VALUES>();
    qRegisterMetaType<FW_RX_PARAMS>();
    qRegisterMetaType<PSW_STATUS>();
    qRegisterMetaType<IO_BOARD_VALUES>();
    qRegisterMetaType<MotorData>();
    qRegisterMetaType<ENCODER_DETECT_RES>();
    qRegisterMetaType<FILE_LIST_ENTRY>();
    qRegisterMetaType<VescPackage>();
    qRegisterMetaType<TCP_HUB_DEVICE>();
    qRegisterMetaType<ConfigParam>();
    qRegisterMetaType<GNSS_DATA>();
    qRegisterMetaType<MiniMp3Dec>();

    /*
     * These two were registered only on the GUI branch, which is why the test
     * binary saw "module Vedder.vesc.utility is not installed" even after
     * every Qt QML import path was set.
     */
    qmlRegisterType<VescInterface>("Vedder.vesc.vescinterface", 1, 0, "VescIf2");
    qmlRegisterType<Utility>("Vedder.vesc.utility", 1, 0, "Utility2");
}
