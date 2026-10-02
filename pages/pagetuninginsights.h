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

#ifndef PAGETUNINGINSIGHTS_H
#define PAGETUNINGINSIGHTS_H

#include <QWidget>
#include <QJsonObject>

#include "vescinterface.h"
#include "insightsprovider.h"

namespace Ui {
class PageTuningInsights;
}

/*
 * Tuning Insights: packages the configuration, a telemetry snapshot and a log
 * window, asks a model about them, and shows the answer.
 *
 * Advisory only. There is deliberately no control on this page that writes a
 * configuration value, so a wrong number in a reply cannot reach a motor
 * without somebody typing it into the page that owns that setting.
 */
class PageTuningInsights : public QWidget
{
    Q_OBJECT

public:
    explicit PageTuningInsights(QWidget *parent = nullptr);
    ~PageTuningInsights();

    VescInterface *vesc() const;
    void setVesc(VescInterface *vesc);

private slots:
    void on_providerBox_currentIndexChanged(int index);
    void on_logBrowseButton_clicked();
    void on_previewButton_clicked();
    void on_analyseButton_clicked();
    void on_copyButton_clicked();
    void on_keySaveButton_clicked();

private:
    Ui::PageTuningInsights *ui;
    VescInterface *mVesc;

    /* The provider as the controls currently describe it. */
    InsightsProvider::Config currentConfig() const;

    /*
     * The payload, or an empty object with the reason in *err. Shared by
     * Preview and Analyse so that what is previewed is what is sent.
     */
    QJsonObject buildPayload(QString *err);

    void updateEndpointLabel();
    void loadLog(QStringList &header, QList<QStringList> &rows, QString *err);

};

#endif // PAGETUNINGINSIGHTS_H
