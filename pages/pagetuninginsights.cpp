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

#include "pagetuninginsights.h"
#include "ui_pagetuninginsights.h"

#include <QApplication>
#include <QClipboard>
#include <QFile>
#include <QFileDialog>
#include <QRegularExpression>
#include <QJsonArray>
#include <QJsonDocument>
#include <QScopedPointer>
#include <QSettings>
#include <QTextStream>

#include "tuninginsights.h"
#include "tuninginsightsconf.h"
#include "tuningclient.h"
#include "utility.h"

PageTuningInsights::PageTuningInsights(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::PageTuningInsights)
{
    ui->setupUi(this);
    layout()->setContentsMargins(0, 0, 0, 0);
    mVesc = nullptr;

    for (const InsightsProvider::Config &c: InsightsProvider::presets()) {
        ui->providerBox->addItem(c.id, c.id);
    }
    ui->providerBox->addItem(tr("Custom"), "");

    /*
     * Default to no thinking budget being sent at all, which leaves the
     * request exactly as it was before this control existed -- local servers
     * and plain OpenAI do not expect the field.
     */
    ui->reasoningBox->addItem(tr("Provider default"), QString());
    ui->reasoningBox->addItem(tr("Off"), QString("off"));
    ui->reasoningBox->addItem(tr("Low"), QString("low"));
    ui->reasoningBox->addItem(tr("Medium"), QString("medium"));
    ui->reasoningBox->addItem(tr("High"), QString("high"));

    ui->kindBox->addItem("openai", int(InsightsProvider::KindOpenAiCompatible));
    ui->kindBox->addItem("anthropic", int(InsightsProvider::KindAnthropic));

    /*
     * The preset that needs no key and no network is the one selected on
     * arrival. Nothing here sends anything by itself, but the default should
     * still be the choice that keeps the data on this machine.
     */
    const int ollamaIx = ui->providerBox->findData(QString("ollama"));
    ui->providerBox->setCurrentIndex(ollamaIx >= 0 ? ollamaIx : 0);

    connect(ui->modelEdit, &QLineEdit::textChanged,
            this, [this]() { updateEndpointLabel(); });
    connect(ui->baseUrlEdit, &QLineEdit::textChanged,
            this, [this]() { updateEndpointLabel(); });
    connect(ui->keyEnvEdit, &QLineEdit::textChanged,
            this, [this]() { updateEndpointLabel(); });
    connect(ui->kindBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this]() { updateEndpointLabel(); });

    /*
     * Shown, not hidden. The box starts holding the real default text so it
     * can be read and edited; unticked, that text is what gets used.
     */
    ui->promptEdit->setPlainText(TuningInsights::defaultInstructions());

    updateEndpointLabel();
}

PageTuningInsights::~PageTuningInsights()
{
    delete ui;
}

VescInterface *PageTuningInsights::vesc() const
{
    return mVesc;
}

void PageTuningInsights::setVesc(VescInterface *vesc)
{
    mVesc = vesc;

    if (mVesc) {
        /*
         * The buttons need a controller as much as they need a provider: the
         * configuration and the telemetry both come off the board. Without
         * one there is nothing to analyse, so say so on the button rather
         * than on a failed attempt.
         */
        connect(mVesc, &VescInterface::portConnectedChanged,
                this, [this]() { updateEndpointLabel(); });
    }

    updateEndpointLabel();
}

QString PageTuningInsights::instructions() const
{
    if (ui->promptGroup->isChecked()) {
        return ui->promptEdit->toPlainText();
    }

    return QString();   // buildPrompt substitutes the built-in text
}

void PageTuningInsights::setInstructions(const QString &text)
{
    if (text.trimmed().isEmpty()) {
        ui->promptEdit->setPlainText(TuningInsights::defaultInstructions());
        ui->promptGroup->setChecked(false);
        return;
    }

    ui->promptEdit->setPlainText(text);
    ui->promptGroup->setChecked(true);
}

void PageTuningInsights::on_promptResetButton_clicked()
{
    ui->promptEdit->setPlainText(TuningInsights::defaultInstructions());
}

void PageTuningInsights::setReasoning(const QString &mode)
{
    const int ix = ui->reasoningBox->findData(mode);

    if (ix >= 0) {
        ui->reasoningBox->setCurrentIndex(ix);
    } else if (!mode.isEmpty()) {
        // A token count rather than one of the named levels.
        ui->reasoningBox->addItem(mode, mode);
        ui->reasoningBox->setCurrentIndex(ui->reasoningBox->count() - 1);
    }
}

void PageTuningInsights::setLimits(int maxTokens, int timeoutMs, int maxRows)
{
    if (maxRows > 0) {
        mMaxRows = maxRows;
    }

    if (maxTokens > 0) {
        mMaxTokens = maxTokens;
    }

    if (timeoutMs > 0) {
        mTimeoutMs = timeoutMs;
    }
}

void PageTuningInsights::applyCliDefaults(const QString &provider,
                                          const QString &model,
                                          const QString &keyEnv,
                                          const QString &logPath,
                                          bool logOnSd)
{
    if (!provider.isEmpty()) {
        const int ix = ui->providerBox->findData(provider);

        if (ix >= 0) {
            ui->providerBox->setCurrentIndex(ix);
        } else {
            /*
             * Not a preset, so treat it as a "kind:baseUrl:model" spec and
             * drive the Custom entry with it.
             */
            QString err;
            const InsightsProvider::Config cfg =
                    InsightsProvider::fromSpec(provider, &err);

            if (err.isEmpty()) {
                ui->providerBox->setCurrentIndex(
                            ui->providerBox->count() - 1);   // Custom
                ui->kindBox->setCurrentIndex(
                            ui->kindBox->findData(int(cfg.kind)));
                ui->baseUrlEdit->setText(cfg.baseUrl);
                ui->modelEdit->setText(cfg.model);
            } else {
                showAnswer(err, false);
            }
        }
    }

    if (!model.isEmpty()) {
        ui->modelEdit->setText(model);
    }

    if (!keyEnv.isEmpty()) {
        ui->keyEnvEdit->setText(keyEnv);
    }

    if (!logPath.isEmpty()) {
        ui->logEdit->setText(logPath);
        ui->sdBox->setChecked(logOnSd);
    }

    if (mMaxRows > 0) {
        // --insightsMaxRows reached the headless path but not this spin box,
        // so a screenshot showed 200 while the payload had been built with 60.
        ui->maxRowsBox->setValue(mMaxRows);
    }

    updateEndpointLabel();
}

InsightsProvider::Config PageTuningInsights::currentConfig() const
{
    const QString id = ui->providerBox->currentData().toString();
    InsightsProvider::Config cfg;

    if (id.isEmpty()) {
        cfg.id = "custom";
        cfg.kind = InsightsProvider::Kind(ui->kindBox->currentData().toInt());
        cfg.baseUrl = ui->baseUrlEdit->text().trimmed();
        cfg.keyEnvVar = ui->keyEnvEdit->text().trimmed();
    } else {
        cfg = InsightsProvider::presetByName(id);
    }

    const QString model = ui->modelEdit->text().trimmed();
    if (!model.isEmpty()) {
        cfg.model = model;
    }

    cfg.reasoning = ui->reasoningBox->currentData().toString();

    return cfg;
}

void PageTuningInsights::updateEndpointLabel()
{
    const InsightsProvider::Config cfg = currentConfig();
    QScopedPointer<InsightsProvider> prov(InsightsProvider::create(cfg));

    /*
     * The endpoint is shown at the moment of sending rather than buried in a
     * preferences dialog: "this is going to localhost" and "this is going to
     * a company" should not look the same.
     */
    QString where = prov.isNull() ? tr("(no provider)") : prov->endpoint();
    if (cfg.isLocal()) {
        where += tr("  -- on this machine, nothing leaves it");
    }

    ui->endpointLabel->setText(tr("Endpoint: %1").arg(where));

    const bool connected = mVesc && mVesc->isPortConnected();
    const QString reason = TuningClient::preflight(cfg);

    ui->analyseButton->setEnabled(reason.isEmpty() && connected);
    ui->previewButton->setEnabled(connected);

    ui->analyseButton->setToolTip(
                connected ? reason : tr("Connect to a controller first"));
    ui->previewButton->setToolTip(
                connected ? tr("Show the exact bytes; sends nothing")
                          : tr("Connect to a controller first"));

    if (!cfg.needsKey()) {
        ui->keyStatusLabel->setText(
                    tr("This provider needs no key, so none is read."));
        ui->keyGroup->setEnabled(false);
    } else {
        ui->keyGroup->setEnabled(true);
        ui->keyStatusLabel->setText(
                    reason.isEmpty()
                    ? tr("A key was found for %1.").arg(cfg.keyEnvVar)
                    : reason);
    }
}

void PageTuningInsights::on_providerBox_currentIndexChanged(int index)
{
    (void)index;

    const QString id = ui->providerBox->currentData().toString();
    ui->customGroup->setVisible(id.isEmpty());

    if (!id.isEmpty()) {
        const InsightsProvider::Config cfg = InsightsProvider::presetByName(id);
        ui->modelEdit->setText(cfg.model);
        ui->baseUrlEdit->setText(cfg.baseUrl);
        ui->keyEnvEdit->setText(cfg.keyEnvVar);
    }

    updateEndpointLabel();
}

void PageTuningInsights::on_keySaveButton_clicked()
{
    const InsightsProvider::Config cfg = currentConfig();
    const QString key = ui->keyEdit->text();

    if (key.isEmpty()) {
        return;
    }

    QSettings set;
    set.setValue(QString("insights/%1/key").arg(cfg.id), key);

    /*
     * Cleared from the field once stored, so it is not left on screen. This
     * is the only place the key is written, and it is never put anywhere a
     * payload, a preview or a log can reach.
     */
    ui->keyEdit->clear();
    updateEndpointLabel();
}

void PageTuningInsights::on_logBrowseButton_clicked()
{
    const QString path = QFileDialog::getOpenFileName(
                this, tr("Choose a log"), "",
                tr("CSV files (*.csv);;All files (*)"));

    if (!path.isEmpty()) {
        ui->logEdit->setText(path);
    }
}

void PageTuningInsights::loadLog(QStringList &header,
                                 QList<QStringList> &rows, QString *err)
{
    const QString path = ui->logEdit->text().trimmed();

    if (path.isEmpty()) {
        return;
    }

    if (ui->sdBox->isChecked()) {
        /*
         * Off the card on the connected device, which is where the logs
         * actually are -- they are written by the Express, not by this
         * program. CAN forwarding has to come off for the transfer: with it
         * on, the file commands go to the controller, which has no card.
         *
         * canTmpOverride rather than setSendCan, because it also suppresses
         * the firmware re-detection that a plain CAN change triggers.
         * Toggling by hand raised the "old but mostly compatible firmware"
         * dialog in the middle of a transfer, and that dialog's modal event
         * loop wedged the analysis behind it.
         */
        if (!mVesc) {
            *err = tr("Not connected");
            return;
        }

        mVesc->canTmpOverride(false, 0);
        const QByteArray raw = mVesc->commands()->fileBlockRead(path);
        mVesc->canTmpOverrideEnd();

        if (raw.isEmpty()) {
            *err = tr("Could not read %1 from the SD card").arg(path);
            return;
        }

        for (const QString &line: QString::fromUtf8(raw).split('\n')) {
            if (line.trimmed().isEmpty()) {
                continue;
            }

            if (header.isEmpty()) {
                header = line.split(";");
            } else {
                rows.append(line.split(";"));
            }
        }

        return;
    }

    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        *err = tr("Could not open %1").arg(path);
        return;
    }

    QTextStream ts(&f);
    if (!ts.atEnd()) {
        header = ts.readLine().split(";");
    }
    while (!ts.atEnd()) {
        const QString line = ts.readLine();
        if (!line.trimmed().isEmpty()) {
            rows.append(line.split(";"));
        }
    }
}

QJsonObject PageTuningInsights::buildPayload(QString *err)
{
    if (!mVesc) {
        *err = tr("Not connected");
        return QJsonObject();
    }

    QStringList logHeader;
    QList<QStringList> logRows;
    loadLog(logHeader, logRows, err);

    if (!err->isEmpty()) {
        return QJsonObject();
    }

    /*
     * A telemetry snapshot. getValues() is a request and the numbers arrive
     * on a signal, so they are captured from it; a run without them is a
     * warning rather than a failure, since the configuration and the log are
     * most of the value.
     */
    MC_VALUES rtVals;
    auto conn = connect(mVesc->commands(), &Commands::valuesReceived,
                        this, [&rtVals](MC_VALUES v, unsigned int) {
        rtVals = v;
    });
    mVesc->commands()->getValues();
    Utility::waitSignal(mVesc->commands(),
                        SIGNAL(valuesReceived(MC_VALUES,uint)), 2000);
    disconnect(conn);

    const FW_RX_PARAMS fwp = mVesc->getLastFwRxParams();
    const QString fwStr = QString("V%1.%2 %3 hw:%4")
            .arg(fwp.major).arg(fwp.minor, 2, 10, QLatin1Char('0'))
            .arg(fwp.fwName, fwp.hw);

    return TuningInsights::buildPayload(
                TuningInsightsConf::extract(mVesc->mcConfig()),
                TuningInsightsConf::extract(mVesc->appConfig()),
                rtVals, logHeader, logRows,
                ui->maxRowsBox->value(), fwStr);
}

void PageTuningInsights::on_previewButton_clicked()
{
    QString err;
    const QJsonObject payload = buildPayload(&err);

    if (!err.isEmpty()) {
        showAnswer(err, false);
        return;
    }

    mLastPayload = payload;

    // The exact bytes, and nothing is sent. Not Markdown, so not rendered.
    showAnswer(QString::fromUtf8(QJsonDocument(payload)
                                 .toJson(QJsonDocument::Indented)), false);
    ui->tabs->setCurrentWidget(ui->answerTab);
}

void PageTuningInsights::on_analyseButton_clicked()
{
    const InsightsProvider::Config cfg = currentConfig();
    QString err = TuningClient::preflight(cfg);

    if (!err.isEmpty()) {
        showAnswer(err, false);
        return;
    }

    const QJsonObject payload = buildPayload(&err);
    if (!err.isEmpty()) {
        showAnswer(err, false);
        return;
    }

    mLastPayload = payload;

    QScopedPointer<InsightsProvider> prov(InsightsProvider::create(cfg));
    if (prov.isNull()) {
        showAnswer(tr("No provider"), false);
        return;
    }

    ui->analyseButton->setEnabled(false);
    showAnswer(tr("Asking %1...").arg(prov->endpoint()), false);
    QApplication::setOverrideCursor(Qt::WaitCursor);

    TuningClient client;
    const QString reply = client.send(prov.data(),
                                      TuningInsights::buildPrompt(payload, instructions()),
                                      mMaxTokens, mTimeoutMs, &err);

    QApplication::restoreOverrideCursor();
    ui->analyseButton->setEnabled(true);

    if (err.isEmpty()) {
        mLastAnswer = reply;
        showAnswer(reply, true);
    } else {
        mLastAnswer.clear();
        showAnswer(err, false);

        // Also on stderr, so a headless or scripted run is not silent about it.
        qWarning() << "insights:" << err.toLocal8Bit().constData();
    }

    /*
     * Switch to the answer tab either way. The setup is what you were looking
     * at while configuring; once Analyse has been pressed, the result -- an
     * answer or the reason there is none -- is what you want to see. Leaving
     * the user on the setup tab after a failure makes it look as though
     * nothing happened at all.
     */
    ui->tabs->setCurrentWidget(ui->answerTab);
}

void PageTuningInsights::on_copyButton_clicked()
{
    // The Markdown, not the rendered HTML: what you paste should be the text.
    QApplication::clipboard()->setText(
                mLastAnswer.isEmpty() ? ui->answerView->toPlainText()
                                      : mLastAnswer);
}

static QString renderAnswer(const QString &md)
{
    /*
     * Markdown reads an underscore as emphasis, so every parameter name in the
     * answer came out mangled: si_battery_cells rendered as si<em>battery</em>
     * cells, l_current_max as l<em>current</em>max. Naming parameters is the
     * whole point of the answer, so that made it unreadable and uncopyable.
     *
     * Each underscore inside a snake_case word is swapped for a sentinel
     * before conversion and restored afterwards, which puts the character out
     * of maddy's reach whatever else it is nested in. Two earlier attempts
     * were worse: backticks around the name were still eaten inside a **bold**
     * run, and skipping text between backticks broke on a reply with an odd
     * number of them -- half the document then went unmasked. Masking inside a
     * code span changes nothing, since the underscore is literal there
     * already, so this simply does not special-case them.
     */
    static const QRegularExpression ident(
                "\\b[A-Za-z][A-Za-z0-9]*(?:_[A-Za-z0-9]+)+\\b");
    const QChar sentinel(0x0001);

    QString masked;
    int last = 0;

    auto it = ident.globalMatch(md);
    while (it.hasNext()) {
        const QRegularExpressionMatch m = it.next();
        masked += md.mid(last, m.capturedStart() - last);
        masked += QString(m.captured()).replace('_', sentinel);
        last = m.capturedEnd();
    }

    masked += md.mid(last);

    // Utility::md2html is the same maddy pass the rest of the app uses.
    return Utility::md2html(masked).replace(sentinel, '_');
}

void PageTuningInsights::showAnswer(const QString &text, bool isMarkdown)
{
    if (isMarkdown) {
        // Utility::md2html is the same maddy pass the rest of the app uses.
        ui->answerView->setHtml(renderAnswer(text));
    } else {
        ui->answerView->setPlainText(text);
    }
}

bool PageTuningInsights::haveResult(const char *what)
{
    if (mLastPayload.isEmpty()) {
        showAnswer(tr("Nothing to save yet: press Preview payload or Analyse "
                      "first (%1).").arg(what), false);
        return false;
    }

    return true;
}

QString PageTuningInsights::sentLogCsv() const
{
    const QJsonObject log = mLastPayload["log"].toObject();
    const QJsonArray cols = log["columns"].toArray();
    const QJsonArray rows = log["rows"].toArray();

    if (cols.isEmpty()) {
        return QString();
    }

    /*
     * The package logger's shape -- ';' between fields, and each header field
     * a "name:label:unit:decimals:..." descriptor -- so the file reads back
     * into the same tools that produced the original.
     */
    QStringList head;
    for (const QJsonValue &c: cols) {
        const QString name = c.toString();
        head << QString("%1:%1::2:0:0").arg(name);
    }

    QString out = head.join(";") + "\n";

    for (const QJsonValue &r: rows) {
        QStringList f;
        for (const QJsonValue &v: r.toArray()) {
            f << v.toString();
        }
        out += f.join(";") + "\n";
    }

    return out;
}

static bool writeTextFile(const QString &path, const QString &text)
{
    QFile f(path);

    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }

    return f.write(text.toUtf8()) == text.toUtf8().size();
}

void PageTuningInsights::on_saveAnswerButton_clicked()
{
    if (mLastAnswer.isEmpty()) {
        showAnswer(tr("No answer to save yet."), false);
        return;
    }

    const QString path = QFileDialog::getSaveFileName(
                this, tr("Save answer"), "tuning-answer.md",
                tr("Markdown (*.md);;All files (*)"));

    if (path.isEmpty()) {
        return;
    }

    if (!writeTextFile(path, mLastAnswer)) {
        showAnswer(tr("Could not write %1").arg(path), false);
    }
}

void PageTuningInsights::on_saveConfigButton_clicked()
{
    if (!mVesc) {
        showAnswer(tr("Not connected."), false);
        return;
    }

    const QString dir = QFileDialog::getExistingDirectory(
                this, tr("Where to save the configuration"));

    if (dir.isEmpty()) {
        return;
    }

    /*
     * The same XML this program writes for --getMcConf and reads back for
     * --setMcConf, so these are restorable rather than a private format.
     */
    const bool okMc = mVesc->mcConfig()->saveXml(
                dir + "/mcconf.xml", "MCConfiguration");
    const bool okApp = mVesc->appConfig()->saveXml(
                dir + "/appconf.xml", "APPConfiguration");

    if (!okMc || !okApp) {
        showAnswer(tr("Could not write the configuration to %1").arg(dir),
                   false);
    }
}

void PageTuningInsights::on_saveLogButton_clicked()
{
    if (!haveResult("log")) {
        return;
    }

    const QString csv = sentLogCsv();

    if (csv.isEmpty()) {
        showAnswer(tr("No log was included in what was sent."), false);
        return;
    }

    const QString path = QFileDialog::getSaveFileName(
                this, tr("Save the log that was sent"), "sent-log.csv",
                tr("CSV (*.csv);;All files (*)"));

    if (path.isEmpty()) {
        return;
    }

    if (!writeTextFile(path, csv)) {
        showAnswer(tr("Could not write %1").arg(path), false);
    }
}

void PageTuningInsights::on_saveBundleButton_clicked()
{
    if (!haveResult("bundle")) {
        return;
    }

    const QString dir = QFileDialog::getExistingDirectory(
                this, tr("Where to save the bundle"));

    if (dir.isEmpty()) {
        return;
    }

    /*
     * A directory rather than an archive: this program has no zip writer, and
     * four files somebody can open is more useful than one they cannot.
     */
    QStringList failed;

    if (!writeTextFile(dir + "/payload.json",
                       QString::fromUtf8(QJsonDocument(mLastPayload)
                                         .toJson(QJsonDocument::Indented)))) {
        failed << "payload.json";
    }

    if (!mLastAnswer.isEmpty() &&
            !writeTextFile(dir + "/answer.md", mLastAnswer)) {
        failed << "answer.md";
    }

    const QString csv = sentLogCsv();
    if (!csv.isEmpty() && !writeTextFile(dir + "/sent-log.csv", csv)) {
        failed << "sent-log.csv";
    }

    if (mVesc) {
        if (!mVesc->mcConfig()->saveXml(dir + "/mcconf.xml",
                                        "MCConfiguration")) {
            failed << "mcconf.xml";
        }
        if (!mVesc->appConfig()->saveXml(dir + "/appconf.xml",
                                         "APPConfiguration")) {
            failed << "appconf.xml";
        }
    }

    if (!failed.isEmpty()) {
        showAnswer(tr("Could not write: %1").arg(failed.join(", ")), false);
    }
}
