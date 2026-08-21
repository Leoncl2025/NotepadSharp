#include <QSignalSpy>
#include <QTemporaryDir>
#include <QEvent>
#include <QFile>
#include <QtTest>

#include "AppearanceManager.h"
#include "ApplicationSettings.h"

class AppearanceManagerTests : public QObject
{
    Q_OBJECT

private slots:
    void parsesOnlyCanonicalWireValues();
    void defaultsMissingSettingsToClassicLight();
    void defaultsInvalidSettingsToSystem();
    void persistsExplicitMode();
    void emitsOnlyWhenEffectiveAppearanceChanges();
    void followsSystemChangesOnlyInSystemMode();
    void systemDarkKeepsSemanticSyntax();
    void ignoresPaletteChangesDuringSynchronousRefresh();
    void writesOptInPerformanceTrace();
};

void AppearanceManagerTests::parsesOnlyCanonicalWireValues()
{
    QCOMPARE(AppearanceManager::modeFromString(QStringLiteral("system")), AppearanceManager::Mode::System);
    QCOMPARE(AppearanceManager::modeFromString(QStringLiteral("light")), AppearanceManager::Mode::Light);
    QCOMPARE(AppearanceManager::modeFromString(QStringLiteral("dark")), AppearanceManager::Mode::Dark);
    QCOMPARE(AppearanceManager::modeFromString(QStringLiteral("Dark")), AppearanceManager::Mode::System);
    QCOMPARE(AppearanceManager::modeFromString(QStringLiteral("2")), AppearanceManager::Mode::System);
    QCOMPARE(AppearanceManager::modeFromString(QString()), AppearanceManager::Mode::System);
    QCOMPARE(AppearanceManager::scintillaElementColor(QColor(0x27, 0x67, 0x82, 0xDD)),
             0xDD826727u);
}

void AppearanceManagerTests::defaultsMissingSettingsToClassicLight()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    ApplicationSettings settings(directory.filePath(QStringLiteral("settings.ini")), QSettings::IniFormat);
    QCOMPARE(settings.appearance(), QStringLiteral("light"));

    AppearanceManager manager(&settings, [] { return Qt::ColorScheme::Dark; });
    QCOMPARE(manager.requestedMode(), AppearanceManager::Mode::Light);
    QCOMPARE(manager.effectiveAppearance(), AppearanceManager::EffectiveAppearance::Light);
}

void AppearanceManagerTests::defaultsInvalidSettingsToSystem()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    ApplicationSettings settings(directory.filePath(QStringLiteral("settings.ini")), QSettings::IniFormat);
    settings.setAppearance(QStringLiteral("invalid"));

    AppearanceManager manager(&settings, [] { return Qt::ColorScheme::Light; });
    QCOMPARE(manager.requestedMode(), AppearanceManager::Mode::System);
    QCOMPARE(manager.effectiveAppearance(), AppearanceManager::EffectiveAppearance::Light);
}

void AppearanceManagerTests::persistsExplicitMode()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString settingsPath = directory.filePath(QStringLiteral("settings.ini"));

    {
        ApplicationSettings settings(settingsPath, QSettings::IniFormat);
        AppearanceManager manager(&settings, [] { return Qt::ColorScheme::Light; });
        manager.setRequestedMode(AppearanceManager::Mode::Dark);
        settings.sync();
    }

    ApplicationSettings settings(settingsPath, QSettings::IniFormat);
    AppearanceManager manager(&settings, [] { return Qt::ColorScheme::Light; });
    QCOMPARE(settings.appearance(), QStringLiteral("dark"));
    QCOMPARE(manager.requestedMode(), AppearanceManager::Mode::Dark);
    QCOMPARE(manager.effectiveAppearance(), AppearanceManager::EffectiveAppearance::Dark);
}

void AppearanceManagerTests::emitsOnlyWhenEffectiveAppearanceChanges()
{
    QTemporaryDir directory;
    ApplicationSettings settings(directory.filePath(QStringLiteral("settings.ini")), QSettings::IniFormat);
    AppearanceManager manager(&settings, [] { return Qt::ColorScheme::Dark; });
    QSignalSpy spy(&manager, &AppearanceManager::effectiveAppearanceChanged);

    manager.setRequestedMode(AppearanceManager::Mode::Dark);
    QCOMPARE(spy.count(), 1);
    QCOMPARE(QApplication::palette().color(QPalette::Base), QColor(QStringLiteral("#121314")));

    manager.setRequestedMode(AppearanceManager::Mode::Light);
    QCOMPARE(spy.count(), 2);
    QCOMPARE(manager.effectiveAppearance(), AppearanceManager::EffectiveAppearance::Light);
}

void AppearanceManagerTests::followsSystemChangesOnlyInSystemMode()
{
    QTemporaryDir directory;
    ApplicationSettings settings(directory.filePath(QStringLiteral("settings.ini")), QSettings::IniFormat);
    settings.setAppearance(QStringLiteral("system"));
    Qt::ColorScheme scheme = Qt::ColorScheme::Light;
    AppearanceManager manager(&settings, [&scheme] { return scheme; });
    QSignalSpy spy(&manager, &AppearanceManager::effectiveAppearanceChanged);

    scheme = Qt::ColorScheme::Dark;
    manager.refreshSystemAppearance();
    QCOMPARE(spy.count(), 1);
    QCOMPARE(manager.effectiveAppearance(), AppearanceManager::EffectiveAppearance::Dark);

    manager.setRequestedMode(AppearanceManager::Mode::Light);
    QCOMPARE(spy.count(), 2);
    scheme = Qt::ColorScheme::Dark;
    manager.refreshSystemAppearance();
    QCOMPARE(spy.count(), 2);
}

void AppearanceManagerTests::systemDarkKeepsSemanticSyntax()
{
    QTemporaryDir directory;
    ApplicationSettings settings(directory.filePath(QStringLiteral("settings.ini")), QSettings::IniFormat);
    settings.setAppearance(QStringLiteral("system"));
    AppearanceManager manager(&settings, [] { return Qt::ColorScheme::Dark; });

    QCOMPARE(manager.effectiveAppearance(), AppearanceManager::EffectiveAppearance::Dark);
    QVERIFY(manager.tokens().syntaxKeyword != manager.tokens().textEditor);
    QVERIFY(manager.tokens().syntaxString != manager.tokens().syntaxKeyword);
    QVERIFY(manager.tokens().diffAddedMarker != manager.tokens().diffDeletedMarker);
}

void AppearanceManagerTests::ignoresPaletteChangesDuringSynchronousRefresh()
{
    QTemporaryDir directory;
    ApplicationSettings settings(directory.filePath(QStringLiteral("settings.ini")), QSettings::IniFormat);
    AppearanceManager manager(&settings, [] { return Qt::ColorScheme::Light; });
    QSignalSpy spy(&manager, &AppearanceManager::effectiveAppearanceChanged);
    int refreshCount = 0;

    connect(&manager, &AppearanceManager::effectiveAppearanceChanged, qApp, [&]() {
        if (++refreshCount < 4) {
            QEvent paletteChange(QEvent::ApplicationPaletteChange);
            QCoreApplication::sendEvent(qApp, &paletteChange);
        }
    });

    manager.setRequestedMode(AppearanceManager::Mode::System);

    QCOMPARE(spy.count(), 1);
    QCOMPARE(refreshCount, 1);
    QCOMPARE(manager.requestedMode(), AppearanceManager::Mode::System);
    QCOMPARE(manager.effectiveAppearance(), AppearanceManager::EffectiveAppearance::Light);
}

void AppearanceManagerTests::writesOptInPerformanceTrace()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString tracePath = directory.filePath(QStringLiteral("appearance-trace.log"));
    const QByteArray previousTracePath = qgetenv("NOTEPADSHARP_APPEARANCE_TRACE");
    qputenv("NOTEPADSHARP_APPEARANCE_TRACE", tracePath.toUtf8());

    ApplicationSettings settings(directory.filePath(QStringLiteral("settings.ini")), QSettings::IniFormat);
    AppearanceManager manager(&settings, [] { return Qt::ColorScheme::Light; });
    manager.setRequestedMode(AppearanceManager::Mode::Dark);
    manager.setRequestedMode(AppearanceManager::Mode::System);

    QFile traceFile(tracePath);
    const bool opened = traceFile.open(QIODevice::ReadOnly | QIODevice::Text);
    const QString trace = opened ? QString::fromUtf8(traceFile.readAll()) : QString();
    if (previousTracePath.isNull())
        qunsetenv("NOTEPADSHARP_APPEARANCE_TRACE");
    else
        qputenv("NOTEPADSHARP_APPEARANCE_TRACE", previousTracePath);

    QVERIFY(opened);
    QVERIFY(trace.contains(QStringLiteral("begin trigger=setting-change requested=dark")));
    QVERIFY(trace.contains(QStringLiteral("begin trigger=setting-change requested=system")));
    QVERIFY(trace.contains(QStringLiteral("component=restore-system-palette")));
    QVERIFY(trace.contains(QStringLiteral("component=application-palette")));
    QVERIFY(trace.contains(QStringLiteral("component=native-windows")));
    QVERIFY(trace.contains(QStringLiteral("component=synchronous-slots")));
    QVERIFY(trace.contains(QStringLiteral(" end elapsed-ms=")));
}

QTEST_MAIN(AppearanceManagerTests)

#include "AppearanceManagerTests.moc"