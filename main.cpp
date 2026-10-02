/*
    Copyright 2016 - 2023 Benjamin Vedder	benjamin@vedder.se
    Copyright 2026 Stephen Bouche

    This file is part of VESC Tool.

    VESC Tool is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    VESC Tool is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
    */

#include "mainwindow.h"
#include "boardsetupwindow.h"
#include "mobile/qmlui.h"
#include "mobile/fwhelper.h"
#include "mobile/vesc3ditem.h"
#include "mobile/logwriter.h"
#include "mobile/logreader.h"
#include "tcpserversimple.h"
#include "pages/pagemotorcomparison.h"
#include "codeloader.h"
#include "configparam.h"
#include "utility.h"
#include "appstyle.h"
#include "appregister.h"
#include <QAbstractButton>
#include <QElapsedTimer>
#include "heatshrink/heatshrinkif.h"
#include "minimp3/qminimp3.h"

#include <QApplication>
#include <QStyleFactory>
#include <QSettings>
#include "tuninginsights.h"
#include "tuninginsightsconf.h"
#include "insightsprovider.h"
#include "tuningclient.h"
#include <QDesktopWidget>
#include <QFontDatabase>
#include <QPixmapCache>

#include "tcphub.h"

#ifndef HAS_BLUETOOTH
#include "bleuartdummy.h"
#endif

#ifdef Q_OS_IOS
#include "ios/src/setIosParameters.h"
#endif

#ifdef Q_OS_LINUX
#include <signal.h>
#include <systemcommandexecutor.h>
#endif

#ifndef USE_MOBILE

#include <QProxyStyle>
#include <QtConcurrent/QtConcurrent>


static void showHelp()
{
    qDebug() << "Arguments";
    qDebug() << "-h, --help : Show help text";
    qDebug() << "--about : Show about text";
    qDebug() << "--version : Show ESCargot Tool version information on one line";
    qDebug() << "--tcpServer [port] : Connect to VESC and start TCP server on [port]";
    qDebug() << "--loadQml [file] : Load QML UI from file instead of the regular ESCargot Tool UI";
    qDebug() << "--loadQmlVesc : Load QML UI from the connected VESC instead of the regular ESCargot Tool UI";
    qDebug() << "--qmlAutoConn : Connect over USB before loading the QML UI";
    qDebug() << "--qmlFullscreen : Run QML UI in fullscreen mode";
    qDebug() << "--qmlOtherScreen : Run QML UI on other screen";
    qDebug() << "--qmlRotation [deg] : Rotate screen by deg degrees";
    qDebug() << "--qmlWindowSize [width:height] : Specify qml window size";
    qDebug() << "--retryConn : Keep trying to reconnect to the VESC when the connection fails";
    qDebug() << "--useMobileUi : Start the mobile UI instead of the full desktop UI";
    qDebug() << "--tcpHub [port] : Start a TCP hub for remote access to connected VESCs";
    qDebug() << "--buildPkg [pkgPath:lispPath:qmlPath:isFullscreen:optMd:optName] : Build VESC Package";
    qDebug() << "--buildPkgFromDesc [qmlDesc] : Build VESC Package from QML description file";
    qDebug() << "--testPkgDesc [hwtype:hwname:optfwname] : Test isCompatible from package QML description after build";
    qDebug() << "--useBoardSetupWindow : Start board setup window instead of the main UI";
    qDebug() << "--xmlConfToCode [xml-file] : Generate C code from XML configuration file (the files are saved in the same directory as the XML)";
    qDebug() << "--vescPort [port] : VESC Port for commands that connect, e.g. /dev/ttyACM0. If this command is left out autoconnect will be used.";
    qDebug() << "--insightsReasoning [off|low|medium|high|N] : Thinking budget for a reasoning model. A large payload can otherwise consume the whole token cap before any answer. OpenAI-compatible providers only.";
    qDebug() << "--insightsPrompt [text] : Replace the instructions sent with the data. The data itself is always appended, so this cannot drop it.";
    qDebug() << "--insightsPromptFile [path] : The same, read from a file.";
    qDebug() << "--insightsPrintPrompt : Print the built-in instructions and exit, so they can be edited and passed back.";
    qDebug() << "--insightsMaxTokens [n] : Cap the length of the answer (default 2048). The reply length is most of the wait, so this is the knob that shortens it.";
    qDebug() << "--insightsTimeout [ms] : Give up on the provider after this long (default 120000).";
    qDebug() << "--screenshotClick [objectName] : Press this control before capturing, e.g. analyseButton. For documenting a page in a finished state.";
    qDebug() << "--screenshot [file] : Render the window to a PNG and exit. Implies --offscreen unless a platform is already chosen, so no display is needed.";
    qDebug() << "--screenshotSize [WxH] : Window size for --screenshot, e.g. 1280x800. Defaults to the remembered size.";
    qDebug() << "--showPage [name] : Open the window on this page, e.g. \"Tuning Insights\". With --vescTcp it also connects first.";
    qDebug() << "--vescTcp [host:port] : Connect over TCP instead of serial, e.g. 172.31.5.34:65102. Port defaults to 65102.";
    qDebug() << "--vescBaud [rate] : Serial rate for --vescPort, e.g. 921600. Defaults to the rate last connected at.";
    qDebug() << "--canFwd [canId] : Can ID for CAN forwarding";
    qDebug() << "--tuningInsights : Connect, gather configuration and telemetry, and ask a model for tuning observations. Advisory only; nothing is applied.";
    qDebug() << "--dryRun : With --tuningInsights, print the exact payload and send nothing.";
    qDebug() << "--insightsKeyEnv [VAR] : Name of the environment variable holding the key for --insightsProvider, e.g. ANTHROPIC_API_KEY.";
    qDebug() << "--insightsOffline : With --tuningInsights --dryRun, do not connect to a controller. Builds the payload from --insightsLog alone, so a log can be inspected without hardware.";
    qDebug() << "--insightsLog [path] : Include this RT log. Location columns (gnss_*) are never sent.";
    qDebug() << "--insightsProvider [id] : ollama (default), local, anthropic, openai, openrouter, or kind:baseUrl:model where kind is anthropic or openai.";
    qDebug() << "--insightsModel [id] : Override the provider's model.";
    qDebug() << "--insightsMaxRows [n] : Log rows to send after downsampling (default 200).";
    qDebug() << "--getMcConf [confPath] : Connect and read motor configuration and store the XML to confPath.";
    qDebug() << "--setMcConf [confPath] : Connect and write motor configuration XML from confPath.";
    qDebug() << "--getAppConf [confPath] : Connect and read app configuration and store the XML to confPath.";
    qDebug() << "--setAppConf [confPath] : Connect and write app configuration XML from confPath.";
    qDebug() << "--getCustomConf [confPath] : Connect and read custom configuration 1 and store the XML to confPath.";
    qDebug() << "--setCustomConf [confPath] : Connect and write custom configuration 1 XML from confPath.";
    qDebug() << "--debugOutFile [path] : Print debug output to file with path.";
    qDebug() << "--uploadLisp [path] : Upload LispBM script.";
    qDebug() << "--installPkg [path] : Connect and install a VESC Package (.vescpkg).";
    qDebug() << "--reduceLisp : Reduce LispBM file size by removing comments, spaces and imports.";
    qDebug() << "--eraseLisp : Erase LispBM script.";
    qDebug() << "--uploadFirmware [path] : Upload firmware-file from path.";
    qDebug() << "--uploadBootloaderBuiltin : Upload bootloader from generic included bootloaders.";
    qDebug() << "--queryDeviceFwParams : Connect and print out device fw parameters.";
    qDebug() << "--writeFileToSdCard [fileLocal:pathSdcard] : Write file to SD-card.";
    qDebug() << "--listSdCard [path] : List a directory on the SD-card, e.g. /.";
    qDebug() << "--insightsLogFromSd [path] : With --tuningInsights, read the log straight off the SD-card instead of from a local file.";
    qDebug() << "--packFirmware [fileIn:fileOut] : Pack firmware-file for compatibility with the bootloader. ";
    qDebug() << "--packLisp [fileIn:fileOut] : Pack LispBM file and the included imports.";
    qDebug() << "--bridgeAppData : Send app data (such as data from send-data in LispBM) to stdout.";
    qDebug() << "--offscreen : Use offscreen QPA so that X is not required for the CLI-mode.";
    qDebug() << "--downloadPackageArchive : Download package archive to application data directory.";
}

#ifdef Q_OS_LINUX
static void m_cleanup(int sig)
{
    (void)sig;
    qApp->quit();
}
#endif
#endif

QFile m_debug_msg_file;

void myMessageOutput(QtMsgType type, const QMessageLogContext &context, const QString &msg) {
    (void)type;
    (void)context;

    if (m_debug_msg_file.isOpen()) {
        m_debug_msg_file.write(msg.toUtf8());
        m_debug_msg_file.write("\n");
        m_debug_msg_file.flush();
    }
}

int main(int argc, char *argv[])
{
    // Settings
    VtAppStyle::initIdentity();

    QSettings set;
    bool isDark = set.value("darkMode", true).toBool();

    /*
     * The colour table, the fonts and the style live in appstyle.cpp so the
     * test binaries can reproduce the application's rendering. See the note
     * there: without the colour table every Utility::getAppQColor lookup
     * returns red and logs a warning.
     */
    VtAppStyle::initColors(isDark);

    // DPI settings
    // TODO: http://www.qcustomplot.com/index.php/support/forum/1344

    QCoreApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);

    VtApp::registerTypes();
#ifdef USE_MOBILE
#ifndef DEBUG_BUILD
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
#endif
#else
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);

#ifdef Q_OS_LINUX
    signal(SIGINT, m_cleanup);
    signal(SIGTERM, m_cleanup);
#endif

    // Parse command line arguments
    QStringList args;
    for (int i = 0;i < argc;i++) {
        args.append(argv[i]);
    }

    bool useTcp = false;
    bool retryConn = false;
    int tcpPort = 65102;
    QString loadQml = "";
    bool qmlAutoConn = false;
    bool qmlFullscreen = false;
    bool loadQmlVesc = false;
    bool qmlOtherScreen = false;
    bool useMobileUi = false;
    bool useBoardSetupWindow = false;
    double qmlRot = 0.0;
    bool isTcpHub = false;
    QStringList pkgArgs;
    QString pkgDesc = "";
    QStringList pkgDescTests;
    QString xmlCodePath = "";
    QString vescPort = "";
    QString showPageName = "";
    QString screenshotPath = "";
    QString screenshotClick = "";
    QSize screenshotSize;
    QString vescTcpHost = "";
    int vescTcpPort = 65102;
    int vescBaud = 0;
    int canFwd = -1;
    bool tuningInsights = false;
    bool insightsDryRun = false;
    bool insightsOffline = false;
    QString insightsLogPath = "";
    QString insightsProviderSpec = "";
    QString insightsModel = "";
    int insightsMaxRows = 200;
    QString getMcConfPath = "";
    QString setMcConfPath = "";
    QString getAppConfPath = "";
    QString setAppConfPath = "";
    QString getCustomConfPath = "";
    QString setCustomConfPath = "";
    QSize qmlWindowSize = QSize(-1, -1);
    QString lispPath = "";
    QString installPkgPath = "";
    bool reduceLisp = false;
    bool eraseLisp = false;
    QString firmwarePath = "";
    bool uploadBootloaderBuiltin = false;
    bool queryDeviceFwParams = false;
    QString fwPackIn = "";
    QString fwPackOut = "";
    QString fileForSdIn = "";
    QString listSdPath = "";
    QString insightsLogSdPath = "";
    QString insightsKeyEnv = "";
    QString insightsReasoning = "";
    QString insightsPrompt = "";
    bool insightsPrintPrompt = false;
    int insightsMaxTokens = 0;   // 0 = leave the default
    int insightsTimeoutMs = 0;
    QString fileForSdOut = "";
    QString lispPackIn = "";
    QString lispPackOut = "";
    bool bridgeAppData = false;
    bool offscreen = false;
    bool downloadPackageArchive = false;

    // Arguments can be hard-coded in a build like this:
//    qmlWindowSize = QSize(400, 800);
//    loadQmlVesc = true;
//    retryConn = true;

    for (int i = 0;i < args.size();i++) {
        // Skip the program argument
        if (i == 0) {
            continue;
        }

        QString str = args.at(i);

        // Skip path argument
        if (i >= args.size() && args.size() >= 3) {
            break;
        }

        bool dash = str.startsWith("-") && !str.startsWith("--");
        bool found = false;

        if ((dash && str.contains('h')) || str == "--help") {
            showHelp();
            return 0;
        }

        if (str == "--about") {
            qDebug() << Utility::aboutText();
            return 0;
        }

        if (str == "--version") {
            qDebug() << Utility::versionText();
            return 0;
        }

        if (str == "--tcpServer") {
            if ((i + 1) < args.size()) {
                i++;
                tcpPort = args.at(i).toInt();
                useTcp = true;
                found = true;
            }
        }

        if (str == "--retryConn") {
            retryConn = true;
            found = true;
        }

        if (str == "--loadQml") {
            if ((i + 1) < args.size()) {
                i++;
                loadQml = args.at(i);
                found = true;
            } else {
                i++;
                qCritical() << "No path to qml UI file";
                return 1;
            }
        }

        if (str == "--loadQmlVesc") {
            loadQmlVesc = true;
            found = true;
        }

        if (str == "--qmlAutoConn") {
            qmlAutoConn = true;
            found = true;
        }

        if (str == "--qmlFullscreen") {
            qmlFullscreen = true;
            found = true;
        }

        if (str == "--qmlOtherScreen") {
            qmlOtherScreen = true;
            found = true;
        }

        if (str == "--useMobileUi") {
            useMobileUi = true;
            found = true;
        }

        if (str == "--useBoardSetupWindow") {
            useBoardSetupWindow = true;
            found = true;
        }

        if (str.startsWith("-qmljsdebugger")) {
            found = true;
        }

        if (str == "--qmlRotation") {
            if ((i + 1) < args.size()) {
                i++;
                qmlRot = args.at(i).toDouble();
                found = true;
            } else {
                i++;
                qCritical() << "No rotation specified";
                return 1;
            }
        }

        if (str == "--qmlWindowSize") {
            if ((i + 1) < args.size()) {
                i++;
                auto p = args.at(i).split(":");
                if (p.size() == 2) {
                    qmlWindowSize.setWidth(p.at(0).toInt());
                    qmlWindowSize.setHeight(p.at(1).toInt());
                } else {
                    qCritical() << "Invalid size specified";
                    return 1;
                }

                found = true;
            } else {
                i++;
                qCritical() << "No size specified";
                return 1;
            }
        }

        if (str == "--tcpHub") {
            if ((i + 1) < args.size()) {
                i++;
                tcpPort = args.at(i).toInt();
                isTcpHub = true;
                found = true;
            }
        }

        if (str == "--buildPkg") {
            if ((i + 1) < args.size()) {
                i++;
                pkgArgs = args.at(i).split(":");
                found = true;
            }
        }

        if (str == "--buildPkgFromDesc") {
            if ((i + 1) < args.size()) {
                i++;
                pkgDesc = args.at(i);
                found = true;
            } else {
                i++;
                qCritical() << "No path to qml file";
                return 1;
            }
        }

        if (str == "--testPkgDesc") {
            if ((i + 1) < args.size()) {
                i++;
                pkgDescTests.append(args.at(i));
                found = true;
            }
        }

        if (str == "--xmlConfToCode") {
            if ((i + 1) < args.size()) {
                i++;
                xmlCodePath = args.at(i);
                found = true;
            } else {
                i++;
                qCritical() << "No path to xml file";
                return 1;
            }
        }

        if (str == "--vescPort") {
            if ((i + 1) < args.size()) {
                i++;
                vescPort = args.at(i);
                found = true;
            } else {
                i++;
                qCritical() << "No port specified";
                return 1;
            }
        }

        if (str == "--insightsReasoning") {
            if ((i + 1) < args.size()) {
                i++;
                insightsReasoning = args.at(i);
                found = true;
            } else {
                showHelp();
                return 1;
            }
        }

        if (str == "--insightsPrintPrompt") {
            insightsPrintPrompt = true;
            found = true;
        }

        if (str == "--insightsPrompt" || str == "--insightsPromptFile") {
            if ((i + 1) < args.size()) {
                i++;

                if (str == "--insightsPrompt") {
                    insightsPrompt = args.at(i);
                } else {
                    QFile pf(args.at(i));

                    if (!pf.open(QIODevice::ReadOnly | QIODevice::Text)) {
                        qCritical() << "Could not read" << args.at(i);
                        return 1;
                    }

                    insightsPrompt = QString::fromUtf8(pf.readAll());
                }

                if (insightsPrompt.trimmed().isEmpty()) {
                    qCritical() << str << "was empty; leave it out to use the "
                                          "built-in instructions";
                    return 1;
                }

                found = true;
            } else {
                showHelp();
                return 1;
            }
        }

        if (str == "--insightsMaxTokens" || str == "--insightsTimeout") {
            if ((i + 1) < args.size()) {
                i++;
                const int v = args.at(i).toInt();

                if (v <= 0) {
                    qCritical() << str << "wants a positive number";
                    return 1;
                }

                if (str == "--insightsMaxTokens") {
                    insightsMaxTokens = v;
                } else {
                    insightsTimeoutMs = v;
                }

                found = true;
            } else {
                showHelp();
                return 1;
            }
        }

        if (str == "--screenshotClick") {
            if ((i + 1) < args.size()) {
                i++;
                screenshotClick = args.at(i);
                found = true;
            } else {
                showHelp();
                return 1;
            }
        }

        if (str == "--screenshot") {
            if ((i + 1) < args.size()) {
                i++;
                screenshotPath = args.at(i);
                found = true;
            } else {
                showHelp();
                return 1;
            }
        }

        if (str == "--screenshotSize") {
            if ((i + 1) < args.size()) {
                i++;
                const QStringList wh = args.at(i).split("x", QString::SkipEmptyParts);

                if (wh.size() != 2) {
                    qCritical() << "--screenshotSize wants WxH, e.g. 1280x800";
                    return 1;
                }

                screenshotSize = QSize(wh.at(0).toInt(), wh.at(1).toInt());

                if (screenshotSize.width() <= 0 || screenshotSize.height() <= 0) {
                    qCritical() << "--screenshotSize wants positive numbers";
                    return 1;
                }

                found = true;
            } else {
                showHelp();
                return 1;
            }
        }

        if (str == "--showPage") {
            if ((i + 1) < args.size()) {
                i++;
                showPageName = args.at(i);
                found = true;
            } else {
                showHelp();
                return 1;
            }
        }

        if (str == "--vescTcp") {
            if ((i + 1) < args.size()) {
                i++;
                const QString spec = args.at(i);
                const int colon = spec.lastIndexOf(':');

                if (colon > 0) {
                    vescTcpHost = spec.left(colon);
                    vescTcpPort = spec.mid(colon + 1).toInt();
                } else {
                    vescTcpHost = spec;
                }

                if (vescTcpHost.isEmpty() || vescTcpPort <= 0) {
                    showHelp();
                    return 1;
                }

                found = true;
            } else {
                showHelp();
                return 1;
            }
        }

        if (str == "--vescBaud") {
            if ((i + 1) < args.size()) {
                i++;
                bool parsed = false;
                vescBaud = args.at(i).toInt(&parsed);
                if (!parsed || vescBaud <= 0) {
                    qCritical() << "Invalid baud rate:" << args.at(i);
                    return 1;
                }
                found = true;
            } else {
                i++;
                qCritical() << "No baud rate specified";
                return 1;
            }
        }

        if (str == "--canFwd") {
            if ((i + 1) < args.size()) {
                i++;
                canFwd = args.at(i).toInt(),
                found = true;
            } else {
                i++;
                qCritical() << "No can id specified";
                return 1;
            }
        }

        if (str == "--tuningInsights") {
            tuningInsights = true;
            found = true;
        }

        if (str == "--dryRun") {
            insightsDryRun = true;
            found = true;
        }

        if (str == "--insightsKeyEnv") {
            if ((i + 1) < args.size()) {
                i++;
                insightsKeyEnv = args.at(i);
                found = true;
            } else {
                showHelp();
                return 1;
            }
        }

        if (str == "--insightsOffline") {
            insightsOffline = true;
            found = true;
        }

        if (str == "--insightsLog" || str == "--insightsProvider" ||
                str == "--insightsModel" || str == "--insightsMaxRows") {
            if ((i + 1) < args.size()) {
                i++;
                if (str == "--insightsLog") {
                    insightsLogPath = args.at(i);
                } else if (str == "--insightsProvider") {
                    insightsProviderSpec = args.at(i);

                    /*
                     * Checked here rather than where it is used, because the
                     * offline dump never resolves a provider at all -- so a
                     * typo in the name used to be accepted in silence and the
                     * run looked like it had used the provider asked for.
                     */
                    QString provErr;
                    InsightsProvider::fromSpec(insightsProviderSpec, &provErr);

                    if (!provErr.isEmpty()) {
                        qCritical() << provErr.toLocal8Bit().constData();
                        return 1;
                    }
                } else if (str == "--insightsModel") {
                    insightsModel = args.at(i);
                } else {
                    bool ok = false;
                    insightsMaxRows = args.at(i).toInt(&ok);
                    if (!ok || insightsMaxRows < 1) {
                        qCritical() << "--insightsMaxRows needs a positive number";
                        return 1;
                    }
                }
                found = true;
            } else {
                i++;
                qCritical() << "No value specified for" << str;
                return 1;
            }
        }

        if (str == "--getMcConf") {
            if ((i + 1) < args.size()) {
                i++;
                getMcConfPath = args.at(i);
                found = true;
            } else {
                i++;
                qCritical() << "No path specified";
                return 1;
            }
        }

        if (str == "--setMcConf") {
            if ((i + 1) < args.size()) {
                i++;
                setMcConfPath = args.at(i);
                found = true;
            } else {
                i++;
                qCritical() << "No path specified";
                return 1;
            }
        }

        if (str == "--getAppConf") {
            if ((i + 1) < args.size()) {
                i++;
                getAppConfPath = args.at(i);
                found = true;
            } else {
                i++;
                qCritical() << "No path specified";
                return 1;
            }
        }

        if (str == "--setAppConf") {
            if ((i + 1) < args.size()) {
                i++;
                setAppConfPath = args.at(i);
                found = true;
            } else {
                i++;
                qCritical() << "No path specified";
                return 1;
            }
        }

        if (str == "--getCustomConf") {
            if ((i + 1) < args.size()) {
                i++;
                getCustomConfPath = args.at(i);
                found = true;
            } else {
                i++;
                qCritical() << "No path specified";
                return 1;
            }
        }

        if (str == "--setCustomConf") {
            if ((i + 1) < args.size()) {
                i++;
                setCustomConfPath = args.at(i);
                found = true;
            } else {
                i++;
                qCritical() << "No path specified";
                return 1;
            }
        }

        if (str == "--installPkg") {
            if ((i + 1) < args.size()) {
                i++;
                installPkgPath = args.at(i);
                found = true;
            } else {
                i++;
                qCritical() << "No path specified";
                return 1;
            }
        }

        if (str == "--uploadLisp") {
            if ((i + 1) < args.size()) {
                i++;
                lispPath = args.at(i);
                found = true;
            } else {
                i++;
                qCritical() << "No path specified";
                return 1;
            }
        }

        if (str == "--reduceLisp") {
            reduceLisp = true;
            found = true;
        }

        if (str == "--eraseLisp") {
            eraseLisp = true;
            found = true;
        }

        if (str == "--uploadFirmware") {
            if ((i + 1) < args.size()) {
                i++;
                firmwarePath = args.at(i);
                found = true;
            } else {
                i++;
                qCritical() << "No path specified";
                return 1;
            }
        }

        if (str == "--uploadBootloaderBuiltin") {
            uploadBootloaderBuiltin = true;
            found = true;
        }

        if (str == "--queryDeviceFwParams") {
            queryDeviceFwParams = true;
            found = true;
        }

        if (str == "--debugOutFile") {
            if ((i + 1) < args.size()) {
                i++;
                if (!m_debug_msg_file.isOpen()) {
                    m_debug_msg_file.setFileName(args.at(i));
                    if (m_debug_msg_file.open(QIODevice::WriteOnly | QIODevice::Text)) {
                        qInstallMessageHandler(myMessageOutput);
                    }
                }
                found = true;
            } else {
                i++;
                qCritical() << "No path specified";
                return 1;
            }
        }

        if (str == "--listSdCard") {
            if ((i + 1) < args.size()) {
                i++;
                listSdPath = args.at(i);
                found = true;
            } else {
                showHelp();
                return 1;
            }
        }

        if (str == "--insightsLogFromSd") {
            if ((i + 1) < args.size()) {
                i++;
                insightsLogSdPath = args.at(i);
                found = true;
            } else {
                showHelp();
                return 1;
            }
        }

        if (str == "--writeFileToSdCard") {
            if ((i + 1) < args.size()) {
                i++;
                auto p = args.at(i).split(":");
                if (p.size() == 2) {
                    fileForSdIn = p.at(0);
                    fileForSdOut = p.at(1);
                } else {
                    qCritical() << "Invalid paths specified";
                    return 1;
                }

                found = true;
            } else {
                i++;
                qCritical() << "No paths specified";
                return 1;
            }
        }

        if (str == "--packFirmware") {
            if ((i + 1) < args.size()) {
                i++;
                auto p = args.at(i).split(":");
                if (p.size() == 2) {
                    fwPackIn = p.at(0);
                    fwPackOut = p.at(1);
                } else {
                    qCritical() << "Invalid paths specified";
                    return 1;
                }

                found = true;
            } else {
                i++;
                qCritical() << "No paths specified";
                return 1;
            }
        }

        if (str == "--packLisp") {
            if ((i + 1) < args.size()) {
                i++;
                auto p = args.at(i).split(":");
                if (p.size() == 2) {
                    lispPackIn = p.at(0);
                    lispPackOut = p.at(1);
                } else {
                    qCritical() << "Invalid paths specified";
                    return 1;
                }

                found = true;
            } else {
                i++;
                qCritical() << "No paths specified";
                return 1;
            }
        }

        if (str == "--bridgeAppData") {
            bridgeAppData = true;
            found = true;
        }

        if (str == "--offscreen") {
            offscreen = true;
            found = true;
        }

        if (str == "--downloadPackageArchive") {
            downloadPackageArchive = true;
            found = true;
        }

        if (!found) {
            if (dash) {
                qCritical() << "At least one of the flags is invalid:" << str;
            } else {
                qCritical() << "Invalid option:" << str;
            }

            showHelp();
            return 1;
        }
    }

    if (downloadPackageArchive) {
        QCoreApplication appTmp(argc, argv);
        CodeLoader loader;
        qDebug() << "Downloading package archive...";
        loader.downloadPackageArchive();
        qDebug() << "Package archive downloaded!";
    }

    if (!xmlCodePath.isEmpty()) {
        ConfigParams conf;
        if (!conf.loadParamsXml(xmlCodePath)) {
            qCritical() << "Could not parse XML-file" << xmlCodePath;
            return 1;
        }

        QString nameConfig = "device_config";
        if (conf.hasParam("config_name") && conf.getParam("config_name")->type == CFG_T_QSTRING) {
            nameConfig = conf.getParamQString("config_name");
        }

        QFileInfo fi(xmlCodePath);
        xmlCodePath.chop(fi.fileName().length());
        QString pathDefines = xmlCodePath + "conf_default.h";
        QString pathParser = xmlCodePath + "confparser.c";
        QString pathCompressed = xmlCodePath + "confxml.c";

        bool ok = true;
        ok = Utility::createCompressedConfigC(&conf, nameConfig, pathCompressed) && ok;
        ok = Utility::createParamParserC(&conf, nameConfig, pathParser) && ok;
        ok = conf.saveCDefines(pathDefines, true) && ok;

        if (ok) {
            qDebug() << "Done!";
            return 0;
        } else {
            qCritical() << "Errors while generating files.";
            return 2;
        }
    }

    if (!fwPackIn.isEmpty()) {
        if (!fwPackIn.endsWith(".bin", Qt::CaseInsensitive)) {
            qWarning() << "Warning: Unexpected file extension for a firmware-file.";
        }

        QFile fIn(fwPackIn);
        if (!fIn.open(QIODevice::ReadOnly)) {
            qWarning() << QString("Could not open %1 for reading.").arg(fwPackIn);
            return 1;
        }

        QByteArray newFirmware = Utility::removeFirmwareHeader(fIn.readAll());
        fIn.close();

        QFile fOut(fwPackOut);
        if (!fOut.open(QIODevice::WriteOnly)) {
            qWarning() << QString("Could not open %1 for writing.").arg(fwPackOut);
            return 1;
        }

        int szTot = newFirmware.size();

        bool useHeatshrink = false;
        if (szTot > 393208 && szTot < 700000) { // If fw is much larger it is probably for the esp32
            useHeatshrink = true;
            qDebug() << "Firmware is big, using heatshrink compression library";
            int szOld = szTot;
            HeatshrinkIf hs;
            newFirmware = hs.encode(newFirmware);
            szTot = newFirmware.size();
            qDebug() << "New size:" << szTot << "(" << 100.0 * (double)szTot / (double)szOld << "%)";

            if (szTot > 393208) {
                qWarning() << "Firmware too big" <<
                            "The firmware you are trying to upload is too large for the bootloader even after compression.";
                return -1;
            }
        }

        if (szTot > 8000000) {
            qWarning() << "Firmware too big" <<
                        "The firmware you are trying to upload is unreasonably "
                        "large, most likely it is an invalid file";
            return -2;
        }

        quint16 crc = Packet::crc16((const unsigned char*)newFirmware.constData(),
                                    uint32_t(newFirmware.size()));
        VByteArray sizeCrc;
        if (useHeatshrink) {
            uint32_t szShift = 0xCC;
            szShift <<= 24;
            szShift |= szTot;
            sizeCrc.vbAppendUint32(szShift);
        } else {
            sizeCrc.vbAppendUint32(szTot);
        }
        sizeCrc.vbAppendUint16(crc);
        newFirmware.prepend(sizeCrc);
        fOut.write(newFirmware);
        fOut.close();

        qDebug() << "Done!";
        return 0;
    }

    if (!lispPackIn.isEmpty()) {
        if (!lispPackIn.endsWith(".lbm", Qt::CaseInsensitive)
                && !lispPackIn.endsWith(".lisp", Qt::CaseInsensitive)) {
            qWarning() << "Warning: Unexpected file extension for a LispBM file.";
        }

        QFile fIn(lispPackIn);
        if (!fIn.open(QIODevice::ReadOnly)) {
            qWarning() << QString("Could not open %1 for reading.").arg(lispPackIn);
            return 1;
        }

        QFile fOut(lispPackOut);
        if (!fOut.open(QIODevice::WriteOnly)) {
            qWarning() << QString("Could not open %1 for writing.").arg(lispPackOut);
            return 1;
        }

        CodeLoader loader;
        QFileInfo fi(fIn);
        VByteArray vb = loader.lispPackImports(fIn.readAll(), fi.canonicalPath(), reduceLisp);
        fIn.close();

        quint16 crc = Packet::crc16((const unsigned char*)vb.constData(), uint32_t(vb.size()));
        VByteArray data;
        data.vbAppendUint32(vb.size() - 2);
        data.vbAppendUint16(crc);
        data.append(vb);

        fOut.write(data);
        fOut.close();

        qDebug() << "Done!";
        return 0;
    }

    if (!pkgArgs.isEmpty()) {
        if (pkgArgs.size() < 4) {
            qWarning() << "Invalid arguments";
            return 1;
        }

        CodeLoader loader;
        QString pkgPath = pkgArgs.at(0);
        lispPath = pkgArgs.at(1);
        QString qmlPath = pkgArgs.at(2);
        bool isFullscreen = pkgArgs.at(3).toInt();

        QString mdPath;
        QString name;

        VescPackage pkg;

        if (pkgArgs.size() >= 6) {
            mdPath = pkgArgs.at(4);
            name = pkgArgs.at(5);

            QFile f(mdPath);
            if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
                qWarning() << "Could not open markdown file.";
                return 1;
            }

            QString desc = QString::fromUtf8(f.readAll());
            f.close();

            pkg.name = name;
            pkg.description_md = desc;
            pkg.description = Utility::md2html(desc);
        } else {
            QFile f(pkgPath);
            if (!f.open(QIODevice::ReadOnly)) {
                qWarning() << QString("Could not open %1 for reading.").arg(pkgPath);
                return 1;
            }

            pkg = loader.unpackVescPackage(f.readAll());
            f.close();

            qDebug() << "Opened package" << pkg.name;
        }

        if (!lispPath.isEmpty()) {
            QFile f(lispPath);
            if (!f.open(QIODevice::ReadOnly)) {
                qWarning() << "Could not open LispBM file for reading.";
                return 1;
            }

            QFileInfo fi(f);
            pkg.lispData = loader.lispPackImports(f.readAll(), fi.canonicalPath(), reduceLisp);
            // Empty array means an error. Otherwise, CodeLoader.lispPackImports() always returns data.
            if (pkg.lispData.isEmpty()) {
                qWarning() << "Errors when processing LispBM imports.";
                return 1;
            }
            f.close();

            qDebug() << "Read LispBM script done";
        }

        if (!qmlPath.isEmpty()) {
            QFile f(qmlPath);
            if (!f.open(QIODevice::ReadOnly)) {
                qWarning() << "Could not open qml file for reading.";
                return 1;
            }

            pkg.qmlFile = f.readAll();
            pkg.qmlIsFullscreen =isFullscreen;
            f.close();

            qDebug() << "Read qml script done";
        }

        QFile file(pkgPath);
        if (!file.open(QIODevice::WriteOnly)) {
            qWarning() << QString("Could not open %1 for writing.").arg(pkgPath);
            return 1;
        }

        file.write(loader.packVescPackage(pkg));
        file.close();

        const int flashBlockSize = 128 * 1024;

        if (!pkg.qmlFile.isEmpty()) {
            int compressedQmlSize = loader.qmlCompress(pkg.qmlFile).size();
            qDebug().noquote() << QString("Compressed QML size : %1 / %2 bytes (%3%)")
                        .arg(compressedQmlSize).arg(flashBlockSize)
                        .arg(100.0 * compressedQmlSize / flashBlockSize, 0, 'f', 1);
        }

        if (!pkg.lispData.isEmpty()) {
            // Against both script limits rather than the QML block size. See
            // the longer note on the same report in codeloader.cpp.
            int lispSize = pkg.lispData.size();
            const int espMax = 512 * 1024 - 6;
            const int stmMax = 128 * 1024 - 6;
            qDebug().noquote() << QString("Script data size    : %1 bytes "
                                          "(%2% of the ESP32 limit, "
                                          "%3% of the STM32 limit)")
                        .arg(lispSize)
                        .arg(100.0 * lispSize / espMax, 0, 'f', 1)
                        .arg(100.0 * lispSize / stmMax, 0, 'f', 1);

            if (lispSize > stmMax) {
                qDebug().noquote() << QString("  Too large for an STM32 "
                                              "target; ESP32 only.");
            }
            if (lispSize > espMax) {
                qWarning().noquote() << QString("  Larger than any target "
                                                "accepts; this will not "
                                                "install.");
            }
        }

        qDebug() << "Package Saved!";
        return 0;
    }

    double scale = set.value("app_scale_factor", 1.0).toDouble();

#ifdef Q_OS_ANDROID
    scale = 1.0;
#endif

    if (scale > 1.01) {
        qputenv("QT_SCALE_FACTOR", QString::number(scale).toLocal8Bit());
    }
#endif

    QCoreApplication *app;

#ifdef USE_MOBILE
    QApplication *a = new QApplication(argc, argv);
    app = a;

    VtAppStyle::registerFonts();

    QmlUi *qml = new QmlUi;
    qml->startQmlUi();

    // As background running is allowed, make sure to not update the GUI when
    // running in the background.
    QObject::connect(a, &QApplication::applicationStateChanged, [&qml](Qt::ApplicationState state) {
        if(state == Qt::ApplicationHidden) {
            qml->setVisible(false);
        } else {
            qml->setVisible(true);
        }
    });
#else
    VescInterface *vesc = nullptr;
    TcpHub *tcpHub = nullptr;
    MainWindow *w = nullptr;
    BoardSetupWindow *bw = nullptr;
    QmlUi *qmlUi = nullptr;
    QString qmlStr;

    bool serialAutoconnect = vescPort.isEmpty() && vescTcpHost.isEmpty();

    QTimer connTimer;
    connTimer.setInterval(1000);
    QObject::connect(&connTimer, &QTimer::timeout, [&]() {
        if (!vesc->isPortConnected()) {
            if (qmlUi != nullptr) {
                qmlUi->clearQmlCache();

                QTimer::singleShot(10, [&]() {
                    qmlUi->emitReloadCustomGui("qrc:/res/qml/DynamicLoader.qml");
                });
            }

            bool ok = false;
            if (serialAutoconnect) {
                ok = vesc->autoconnect();
            } else {
                ok = vescBaud > 0 ? vesc->connectSerial(vescPort, vescBaud)
                                  : vesc->connectSerial(vescPort);
            }

            if (ok) {
                qDebug() << "Connected";
            } else {
                qDebug() << "Could not connect";

                if (!retryConn) {
                    qApp->quit();
                }
            }
        }
    });

    if (insightsPrintPrompt) {
        // So the default can be seen, edited and handed back with
        // --insightsPromptFile, rather than read out of the source.
        printf("%s", TuningInsights::defaultInstructions()
               .toUtf8().constData());
        return 0;
    }

    if (tuningInsights && insightsOffline) {
        /*
         * No controller, no network: the payload built from a log file alone.
         *
         * This exists so the redaction can be checked against a real log on a
         * machine with no board attached -- which is the only way most people
         * will ever confirm for themselves that their coordinates are not in
         * there. It refuses to run without --dryRun, because an offline run
         * has no configuration and no telemetry in it and is therefore not
         * worth asking a model about.
         */
        if (!insightsDryRun) {
            qCritical() << "--insightsOffline only makes sense with --dryRun:"
                           "without a controller there is no configuration or"
                           "telemetry to analyse.";
            return -1;
        }

        if (insightsLogPath.isEmpty()) {
            qCritical() << "--insightsOffline needs --insightsLog [path].";
            return -1;
        }

        QFile lf(insightsLogPath);
        if (!lf.open(QIODevice::ReadOnly | QIODevice::Text)) {
            qCritical() << "Could not open" << insightsLogPath;
            return -3;
        }

        QStringList logHeader;
        QList<QStringList> logRows;
        QTextStream ts(&lf);

        if (!ts.atEnd()) {
            logHeader = ts.readLine().split(";");
        }
        while (!ts.atEnd()) {
            const QString line = ts.readLine();
            if (!line.trimmed().isEmpty()) {
                logRows.append(line.split(";"));
            }
        }

        fprintf(stderr, "Read %d log rows, %d columns\n",
                logRows.size(), logHeader.size());

        MC_VALUES rtNone;
        const QJsonObject payload = TuningInsights::buildPayload(
                    QList<TuningInsights::ConfigValue>(),
                    QList<TuningInsights::ConfigValue>(),
                    rtNone, logHeader, logRows, insightsMaxRows,
                    "offline, no controller");

        printf("%s\n", QJsonDocument(payload)
               .toJson(QJsonDocument::Indented).constData());
        return 0;
    }

    bool isMcConf = !getMcConfPath.isEmpty() || !setMcConfPath.isEmpty();
    bool isAppConf = !getAppConfPath.isEmpty() || !setAppConfPath.isEmpty();
    bool isCustomConf = !getCustomConfPath.isEmpty() || !setCustomConfPath.isEmpty();

    if (isMcConf || isAppConf || isCustomConf || tuningInsights ||
            !lispPath.isEmpty() ||
            !installPkgPath.isEmpty() ||
            eraseLisp || !firmwarePath.isEmpty() || uploadBootloaderBuiltin ||
            queryDeviceFwParams || !fileForSdIn.isEmpty() || bridgeAppData ||
            !listSdPath.isEmpty()) {
        if (offscreen) {
            qputenv("QT_QPA_PLATFORM", "offscreen");
        }
        app = new QCoreApplication(argc, argv);
        vesc = new VescInterface;
        vesc->setBlockFwSwap(true);
        vesc->setIgnoreCustomConfigs(!isCustomConf);
        vesc->setShowFwUpdateAvailable(false);
        vesc->setIgnoreTestVersion(true);

        vesc->fwConfig()->loadParamsXml(Utility::configPath("fw.xml"));
        Utility::configLoadLatest(vesc);

        if (bridgeAppData) {
            QObject::connect(vesc->commands(), &Commands::customAppDataReceived, [] (QByteArray data) {
                fprintf(stdout, "%s", data.constData());
                fflush(stdout);
            });
        }

        QObject::connect(vesc, &VescInterface::statusMessage, [firmwarePath]
                         (const QString &msg, bool isGood) {
            if (isGood) {
                qDebug() << msg;
            } else {
                // Firmware upload tends to end with a serial port error when jumping to the bootloader, do not print it
                if (firmwarePath.isEmpty() || !msg.startsWith("Serial port error")) {
                    qWarning() << msg;
                }
            }
        });

        QObject::connect(vesc, &VescInterface::messageDialog, []
                         (const QString &title, const QString &msg, bool isGood, bool richText) {
            (void)richText;
            if (isGood) {
                qDebug() << title << ":" << msg;
            } else {
                qWarning() << title << ":" << msg;
            }
        });

        QObject::connect(vesc, &VescInterface::fwUploadStatus, []
                         (const QString &status, double progress, bool isOngoing) {
            (void)status;
            (void)isOngoing;

            static double progress_last = 0.0;
            progress *= 100.0;

            if (progress < progress_last) {
                progress_last = 0.0;
            }

            if (progress > 0.5 && (progress - progress_last) >= 1.0) {
                fprintf(stderr, "%s", QString("\rUpload progress: %1%").arg(floor(progress)).toLatin1().data());
                progress_last = progress;
            }
        });

        QObject::connect(vesc->commands(), &Commands::fileProgress, []
                         (int32_t prog, int32_t tot, double percentage, double bytesPerSec) {
            (void)prog;
            (void)tot;

            fprintf(stderr, "%s", QString("\rUpload progress: %1% (%2 kbps)").
                    arg(floor(percentage)).arg(bytesPerSec / 1024).toLatin1().data());
        });

        QTimer::singleShot(10, [&]() {
            int exitCode = 0;
            bool ok = false;
            if (!vescTcpHost.isEmpty()) {
                /*
                 * connectTcp() is asynchronous and returns nothing, so the
                 * firmware handshake is what says whether it worked. A longer
                 * wait than the serial path gets: this is a network hop to a
                 * board, not a USB enumeration.
                 */
                qDebug() << "Connecting to" << vescTcpHost << vescTcpPort;
                vesc->connectTcp(vescTcpHost, vescTcpPort);

                /*
                 * Polled rather than waited on. connectTcp() is
                 * asynchronous, and a board on the local network answers the
                 * firmware request faster than the next statement runs: the
                 * first version of this waited on fwRxChanged and timed out
                 * every time, having missed an edge that had already
                 * happened. Asking for the state instead cannot lose a race
                 * against it.
                 */
                for (int t = 0; t < 80; t++) {
                    Utility::sleepWithEventLoop(100);

                    if (vesc->isPortConnected() &&
                            vesc->getLastFwRxParams().major > 0) {
                        ok = true;
                        break;
                    }
                }

                if (!ok) {
                    qWarning() << "Could not read firmware version over TCP";
                }
            } else if (serialAutoconnect) {
                ok = vesc->autoconnect();
            } else {
                ok = vescBaud > 0 ? vesc->connectSerial(vescPort, vescBaud)
                                  : vesc->connectSerial(vescPort);
                if (ok) {
                    ok = Utility::waitSignal(vesc, SIGNAL(fwRxChanged(bool, bool)), 1000);
                    if (!ok) {
                        qWarning() << "Could not read firmware version";
                    }
                }
            }

            if (ok) {
                qDebug() << "Connected";
                Utility::sleepWithEventLoop(100);

                if (canFwd >= 0) {
                    vesc->commands()->setSendCan(true, canFwd);
                } else if (!serialAutoconnect) {
                    //Ensure we talk to the USB connected Vesc, if no CAN id was specified and we are not autoconnecting
                    //If autoconnecting then we will use the last CAN id in settings
                    vesc->commands()->setSendCan(false, 0);
                }

                if (vesc->commands()->getSendCan()) {
                    qDebug() << "Sending to CAN ID" << vesc->commands()->getCanSendId();
                }

                CodeLoader loader;
                loader.setVesc(vesc);

                if (eraseLisp) {
                    if (loader.lispErase(16)) {
                        qDebug() << "LispBM erase OK!";
                    } else {
                        qWarning() << "Could not erase LispBM";
                        exitCode = -10;
                    }
                }

                /*
                 * Install a built package, which until now was reachable only
                 * from the GUI. Without it the packaging path -- build,
                 * embed, install, run -- could not be exercised by a script,
                 * so the only way to find out whether a package worked was to
                 * click through it.
                 *
                 * installVescPackage does the unpacking, the erase, the
                 * upload and the QML registration; this is the connection and
                 * the exit code.
                 */
                if (!installPkgPath.isEmpty()) {
                    QFile f(installPkgPath);
                    if (f.open(QIODevice::ReadOnly)) {
                        auto pkgData = f.readAll();
                        f.close();

                        if (loader.installVescPackage(pkgData)) {
                            qDebug() << "Package install OK!";
                        } else {
                            qWarning() << "Could not install package";
                            exitCode = -22;
                        }
                    } else {
                        qCritical() << "Could not open" << installPkgPath;
                        exitCode = -21;
                    }
                }

                if (!lispPath.isEmpty()) {
                    QFile f(lispPath);
                    if (f.open(QIODevice::ReadOnly)) {
                        QFileInfo fi(f);
                        VByteArray lispData = loader.lispPackImports(f.readAll(), fi.canonicalPath(), reduceLisp);
                        f.close();

                        if (!lispData.isEmpty()) {
                            bool ok = loader.lispErase(lispData.size() + 100);
                            if (ok) {
                                ok = loader.lispUpload(lispData);
                            } else {
                                qWarning() << "Could not erase LispBM";
                                exitCode = -10;
                            }
                            if (ok) {
                                qDebug() << "LispBM upload OK!";
                                vesc->commands()->lispSetRunning(1);
                                Utility::sleepWithEventLoop(100);
                            } else {
                                qWarning() << "Could not upload LispBM";
                                exitCode = -11;
                            }
                        } else {
                            qWarning() << "Empty or invalid LispBM file.";
                            exitCode = -12;
                        }
                    } else {
                        qWarning() << "Could not open LispBM file for reading.";
                        exitCode = -13;
                    }
                }

                if (!listSdPath.isEmpty()) {
                    const QVariantList ls =
                            vesc->commands()->fileBlockList(listSdPath);

                    if (ls.isEmpty()) {
                        qWarning() << "Nothing listed at" << listSdPath;
                    }

                    for (const QVariant &v: ls) {
                        const FILE_LIST_ENTRY e = v.value<FILE_LIST_ENTRY>();
                        printf("%s%s\t%d\n", e.name.toLocal8Bit().constData(),
                               e.isDir ? "/" : "", e.size);
                    }
                }

                if (!fileForSdIn.isEmpty()) {
                    QFile f(fileForSdIn);
                    QFileInfo fi(f);
                    if (f.open(QIODevice::ReadOnly)) {
                        QFileInfo fi(f);
                        vesc->commands()->fileBlockMkdir(fileForSdOut);
                        QString target = fileForSdOut + "/" + fi.fileName();
                        if (vesc->commands()->fileBlockWrite(target.replace("//", "/"), f.readAll())) {
                            qDebug() << "Done!";
                        } else {
                            qWarning() << "Could not write file";
                            exitCode = -51;
                        }

                        f.close();
                    } else {
                        qWarning() << "Could not open file for reading.";
                        exitCode = -50;
                    }
                }

                if (isMcConf || isAppConf || isCustomConf || queryDeviceFwParams) {
                    bool res = vesc->customConfigRxDone();
                    if (!res) {
                        res = Utility::waitSignal(vesc, SIGNAL(customConfigLoadDone()), 4000);
                    }

                    if (res) {
                        if (isMcConf) {
                            ConfigParams *p = vesc->mcConfig();
                            vesc->commands()->getMcconf();
                            res = Utility::waitSignal(p, SIGNAL(updated()), 4000);

                            if (res) {
                                if (!setMcConfPath.isEmpty()) {
                                    res = p->loadXml(setMcConfPath, "MCConfiguration");

                                    if (res) {
                                        vesc->commands()->setMcconf(false);
                                        res = Utility::waitSignal(vesc->commands(), SIGNAL(ackReceived(QString)), 4000);

                                        if (res) {
                                            qDebug() << "Wrote XML from" << setMcConfPath;
                                        } else {
                                            qWarning() << "Could not write config";
                                            exitCode = -4;
                                        }
                                    } else {
                                        qWarning() << "Could not load XML from" << setMcConfPath;
                                        exitCode = -3;
                                    }
                                } else {
                                    res = p->saveXml(getMcConfPath, "MCConfiguration");

                                    if (res) {
                                        qDebug() << "Saved XML to" << getMcConfPath;
                                    } else {
                                        qWarning() << "Could not save XML";
                                        exitCode = -3;
                                    }
                                }
                            } else {
                                qWarning() << "Could not load config";
                                exitCode = -2;
                            }
                        }

                        if (isAppConf) {
                            ConfigParams *p = vesc->appConfig();
                            vesc->commands()->getAppConf();
                            res = Utility::waitSignal(p, SIGNAL(updated()), 4000);

                            if (res) {
                                if (!setAppConfPath.isEmpty()) {
                                    res = p->loadXml(setAppConfPath, "APPConfiguration");

                                    if (res) {
                                        vesc->commands()->setAppConf();
                                        res = Utility::waitSignal(vesc->commands(), SIGNAL(ackReceived(QString)), 4000);

                                        if (res) {
                                            qDebug() << "Wrote XML from" << setAppConfPath;
                                        } else {
                                            qWarning() << "Could not write config";
                                            exitCode = -4;
                                        }
                                    } else {
                                        qWarning() << "Could not load XML from" << setAppConfPath;
                                        exitCode = -3;
                                    }
                                } else {
                                    res = p->saveXml(getAppConfPath, "APPConfiguration");

                                    if (res) {
                                        qDebug() << "Saved XML to" << getAppConfPath;
                                    } else {
                                        qWarning() << "Could not save XML";
                                        exitCode = -3;
                                    }
                                }
                            } else {
                                qWarning() << "Could not load config";
                                exitCode = -2;
                            }
                        }

                        if (isCustomConf) {
                            ConfigParams *p = vesc->customConfig(0);
                            vesc->commands()->customConfigGet(0, false);
                            res = Utility::waitSignal(p, SIGNAL(updated()), 4000);

                            if (res) {
                                if (!setCustomConfPath.isEmpty()) {
                                    res = p->loadXml(setCustomConfPath, "CustomConfiguration");

                                    if (res) {
                                        vesc->commands()->customConfigSet(0, p);
                                        res = Utility::waitSignal(vesc->commands(), SIGNAL(ackReceived(QString)), 4000);

                                        if (res) {
                                            qDebug() << "Wrote XML from" << setCustomConfPath;
                                        } else {
                                            qWarning() << "Could not write config";
                                            exitCode = -4;
                                        }
                                    } else {
                                        qWarning() << "Could not load XML from" << setCustomConfPath;
                                        exitCode = -3;
                                    }
                                } else {
                                    res = p->saveXml(getCustomConfPath, "CustomConfiguration");

                                    if (res) {
                                        qDebug() << "Saved XML to" << getCustomConfPath;
                                    } else {
                                        qWarning() << "Could not save XML";
                                        exitCode = -3;
                                    }
                                }
                            } else {
                                qWarning() << "Could not load config";
                                exitCode = -2;
                            }
                        }

                    } else {
                        qWarning() << "Could not load config";
                        exitCode = -1;
                    }
                }

                /*
                 * Its own block, not nested in the configuration-export one
                 * above: this depends on neither a custom-config load nor any
                 * of the --get/--set flags. It was inside that nest at first,
                 * which meant a plain --tuningInsights run did nothing at all
                 * and exited 0.
                 */
                if (tuningInsights) {
                    /*
                     * Fetches its own configurations rather than
                     * borrowing the --getMcConf path, which would then
                     * try to save XML to an empty filename.
                     */
                    ConfigParams *mcp = vesc->mcConfig();
                    ConfigParams *app = vesc->appConfig();

                    /*
                     * Retried, and with a longer wait than the local-serial
                     * paths use. Over TCP and then forwarded across CAN to a
                     * controller, the motor configuration did not arrive
                     * within 4 s on the first attempt against real hardware.
                     * Which one failed is also worth saying: "could not read
                     * the configuration" sent me looking in the wrong place.
                     */
                    bool okMc = false;
                    bool okApp = false;

                    for (int attempt = 0; attempt < 3 && !okMc; attempt++) {
                        vesc->commands()->getMcconf();
                        okMc = Utility::waitSignal(mcp, SIGNAL(updated()), 8000);
                    }

                    for (int attempt = 0; attempt < 3 && !okApp; attempt++) {
                        vesc->commands()->getAppConf();
                        okApp = Utility::waitSignal(app, SIGNAL(updated()), 8000);
                    }

                    if (!okMc || !okApp) {
                        qWarning() << "Could not read"
                                   << (okMc ? "the app configuration"
                                            : (okApp ? "the motor configuration"
                                                     : "either configuration"));
                        exitCode = -2;
                    } else {
                        /*
                         * A telemetry snapshot. getValues() is a
                         * request; the numbers arrive on a signal, so
                         * capture them from it rather than guessing
                         * at a getter.
                         */
                        MC_VALUES rtVals;
                        QObject::connect(vesc->commands(),
                                         &Commands::valuesReceived,
                                         [&rtVals](MC_VALUES v, unsigned int) {
                            rtVals = v;
                        });
                        vesc->commands()->getValues();
                        if (!Utility::waitSignal(
                                    vesc->commands(),
                                    SIGNAL(valuesReceived(MC_VALUES,uint)),
                                    4000)) {
                            qWarning() << "No realtime data; "
                                          "continuing without it";
                        }

                        FW_RX_PARAMS fwp = vesc->getLastFwRxParams();
                        const QString fwStrInsights =
                                QString("V%1.%2 %3 hw:%4")
                                .arg(fwp.major).arg(fwp.minor, 2, 10,
                                                    QLatin1Char('0'))
                                .arg(fwp.fwName, fwp.hw);

                        QStringList logHeader;
                        QList<QStringList> logRows;

                        if (!insightsLogSdPath.isEmpty()) {
                            /*
                             * Straight off the SD-card, which is where the
                             * logs actually are: they are written by the
                             * Express, not by this program, so requiring a
                             * copy on this machine first would be an odd way
                             * to analyse a ride.
                             */
                            /*
                             * The file lives on the device holding the card
                             * -- the Express -- while the configuration came
                             * from a controller across CAN. With forwarding
                             * still on, the read goes to the controller,
                             * which has no card, and fails. So it is turned
                             * off for the transfer and put back afterwards.
                             */
                            vesc->canTmpOverride(false, 0);
                            const QByteArray raw = vesc->commands()->
                                    fileBlockRead(insightsLogSdPath);
                            vesc->canTmpOverrideEnd();

                            if (raw.isEmpty()) {
                                qCritical() << "Could not read"
                                            << insightsLogSdPath
                                            << "from the SD-card";
                                exitCode = -3;
                            } else {
                                const QStringList ls =
                                        QString::fromUtf8(raw).split('\n');

                                for (int li = 0; li < ls.size(); li++) {
                                    const QString line = ls.at(li);

                                    if (line.trimmed().isEmpty()) {
                                        continue;
                                    }

                                    if (logHeader.isEmpty()) {
                                        logHeader = line.split(";");
                                    } else {
                                        logRows.append(line.split(";"));
                                    }
                                }

                                fprintf(stderr, "Read %d log rows, %d columns"
                                        " from the SD-card\n",
                                        logRows.size(), logHeader.size());
                            }
                        } else if (!insightsLogPath.isEmpty()) {
                            QFile lf(insightsLogPath);
                            if (lf.open(QIODevice::ReadOnly | QIODevice::Text)) {
                                QTextStream ts(&lf);
                                if (!ts.atEnd()) {
                                    logHeader = ts.readLine().split(";");
                                }
                                while (!ts.atEnd()) {
                                    const QString line = ts.readLine();
                                    if (!line.trimmed().isEmpty()) {
                                        logRows.append(line.split(";"));
                                    }
                                }
                                qDebug() << "Read" << logRows.size()
                                         << "log rows," << logHeader.size()
                                         << "columns";
                            } else {
                                qCritical() << "Could not open"
                                            << insightsLogPath;
                                exitCode = -3;
                            }
                        }

                        if (exitCode == 0) {
                            QJsonObject payload =
                                    TuningInsights::buildPayload(
                                        TuningInsightsConf::extract(mcp),
                                        TuningInsightsConf::extract(app),
                                        rtVals,
                                        logHeader, logRows,
                                        insightsMaxRows, fwStrInsights);

                            if (insightsDryRun) {
                                // Exactly what would be sent, and
                                // nothing is.
                                printf("%s\n", QJsonDocument(payload)
                                       .toJson(QJsonDocument::Indented)
                                       .constData());
                            } else {
                                QString err;
                                InsightsProvider::Config cfg =
                                        InsightsProvider::fromSpec(
                                            insightsProviderSpec.isEmpty()
                                            ? "ollama"
                                            : insightsProviderSpec, &err);

                                if (!err.isEmpty()) {
                                    qCritical() << err.toLocal8Bit().constData();
                                    exitCode = -4;
                                } else {
                                    if (!insightsModel.isEmpty()) {
                                        cfg.model = insightsModel;
                                    }

                                    if (!insightsKeyEnv.isEmpty()) {
                                        cfg.keyEnvVar = insightsKeyEnv;
                                    }

                                    if (!insightsReasoning.isEmpty()) {
                                        cfg.reasoning = insightsReasoning;
                                    }

                                    QScopedPointer<InsightsProvider> prov(
                                                InsightsProvider::create(cfg));
                                    qDebug() << "Sending to"
                                             << prov->endpoint()
                                             << "model" << cfg.model;

                                    TuningClient client;
                                    const QString text = client.send(
                                                prov.data(),
                                                TuningInsights::buildPrompt(
                                                    payload, insightsPrompt),
                                                insightsMaxTokens > 0
                                                    ? insightsMaxTokens : 4096,
                                                insightsTimeoutMs > 0
                                                    ? insightsTimeoutMs : 120000,
                                                &err);

                                    if (text.isEmpty()) {
                                        qCritical() << err.toLocal8Bit().constData();
                                        exitCode = -5;
                                    } else {
                                        printf("%s\n", text.toLocal8Bit().constData());
                                    }
                                }
                            }
                        }
                    }
                }

                if (queryDeviceFwParams) {
                    FW_RX_PARAMS params;
                    Utility::getFwVersionBlocking(vesc, &params);

                    QString fwStr;
                    QString strUuid = Utility::uuid2Str(params.uuid, true);

                    if (params.major >= 0) {
                        fwStr = QString("FW: V%1.%2").arg(params.major).arg(params.minor, 2, 10, QLatin1Char('0'));
                        if (!params.fwName.isEmpty()) {
                            fwStr += " (" + params.fwName + ")";
                        }

                        if (!params.hw.isEmpty()) {
                            fwStr += ", Hw: " + params.hw;
                        }

                        if (!strUuid.isEmpty()) {
                            fwStr += ", UUID: " + strUuid;
                        }

                        fwStr += ", isTestFw: " + QString::number(params.isTestFw);
                        fwStr += ", hwType: " + params.hwTypeStr();
                        fwStr += ", hwConfCrc: " + QString::number(params.hwConfCrc);
                    }
                    qInfo() << fwStr;
                }

                if (uploadBootloaderBuiltin) {
                    FW_RX_PARAMS params = vesc->getLastFwRxParams();
                    QString path = "";

                    switch (params.hwType) {
                    case HW_TYPE_VESC:
                        path = "://res/bootloaders/generic.bin";
                        break;

                    case HW_TYPE_VESC_BMS:
                        path = "://res/bootloaders_bms/generic.bin";
                        break;

                    case HW_TYPE_CUSTOM_MODULE:
                        QByteArray endEsp;
                        endEsp.append('\0');
                        endEsp.append('\0');
                        endEsp.append('\0');
                        endEsp.append('\0');

                        if (!params.uuid.endsWith(endEsp)) {
                            if (params.hw == "hm1") {
                                path = "://res/bootloaders_bms/generic.bin";
                            } else {
                                path = "://res/bootloaders_custom_module/stm32g431/stm32g431.bin";
                            }
                        }
                        break;
                    }

                    if (!path.isEmpty()) {
                        QFile f(path);
                        if (f.open(QIODevice::ReadOnly)) {
                            auto fwData = f.readAll();
                            qDebug() << "Erasing old bootloader...";
                            if (vesc->fwUpload(fwData, true, false, false)) {
                                fprintf(stderr, "\r\n");
                                qDebug() << "Bootloader upload OK!";
                            } else {
                                qWarning() << "Bootloader upload failed.";
                                exitCode = -20;
                            }
                        } else {
                            qWarning() << "Could not open bootloader file for reading.";
                            exitCode = -21;
                        }
                    } else {
                        qWarning() << "No included bootloader found.";
                        exitCode = -30;
                    }
                }

                if (!firmwarePath.isEmpty()) {
                    QFile f(firmwarePath);
                    if (f.open(QIODevice::ReadOnly)) {
                        auto fwData = f.readAll();
                        qDebug() << "Erasing firmware buffer...";
                        if (vesc->fwUpload(fwData, false, false, false)) {
                            fprintf(stderr, "\r\n");
                            qDebug() << "Firmware upload OK!";
                        } else {
                            qWarning() << "Firmware upload failed.";
                            exitCode = -20;
                        }
                    } else {
                        qWarning() << "Could not open firmware file for reading.";
                        exitCode = -21;
                    }
                }
            } else {
                qWarning() << "Could not connect";
                exitCode = -1;
            }

            if (!bridgeAppData) {
                qApp->exit(exitCode);
            }
        });
    } else if (!pkgDesc.isEmpty()) {
        app = new QCoreApplication(argc, argv);

        QTimer::singleShot(10, [pkgDesc, &pkgDescTests, reduceLisp]() {
            CodeLoader loader;
            VescPackage pkg;

            if (loader.createPackageFromDescription(pkgDesc, &pkg, reduceLisp)) {
                int retCode = 0;

                if (!pkgDescTests.isEmpty()) {
                    qDebug() << "== Running isCompatible with testPkgDesc tests ==";
                }

                foreach (auto str, pkgDescTests) {
                    auto args = str.split(":");
                    // hwtype:hwname:optfwname
                    if (args.size() >= 2) {
                        FW_RX_PARAMS rxp;

                        if (args.at(0).toLower() == "vesc") {
                            rxp.hwType = HW_TYPE_VESC;
                        } else if (args.at(0).toLower() == "vesc bms") {
                            rxp.hwType = HW_TYPE_VESC_BMS;
                        } else {
                            rxp.hwType = HW_TYPE_CUSTOM_MODULE;
                        }

                        rxp.hw = args.at(1);

                        if (args.size() >= 3) {
                            rxp.fwName = args.at(2);
                        }

                        bool runOk = false;
                        bool isCompatibleRes = CodeLoader::shouldShowPackageFromRxp(pkg, rxp, &runOk);

                        if (!runOk) {
                            retCode = 1;
                            break;
                        }

                        qDebug() << str << "=>" <<
                            "HW Type:" << rxp.hwTypeStr() <<
                            "HW Name:" << rxp.hw <<
                            "FW Name:" << rxp.fwName <<
                            "isCompatible() =>" << isCompatibleRes;
                    } else {
                        qWarning() << "Invalid package test" << str;
                    }
                }

                qApp->exit(retCode);
            } else {
                qWarning() << "Could not save package.";
                qApp->exit(1);
            }
        });
    } else if (useTcp) {
        if (offscreen) {
            qputenv("QT_QPA_PLATFORM", "offscreen");
        }
        app = new QCoreApplication(argc, argv);
        vesc = new VescInterface;
        vesc->fwConfig()->loadParamsXml(Utility::configPath("fw.xml"));
        Utility::configLoadLatest(vesc);

        QTimer::singleShot(10, [&]() {
            if (vesc->tcpServerStart(tcpPort)) {
                connTimer.start();
            } else {
                qCritical() << "Could not start TCP server on port" << tcpPort;
                qApp->quit();
            }
        });

        QObject::connect(vesc, &VescInterface::statusMessage, [&](QString msg, bool isGood) {
            if (isGood) {
                qDebug() << msg;
            } else  {
                qWarning() << msg;
            }
        });
    } else if (isTcpHub) {
        if (offscreen) {
            qputenv("QT_QPA_PLATFORM", "offscreen");
        }
        app = new QCoreApplication(argc, argv);
        tcpHub = new TcpHub;
        if (tcpHub->start(tcpPort)) {
            qDebug() << "TcpHub started";
        } else {
            qCritical() << "Could not start TcpHub on port" << tcpPort;
            qApp->quit();
        }
    } else if (downloadPackageArchive) {
        return 0;
    } else {
        /*
         * --offscreen only ever reached the QCoreApplication branches, each of
         * which constructs an application that loads no QPA plugin at all, so
         * the flag did nothing. Here is the one place it means something.
         *
         * A screenshot implies it: rendering the window needs no display, and
         * requiring one is what made capturing a page a six-minute exercise in
         * Xvfb and synthetic mouse clicks.
         */
        if (offscreen || !screenshotPath.isEmpty()) {
            if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM")) {
                qputenv("QT_QPA_PLATFORM", "offscreen");
            }
        }

        /*
         * Without a display there is nothing to press OK with, and a modal
         * dialog's own event loop then blocks everything behind it for good.
         * This is not hypothetical: connecting to a board raises the
         * "firmware update available" box, which wedged a screenshot run
         * until it was killed -- the connection had succeeded and the capture
         * never happened.
         *
         * So in screenshot mode every modal that appears is closed as it
         * arrives. Only in screenshot mode: a person at a window should see
         * their dialogs.
         */
        const bool autoDismissDialogs = !screenshotPath.isEmpty();

        QApplication *a = new QApplication(argc, argv);
        app = a;

        VtAppStyle::registerFonts();

        // Style
        VtAppStyle::applyStyle(a, isDark);

        // Register this to not stop on the import statement when reusing components
        // from the mobile UI. In the mobile UI these are provided as singletons, whereas
        // in the desktop GUI they are provided as context properties.

        if (!loadQml.isEmpty() || loadQmlVesc) {
            vesc = new VescInterface;
            vesc->fwConfig()->loadParamsXml(Utility::configPath("fw.xml"));
            Utility::configLoadLatest(vesc);

            if (loadQmlVesc) {
                QObject::connect(vesc, &VescInterface::qmlLoadDone, [&]() {
                    if (vesc->qmlAppLoaded()) {
                        qmlStr = vesc->qmlApp();
                    } else if (vesc->qmlHwLoaded()) {
                        qmlStr = vesc->qmlHw();
                    } else {
                        qmlStr = "";
                    }

                    qmlUi->clearQmlCache();

                    QTimer::singleShot(10, [&]() {
                        qmlUi->emitReloadCustomGui("qrc:/res/qml/DynamicLoader.qml");
                    });

                    QTimer::singleShot(1000, [&]() {
                        qmlUi->emitReloadQml(qmlStr);
                    });
                });

                connTimer.start();
            } else {
                QFile qmlFile(loadQml);
                if (qmlFile.exists() && qmlFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
                    QFileInfo fi(loadQml);

                    qmlStr = QString::fromUtf8(qmlFile.readAll());
                    qmlFile.close();

                    qmlStr.prepend("import \"qrc:/mobile\";");
                    qmlStr.prepend("import Vedder.vesc.vescinterface 1.0;");
                    qmlStr.prepend("import \"file:/" + fi.canonicalPath() + "\";");

                    QTimer::singleShot(10, [&]() {
                        qmlUi->emitReloadCustomGui("qrc:/res/qml/DynamicLoader.qml");
                    });

                    QTimer::singleShot(1000, [&]() {
                        qmlUi->emitReloadQml(qmlStr);
                        if (qmlAutoConn) {
                            connTimer.start();
                        }
                    });
                } else {
                    qCritical() << "Could not open" << loadQml;
                    delete app;
                    delete vesc;
                    return -1;
                }
            }

            qmlUi = new QmlUi;
            qmlUi->startCustomGui(vesc, "qrc:/res/qml/MainLoader.qml",
                                  qmlWindowSize.width(), qmlWindowSize.height());

            if (qmlFullscreen) {
                qmlUi->emitToggleFullscreen();
            }

            if (qmlOtherScreen) {
                qmlUi->emitMoveToOtherScreen();
            }

            qmlUi->emitRotateScreen(qmlRot);

            QObject::connect(vesc, &VescInterface::statusMessage, [&](QString msg, bool isGood) {
                if (isGood) {
                    qDebug() << msg;
                } else  {
                    qWarning() << msg;
                }
            });
        } else if (useMobileUi) {
            qmlUi = new QmlUi;
            qmlUi->startQmlUi();
        } else if (useBoardSetupWindow){
            bw = new BoardSetupWindow;
            bw->show();
        } else {
            QPixmapCache::setCacheLimit(256000);
            w = new MainWindow;
            w->show();

            if (!screenshotSize.isEmpty()) {
                w->resize(screenshotSize);
            }

            if (autoDismissDialogs) {
                QTimer *dismiss = new QTimer(w);

                QObject::connect(dismiss, &QTimer::timeout, []() {
                    if (QWidget *m = QApplication::activeModalWidget()) {
                        qWarning() << "screenshot: dismissing modal"
                                   << m->windowTitle();
                        m->close();
                    }
                });

                dismiss->start(250);
            }

            if (!vescTcpHost.isEmpty() || !showPageName.isEmpty() ||
                    !screenshotPath.isEmpty()) {
                /*
                 * Connect and deep-link after the window is up, so the pages
                 * that only exist once a board has answered are present
                 * before one of them is asked for.
                 */
                QString host = vescTcpHost;
                int port = vescTcpPort;
                int can = canFwd;
                QString page = showPageName;
                QString shot = screenshotPath;
                QSize shotSize = screenshotSize;
                QString click = screenshotClick;
                QString provSpec = insightsProviderSpec;
                QString provModel = insightsModel;
                QString provKeyEnv = insightsKeyEnv;
                QString provLog = insightsLogPath.isEmpty()
                        ? insightsLogSdPath : insightsLogPath;
                bool provLogOnSd = !insightsLogSdPath.isEmpty();
                int maxTok = insightsMaxTokens;
                int timeoutMs = insightsTimeoutMs;
                int maxRows = insightsMaxRows;
                QString prompt = insightsPrompt;
                QString reasoning = insightsReasoning;
                MainWindow *mw = w;

                QTimer::singleShot(500, [mw, host, port, can, page, shot, click,
                                         provSpec, provModel, provKeyEnv,
                                         provLog, provLogOnSd, maxTok,
                                         timeoutMs, maxRows, prompt,
                                         reasoning, shotSize]() {
                    QElapsedTimer stage;
                    stage.start();

                    if (!host.isEmpty()) {
                        VescInterface *vi = mw->vesc();

                        if (vi) {
                            vi->connectTcp(host, port);

                            for (int t = 0; t < 80; t++) {
                                Utility::sleepWithEventLoop(100);

                                if (vi->isPortConnected() &&
                                        vi->getLastFwRxParams().major > 0) {
                                    break;
                                }
                            }

                            if (can >= 0) {
                                vi->commands()->setSendCan(true, can);
                                Utility::sleepWithEventLoop(500);
                            }

                            fprintf(stderr, "stage: connected in %.1fs\n",
                                    stage.restart() / 1000.0);
                        }
                    }

                    if (!page.isEmpty()) {
                        mw->openPage(page);
                    }

                    /*
                     * The insights flags double as the GUI page's initial
                     * state, so the same options describe a headless run and a
                     * screenshot of the page doing the same thing.
                     */
                    if (!provSpec.isEmpty() || !provModel.isEmpty() ||
                            !provKeyEnv.isEmpty() || !provLog.isEmpty() ||
                            !prompt.isEmpty() || maxRows > 0 ||
                            !reasoning.isEmpty()) {
                        if (auto *ti = mw->findChild<PageTuningInsights*>()) {
                            ti->setLimits(maxTok, timeoutMs, maxRows);
                            ti->setInstructions(prompt);
                            ti->setReasoning(reasoning);
                            ti->applyCliDefaults(provSpec, provModel,
                                                 provKeyEnv, provLog,
                                                 provLogOnSd);
                        }
                    }

                    if (!click.isEmpty()) {
                        /*
                         * Pressed, not synthesised through the window system.
                         * The analysis runs on this thread, so by the time
                         * click() returns the reply is already on the page and
                         * the capture below needs no guess at how long to wait
                         * -- which is what made the previous attempt at this
                         * screenshot a five-minute fixed sleep.
                         */
                        auto *b = mw->findChild<QAbstractButton*>(click);

                        if (b == nullptr) {
                            qCritical() << "No control named" << click;
                            QCoreApplication::exit(-61);
                            return;
                        }

                        if (!b->isEnabled()) {
                            qCritical() << click << "is disabled; nothing to press";
                            QCoreApplication::exit(-62);
                            return;
                        }

                        fprintf(stderr, "stage: pressing %s\n",
                                click.toLocal8Bit().constData());
                        fflush(stderr);

                        b->click();
                        Utility::sleepWithEventLoop(200);

                        fprintf(stderr, "stage: %s finished in %.1fs\n",
                                click.toLocal8Bit().constData(),
                                stage.restart() / 1000.0);
                    }

                    if (!shot.isEmpty()) {
                        /*
                         * Two turns of the event loop after switching pages,
                         * so deferred layout work has run -- several pages
                         * finish building through a zero timer.
                         */
                        Utility::sleepWithEventLoop(300);

                        const QPixmap pm = mw->grab();

                        if (!shotSize.isEmpty() && pm.size() != shotSize) {
                            /*
                             * The window has a minimum size its layouts will
                             * not go below, so --screenshotSize can enlarge
                             * but not shrink past it. Reporting that is better
                             * than handing back a different size in silence.
                             */
                            fprintf(stderr, "screenshot: asked for %dx%d, "
                                    "got %dx%d (the window will not go "
                                    "smaller)\n",
                                    shotSize.width(), shotSize.height(),
                                    pm.width(), pm.height());
                        }

                        if (pm.isNull() || !pm.save(shot, "PNG")) {
                            qCritical() << "Could not write" << shot;
                            QCoreApplication::exit(-60);
                            return;
                        }

                        fprintf(stderr, "stage: captured in %.1fs\n",
                                stage.elapsed() / 1000.0);
                        printf("wrote %s (%dx%d)\n",
                               shot.toLocal8Bit().constData(),
                               pm.width(), pm.height());
                        QCoreApplication::quit();
                    }
                });
            }
        }
    }
#endif
#ifdef Q_OS_IOS
    SetIosParams();
#endif

    int res = app->exec();

#ifdef USE_MOBILE
    delete qml;
#else
    if (vesc) {
        delete vesc;
    }

    if (tcpHub) {
        delete tcpHub;
    }

    if (w) {
        delete w;
    }

    if (qmlUi) {
        delete qmlUi;
    }
#endif

    delete app;

    if (m_debug_msg_file.isOpen()) {
        m_debug_msg_file.close();
    }

    return res;
}
