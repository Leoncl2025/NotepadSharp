#include "EditorAppearance.h"

#include "AppearanceManager.h"

#include "Scintilla.h"
#include "ScintillaEdit.h"

namespace {

constexpr int HiddenLinesUnderlineMarker = 21;

}

void EditorAppearance::apply(ScintillaEdit *editor, const AppearanceTokens &tokens,
                             const QString &fontName, int fontSize)
{
    editor->styleSetFore(STYLE_DEFAULT, AppearanceManager::scintillaColor(tokens.textEditor));
    editor->styleSetBack(STYLE_DEFAULT, AppearanceManager::scintillaColor(tokens.surfaceEditor));
    editor->styleSetSize(STYLE_DEFAULT, fontSize);
    editor->styleSetFont(STYLE_DEFAULT, fontName.toUtf8().data());
    editor->styleClearAll();

    for (int marker = SC_MARKNUM_FOLDEREND; marker <= SC_MARKNUM_FOLDEROPEN; ++marker) {
        editor->markerSetFore(marker, AppearanceManager::scintillaColor(tokens.surfaceEditor));
        editor->markerSetBack(marker, AppearanceManager::scintillaColor(tokens.textSecondary));
        editor->markerSetBackSelected(marker, AppearanceManager::scintillaColor(tokens.accentPrimary));
    }

    editor->markerSetBack(HiddenLinesUnderlineMarker,
                          AppearanceManager::scintillaColor(tokens.stateSuccess));
    editor->setEdgeColour(AppearanceManager::scintillaColor(tokens.borderDefault));
    editor->setElementColour(SC_ELEMENT_SELECTION_BACK,
                             AppearanceManager::scintillaElementColor(tokens.selectionActive));
    editor->setElementColour(SC_ELEMENT_SELECTION_INACTIVE_BACK,
                             AppearanceManager::scintillaElementColor(tokens.selectionInactive));
    editor->setElementColour(SC_ELEMENT_CARET_LINE_BACK,
                             AppearanceManager::scintillaElementColor(tokens.editorCurrentLine));
    editor->setElementColour(SC_ELEMENT_WHITE_SPACE,
                             AppearanceManager::scintillaElementColor(tokens.editorWhitespace));
    editor->setElementColour(SC_ELEMENT_FOLD_LINE,
                             AppearanceManager::scintillaElementColor(tokens.editorIndentGuide));
    editor->setFoldMarginColour(true, AppearanceManager::scintillaColor(tokens.surfaceShell));
    editor->setFoldMarginHiColour(true, AppearanceManager::scintillaColor(tokens.surfaceRaised));
    editor->setCaretFore(AppearanceManager::scintillaColor(tokens.editorCaret));

    applyNamedStyles(editor, tokens);
}

void EditorAppearance::applyNamedStyles(ScintillaEdit *editor, const AppearanceTokens &tokens)
{
    editor->styleSetFore(STYLE_LINENUMBER,
                         AppearanceManager::scintillaColor(tokens.textSecondary));
    editor->styleSetBack(STYLE_LINENUMBER,
                         AppearanceManager::scintillaColor(tokens.surfaceShell));
    editor->styleSetBold(STYLE_LINENUMBER, false);
    editor->styleSetFore(STYLE_BRACELIGHT,
                         AppearanceManager::scintillaColor(tokens.textPrimary));
    editor->styleSetBack(STYLE_BRACELIGHT,
                         AppearanceManager::scintillaColor(tokens.editorBraceMatch));
    editor->styleSetFore(STYLE_BRACEBAD,
                         AppearanceManager::scintillaColor(tokens.textPrimary));
    editor->styleSetBack(STYLE_BRACEBAD,
                         AppearanceManager::scintillaColor(tokens.editorBraceError));
    editor->styleSetFore(STYLE_INDENTGUIDE,
                         AppearanceManager::scintillaColor(tokens.editorIndentGuide));
    editor->styleSetBack(STYLE_INDENTGUIDE,
                         AppearanceManager::scintillaColor(tokens.surfaceEditor));
}
