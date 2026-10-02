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

    /*
     * Pre-fills the controls from the command line, so a configured page can
     * be opened in one command -- which is what makes a screenshot of a real
     * analysis reproducible instead of a sequence of guessed mouse clicks.
     * An empty argument leaves that control alone.
     */
    void applyCliDefaults(const QString &provider, const QString &model,
                          const QString &keyEnv, const QString &logPath,
                          bool logOnSd);

    /*
     * How long an answer may be, and how long to wait for it. The reply length
     * is what the wait is mostly made of -- a model writing 2000 tokens of
     * prose takes about a minute however fast the link is -- so this is the
     * one knob that actually shortens it.
     */
    void setLimits(int maxTokens, int timeoutMs, int maxRows);

    // Replaces the instruction text. Empty restores the built-in one.
    void setInstructions(const QString &text);

    // "", "off", "low", "medium", "high" or a token count.
    void setReasoning(const QString &mode);

    /*
     * The saves, separated from the buttons that drive them.
     *
     * Each slot's first statement is a modal file dialog, which makes the slot
     * untestable and the writing unreusable. These take the destination
     * instead, so what gets written can be checked -- including the claim that
     * the configuration comes back out in a form this program can load again.
     *
     * Each returns false and sets *err on failure.
     */
    bool saveAnswerTo(const QString &path, QString *err);
    bool saveConfigTo(const QString &dir, QString *err);
    bool saveSentLogTo(const QString &path, QString *err);
    bool saveBundleTo(const QString &dir, QString *err);

    // What the log would be saved as. Empty when no log was sent.
    QString sentLogCsv() const;

private slots:
    void on_providerBox_currentIndexChanged(int index);
    void on_logBrowseButton_clicked();
    void on_previewButton_clicked();
    void on_analyseButton_clicked();
    void on_copyButton_clicked();
    void on_keySaveButton_clicked();
    void on_promptResetButton_clicked();
    void on_saveAnswerButton_clicked();
    void on_saveConfigButton_clicked();
    void on_saveLogButton_clicked();
    void on_saveBundleButton_clicked();

private:
    Ui::PageTuningInsights *ui;
    VescInterface *mVesc;

    int mMaxTokens = 2048;
    int mTimeoutMs = 120000;
    int mMaxRows = 0;

    /*
     * What was last sent and what came back, kept so the save buttons write
     * the thing that was actually used rather than rebuilding it and hoping
     * it comes out the same.
     */
    QJsonObject mLastPayload;
    QString mLastAnswer;

    /* The provider as the controls currently describe it. */
    InsightsProvider::Config currentConfig() const;

    /*
     * The payload, or an empty object with the reason in *err. Shared by
     * Preview and Analyse so that what is previewed is what is sent.
     */
    QJsonObject buildPayload(QString *err);

    void updateEndpointLabel();

    // The instruction text in force: the box when enabled, else the default.
    QString instructions() const;

    // Shows text in the Answer tab: Markdown rendered, anything else verbatim.
    void showAnswer(const QString &markdown, bool isMarkdown);

    bool haveResult(const char *what);
    void loadLog(QStringList &header, QList<QStringList> &rows, QString *err);

};

#endif // PAGETUNINGINSIGHTS_H
