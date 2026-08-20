/*
 * This file is part of Notepad Next.
 *
 * Notepad Next is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "CompareOverlay.h"

#include "ScintillaNext.h"

#include <QEvent>
#include <QPainter>

#include <algorithm>

namespace Compare
{

namespace
{

constexpr int AddedBackgroundMarker = 13;
constexpr int DeletedBackgroundMarker = 14;
constexpr int AnnotationAnchorMarker = 12;
constexpr int AddedMarker = 16;
constexpr int DeletedMarker = 17;
constexpr int CurrentMarker = 19;
constexpr int CompareMargin = 3;
constexpr int CompareMarginWidth = 10;
constexpr int CompareMarkerMask = (1 << AddedBackgroundMarker)
    | (1 << DeletedBackgroundMarker)
    | (1 << AddedMarker)
    | (1 << DeletedMarker)
    | (1 << CurrentMarker);

constexpr int AddedColor = 0xC7E4B7;
constexpr int DeletedColor = 0xC1C1F7;
constexpr int InlineAddedColor = 0x9CCC9C;
constexpr int InlineDeletedColor = 0x8B8BE5;
constexpr int CurrentColor = 0xE97D2E;
constexpr int CompareMarginBackground = 0xF2F2F2;
constexpr int GapBackground = 0xF4F4F4;
constexpr int GapForeground = 0xD7D7D7;
constexpr auto AnnotationStyleProperty = "compareAnnotationStyle";

class TopGapWidget : public QWidget
{
public:
    TopGapWidget(ScintillaNext *editor, QMargins baseMargins, int lineCount, int lineHeight) :
        QWidget(editor),
        editor(editor),
        baseMargins(baseMargins),
        lineCount(lineCount),
        lineHeight(lineHeight)
    {
        setAttribute(Qt::WA_TransparentForMouseEvents);
        setProperty("compareGapLineCount", lineCount);
        editor->installEventFilter(this);
        setVisibleLineCount(lineCount);
        raise();
    }

    ~TopGapWidget() override
    {
        if (editor) {
            editor->removeEventFilter(this);
        }
    }

    void setVisibleLineCount(int visibleLineCount)
    {
        lineCount = std::clamp(visibleLineCount, 0, property("compareGapLineCount").toInt());
        QMargins margins = baseMargins;
        margins.setTop(margins.top() + lineCount * lineHeight);
        editor->setContentViewportMargins(margins);
        setVisible(lineCount > 0);
        syncGeometry();
        update();
    }

protected:
    bool eventFilter(QObject *watched, QEvent *event) override
    {
        if (watched == editor && (event->type() == QEvent::Resize || event->type() == QEvent::Show)) {
            syncGeometry();
        }
        return false;
    }

    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        painter.fillRect(rect(), QColor(244, 244, 244));
        painter.setPen(QColor(215, 215, 215));
        for (int x = -height(); x < width(); x += 8) {
            painter.drawLine(x, height(), x + height(), 0);
        }
        painter.setPen(QColor(232, 232, 232));
        for (int line = 1; line < lineCount; ++line) {
            painter.drawLine(0, line * lineHeight, width(), line * lineHeight);
        }
    }

private:
    void syncGeometry()
    {
        if (!editor) {
            return;
        }

        const QRect viewportGeometry = editor->viewport()->geometry();
        setGeometry(viewportGeometry.left(),
                    viewportGeometry.top() - lineCount * lineHeight,
                    viewportGeometry.width(),
                    lineCount * lineHeight);
    }

    QPointer<ScintillaNext> editor;
    QMargins baseMargins;
    int lineCount;
    int lineHeight;
};

const QString InlineAddedIndicator = QStringLiteral("compare_inline_added");
const QString InlineDeletedIndicator = QStringLiteral("compare_inline_deleted");

}

void Overlay::apply(ScintillaNext *newLeftEditor,
                    ScintillaNext *newRightEditor,
                    const QVector<DiffHunk> &hunks)
{
    clear();

    leftEditor = newLeftEditor;
    rightEditor = newRightEditor;
    leftMarginState = configureEditor(leftEditor);
    rightMarginState = configureEditor(rightEditor);

    for (const DiffHunk &hunk : hunks) {
        if (hunk.kind == ChangeKind::Added) {
            addGapLines(leftEditor, leftMarginState, hunk.leftStart - 1, hunk.rightCount);
            addLines(rightEditor, hunk.rightStart, hunk.rightCount, AddedBackgroundMarker);
            addLines(rightEditor, hunk.rightStart, hunk.rightCount, AddedMarker);
        }
        else if (hunk.kind == ChangeKind::Deleted) {
            addGapLines(rightEditor, rightMarginState, hunk.rightStart - 1, hunk.leftCount);
            addLines(leftEditor, hunk.leftStart, hunk.leftCount, DeletedBackgroundMarker);
            addLines(leftEditor, hunk.leftStart, hunk.leftCount, DeletedMarker);
        }
        else {
            addLines(leftEditor, hunk.leftStart, hunk.leftCount, DeletedBackgroundMarker);
            addLines(rightEditor, hunk.rightStart, hunk.rightCount, AddedBackgroundMarker);
            addLines(leftEditor, hunk.leftStart, hunk.leftCount, DeletedMarker);
            addLines(rightEditor, hunk.rightStart, hunk.rightCount, AddedMarker);
            fillInlineRanges(
                leftEditor, hunk.leftStart, hunk.leftInlineSpans, InlineDeletedIndicator);
            fillInlineRanges(
                rightEditor, hunk.rightStart, hunk.rightInlineSpans, InlineAddedIndicator);

            if (hunk.leftCount < hunk.rightCount) {
                addGapLines(leftEditor,
                            leftMarginState,
                            hunk.leftStart + hunk.leftCount - 1,
                            hunk.rightCount - hunk.leftCount);
            }
            else if (hunk.rightCount < hunk.leftCount) {
                addGapLines(rightEditor,
                            rightMarginState,
                            hunk.rightStart + hunk.rightCount - 1,
                            hunk.leftCount - hunk.rightCount);
            }
        }
    }
}

void Overlay::setCurrent(const DiffHunk *hunk)
{
    if (leftEditor) {
        leftEditor->markerDeleteAll(CurrentMarker);
        const int indicator = leftEditor->allocateIndicator(QStringLiteral("compare_current"));
        leftEditor->setIndicatorCurrent(indicator);
        leftEditor->indicatorClearRange(0, leftEditor->length());
    }
    if (rightEditor) {
        rightEditor->markerDeleteAll(CurrentMarker);
        const int indicator = rightEditor->allocateIndicator(QStringLiteral("compare_current"));
        rightEditor->setIndicatorCurrent(indicator);
        rightEditor->indicatorClearRange(0, rightEditor->length());
    }

    if (hunk == nullptr) {
        return;
    }

    setCurrentMarker(leftEditor, hunk->leftStart);
    setCurrentMarker(rightEditor, hunk->rightStart);
    setCurrentRange(leftEditor, hunk->leftStart, hunk->leftCount);
    setCurrentRange(rightEditor, hunk->rightStart, hunk->rightCount);
}

void Overlay::clear()
{
    clearEditor(leftEditor, leftMarginState);
    clearEditor(rightEditor, rightMarginState);
    leftEditor.clear();
    rightEditor.clear();
    leftMarginState = {};
    rightMarginState = {};
}

qsizetype Overlay::leadingGapLineCount(const ScintillaNext *editor) const
{
    if (leftMarginState.editor == editor) {
        return leftMarginState.leadingGapLines;
    }
    if (rightMarginState.editor == editor) {
        return rightMarginState.leadingGapLines;
    }
    return 0;
}

qsizetype Overlay::visibleLeadingGapLineCount(const ScintillaNext *editor) const
{
    if (leftMarginState.editor == editor) {
        return leftMarginState.visibleLeadingGapLines;
    }
    if (rightMarginState.editor == editor) {
        return rightMarginState.visibleLeadingGapLines;
    }
    return 0;
}

void Overlay::setLeadingGapVisibleLines(ScintillaNext *editor, qsizetype visibleLines)
{
    MarginState *state = nullptr;
    if (leftMarginState.editor == editor) {
        state = &leftMarginState;
    }
    else if (rightMarginState.editor == editor) {
        state = &rightMarginState;
    }

    if (state == nullptr || state->topGapWidget.isNull()) {
        return;
    }

    state->visibleLeadingGapLines = std::clamp<qsizetype>(visibleLines, 0, state->leadingGapLines);
    static_cast<TopGapWidget *>(state->topGapWidget.data())->setVisibleLineCount(
        static_cast<int>(state->visibleLeadingGapLines));
}

Overlay::MarginState Overlay::configureEditor(ScintillaNext *editor)
{
    if (editor == nullptr) {
        return {};
    }

    MarginState marginState;
    marginState.editor = editor;
    marginState.type = editor->marginTypeN(CompareMargin);
    marginState.width = editor->marginWidthN(CompareMargin);
    marginState.mask = editor->marginMaskN(CompareMargin);
    marginState.background = editor->marginBackN(CompareMargin);
    marginState.annotationVisible = editor->annotationVisible();
    marginState.annotationStyleOffset = editor->annotationStyleOffset();
    marginState.viewportMargins = editor->contentViewportMargins();

    editor->markerDefine(AnnotationAnchorMarker, SC_MARK_EMPTY);

    for (qsizetype line = 0; line < editor->lineCount(); ++line) {
        if (editor->annotationLines(line) <= 0) {
            continue;
        }

        marginState.annotations.append({
            line,
            editor->annotationText(line),
            editor->annotationStyle(line),
            editor->annotationStyles(line),
            editor->markerAdd(line, AnnotationAnchorMarker),
        });
    }

    editor->setMarginTypeN(CompareMargin, SC_MARGIN_SYMBOL);
    editor->setMarginWidthN(CompareMargin, CompareMarginWidth);
    editor->setMarginMaskN(CompareMargin, CompareMarkerMask);
    editor->setMarginBackN(CompareMargin, CompareMarginBackground);

    editor->annotationClearAll();

    if (marginState.topGapWidget) {
        delete marginState.topGapWidget.data();
    }
    int annotationStyle = editor->QObject::property(AnnotationStyleProperty).toInt();
    if (annotationStyle <= STYLE_MAX) {
        annotationStyle = editor->allocateExtendedStyles(1);
        editor->QObject::setProperty(AnnotationStyleProperty, annotationStyle);
    }
    const QByteArray defaultFont = editor->styleFont(STYLE_DEFAULT);
    if (!defaultFont.isEmpty()) {
        editor->styleSetFont(annotationStyle, defaultFont.constData());
    }
    editor->styleSetSizeFractional(annotationStyle, editor->styleSizeFractional(STYLE_DEFAULT));
    editor->styleSetWeight(annotationStyle, editor->styleWeight(STYLE_DEFAULT));
    editor->styleSetFore(annotationStyle, GapForeground);
    editor->styleSetBack(annotationStyle, GapBackground);
    editor->styleSetEOLFilled(annotationStyle, true);
    editor->annotationSetStyleOffset(annotationStyle);
    editor->annotationSetVisible(ANNOTATION_STANDARD);

    auto configureBackgroundMarker = [editor](int marker, int color) {
        editor->markerDefine(marker, SC_MARK_BACKGROUND);
        editor->markerSetBack(marker, color);
        editor->markerSetLayer(marker, SC_LAYER_UNDER_TEXT);
        editor->markerSetAlpha(marker, 150);
    };
    configureBackgroundMarker(AddedBackgroundMarker, AddedColor);
    configureBackgroundMarker(DeletedBackgroundMarker, DeletedColor);

    editor->markerDefine(AddedMarker, SC_MARK_FULLRECT);
    editor->markerSetFore(AddedMarker, AddedColor);
    editor->markerSetBack(AddedMarker, AddedColor);

    editor->markerDefine(DeletedMarker, SC_MARK_FULLRECT);
    editor->markerSetFore(DeletedMarker, DeletedColor);
    editor->markerSetBack(DeletedMarker, DeletedColor);

    editor->markerDefine(CurrentMarker, SC_MARK_SHORTARROW);
    editor->markerSetFore(CurrentMarker, 0xFFFFFF);
    editor->markerSetBack(CurrentMarker, CurrentColor);

    auto configureInlineIndicator = [editor](const QString &name, int color) {
        const int indicator = editor->allocateIndicator(name);
        editor->indicSetStyle(indicator, INDIC_FULLBOX);
        editor->indicSetFore(indicator, color);
        editor->indicSetAlpha(indicator, 230);
        editor->indicSetOutlineAlpha(indicator, 90);
        editor->indicSetUnder(indicator, true);
    };
    configureInlineIndicator(InlineAddedIndicator, InlineAddedColor);
    configureInlineIndicator(InlineDeletedIndicator, InlineDeletedColor);

    const int indicator = editor->allocateIndicator(QStringLiteral("compare_current"));
    editor->indicSetStyle(indicator, INDIC_STRAIGHTBOX);
    editor->indicSetFore(indicator, CurrentColor);
    editor->indicSetAlpha(indicator, 0);
    editor->indicSetOutlineAlpha(indicator, 220);
    editor->indicSetUnder(indicator, false);

    return marginState;
}

void Overlay::clearEditor(ScintillaNext *editor, const MarginState &marginState)
{
    if (editor == nullptr) {
        return;
    }

    editor->markerDeleteAll(AddedMarker);
    editor->markerDeleteAll(DeletedMarker);
    editor->markerDeleteAll(CurrentMarker);
    editor->markerDeleteAll(AddedBackgroundMarker);
    editor->markerDeleteAll(DeletedBackgroundMarker);
    editor->annotationClearAll();

    for (const QString &name : {InlineAddedIndicator, InlineDeletedIndicator}) {
        const int changeIndicator = editor->allocateIndicator(name);
        editor->setIndicatorCurrent(changeIndicator);
        editor->indicatorClearRange(0, editor->length());
    }

    const int indicator = editor->allocateIndicator(QStringLiteral("compare_current"));
    editor->setIndicatorCurrent(indicator);
    editor->indicatorClearRange(0, editor->length());

    if (marginState.editor == editor) {
        editor->setContentViewportMargins(marginState.viewportMargins);
        editor->setMarginTypeN(CompareMargin, marginState.type);
        editor->setMarginWidthN(CompareMargin, marginState.width);
        editor->setMarginMaskN(CompareMargin, marginState.mask);
        editor->setMarginBackN(CompareMargin, marginState.background);
        editor->annotationSetStyleOffset(marginState.annotationStyleOffset);
        editor->annotationSetVisible(marginState.annotationVisible);
        for (const MarginState::Annotation &annotation : marginState.annotations) {
            const qsizetype currentLine = editor->markerLineFromHandle(annotation.markerHandle);
            editor->markerDeleteHandle(annotation.markerHandle);
            if (currentLine < 0 || currentLine >= editor->lineCount()) {
                continue;
            }

            editor->annotationSetText(currentLine, annotation.text.constData());
            editor->annotationSetStyle(currentLine, annotation.style);
            if (!annotation.styles.isEmpty()) {
                editor->annotationSetStyles(currentLine, annotation.styles.constData());
            }
        }
    }
}

void Overlay::addLines(ScintillaNext *editor, qsizetype start, qsizetype count, int marker)
{
    if (editor == nullptr || count <= 0) {
        return;
    }

    const qsizetype end = std::min<qsizetype>(editor->lineCount(), start + count);
    for (qsizetype line = start; line < end; ++line) {
        editor->markerAdd(line, marker);
    }
}

void Overlay::fillInlineRanges(ScintillaNext *editor,
                               qsizetype lineStart,
                               const QVector<InlineSpan> &spans,
                               const QString &indicatorName)
{
    if (editor == nullptr || spans.isEmpty()) {
        return;
    }

    const int indicator = editor->allocateIndicator(indicatorName);
    editor->setIndicatorCurrent(indicator);
    for (const InlineSpan &span : spans) {
        const qsizetype line = lineStart + span.lineOffset;
        if (line < 0 || line >= editor->lineCount() || span.byteLength <= 0) {
            continue;
        }

        const qsizetype lineStartPosition = editor->positionFromLine(line);
        const qsizetype lineEndPosition = editor->lineEndPosition(line);
        const qsizetype startPosition = std::min(
            lineStartPosition + span.byteStart, lineEndPosition);
        const qsizetype endPosition = std::min(
            startPosition + span.byteLength, lineEndPosition);
        if (endPosition > startPosition) {
            editor->indicatorFillRange(startPosition, endPosition - startPosition);
        }
    }
}

void Overlay::addGapLines(ScintillaNext *editor,
                          MarginState &marginState,
                          qsizetype anchorLine,
                          qsizetype count)
{
    if (editor == nullptr || count <= 0 || editor->lineCount() <= 0) {
        return;
    }

    if (anchorLine < 0) {
        addLeadingGap(editor, marginState, count);
        return;
    }

    anchorLine = std::min(anchorLine, editor->lineCount() - 1);
    const QByteArray hatchUnit("/ ");
    const int annotationStyle = editor->QObject::property(AnnotationStyleProperty).toInt();
    const qsizetype hatchUnitWidth = std::max<qsizetype>(1, editor->textWidth(annotationStyle, hatchUnit.constData()));
    const qsizetype hatchCount = std::max<qsizetype>(8, editor->viewport()->width() / hatchUnitWidth + 2);
    QByteArray hatchLine;
    hatchLine.reserve(hatchCount * hatchUnit.size());
    for (qsizetype hatch = 0; hatch < hatchCount; ++hatch) {
        hatchLine.append(hatchUnit);
    }

    QByteArray annotation = editor->annotationText(anchorLine);
    for (qsizetype gapLine = 0; gapLine < count; ++gapLine) {
        if (!annotation.isEmpty()) {
            annotation.append('\n');
        }
        annotation.append(hatchLine);
    }

    editor->annotationSetText(anchorLine, annotation.constData());
    editor->annotationSetStyle(anchorLine, 0);
}

void Overlay::addLeadingGap(ScintillaNext *editor, MarginState &marginState, qsizetype count)
{
    const int existingCount = static_cast<int>(marginState.leadingGapLines);
    if (marginState.topGapWidget) {
        delete marginState.topGapWidget.data();
    }

    const int totalCount = existingCount + static_cast<int>(count);
    const int lineHeight = std::max(1, static_cast<int>(editor->textHeight(0)));
    marginState.leadingGapLines = totalCount;
    marginState.visibleLeadingGapLines = totalCount;
    marginState.topGapWidget = new TopGapWidget(
        editor, marginState.viewportMargins, totalCount, lineHeight);
}

void Overlay::setCurrentMarker(ScintillaNext *editor, qsizetype line)
{
    if (editor == nullptr || editor->lineCount() <= 0) {
        return;
    }

    line = std::clamp<qsizetype>(line, 0, editor->lineCount() - 1);
    editor->markerAdd(line, CurrentMarker);
}

void Overlay::setCurrentRange(ScintillaNext *editor, qsizetype start, qsizetype count)
{
    if (editor == nullptr || count <= 0 || start >= editor->lineCount()) {
        return;
    }

    const qsizetype endLine = std::min<qsizetype>(editor->lineCount(), start + count);
    const qsizetype startPosition = editor->positionFromLine(start);
    qsizetype endPosition = endLine < editor->lineCount()
        ? editor->positionFromLine(endLine)
        : editor->length();
    endPosition = std::max(endPosition, startPosition);

    const int indicator = editor->allocateIndicator(QStringLiteral("compare_current"));
    editor->setIndicatorCurrent(indicator);
    editor->indicatorFillRange(startPosition, endPosition - startPosition);
}

}