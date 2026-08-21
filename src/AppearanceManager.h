#pragma once

#include <functional>

#include <QColor>
#include <QObject>
#include <QPalette>
#include <QString>
#include <Qt>

class ApplicationSettings;
class QWidget;

struct AppearanceTokens
{
    QColor surfaceEditor;
    QColor surfaceShell;
    QColor surfaceRaised;
    QColor surfaceHover;
    QColor borderDefault;
    QColor textPrimary;
    QColor textEditor;
    QColor textSecondary;
    QColor textDisabled;
    QColor accentPrimary;
    QColor accentHover;
    QColor focusBorder;
    QColor selectionActive;
    QColor selectionInactive;
    QColor stateError;
    QColor stateWarning;
    QColor stateSuccess;
    QColor stateInformation;
    QColor editorCaret;
    QColor editorCurrentLine;
    QColor editorWhitespace;
    QColor editorIndentGuide;
    QColor editorIndentGuideActive;
    QColor editorBraceMatch;
    QColor editorBraceError;
    QColor syntaxComment;
    QColor syntaxString;
    QColor syntaxNumber;
    QColor syntaxKeyword;
    QColor syntaxControlFlow;
    QColor syntaxFunction;
    QColor syntaxType;
    QColor syntaxVariable;
    QColor syntaxConstant;
    QColor syntaxTag;
    QColor syntaxAttribute;
    QColor diffAddedMarker;
    QColor diffModifiedMarker;
    QColor diffDeletedMarker;
    QColor diffAddedFill;
    QColor diffDeletedFill;
};

class AppearanceManager : public QObject
{
    Q_OBJECT

public:
    enum class Mode {
        System,
        Light,
        Dark
    };
    Q_ENUM(Mode)

    enum class EffectiveAppearance {
        Light,
        Dark
    };
    Q_ENUM(EffectiveAppearance)

    using ColorSchemeProvider = std::function<Qt::ColorScheme()>;

    explicit AppearanceManager(ApplicationSettings *settings, QObject *parent = nullptr);
    AppearanceManager(ApplicationSettings *settings, ColorSchemeProvider colorSchemeProvider,
                      QObject *parent = nullptr);

    Mode requestedMode() const { return mode; }
    EffectiveAppearance effectiveAppearance() const { return effective; }
    bool isDark() const { return effective == EffectiveAppearance::Dark; }
    const AppearanceTokens &tokens() const { return currentTokens; }

    static Mode modeFromString(const QString &value);
    static QString modeToString(Mode mode);
    static EffectiveAppearance resolve(Mode mode, Qt::ColorScheme systemScheme,
                                       EffectiveAppearance unknownFallback);
    static int scintillaColor(const QColor &color);
    static unsigned int scintillaElementColor(const QColor &color);

public slots:
    void setRequestedMode(Mode requestedMode);
    void refreshSystemAppearance();

signals:
    void effectiveAppearanceChanged(AppearanceManager::EffectiveAppearance appearance);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void onAppearanceSettingChanged(const QString &value);
    EffectiveAppearance resolveEffectiveAppearance() const;
    EffectiveAppearance paletteFallback() const;
    void restoreSystemPalette();
    void applyApplicationAppearance();
    void applyNativeAppearance();
    void applyNativeAppearance(QWidget *window);
    void updateEffectiveAppearance(bool forceUpdate = false);

    static AppearanceTokens darkTokens();
    static AppearanceTokens lightTokens();
    static AppearanceTokens systemTokens(const QPalette &palette,
                                         EffectiveAppearance effectiveAppearance,
                                         bool highContrast);
    static QPalette explicitPalette(const AppearanceTokens &tokens);

    ApplicationSettings *settings;
    ColorSchemeProvider colorSchemeProvider;
    Mode mode;
    EffectiveAppearance effective;
    AppearanceTokens currentTokens;
    bool applyingPalette = false;
    bool updatingAppearance = false;
};
