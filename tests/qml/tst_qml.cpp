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
 * The C++ half of the QML suite: the environment, the type registrations, the
 * context properties the mobile QML expects to find, and a probe object the
 * tests use to enumerate the QML files and to read back engine warnings.
 *
 * main() is written out rather than using QUICK_TEST_MAIN_WITH_SETUP for the
 * same reason tests/ui/tst_ui.cpp does not use QTEST_MAIN: the environment has
 * to be pinned before any application object reads the locale or an XDG path,
 * and quick_test_main_with_setup constructs one if none exists.
 */

#include <QApplication>
#include <QDir>
#include <QQmlContext>
#include <QQmlEngine>
#include <QStringList>
#include <QtQml>
#include <QtQuickTest/quicktest.h>

#include "appregister.h"
#include "appstyle.h"
#include "mobile/qmlui.h"
#include "utility.h"
#include "vescinterface.h"

#include "../ui/uiharness.h"

/*
 * Enumerates the mobile QML and collects engine warnings.
 *
 * The file list is read from the resource directory rather than written out,
 * so a QML file added to mobile/qml.qrc is covered by the load test without
 * anyone remembering to add it here. That is the whole point: the files that
 * went unnoticed are exactly the ones nobody would have listed.
 */
class QmlProbe : public QObject
{
    Q_OBJECT

public:
    explicit QmlProbe(QObject *parent = nullptr) : QObject(parent) {}

    Q_INVOKABLE QStringList mobileFiles() const
    {
        QStringList out = QDir(":/mobile").entryList(QStringList() << "*.qml",
                                                     QDir::Files, QDir::Name);
        return out;
    }

    /*
     * QObject::findChild from QML, which has no equivalent of its own:
     * findChild matches on objectName, and QML's own lookup by id does not
     * cross a component boundary.
     */
    Q_INVOKABLE QObject *findChild(QObject *root, const QString &name) const
    {
        if (root == nullptr) {
            return nullptr;
        }

        return root->findChild<QObject *>(name);
    }

    Q_INVOKABLE QStringList warnings() const { return mWarnings; }
    Q_INVOKABLE void clearWarnings() { mWarnings.clear(); }

    void addWarnings(const QList<QQmlError> &errors)
    {
        for (const QQmlError &e : errors) {
            mWarnings.append(e.toString());
        }
    }

private:
    QStringList mWarnings;
};

/*
 * What qmlui.cpp sets on the real engine, minus the engine itself.
 *
 * Every mobile component that draws anything calls Utility.getAppHexColor,
 * and most of the pages call VescIf. Without these the components load and
 * then fail at first binding, which reads as a pass if nothing is watching
 * the warning stream -- hence the probe above.
 */
class QmlSetup : public QObject
{
    Q_OBJECT

public:
    QmlProbe *probe = nullptr;

public slots:
    void qmlEngineAvailable(QQmlEngine *engine)
    {
        probe = new QmlProbe(engine);

        /*
         * Collected through the engine's own signal rather than a message
         * handler, because qExec installs one of those for itself and the
         * last installer wins.
         */
        connect(engine, &QQmlEngine::warnings,
                probe, [this](const QList<QQmlError> &errors) {
            probe->addWarnings(errors);
        });

        QQmlContext *ctx = engine->rootContext();

        ctx->setContextProperty("VescIf", UiHarness::makeVesc());
        ctx->setContextProperty("Utility", new Utility(engine));
        ctx->setContextProperty("QmlProbe", probe);

        /*
         * QmlUi is referenced by the pages that drive the mobile application
         * itself -- startup, page switching, the setup wizards. It is not a
         * QML-registered type, so it goes in as an instance; constructing one
         * does not start an engine of its own.
         */
        ctx->setContextProperty("QmlUi", new QmlUi(engine));
    }
};

int main(int argc, char **argv)
{
    /*
     * Order matters, exactly as in the widget suite: pinEnvironment before any
     * application object exists.
     */
    UiHarness::pinEnvironment();
    VtAppStyle::initIdentity();
    UiHarness::seedSettings();

    QApplication app(argc, argv);

    VtApp::registerTypes();

    VtAppStyle::initColors(true);
    VtAppStyle::registerFonts();
    VtAppStyle::applyStyle(&app, true);

    QmlSetup setup;

    return quick_test_main_with_setup(argc, argv, "tst_qml",
                                      QUICK_TEST_SOURCE_DIR, &setup);
}

#include "tst_qml.moc"
