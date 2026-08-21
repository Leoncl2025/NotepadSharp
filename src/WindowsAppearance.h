#pragma once

#include <QColor>

class QWidget;

namespace WindowsAppearance {

bool isHighContrast();
void applyToWindow(QWidget *window, bool dark, bool followSystem,
                   const QColor &caption, const QColor &text, const QColor &border);

}
