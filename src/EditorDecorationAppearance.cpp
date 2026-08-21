#include "EditorAppearance.h"

#include "AppearanceManager.h"
#include "ScintillaNext.h"

void EditorAppearance::applyDecorations(ScintillaNext *editor, const AppearanceTokens &tokens)
{
    editor->markerSetFore(24, AppearanceManager::scintillaColor(tokens.diffModifiedMarker));
    editor->markerSetBack(24, AppearanceManager::scintillaColor(tokens.diffModifiedMarker));

    const int braceHighlight = editor->allocateIndicator("brace_highlight");
    const int braceBadlight = editor->allocateIndicator("brace_badlight");
    const int smartHighlighter = editor->allocateIndicator("smart_highlighter");
    const int urlFinder = editor->allocateIndicator("url_finder");
    editor->indicSetFore(braceHighlight,
                         AppearanceManager::scintillaColor(tokens.editorBraceMatch));
    editor->indicSetFore(braceBadlight,
                         AppearanceManager::scintillaColor(tokens.editorBraceError));
    editor->indicSetFore(smartHighlighter,
                         AppearanceManager::scintillaColor(tokens.accentHover));
    editor->indicSetFore(urlFinder, AppearanceManager::scintillaColor(tokens.accentHover));
    editor->indicSetHoverFore(urlFinder,
                              AppearanceManager::scintillaColor(tokens.accentPrimary));

    const QColor markerColors[] = {
        tokens.syntaxString,
        tokens.syntaxVariable,
        tokens.syntaxTag,
    };
    for (int index = 0; index < 3; ++index) {
        const int indicator = editor->allocateIndicator(
            QStringLiteral("marker_%1").arg(index));
        editor->indicSetFore(indicator,
                             AppearanceManager::scintillaColor(markerColors[index]));
    }
}
