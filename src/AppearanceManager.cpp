#include "AppearanceManager.h"

#include "AppearanceTrace.h"
#include "ApplicationSettings.h"
#include "WindowsAppearance.h"

#include <QApplication>
#include <QColor>
#include <QEvent>
#include <QGuiApplication>
#include <QPalette>
#include <QScopedValueRollback>
#include <QStyle>
#include <QStyleHints>
#include <QWidget>
#include <QtMath>

namespace {

double relativeLuminance(const QColor &color)
{
    auto linearChannel = [](int channel) {
        const double value = channel / 255.0;
        return value <= 0.04045 ? value / 12.92 : qPow((value + 0.055) / 1.055, 2.4);
    };

    return 0.2126 * linearChannel(color.red())
        + 0.7152 * linearChannel(color.green())
        + 0.0722 * linearChannel(color.blue());
}

} // namespace

AppearanceManager::AppearanceManager(ApplicationSettings *settings, QObject *parent)
    : AppearanceManager(settings, {}, parent)
{
}

AppearanceManager::AppearanceManager(ApplicationSettings *settings,
                                     ColorSchemeProvider colorSchemeProvider, QObject *parent)
    : QObject(parent)
    , settings(settings)
    , colorSchemeProvider(colorSchemeProvider ? std::move(colorSchemeProvider) : [] {
        const QStyleHints *styleHints = QGuiApplication::styleHints();
        return styleHints ? styleHints->colorScheme() : Qt::ColorScheme::Unknown;
    })
    , mode(modeFromString(settings->appearance()))
    , effective(EffectiveAppearance::Light)
{
    AppearanceTrace::initialize();
    if (mode == Mode::System)
        restoreSystemPalette();
    effective = resolveEffectiveAppearance();
    applyApplicationAppearance();
    qApp->installEventFilter(this);

    connect(settings, &ApplicationSettings::appearanceChanged,
            this, &AppearanceManager::onAppearanceSettingChanged);

    if (QStyleHints *styleHints = QGuiApplication::styleHints()) {
        connect(styleHints, &QStyleHints::colorSchemeChanged,
                this, &AppearanceManager::refreshSystemAppearance);
    }
}

AppearanceManager::Mode AppearanceManager::modeFromString(const QString &value)
{
    if (value == QStringLiteral("light"))
        return Mode::Light;
    if (value == QStringLiteral("dark"))
        return Mode::Dark;
    return Mode::System;
}

QString AppearanceManager::modeToString(Mode mode)
{
    switch (mode) {
    case Mode::Light:
        return QStringLiteral("light");
    case Mode::Dark:
        return QStringLiteral("dark");
    case Mode::System:
    default:
        return QStringLiteral("system");
    }
}

AppearanceManager::EffectiveAppearance AppearanceManager::resolve(
    Mode mode, Qt::ColorScheme systemScheme, EffectiveAppearance unknownFallback)
{
    if (mode == Mode::Light)
        return EffectiveAppearance::Light;
    if (mode == Mode::Dark)
        return EffectiveAppearance::Dark;
    if (systemScheme == Qt::ColorScheme::Dark)
        return EffectiveAppearance::Dark;
    if (systemScheme == Qt::ColorScheme::Light)
        return EffectiveAppearance::Light;
    return unknownFallback;
}

int AppearanceManager::scintillaColor(const QColor &color)
{
    return color.red() | (color.green() << 8) | (color.blue() << 16);
}

unsigned int AppearanceManager::scintillaElementColor(const QColor &color)
{
    return static_cast<unsigned int>(scintillaColor(color))
        | (static_cast<unsigned int>(color.alpha()) << 24);
}

void AppearanceManager::setRequestedMode(Mode requestedMode)
{
    settings->setAppearance(modeToString(requestedMode));
}

void AppearanceManager::refreshSystemAppearance()
{
    if (mode != Mode::System)
        return;

    QElapsedTimer total;
    quint64 cycle = 0;
    if (AppearanceTrace::enabled()) {
        total.start();
        cycle = AppearanceTrace::beginCycle(
            QStringLiteral("system-color-scheme"),
            QStringLiteral("requested=system effective-before=%1 windows=%2")
                .arg(isDark() ? QStringLiteral("dark") : QStringLiteral("light"))
                .arg(QApplication::topLevelWidgets().size()));
    }
    {
        AppearanceTrace::Scope trace(QStringLiteral("restore-system-palette"));
        restoreSystemPalette();
    }
    updateEffectiveAppearance(true);
    if (cycle != 0) {
        AppearanceTrace::endCycle(cycle, total.elapsed(),
            QStringLiteral("effective-after=%1")
                .arg(isDark() ? QStringLiteral("dark") : QStringLiteral("light")));
    }
}

void AppearanceManager::onAppearanceSettingChanged(const QString &value)
{
    const Mode requestedMode = modeFromString(value);
    if (mode == requestedMode)
        return;

    QElapsedTimer total;
    quint64 cycle = 0;
    if (AppearanceTrace::enabled()) {
        total.start();
        cycle = AppearanceTrace::beginCycle(
            QStringLiteral("setting-change"),
            QStringLiteral("requested=%1 previous=%2 effective-before=%3 windows=%4")
                .arg(modeToString(requestedMode), modeToString(mode),
                     isDark() ? QStringLiteral("dark") : QStringLiteral("light"))
                .arg(QApplication::topLevelWidgets().size()));
    }
    mode = requestedMode;
    if (mode == Mode::System) {
        AppearanceTrace::Scope trace(QStringLiteral("restore-system-palette"));
        restoreSystemPalette();
    }
    updateEffectiveAppearance(true);
    if (cycle != 0) {
        AppearanceTrace::endCycle(cycle, total.elapsed(),
            QStringLiteral("effective-after=%1")
                .arg(isDark() ? QStringLiteral("dark") : QStringLiteral("light")));
    }
}

AppearanceManager::EffectiveAppearance AppearanceManager::resolveEffectiveAppearance() const
{
    return resolve(mode, colorSchemeProvider(), paletteFallback());
}

AppearanceManager::EffectiveAppearance AppearanceManager::paletteFallback() const
{
    const QPalette palette = QApplication::palette();
    return relativeLuminance(palette.color(QPalette::Base))
            < relativeLuminance(palette.color(QPalette::Text))
        ? EffectiveAppearance::Dark
        : EffectiveAppearance::Light;
}

void AppearanceManager::restoreSystemPalette()
{
    if (QApplication::style()) {
        applyingPalette = true;
        QApplication::setPalette(QApplication::style()->standardPalette());
        applyingPalette = false;
    }
    systemPaletteSnapshot = QApplication::palette();
    hasSystemPaletteSnapshot = true;
}

void AppearanceManager::applyApplicationAppearance()
{
    if (mode == Mode::System) {
        currentTokens = systemTokens(QApplication::palette(), effective,
                                     WindowsAppearance::isHighContrast());
        return;
    }

    currentTokens = effective == EffectiveAppearance::Dark ? darkTokens() : lightTokens();
    applyingPalette = true;
    QApplication::setPalette(explicitPalette(currentTokens));
    applyingPalette = false;
}

bool AppearanceManager::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::ApplicationPaletteChange
        && mode == Mode::System && !applyingPalette && !updatingAppearance) {
        const QPalette applicationPalette = QApplication::palette();
        if (hasSystemPaletteSnapshot && applicationPalette == systemPaletteSnapshot)
            return QObject::eventFilter(watched, event);

        systemPaletteSnapshot = applicationPalette;
        hasSystemPaletteSnapshot = true;
        QElapsedTimer total;
        quint64 cycle = 0;
        if (AppearanceTrace::enabled()) {
            total.start();
            cycle = AppearanceTrace::beginCycle(
                QStringLiteral("application-palette-change"),
                QStringLiteral("requested=system effective-before=%1 windows=%2")
                    .arg(isDark() ? QStringLiteral("dark") : QStringLiteral("light"))
                    .arg(QApplication::topLevelWidgets().size()));
        }
        updateEffectiveAppearance(true);
        if (cycle != 0) {
            AppearanceTrace::endCycle(cycle, total.elapsed(),
                QStringLiteral("effective-after=%1")
                    .arg(isDark() ? QStringLiteral("dark") : QStringLiteral("light")));
        }
    }
    else if (event->type() == QEvent::Show || event->type() == QEvent::WinIdChange) {
        if (QWidget *widget = qobject_cast<QWidget *>(watched); widget && widget->isWindow())
            applyNativeAppearance(widget);
    }

    return QObject::eventFilter(watched, event);
}

void AppearanceManager::applyNativeAppearance()
{
    for (QWidget *window : QApplication::topLevelWidgets())
        applyNativeAppearance(window);
}

void AppearanceManager::applyNativeAppearance(QWidget *window)
{
    WindowsAppearance::applyToWindow(window, isDark(), mode == Mode::System,
                                     currentTokens.surfaceShell, currentTokens.textPrimary,
                                     currentTokens.borderDefault);
}

void AppearanceManager::updateEffectiveAppearance(bool forceUpdate)
{
    if (updatingAppearance)
        return;
    QScopedValueRollback<bool> guard(updatingAppearance, true);

    EffectiveAppearance resolved;
    {
        AppearanceTrace::Scope trace(QStringLiteral("resolve-effective-appearance"));
        resolved = resolveEffectiveAppearance();
    }
    if (effective == resolved && !forceUpdate)
        return;

    effective = resolved;
    {
        AppearanceTrace::Scope trace(QStringLiteral("application-palette"));
        applyApplicationAppearance();
    }
    {
        AppearanceTrace::Scope trace(QStringLiteral("native-windows"),
            QStringLiteral("windows=%1").arg(QApplication::topLevelWidgets().size()));
        applyNativeAppearance();
    }
    {
        AppearanceTrace::Scope trace(QStringLiteral("synchronous-slots"));
        emit effectiveAppearanceChanged(effective);
    }
}

AppearanceTokens AppearanceManager::darkTokens()
{
    return {
        QColor(QStringLiteral("#121314")), QColor(QStringLiteral("#191A1B")),
        QColor(QStringLiteral("#202122")), QColor(QStringLiteral("#242526")),
        QColor(QStringLiteral("#333536")), QColor(QStringLiteral("#BFBFBF")),
        QColor(QStringLiteral("#BBBEBF")), QColor(QStringLiteral("#8C8C8C")),
        QColor(QStringLiteral("#555555")), QColor(QStringLiteral("#297AA0")),
        QColor(QStringLiteral("#2B7DA3")), QColor(57, 148, 188, 179),
        QColor(39, 103, 130, 221), QColor(39, 103, 130, 96),
        QColor(QStringLiteral("#F48771")), QColor(QStringLiteral("#CCA700")),
        QColor(QStringLiteral("#72C892")), QColor(QStringLiteral("#75BEFF")),
        QColor(QStringLiteral("#BBBEBF")), QColor(QStringLiteral("#242526")),
        QColor(140, 140, 140, 77), QColor(QStringLiteral("#404040")),
        QColor(QStringLiteral("#707070")), QColor(QStringLiteral("#297AA0")),
        QColor(QStringLiteral("#F48771")), QColor(QStringLiteral("#8B949E")),
        QColor(QStringLiteral("#A5D6FF")), QColor(QStringLiteral("#B5CEA8")),
        QColor(QStringLiteral("#FF7B72")), QColor(QStringLiteral("#C586C0")),
        QColor(QStringLiteral("#D2A8FF")), QColor(QStringLiteral("#4EC9B0")),
        QColor(QStringLiteral("#FFA657")), QColor(QStringLiteral("#79C0FF")),
        QColor(QStringLiteral("#7EE787")), QColor(QStringLiteral("#9CDCFE")),
        QColor(QStringLiteral("#72C892")), QColor(QStringLiteral("#0078D4")),
        QColor(QStringLiteral("#F28772")), QColor(52, 125, 57, 38),
        QColor(201, 60, 55, 38)
    };
}

AppearanceTokens AppearanceManager::lightTokens()
{
    return {
        QColor(QStringLiteral("#FFFFFF")), QColor(QStringLiteral("#F0F0F0")),
        QColor(QStringLiteral("#FFFFFF")), QColor(QStringLiteral("#E5F3FF")),
        QColor(QStringLiteral("#A0A0A0")), QColor(QStringLiteral("#000000")),
        QColor(QStringLiteral("#000000")), QColor(QStringLiteral("#555555")),
        QColor(QStringLiteral("#999999")), QColor(QStringLiteral("#0078D4")),
        QColor(QStringLiteral("#106EBE")), QColor(QStringLiteral("#0078D4")),
        QColor(0, 120, 212, 128), QColor(128, 128, 128, 80),
        QColor(QStringLiteral("#A1260D")), QColor(QStringLiteral("#8A6D00")),
        QColor(QStringLiteral("#107C10")), QColor(QStringLiteral("#0067B8")),
        QColor(QStringLiteral("#000000")), QColor(QStringLiteral("#FFF4CE")),
        QColor(QStringLiteral("#A0A0A0")), QColor(QStringLiteral("#C0C0C0")),
        QColor(QStringLiteral("#707070")), QColor(QStringLiteral("#0078D4")),
        QColor(QStringLiteral("#A1260D")), QColor(QStringLiteral("#008000")),
        QColor(QStringLiteral("#A31515")), QColor(QStringLiteral("#098658")),
        QColor(QStringLiteral("#0000FF")), QColor(QStringLiteral("#AF00DB")),
        QColor(QStringLiteral("#795E26")), QColor(QStringLiteral("#267F99")),
        QColor(QStringLiteral("#001080")), QColor(QStringLiteral("#0070C1")),
        QColor(QStringLiteral("#800000")), QColor(QStringLiteral("#E50000")),
        QColor(QStringLiteral("#2F9E44")), QColor(QStringLiteral("#0078D4")),
        QColor(QStringLiteral("#E03131")), QColor(47, 158, 68, 38),
        QColor(224, 49, 49, 38)
    };
}

AppearanceTokens AppearanceManager::systemTokens(const QPalette &palette,
                                                 EffectiveAppearance effectiveAppearance,
                                                 bool highContrast)
{
    AppearanceTokens tokens = effectiveAppearance == EffectiveAppearance::Dark
        ? darkTokens()
        : lightTokens();
    const QColor text = palette.color(QPalette::Text);
    const QColor highlight = palette.color(QPalette::Highlight);
    tokens.surfaceEditor = palette.color(QPalette::Base);
    tokens.surfaceShell = palette.color(QPalette::Window);
    tokens.surfaceRaised = palette.color(QPalette::Button);
    tokens.surfaceHover = palette.color(QPalette::AlternateBase);
    tokens.borderDefault = palette.color(QPalette::Mid);
    tokens.textPrimary = palette.color(QPalette::WindowText);
    tokens.textEditor = text;
    tokens.textSecondary = palette.color(QPalette::PlaceholderText);
    tokens.textDisabled = palette.color(QPalette::Disabled, QPalette::Text);
    tokens.accentPrimary = highlight;
    tokens.accentHover = highlight;
    tokens.focusBorder = highlight;
    tokens.selectionActive = highlight;
    tokens.selectionInactive = palette.color(QPalette::Inactive, QPalette::Highlight);
    tokens.editorCaret = text;
    tokens.editorCurrentLine = palette.color(QPalette::AlternateBase);
    tokens.editorWhitespace = palette.color(QPalette::PlaceholderText);
    tokens.editorIndentGuide = palette.color(QPalette::Mid);
    tokens.editorIndentGuideActive = palette.color(QPalette::Dark);
    tokens.editorBraceMatch = highlight;
    tokens.editorBraceError = palette.color(QPalette::BrightText);

    if (highContrast) {
        tokens.stateError = palette.color(QPalette::BrightText);
        tokens.stateWarning = palette.color(QPalette::LinkVisited);
        tokens.stateSuccess = palette.color(QPalette::Link);
        tokens.stateInformation = palette.color(QPalette::Link);
        tokens.syntaxComment = text;
        tokens.syntaxString = text;
        tokens.syntaxNumber = text;
        tokens.syntaxKeyword = text;
        tokens.syntaxControlFlow = text;
        tokens.syntaxFunction = text;
        tokens.syntaxType = text;
        tokens.syntaxVariable = text;
        tokens.syntaxConstant = text;
        tokens.syntaxTag = text;
        tokens.syntaxAttribute = text;
        tokens.diffAddedMarker = palette.color(QPalette::Link);
        tokens.diffModifiedMarker = highlight;
        tokens.diffDeletedMarker = palette.color(QPalette::BrightText);
        tokens.diffAddedFill = palette.color(QPalette::Link);
        tokens.diffDeletedFill = palette.color(QPalette::BrightText);
    }

    return tokens;
}

QPalette AppearanceManager::explicitPalette(const AppearanceTokens &tokens)
{
    QPalette palette;
    palette.setColor(QPalette::Window, tokens.surfaceShell);
    palette.setColor(QPalette::WindowText, tokens.textPrimary);
    palette.setColor(QPalette::Base, tokens.surfaceEditor);
    palette.setColor(QPalette::AlternateBase, tokens.surfaceHover);
    palette.setColor(QPalette::Text, tokens.textEditor);
    palette.setColor(QPalette::Button, tokens.surfaceRaised);
    palette.setColor(QPalette::ButtonText, tokens.textPrimary);
    palette.setColor(QPalette::ToolTipBase, tokens.surfaceRaised);
    palette.setColor(QPalette::ToolTipText, tokens.textPrimary);
    palette.setColor(QPalette::Highlight, tokens.accentPrimary);
    palette.setColor(QPalette::HighlightedText, tokens.textPrimary);
    palette.setColor(QPalette::Link, tokens.accentHover);
    palette.setColor(QPalette::LinkVisited, tokens.syntaxControlFlow);
    palette.setColor(QPalette::PlaceholderText, tokens.textSecondary);
    palette.setColor(QPalette::Light, tokens.surfaceHover);
    palette.setColor(QPalette::Midlight, tokens.surfaceRaised);
    palette.setColor(QPalette::Mid, tokens.borderDefault);
    palette.setColor(QPalette::Dark, tokens.surfaceEditor);
    palette.setColor(QPalette::Shadow, tokens.surfaceEditor);
    palette.setColor(QPalette::BrightText, tokens.stateError);
    palette.setColor(QPalette::Disabled, QPalette::WindowText, tokens.textDisabled);
    palette.setColor(QPalette::Disabled, QPalette::Text, tokens.textDisabled);
    palette.setColor(QPalette::Disabled, QPalette::ButtonText, tokens.textDisabled);
    palette.setColor(QPalette::Disabled, QPalette::Highlight, tokens.selectionInactive);
    return palette;
}
