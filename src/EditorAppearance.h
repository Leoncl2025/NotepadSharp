#pragma once

#include <QString>

struct AppearanceTokens;
class ScintillaEdit;
class ScintillaNext;

namespace EditorAppearance {

void apply(ScintillaEdit *editor, const AppearanceTokens &tokens,
           const QString &fontName, int fontSize);
void applyNamedStyles(ScintillaEdit *editor, const AppearanceTokens &tokens);
void applyDecorations(ScintillaNext *editor, const AppearanceTokens &tokens);

}
