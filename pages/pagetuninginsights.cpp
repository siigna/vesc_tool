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
        ui->replyEdit->setPlainText(err);
        return;
    }

    // The exact bytes, and nothing is sent.
    ui->replyEdit->setPlainText(
                QString::fromUtf8(QJsonDocument(payload)
                                  .toJson(QJsonDocument::Indented)));
}

void PageTuningInsights::on_analyseButton_clicked()
{
    const InsightsProvider::Config cfg = currentConfig();
    QString err = TuningClient::preflight(cfg);

    if (!err.isEmpty()) {
        ui->replyEdit->setPlainText(err);
        return;
    }

    const QJsonObject payload = buildPayload(&err);
    if (!err.isEmpty()) {
        ui->replyEdit->setPlainText(err);
        return;
    }

    QScopedPointer<InsightsProvider> prov(InsightsProvider::create(cfg));
    if (prov.isNull()) {
        ui->replyEdit->setPlainText(tr("No provider"));
        return;
    }

    ui->analyseButton->setEnabled(false);
    ui->replyEdit->setPlainText(tr("Asking %1...").arg(prov->endpoint()));
    QApplication::setOverrideCursor(Qt::WaitCursor);

    TuningClient client;
    const QString reply = client.send(prov.data(),
                                      TuningInsights::buildPrompt(payload),
                                      2048, 120000, &err);

    QApplication::restoreOverrideCursor();
    ui->analyseButton->setEnabled(true);

    ui->replyEdit->setPlainText(err.isEmpty() ? reply : err);
}

void PageTuningInsights::on_copyButton_clicked()
{
    QApplication::clipboard()->setText(ui->replyEdit->toPlainText());
}
