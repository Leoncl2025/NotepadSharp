#include <QTemporaryDir>
#include <QtTest>

#include "AppearanceManager.h"
#include "ApplicationSettings.h"
#include "EditorAppearance.h"

#include "Scintilla.h"
#include "ScintillaEdit.h"

class EditorAppearanceTests : public QObject
{
    Q_OBJECT

private slots:
    void appliesDarkTokensAndPreservesDocumentState();
    void restoresNamedStylesAfterStyleClearAll();
    void switchesTheSameEditorBackToLight();
};

void EditorAppearanceTests::appliesDarkTokensAndPreservesDocumentState()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationSettings settings(directory.filePath(QStringLiteral("settings.ini")), QSettings::IniFormat);
    AppearanceManager manager(&settings, [] { return Qt::ColorScheme::Light; });
    manager.setRequestedMode(AppearanceManager::Mode::Dark);

    ScintillaEdit editor;
    editor.setUndoCollection(true);
    editor.setText("alpha\nbeta\n");
    editor.setSavePoint();
    editor.gotoPos(editor.length());
    editor.addText(1, "!");
    editor.setSelection(5, 1);

    const QByteArray text = editor.textRange(0, editor.length());
    const sptr_t currentPosition = editor.currentPos();
    const sptr_t anchor = editor.anchor();
    const bool modified = editor.modify();
    const bool canUndo = editor.canUndo();

    EditorAppearance::apply(&editor, manager.tokens(), QStringLiteral("Courier New"), 11);

    QCOMPARE(editor.styleFore(STYLE_DEFAULT),
             static_cast<sptr_t>(AppearanceManager::scintillaColor(manager.tokens().textEditor)));
    QCOMPARE(editor.styleBack(STYLE_DEFAULT),
             static_cast<sptr_t>(AppearanceManager::scintillaColor(manager.tokens().surfaceEditor)));
    QCOMPARE(static_cast<quint32>(editor.elementColour(SC_ELEMENT_SELECTION_BACK)),
             AppearanceManager::scintillaElementColor(manager.tokens().selectionActive));
    QCOMPARE(editor.textRange(0, editor.length()), text);
    QCOMPARE(editor.currentPos(), currentPosition);
    QCOMPARE(editor.anchor(), anchor);
    QCOMPARE(editor.modify(), modified);
    QCOMPARE(editor.canUndo(), canUndo);
}

void EditorAppearanceTests::restoresNamedStylesAfterStyleClearAll()
{
    QTemporaryDir directory;
    ApplicationSettings settings(directory.filePath(QStringLiteral("settings.ini")), QSettings::IniFormat);
    AppearanceManager manager(&settings, [] { return Qt::ColorScheme::Dark; });
    manager.setRequestedMode(AppearanceManager::Mode::Dark);

    ScintillaEdit editor;
    EditorAppearance::apply(&editor, manager.tokens(), QStringLiteral("Courier New"), 11);
    editor.styleClearAll();
    QVERIFY(editor.styleBack(STYLE_LINENUMBER)
            != AppearanceManager::scintillaColor(manager.tokens().surfaceShell));

    EditorAppearance::applyNamedStyles(&editor, manager.tokens());

    QCOMPARE(editor.styleFore(STYLE_LINENUMBER),
             static_cast<sptr_t>(AppearanceManager::scintillaColor(manager.tokens().textSecondary)));
    QCOMPARE(editor.styleBack(STYLE_LINENUMBER),
             static_cast<sptr_t>(AppearanceManager::scintillaColor(manager.tokens().surfaceShell)));
    QCOMPARE(editor.styleBack(STYLE_INDENTGUIDE),
             static_cast<sptr_t>(AppearanceManager::scintillaColor(manager.tokens().surfaceEditor)));
}

void EditorAppearanceTests::switchesTheSameEditorBackToLight()
{
    QTemporaryDir directory;
    ApplicationSettings settings(directory.filePath(QStringLiteral("settings.ini")), QSettings::IniFormat);
    AppearanceManager manager(&settings, [] { return Qt::ColorScheme::Dark; });
    ScintillaEdit editor;

    manager.setRequestedMode(AppearanceManager::Mode::Dark);
    EditorAppearance::apply(&editor, manager.tokens(), QStringLiteral("Courier New"), 11);
    QCOMPARE(editor.styleBack(STYLE_DEFAULT),
             static_cast<sptr_t>(AppearanceManager::scintillaColor(QColor(QStringLiteral("#121314")))));

    manager.setRequestedMode(AppearanceManager::Mode::Light);
    EditorAppearance::apply(&editor, manager.tokens(), QStringLiteral("Courier New"), 11);
    QCOMPARE(editor.styleBack(STYLE_DEFAULT),
             static_cast<sptr_t>(AppearanceManager::scintillaColor(QColor(QStringLiteral("#FFFFFF")))));
}

QTEST_MAIN(EditorAppearanceTests)

#include "EditorAppearanceTests.moc"