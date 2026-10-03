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

/*
 * Ride logging, end to end, with no controller and no user interface.
 *
 * This replaces what was being done by hand: installing the app on an
 * emulator and tapping through a wizard, three dialogs, a menu and a scroll
 * to reach the logging controls. That found real bugs, but it is not a test
 * -- it cannot run twice the same way, and it cannot run in CI.
 *
 * What it covers is the path the Android storage access framework work added,
 * minus the framework: VescInterface::openRtLogFileFd takes a descriptor
 * someone else opened, writes the CSV header, and appends a row per set of
 * values received. On Android that descriptor comes from a document the
 * system created in a folder the user granted; here it comes from a
 * temporary file, and everything after it is identical code.
 *
 * The values are real. They come from bldc's own protocol implementation
 * through tests/vescsim, so the framing, the CRC and the field order are the
 * firmware's rather than this test's idea of them.
 *
 * What this cannot cover, and nothing off a device can: the picker itself,
 * Utils.createLogFile, and whether a grant survives a reboot.
 */

#include <QtTest>
#include <QProcess>
#include <QTemporaryDir>
#include <QFile>
#include <QSignalSpy>

// dup: the descriptor is handed over, so the test keeps its own.
#include <unistd.h>

#include "vescinterface.h"
#include "commands.h"

class RtLogTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();

    void headerIsWritten();
    void rowsAppendWhileValuesArrive();
    void closingLeavesTheFileReadable();
    void aBadDescriptorIsRefused();

private:
    bool connectToSim(VescInterface *vesc);

    QProcess mSim;
    int mPort = 0;
    QTemporaryDir mDir;
};

/*
 * The simulator is a sibling repository's build artefact, so its absence is a
 * skip rather than a failure -- but a loud one. A silent skip is how the
 * OpenGL tier of the widget suite went unrun for the life of that suite.
 */
void RtLogTest::initTestCase()
{
    QVERIFY2(mDir.isValid(), "could not make a temporary directory");

    QString sim = qEnvironmentVariable("VT_VESCSIM");

    if (sim.isEmpty()) {
        // Where a checkout of the firmware sits next to this one.
        sim = QCoreApplication::applicationDirPath() +
                "/../../../bldc/tests/vescsim/vescsim";
    }

    if (!QFile::exists(sim)) {
        QSKIP(qPrintable(QString(
            "no vescsim at %1. Build it with `make -C tests/vescsim` in the "
            "firmware tree, or set VT_VESCSIM.").arg(sim)));
    }

    // A port unlikely to collide with a real bridge or another run.
    mPort = 65390;

    mSim.setProgram(sim);
    mSim.setArguments({"-p", QString::number(mPort)});
    mSim.start();

    QVERIFY2(mSim.waitForStarted(5000), "vescsim did not start");

    // It prints a line once it is listening; waiting for that rather than
    // sleeping, so a slow machine does not produce a flaky connect.
    QVERIFY2(mSim.waitForReadyRead(5000), "vescsim never said it was listening");
    QByteArray hello = mSim.readAllStandardOutput();
    QVERIFY2(hello.contains("listening"),
             qPrintable(QString("unexpected vescsim output: %1")
                        .arg(QString::fromUtf8(hello))));
}

void RtLogTest::cleanupTestCase()
{
    if (mSim.state() != QProcess::NotRunning) {
        mSim.terminate();
        if (!mSim.waitForFinished(3000)) {
            mSim.kill();
        }
    }
}

bool RtLogTest::connectToSim(VescInterface *vesc)
{
    // The same suppressions the headless paths use, so nothing reaches the
    // network or swaps firmware.
    vesc->setBlockFwSwap(true);
    vesc->setIgnoreCustomConfigs(true);
    vesc->setShowFwUpdateAvailable(false);
    vesc->setIgnoreTestVersion(true);

    QSignalSpy fw(vesc, &VescInterface::fwRxChanged);
    vesc->connectTcp("127.0.0.1", mPort);

    // fwRxChanged is what the application itself treats as connected.
    return fw.wait(8000);
}

void RtLogTest::headerIsWritten()
{
    VescInterface vesc;
    QVERIFY2(connectToSim(&vesc), "did not connect to vescsim");

    QString path = mDir.filePath("header.csv");
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly));

    // The descriptor is handed over, exactly as the framework path hands over
    // one from a document the system created. dup, because QFile closes its
    // own on destruction and openRtLogFileFd takes ownership of what it gets.
    int fd = dup(f.handle());
    QVERIFY(fd >= 0);
    f.close();

    QVERIFY2(vesc.openRtLogFileFd(fd, "header.csv"),
             "openRtLogFileFd refused a good descriptor");
    QVERIFY(vesc.isRtLogOpen());

    // The display name, because an fd-backed QFile has no file name and
    // rtLogFilePath had nothing to report before mRtLogName existed.
    QCOMPARE(vesc.rtLogFilePath(), QString("header.csv"));

    vesc.closeRtLogFile();

    QVERIFY(f.open(QIODevice::ReadOnly));
    QString head = QString::fromUtf8(f.readLine());
    f.close();

    // Spot checks rather than the whole header: this asserts that the header
    // is the real one, not that the column list never changes.
    QVERIFY2(head.startsWith("ms_today"),
             qPrintable(QString("header starts with %1").arg(head.left(40))));
    QVERIFY(head.contains("gnss_lat"));
    QVERIFY(head.contains("pas_cadence"));
}

void RtLogTest::rowsAppendWhileValuesArrive()
{
    VescInterface vesc;
    QVERIFY2(connectToSim(&vesc), "did not connect to vescsim");

    QString path = mDir.filePath("rows.csv");
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly));
    int fd = dup(f.handle());
    QVERIFY(fd >= 0);
    f.close();

    QVERIFY(vesc.openRtLogFileFd(fd, "rows.csv"));

    /*
     * Rows are appended from Commands::valuesReceived, so the values have to
     * be asked for. The simulator moves them on every request, which is what
     * makes a stuck logger distinguishable from a working one.
     */
    for (int i = 0; i < 8; i++) {
        vesc.commands()->getValues();
        QTest::qWait(120);
    }

    vesc.closeRtLogFile();

    QVERIFY(f.open(QIODevice::ReadOnly));
    QStringList lines = QString::fromUtf8(f.readAll()).split("\n",
                                                             Qt::SkipEmptyParts);
    f.close();

    QVERIFY2(lines.size() >= 3,
             qPrintable(QString("expected a header and rows, got %1 line(s)")
                        .arg(lines.size())));

    /*
     * A value column has to change, not just the row.
     *
     * The first version of this compared whole rows and required two
     * distinct ones, which is vacuous: every row begins with ms_today, so
     * rows always differ by their timestamp no matter what the data does.
     * Mutation-tested by making the simulator report constant values -- a
     * stuck controller -- and the assertion passed, which is how it was
     * caught. It was testing the clock.
     *
     * duty_cycle is read out of the header by name rather than by a fixed
     * index, because the column list has changed before and will again.
     */
    QStringList header = lines.at(0).split(';');
    int col = header.indexOf("duty_cycle");

    QVERIFY2(col >= 0,
             qPrintable(QString("no duty_cycle column in the header: %1")
                        .arg(lines.at(0).left(120))));

    QSet<QString> values;
    for (int i = 1; i < lines.size(); i++) {
        QStringList fields = lines.at(i).split(';');
        QVERIFY2(col < fields.size(),
                 qPrintable(QString("row %1 has %2 fields, needed %3")
                            .arg(i).arg(fields.size()).arg(col + 1)));
        values.insert(fields.at(col));
    }

    QVERIFY2(values.size() >= 2,
             qPrintable(QString("%1 row(s) but duty_cycle took only %2 value(s)"
                                " (%3); the values are not reaching the log")
                        .arg(lines.size() - 1).arg(values.size())
                        .arg(QStringList(values.values()).join(","))));

    // The column count has to match the header, or the file is unparseable by
    // the desktop log analysis page whatever else is right.
    int cols = lines.at(0).count(';');
    for (int i = 1; i < lines.size(); i++) {
        QCOMPARE(lines.at(i).count(';'), cols);
    }
}

void RtLogTest::closingLeavesTheFileReadable()
{
    VescInterface vesc;
    QVERIFY2(connectToSim(&vesc), "did not connect to vescsim");

    QString path = mDir.filePath("closed.csv");
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly));
    int fd = dup(f.handle());
    QVERIFY(fd >= 0);
    f.close();

    QVERIFY(vesc.openRtLogFileFd(fd, "closed.csv"));
    vesc.commands()->getValues();
    QTest::qWait(200);

    QVERIFY(vesc.isRtLogOpen());
    vesc.closeRtLogFile();
    QVERIFY2(!vesc.isRtLogOpen(), "the log is still open after closing it");

    // AutoCloseHandle means the descriptor went with the QFile. Writing
    // through the original fd number afterwards must not corrupt the file,
    // and the file must still be complete.
    QVERIFY(f.open(QIODevice::ReadOnly));
    QByteArray all = f.readAll();
    f.close();

    QVERIFY2(all.endsWith("\n"),
             "the log does not end with a newline; the last row was truncated");
}

void RtLogTest::aBadDescriptorIsRefused()
{
    VescInterface vesc;

    // No connection needed: this is about the descriptor, and it is the path
    // taken when the framework hands back a failure.
    QVERIFY2(!vesc.openRtLogFileFd(-1, "nope.csv"),
             "a negative descriptor was accepted");
    QVERIFY2(!vesc.isRtLogOpen(),
             "the log reports open after a refused descriptor");
}

QTEST_MAIN(RtLogTest)

#include "tst_rtlog.moc"
