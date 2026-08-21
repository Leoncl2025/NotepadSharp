/*
 * This file is part of Notepad Next.
 * Copyright 2019 Justin Dailey
 *
 * Notepad Next is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Notepad Next is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Notepad Next.  If not, see <https://www.gnu.org/licenses/>.
 */


#include <QPainter>

#include "AppearanceManager.h"
#include "HighlightedScrollBar.h"


using namespace Scintilla;

const int DEFAULT_TICK_HEIGHT = 3;
const int DEFAULT_TICK_PADDING = 3;
HighlightedScrollBarDecorator::HighlightedScrollBarDecorator(
        ScintillaNext *editor, AppearanceManager *appearanceManager)
        : EditorDecorator(editor),
            scrollBar(new HighlightedScrollBar(editor, appearanceManager, Qt::Vertical, editor))
{
    connect(scrollBar, &QScrollBar::valueChanged, editor, &ScintillaEditBase::scrollVertical);

    editor->setVerticalScrollBar(scrollBar);
}

HighlightedScrollBarDecorator::~HighlightedScrollBarDecorator()
{
}

void HighlightedScrollBarDecorator::notify(const NotificationData *pscn)
{
    if (pscn->nmhdr.code == Notification::UpdateUI && (FlagSet(pscn->updated, Update::Content) || FlagSet(pscn->updated, Update::Selection))) {
        scrollBar->update();
    }
    else if (pscn->nmhdr.code == Notification::Modified && FlagSet(pscn->modificationType, ModificationFlags::ChangeMarker)) {
        scrollBar->update();
    }
}




HighlightedScrollBar::HighlightedScrollBar(ScintillaNext *editor,
                                                                                     AppearanceManager *appearanceManager,
                                                                                     Qt::Orientation orientation,
                                                                                     QWidget *parent)
        : QScrollBar(orientation, parent),
            editor(editor),
            appearanceManager(appearanceManager)
{
    smartHighlighterIndicator = editor->allocateIndicator("smart_highlighter");
}

void HighlightedScrollBar::paintEvent(QPaintEvent *event)
{
    // Paint the default scrollbar first
    QScrollBar::paintEvent(event);
    QPainter p(this);
    const AppearanceTokens &tokens = appearanceManager->tokens();

    drawMarker(p, 24, tokens.diffModifiedMarker);
    drawMarker(p, 16, tokens.diffAddedMarker, 5);
    drawMarker(p, 17, tokens.diffDeletedMarker, 5);
    drawMarker(p, 19, tokens.accentPrimary, 7);
    drawIndicator(p, smartHighlighterIndicator);
    drawCursors(p);
}

void HighlightedScrollBar::drawMarker(QPainter &p, int marker, QColor color, int height)
{
    int curLine = 0;

    while ((curLine = editor->markerNext(curLine, 1 << marker)) != -1) {
        drawTickMark(p, lineToScrollBarY(curLine), height, color);

        curLine++;
    }
}

void HighlightedScrollBar::drawIndicator(QPainter &p, int indicator)
{
    int curPos = editor->indicatorEnd(indicator, 0);
    int color = editor->indicFore(indicator);

    if (curPos > 0) {
        while ((curPos = editor->indicatorEnd(indicator, curPos)) < editor->length()) {
            drawTickMark(p, posToScrollBarY(curPos), DEFAULT_TICK_HEIGHT, color);

            curPos = editor->indicatorEnd(indicator, curPos);
        }
    }
}

void HighlightedScrollBar::drawCursors(QPainter &p)
{
    QColor selectionColor = appearanceManager->tokens().selectionInactive;
    QColor caretColor = appearanceManager->tokens().editorCaret;
    caretColor.setAlpha(130);
    for (int i = 0; i < editor->selections() ; i++) {
        int startCaretY = posToScrollBarY(editor->selectionNCaret(i));
        int startAnchorY = posToScrollBarY(editor->selectionNAnchor(i));

        if (startCaretY != startAnchorY) {
            drawTickMark(p, startAnchorY, startCaretY - startAnchorY, selectionColor);
        }

        drawTickMark(p, startCaretY, DEFAULT_TICK_HEIGHT, caretColor);
    }
}

void HighlightedScrollBar::drawTickMark(QPainter &p, int y, int height, QColor color)
{
    p.fillRect(rect().x() + DEFAULT_TICK_PADDING, y + scrollbarArrowHeight(), rect().width() - (DEFAULT_TICK_PADDING * 2), height, color);
}

int HighlightedScrollBar::posToScrollBarY(int pos) const
{
    int line = editor->visibleFromDocLine(editor->lineFromPosition(pos));

    return lineToScrollBarY(line);
}

int HighlightedScrollBar::lineToScrollBarY(int line) const
{
    int lineCount = editor->visibleFromDocLine(editor->lineCount());

    if (!editor->endAtLastLine()) {
        lineCount += editor->linesOnScreen();
    }

    return static_cast<double>(line) / lineCount * (rect().height() - scrollbarArrowHeight() * 2);
}

int HighlightedScrollBar::scrollbarArrowHeight() const
{
    // NOTE: There is no official way to get the height of the scrollbar arrow buttons, however for now we can
    // assume that the buttons are square, meaning the height of them will be the same as the width of
    // the scroll bar.
    return rect().width();
}
