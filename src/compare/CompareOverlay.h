/*
 * This file is part of Notepad Next.
 *
 * Notepad Next is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#pragma once

#include "CompareTypes.h"

#include <QByteArray>
#include <QMargins>
#include <QPointer>
#include <QWidget>

class ScintillaNext;

namespace Compare
{

class Overlay
{
public:
    void apply(ScintillaNext *leftEditor,
               ScintillaNext *rightEditor,
               const QVector<DiffHunk> &hunks);
    void setCurrent(const DiffHunk *hunk);
    void clear();
    qsizetype leadingGapLineCount(const ScintillaNext *editor) const;
    qsizetype visibleLeadingGapLineCount(const ScintillaNext *editor) const;
    void setLeadingGapVisibleLines(ScintillaNext *editor, qsizetype visibleLines);

private:
    struct MarginState
    {
        struct Annotation
        {
            qsizetype line = 0;
            QByteArray text;
            qintptr style = 0;
            QByteArray styles;
            qintptr markerHandle = -1;
        };

        QPointer<ScintillaNext> editor;
        qintptr type = 0;
        qintptr width = 0;
        qintptr mask = 0;
        qintptr background = 0;
        bool scrollWidthTracking = false;
        qintptr annotationVisible = 0;
        qintptr annotationStyleOffset = 0;
        QVector<Annotation> annotations;
        QMargins viewportMargins;
        QPointer<QWidget> topGapWidget;
        qsizetype leadingGapLines = 0;
        qsizetype visibleLeadingGapLines = 0;
    };

    static MarginState configureEditor(ScintillaNext *editor, bool suspendScrollWidthTracking);
    static void clearEditor(ScintillaNext *editor, const MarginState &marginState);
    static void addLines(ScintillaNext *editor, qsizetype start, qsizetype count, int marker);
    static void fillInlineRanges(ScintillaNext *editor,
                                 qsizetype lineStart,
                                 const QVector<InlineSpan> &spans,
                                 const QString &indicatorName);
    static void addGapLines(ScintillaNext *editor,
                            MarginState &marginState,
                            qsizetype anchorLine,
                            qsizetype count);
    static void addLeadingGap(ScintillaNext *editor, MarginState &marginState, qsizetype count);
    static void setCurrentMarker(ScintillaNext *editor, qsizetype line);
    static void setCurrentRange(ScintillaNext *editor, qsizetype start, qsizetype count);

    QPointer<ScintillaNext> leftEditor;
    QPointer<ScintillaNext> rightEditor;
    MarginState leftMarginState;
    MarginState rightMarginState;
};

}