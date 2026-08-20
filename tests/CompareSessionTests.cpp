/*
 * This file is part of Notepad Next.
 *
 * Notepad Next is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "CompareNavigator.h"
#include "CompareSession.h"
#include "CompareToolBar.h"
#include "DockedEditor.h"
#include "ScintillaNext.h"

#include <QAction>
#include <QFile>
#include <QIcon>
#include <QLabel>
#include <QMainWindow>
#include <QPointer>
#include <QSplitter>
#include <QTemporaryDir>
#include <QTest>

using Compare::ChangeKind;
using Compare::DiffHunk;
using Compare::Navigator;
using Compare::Session;

class CompareSessionTests : public QObject
{
    Q_OBJECT

private slots:
    void navigationWrapsInBothDirections()
    {
        Navigator navigator;
        navigator.reset({
            {1, 1, 1, 1, ChangeKind::Modified},
            {4, 0, 4, 1, ChangeKind::Added},
        });

        QCOMPARE(navigator.next()->leftStart, 1);
        QCOMPARE(navigator.next()->leftStart, 4);
        QCOMPARE(navigator.next()->leftStart, 1);
        QCOMPARE(navigator.previous()->leftStart, 4);

        navigator.reset();
        QVERIFY(!navigator.hasDifferences());
        QVERIFY(navigator.next() == nullptr);
        QVERIFY(navigator.previous() == nullptr);
    }

    void previousEditorUsesOnlyTheCurrentTabArea()
    {
        QWidget host;
        host.resize(900, 600);
        DockedEditor dockedEditor(&host);
        auto *firstEditor = new ScintillaNext(QStringLiteral("first"));
        auto *secondEditor = new ScintillaNext(QStringLiteral("second"));
        auto *thirdEditor = new ScintillaNext(QStringLiteral("third"));

        dockedEditor.addEditor(firstEditor);
        dockedEditor.addEditor(secondEditor);
        dockedEditor.addEditor(thirdEditor);

        QVERIFY(dockedEditor.previousEditor(firstEditor) == nullptr);
        QCOMPARE(dockedEditor.previousEditor(secondEditor), firstEditor);
        QCOMPARE(dockedEditor.previousEditor(thirdEditor), secondEditor);

        dockedEditor.splitToRightOf(secondEditor, thirdEditor);
        QVERIFY(dockedEditor.previousEditor(thirdEditor) == nullptr);
        QCOMPARE(dockedEditor.previousEditor(secondEditor), firstEditor);

        const QIcon compareIcon(QStringLiteral(":/icons/git-compare-arrows.svg"));
        QVERIFY(!compareIcon.isNull());
    }

    void mapsInsertedRowsToTheirNearestBoundary()
    {
        const QVector<DiffHunk> hunks = {
            {2, 0, 2, 2, ChangeKind::Added},
        };

        QCOMPARE(Navigator::mapLine(1, hunks, true), 1);
        QCOMPARE(Navigator::mapLine(2, hunks, true), 4);
        QCOMPARE(Navigator::mapLine(2, hunks, false), 2);
        QCOMPARE(Navigator::mapLine(3, hunks, false), 2);
        QCOMPARE(Navigator::mapLine(4, hunks, false), 2);
    }

    void mapsDeletedRowsToTheirNearestBoundary()
    {
        const QVector<DiffHunk> hunks = {
            {2, 2, 2, 0, ChangeKind::Deleted},
        };

        QCOMPARE(Navigator::mapLine(2, hunks, true), 2);
        QCOMPARE(Navigator::mapLine(3, hunks, true), 2);
        QCOMPARE(Navigator::mapLine(4, hunks, true), 2);
        QCOMPARE(Navigator::mapLine(2, hunks, false), 4);
    }

    void mapsModifiedRowsAndFollowingContent()
    {
        const QVector<DiffHunk> hunks = {
            {2, 3, 2, 1, ChangeKind::Modified},
        };

        QCOMPARE(Navigator::mapLine(2, hunks, true), 2);
        QCOMPARE(Navigator::mapLine(4, hunks, true), 2);
        QCOMPARE(Navigator::mapLine(5, hunks, true), 3);
        QCOMPARE(Navigator::mapLine(2, hunks, false), 2);
        QCOMPARE(Navigator::mapLine(3, hunks, false), 5);
    }

    void mapsAcrossMultipleHunksCumulatively()
    {
        const QVector<DiffHunk> hunks = {
            {2, 0, 2, 1, ChangeKind::Added},
            {5, 0, 6, 1, ChangeKind::Added},
        };

        QCOMPARE(Navigator::mapLine(4, hunks, true), 5);
        QCOMPARE(Navigator::mapLine(5, hunks, true), 7);
        QCOMPARE(Navigator::mapLine(8, hunks, true), 10);
        QCOMPARE(Navigator::mapLine(10, hunks, false), 8);
    }

    void sessionAppliesAndClearsNonMutatingOverlays()
    {
        ScintillaNext leftEditor(QStringLiteral("left"));
        ScintillaNext rightEditor(QStringLiteral("right"));
        const qsizetype originalLeftMarginWidth = leftEditor.marginWidthN(3);
        const qsizetype originalRightMarginWidth = rightEditor.marginWidthN(3);
        const qsizetype originalLeftMarginMask = leftEditor.marginMaskN(3);
        const qsizetype originalRightMarginMask = rightEditor.marginMaskN(3);
        leftEditor.setText("zero\none\ntwo\nthree\n");
        rightEditor.setText("zero\nONE\ntwo\nfour\n");
        leftEditor.emptyUndoBuffer();
        rightEditor.emptyUndoBuffer();
        leftEditor.setSavePoint();
        rightEditor.setSavePoint();
        const QByteArray leftBefore = leftEditor.get_text_range(0, leftEditor.length());
        const QByteArray rightBefore = rightEditor.get_text_range(0, rightEditor.length());

        Session session;
        session.start(&leftEditor, &rightEditor);
        QTRY_COMPARE_WITH_TIMEOUT(session.state(), Session::State::Ready, 5000);
        QVERIFY(session.hasDifferences());
        QCOMPARE(session.differenceCount(), 2);
        QCOMPARE(session.addedCount(), 0);
        QCOMPARE(session.deletedCount(), 0);
        QCOMPARE(session.modifiedCount(), 2);
        QCOMPARE(session.currentDifferenceNumber(), 0);
        QVERIFY(leftEditor.markerGet(1) & (1 << 17));
        QVERIFY(rightEditor.markerGet(1) & (1 << 16));
        QVERIFY(leftEditor.markerGet(1) & (1 << 14));
        QVERIFY(rightEditor.markerGet(1) & (1 << 13));
        QVERIFY(leftEditor.markerGet(3) & (1 << 17));
        QVERIFY(rightEditor.markerGet(3) & (1 << 16));
        QCOMPARE(leftEditor.marginWidthN(3), 10);
        QCOMPARE(rightEditor.marginWidthN(3), 10);
        QVERIFY(leftEditor.marginMaskN(3) & (1 << 17));
        QVERIFY(rightEditor.marginMaskN(3) & (1 << 16));

        session.nextDifference();
        QCOMPARE(session.currentDifferenceNumber(), 1);
        QVERIFY(leftEditor.markerGet(1) & (1 << 19));
        QVERIFY(rightEditor.markerGet(1) & (1 << 19));
        session.nextDifference();
        QCOMPARE(session.currentDifferenceNumber(), 2);
        QCOMPARE(leftEditor.get_text_range(0, leftEditor.length()), leftBefore);
        QCOMPARE(rightEditor.get_text_range(0, rightEditor.length()), rightBefore);
        QVERIFY(!leftEditor.modify());
        QVERIFY(!rightEditor.modify());
        QVERIFY(!leftEditor.canUndo());
        QVERIFY(!rightEditor.canUndo());

        session.clear();
        QCOMPARE(session.state(), Session::State::None);
        QCOMPARE(leftEditor.markerGet(1) & (1 << 17), 0);
        QCOMPARE(rightEditor.markerGet(1) & (1 << 16), 0);
        QCOMPARE(leftEditor.markerGet(1) & (1 << 14), 0);
        QCOMPARE(rightEditor.markerGet(1) & (1 << 13), 0);
        QCOMPARE(leftEditor.markerGet(1) & (1 << 19), 0);
        QCOMPARE(rightEditor.markerGet(1) & (1 << 19), 0);
        QCOMPARE(leftEditor.marginWidthN(3), originalLeftMarginWidth);
        QCOMPARE(rightEditor.marginWidthN(3), originalRightMarginWidth);
        QCOMPARE(leftEditor.marginMaskN(3), originalLeftMarginMask);
        QCOMPARE(rightEditor.marginMaskN(3), originalRightMarginMask);
        QCOMPARE(leftEditor.get_text_range(0, leftEditor.length()), leftBefore);
        QCOMPARE(rightEditor.get_text_range(0, rightEditor.length()), rightBefore);
    }

    void sessionMarksContentChangesStaleAndRefreshes()
    {
        ScintillaNext leftEditor(QStringLiteral("left"));
        ScintillaNext rightEditor(QStringLiteral("right"));
        leftEditor.setText("alpha\nbeta\n");
        rightEditor.setText("alpha\nBETA\n");
        leftEditor.emptyUndoBuffer();
        rightEditor.emptyUndoBuffer();
        leftEditor.setSavePoint();
        rightEditor.setSavePoint();

        Session session;
        session.start(&leftEditor, &rightEditor);
        QTRY_COMPARE_WITH_TIMEOUT(session.state(), Session::State::Ready, 5000);
        QVERIFY(leftEditor.markerGet(1) & (1 << 17));

        const QByteArray change("gamma\n");
        leftEditor.appendText(change.size(), change.constData());
        QTRY_COMPARE_WITH_TIMEOUT(session.state(), Session::State::Stale, 5000);
        QCOMPARE(leftEditor.markerGet(1) & (1 << 17), 0);
        QCOMPARE(rightEditor.markerGet(1) & (1 << 16), 0);

        session.refresh();
        QTRY_COMPARE_WITH_TIMEOUT(session.state(), Session::State::Ready, 5000);
        QVERIFY(session.hasDifferences());
    }

    void visualMetadataDoesNotInvalidateTheSession()
    {
        ScintillaNext leftEditor(QStringLiteral("left"));
        ScintillaNext rightEditor(QStringLiteral("right"));
        leftEditor.setText("alpha\nbeta\n");
        rightEditor.setText("alpha\nBETA\n");

        Session session;
        session.start(&leftEditor, &rightEditor);
        QTRY_COMPARE_WITH_TIMEOUT(session.state(), Session::State::Ready, 5000);

        leftEditor.markerDefine(11, SC_MARK_DOTDOTDOT);
        leftEditor.markerAdd(0, 11);
        leftEditor.annotationSetText(0, "visual metadata");
        const int indicator = leftEditor.allocateIndicator(QStringLiteral("metadata_test"));
        leftEditor.setIndicatorCurrent(indicator);
        leftEditor.indicatorFillRange(0, 1);
        QCoreApplication::processEvents();

        QCOMPARE(session.state(), Session::State::Ready);
        QVERIFY(session.hasDifferences());
    }

    void sessionRendersAddedAndDeletedRanges()
    {
        ScintillaNext leftEditor(QStringLiteral("left"));
        ScintillaNext rightEditor(QStringLiteral("right"));
        leftEditor.setText("alpha\ngamma\n");
        rightEditor.setText("alpha\nbeta\ngamma\n");

        Session session;
        session.start(&leftEditor, &rightEditor);
        QTRY_COMPARE_WITH_TIMEOUT(session.state(), Session::State::Ready, 5000);
        QVERIFY(rightEditor.markerGet(1) & (1 << 16));
        QVERIFY(rightEditor.markerGet(1) & (1 << 13));
        QCOMPARE(leftEditor.annotationLines(0), 1);
        QCOMPARE(rightEditor.getLine(1), QByteArray("beta\n"));

        session.clear();
        QCOMPARE(leftEditor.annotationLines(0), 0);
        session.start(&rightEditor, &leftEditor);
        QTRY_COMPARE_WITH_TIMEOUT(session.state(), Session::State::Ready, 5000);
        QVERIFY(rightEditor.markerGet(1) & (1 << 17));
        QVERIFY(rightEditor.markerGet(1) & (1 << 14));
        QCOMPARE(leftEditor.annotationLines(0), 1);
        QCOMPARE(rightEditor.getLine(1), QByteArray("beta\n"));
    }

    void sessionAlignsUnequalModifiedHunksAndRestoresAnnotations()
    {
        ScintillaNext leftEditor(QStringLiteral("left"));
        ScintillaNext rightEditor(QStringLiteral("right"));
        leftEditor.setText("anchor\nold\ntail\n");
        rightEditor.setText("anchor\nnew-one\nnew-two\ntail\n");
        leftEditor.emptyUndoBuffer();
        rightEditor.emptyUndoBuffer();
        leftEditor.setSavePoint();
        rightEditor.setSavePoint();
        leftEditor.annotationSetVisible(ANNOTATION_BOXED);
        leftEditor.annotationSetText(2, "existing annotation");
        leftEditor.annotationSetStyle(2, 0);
        const QByteArray leftBefore = leftEditor.get_text_range(0, leftEditor.length());
        const QByteArray rightBefore = rightEditor.get_text_range(0, rightEditor.length());

        Session session;
        session.start(&leftEditor, &rightEditor);
        QTRY_COMPARE_WITH_TIMEOUT(session.state(), Session::State::Ready, 5000);
        QCOMPARE(leftEditor.annotationLines(1), 1);
        QCOMPARE(rightEditor.annotationLines(1), 0);
        QCOMPARE(leftEditor.get_text_range(0, leftEditor.length()), leftBefore);
        QCOMPARE(rightEditor.get_text_range(0, rightEditor.length()), rightBefore);
        QVERIFY(!leftEditor.modify());
        QVERIFY(!rightEditor.modify());
        QVERIFY(!leftEditor.canUndo());
        QVERIFY(!rightEditor.canUndo());

        session.clear();
        QCOMPARE(leftEditor.annotationLines(1), 0);
        QCOMPARE(leftEditor.annotationText(2), QByteArray("existing annotation"));
        QCOMPARE(leftEditor.annotationVisible(), ANNOTATION_BOXED);
        QCOMPARE(leftEditor.get_text_range(0, leftEditor.length()), leftBefore);
        QCOMPARE(rightEditor.get_text_range(0, rightEditor.length()), rightBefore);
    }

    void sessionAlignsLeadingAdditionsAndDeletions()
    {
        ScintillaNext leftEditor(QStringLiteral("left"));
        ScintillaNext rightEditor(QStringLiteral("right"));
        leftEditor.setText("anchor\ntail\n");
        rightEditor.setText("leading\nanchor\ntail\n");
        const QMargins originalLeftMargins = leftEditor.contentViewportMargins();
        const QMargins originalRightMargins = rightEditor.contentViewportMargins();

        Session session;
        session.start(&leftEditor, &rightEditor);
        QTRY_COMPARE_WITH_TIMEOUT(session.state(), Session::State::Ready, 5000);
        QCOMPARE(leftEditor.contentViewportMargins().top(),
                 originalLeftMargins.top() + leftEditor.textHeight(0));
        QCOMPARE(rightEditor.contentViewportMargins(), originalRightMargins);

        session.clear();
        QCOMPARE(leftEditor.contentViewportMargins(), originalLeftMargins);
        session.start(&rightEditor, &leftEditor);
        QTRY_COMPARE_WITH_TIMEOUT(session.state(), Session::State::Ready, 5000);
        QCOMPARE(leftEditor.contentViewportMargins().top(),
                 originalLeftMargins.top() + leftEditor.textHeight(0));
        QCOMPARE(rightEditor.contentViewportMargins(), originalRightMargins);
        session.clear();
        QCOMPARE(leftEditor.contentViewportMargins(), originalLeftMargins);
        QCOMPARE(rightEditor.contentViewportMargins(), originalRightMargins);
    }

    void leadingGapScrollsWithTheComparedContent()
    {
        QMainWindow window;
        auto *splitter = new QSplitter(&window);
        auto *leftEditor = new ScintillaNext(QStringLiteral("left"), splitter);
        auto *rightEditor = new ScintillaNext(QStringLiteral("right"), splitter);
        QByteArray leftText;
        QByteArray rightText("leading\n");
        for (int line = 0; line < 100; ++line) {
            const QByteArray text = QByteArray("line-") + QByteArray::number(line) + '\n';
            leftText.append(text);
            rightText.append(text);
        }
        leftEditor->setText(leftText.constData());
        rightEditor->setText(rightText.constData());
        splitter->addWidget(leftEditor);
        splitter->addWidget(rightEditor);
        window.setCentralWidget(splitter);
        window.resize(900, 260);
        window.show();

        const QMargins originalLeftMargins = leftEditor->contentViewportMargins();
        Session session;
        session.start(leftEditor, rightEditor);
        QTRY_COMPARE_WITH_TIMEOUT(session.state(), Session::State::Ready, 5000);
        QCOMPARE(leftEditor->contentViewportMargins().top(),
                 originalLeftMargins.top() + leftEditor->textHeight(0));

        rightEditor->setFirstVisibleLine(1);
        QTRY_COMPARE(leftEditor->contentViewportMargins(), originalLeftMargins);
        QCOMPARE(session.state(), Session::State::Ready);

        rightEditor->setFirstVisibleLine(0);
        QTRY_COMPARE(leftEditor->contentViewportMargins().top(),
                     originalLeftMargins.top() + leftEditor->textHeight(0));
        QCOMPARE(session.state(), Session::State::Ready);
    }

    void middleGapRemainsAlignedWhileScrollingPastIt()
    {
        QMainWindow window;
        auto *splitter = new QSplitter(&window);
        auto *leftEditor = new ScintillaNext(QStringLiteral("left"), splitter);
        auto *rightEditor = new ScintillaNext(QStringLiteral("right"), splitter);
        QByteArray leftText;
        QByteArray rightText;
        for (int line = 0; line < 100; ++line) {
            const QByteArray text = QByteArray("line-") + QByteArray::number(line) + '\n';
            leftText.append(text);
            if (line != 12) {
                rightText.append(text);
            }
        }
        leftEditor->setText(leftText.constData());
        rightEditor->setText(rightText.constData());
        splitter->addWidget(leftEditor);
        splitter->addWidget(rightEditor);
        window.setCentralWidget(splitter);
        window.resize(900, 260);
        window.show();

        Session session;
        session.start(leftEditor, rightEditor);
        QTRY_COMPARE_WITH_TIMEOUT(session.state(), Session::State::Ready, 5000);
        QCOMPARE(rightEditor->annotationLines(11), 1);

        leftEditor->setFirstVisibleLine(12);
        QTRY_COMPARE(rightEditor->firstVisibleLine(), leftEditor->firstVisibleLine());
        QCOMPARE(rightEditor->docLineFromVisible(rightEditor->firstVisibleLine()), 11);
        QCOMPARE(rightEditor->annotationLines(11), 1);
        QCOMPARE(session.state(), Session::State::Ready);

        leftEditor->setFirstVisibleLine(30);
        QTRY_COMPARE(rightEditor->firstVisibleLine(), leftEditor->firstVisibleLine());
        QCOMPARE(rightEditor->annotationLines(11), 1);
        QCOMPARE(session.state(), Session::State::Ready);

        rightEditor->setFirstVisibleLine(12);
        QTRY_COMPARE(leftEditor->firstVisibleLine(), rightEditor->firstVisibleLine());
        QCOMPARE(rightEditor->annotationLines(11), 1);
        QCOMPARE(session.state(), Session::State::Ready);

        rightEditor->setFirstVisibleLine(0);
        QTRY_COMPARE(leftEditor->firstVisibleLine(), 0);
        QCOMPARE(rightEditor->annotationLines(11), 1);
        QCOMPARE(session.state(), Session::State::Ready);
    }

    void userJsonShapeMarksOnlyTheExpectedLines()
    {
        ScintillaNext leftEditor(QStringLiteral("f2.json"));
        ScintillaNext rightEditor(QStringLiteral("f1.json"));
        QVector<QByteArray> leftLines;
        QVector<QByteArray> rightLines;
        for (int line = 0; line < 220; ++line) {
            const QByteArray text = QByteArray("common-") + QByteArray::number(line);
            leftLines.append(text);
            rightLines.append(text);
        }
        leftLines[7] = "  \"type\": \"bugs\",";
        rightLines[7] = "  \"type\": \"feature\",";
        rightLines.insert(12, "  \"owner\": \"lianc\",");
        for (int extra = 0; extra < 8; ++extra) {
            leftLines.insert(203 + extra, QByteArray("removed-history-") + QByteArray::number(extra));
        }

        auto encode = [](const QVector<QByteArray> &lines) {
            QByteArray bytes;
            for (const QByteArray &line : lines) {
                bytes.append(line).append('\n');
            }
            return bytes;
        };
        const QByteArray leftBytes = encode(leftLines);
        const QByteArray rightBytes = encode(rightLines);
        leftEditor.setText(leftBytes.constData());
        rightEditor.setText(rightBytes.constData());

        Session session;
        session.start(&leftEditor, &rightEditor);
        QTRY_COMPARE_WITH_TIMEOUT(session.state(), Session::State::Ready, 5000);
        QCOMPARE(session.differenceCount(), 3);
        QCOMPARE(session.addedCount(), 1);
        QCOMPARE(session.deletedCount(), 1);
        QCOMPARE(session.modifiedCount(), 1);

        QVERIFY(leftEditor.markerGet(7) & (1 << 17));
        QVERIFY(rightEditor.markerGet(7) & (1 << 16));
        const qsizetype leftTypeLine = leftEditor.positionFromLine(7);
        const qsizetype rightTypeLine = rightEditor.positionFromLine(7);
        const int inlineDeleted = leftEditor.allocateIndicator(QStringLiteral("compare_inline_deleted"));
        const int inlineAdded = rightEditor.allocateIndicator(QStringLiteral("compare_inline_added"));
        QCOMPARE(leftEditor.indicatorValueAt(inlineDeleted, leftTypeLine + 3), 0);
        QCOMPARE(rightEditor.indicatorValueAt(inlineAdded, rightTypeLine + 3), 0);
        QVERIFY(leftEditor.indicatorValueAt(inlineDeleted, leftTypeLine + 11) != 0);
        QVERIFY(rightEditor.indicatorValueAt(inlineAdded, rightTypeLine + 11) != 0);
        QVERIFY(leftEditor.indicatorValueAt(inlineDeleted, leftTypeLine + 14) != 0);
        QVERIFY(rightEditor.indicatorValueAt(inlineAdded, rightTypeLine + 17) != 0);
        QCOMPARE(leftEditor.indicatorValueAt(inlineDeleted, leftTypeLine + 15), 0);
        QCOMPARE(rightEditor.indicatorValueAt(inlineAdded, rightTypeLine + 18), 0);
        QCOMPARE(leftEditor.markerGet(6) & (1 << 17), 0);
        QCOMPARE(rightEditor.markerGet(8) & (1 << 16), 0);
        QCOMPARE(leftEditor.annotationLines(11), 1);
        QVERIFY(rightEditor.markerGet(12) & (1 << 16));
        for (int line = 203; line < 211; ++line) {
            QVERIFY(leftEditor.markerGet(line) & (1 << 17));
        }
        QCOMPARE(rightEditor.annotationLines(203), 8);
        QCOMPARE(leftEditor.get_text_range(0, leftEditor.length()), leftBytes);
        QCOMPARE(rightEditor.get_text_range(0, rightEditor.length()), rightBytes);

        session.clear();
        QCOMPARE(leftEditor.indicatorValueAt(inlineDeleted, leftTypeLine + 11), 0);
        QCOMPARE(rightEditor.indicatorValueAt(inlineAdded, rightTypeLine + 11), 0);
    }

    void editingBeforeExistingAnnotationRestoresItsMovedLine()
    {
        ScintillaNext leftEditor(QStringLiteral("left"));
        ScintillaNext rightEditor(QStringLiteral("right"));
        leftEditor.setText("zero\none\ntwo\n");
        rightEditor.setText("zero\nONE\ntwo\n");
        leftEditor.annotationSetVisible(ANNOTATION_BOXED);
        leftEditor.annotationSetText(2, "moves with line two");

        Session session;
        session.start(&leftEditor, &rightEditor);
        QTRY_COMPARE_WITH_TIMEOUT(session.state(), Session::State::Ready, 5000);
        leftEditor.insertText(0, "prefix\n");
        QTRY_COMPARE_WITH_TIMEOUT(session.state(), Session::State::Stale, 5000);
        QCOMPARE(leftEditor.annotationText(3), QByteArray("moves with line two"));
        QCOMPARE(leftEditor.annotationLines(2), 0);
        QCOMPARE(leftEditor.annotationVisible(), ANNOTATION_BOXED);
    }

    void reloadingEitherInputMarksTheSessionStale()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString leftPath = directory.filePath(QStringLiteral("left.txt"));
        const QString rightPath = directory.filePath(QStringLiteral("right.txt"));

        auto writeFile = [](const QString &path, const QByteArray &contents) {
            QFile file(path);
            if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
                return false;
            }
            return file.write(contents) == contents.size();
        };
        QVERIFY(writeFile(leftPath, "alpha\nbeta\n"));
        QVERIFY(writeFile(rightPath, "alpha\nBETA\n"));

        std::unique_ptr<ScintillaNext> leftEditor(ScintillaNext::fromFile(leftPath));
        std::unique_ptr<ScintillaNext> rightEditor(ScintillaNext::fromFile(rightPath));
        QVERIFY(leftEditor);
        QVERIFY(rightEditor);

        Session session;
        session.start(leftEditor.get(), rightEditor.get());
        QTRY_COMPARE_WITH_TIMEOUT(session.state(), Session::State::Ready, 5000);
        QVERIFY(writeFile(leftPath, "alpha\ngamma\n"));
        leftEditor->reload();
        QTRY_COMPARE_WITH_TIMEOUT(session.state(), Session::State::Stale, 5000);
        QCOMPARE(leftEditor->get_text_range(0, leftEditor->length()), QByteArray("alpha\ngamma\n"));
    }

    void sessionFailureLeavesInputsIntact()
    {
        ScintillaNext leftEditor(QStringLiteral("left"));
        ScintillaNext rightEditor(QStringLiteral("right"));
        const QByteArray binary("alpha\0beta", 10);
        leftEditor.appendText(binary.size(), binary.constData());
        rightEditor.setText("alpha beta");
        leftEditor.emptyUndoBuffer();
        rightEditor.emptyUndoBuffer();
        leftEditor.setSavePoint();
        rightEditor.setSavePoint();
        const QByteArray leftBefore = leftEditor.get_text_range(0, leftEditor.length());
        const QByteArray rightBefore = rightEditor.get_text_range(0, rightEditor.length());

        Session session;
        session.start(&leftEditor, &rightEditor);
        QTRY_COMPARE_WITH_TIMEOUT(session.state(), Session::State::Failed, 5000);
        QCOMPARE(leftEditor.get_text_range(0, leftEditor.length()), leftBefore);
        QCOMPARE(rightEditor.get_text_range(0, rightEditor.length()), rightBefore);
        QVERIFY(!leftEditor.modify());
        QVERIFY(!rightEditor.modify());
    }

    void sessionCancelsWithoutChangingInputs()
    {
        ScintillaNext leftEditor(QStringLiteral("left"));
        ScintillaNext rightEditor(QStringLiteral("right"));
        QByteArray left;
        QByteArray right;
        for (int line = 0; line < 100000; ++line) {
            left.append("left-").append(QByteArray::number(line)).append('\n');
            right.append("right-").append(QByteArray::number(line)).append('\n');
        }
        leftEditor.setText(left.constData());
        rightEditor.setText(right.constData());
        leftEditor.emptyUndoBuffer();
        rightEditor.emptyUndoBuffer();
        leftEditor.setSavePoint();
        rightEditor.setSavePoint();

        Session session;
        session.start(&leftEditor, &rightEditor);
        session.cancel();
        QCOMPARE(session.state(), Session::State::Cancelled);
        QCOMPARE(leftEditor.get_text_range(0, leftEditor.length()), left);
        QCOMPARE(rightEditor.get_text_range(0, rightEditor.length()), right);
        QVERIFY(!leftEditor.modify());
        QVERIFY(!rightEditor.modify());
    }

    void closingEitherInputClearsTheSession()
    {
        auto *leftEditor = new ScintillaNext(QStringLiteral("left"));
        auto *rightEditor = new ScintillaNext(QStringLiteral("right"));
        QPointer<ScintillaNext> leftGuard(leftEditor);
        leftEditor->setText("left\n");
        rightEditor->setText("right\n");

        Session session;
        session.start(leftEditor, rightEditor);
        QTRY_COMPARE_WITH_TIMEOUT(session.state(), Session::State::Ready, 5000);
        leftEditor->close();
        QCOMPARE(session.state(), Session::State::None);
        QTRY_VERIFY_WITH_TIMEOUT(leftGuard.isNull(), 5000);
        delete rightEditor;
    }

    void visualToolbarShowsStrongCompareState()
    {
        QMainWindow window;
        auto *splitter = new QSplitter(&window);
        auto *leftEditor = new ScintillaNext(QStringLiteral("f2.json"), splitter);
        auto *rightEditor = new ScintillaNext(QStringLiteral("f1.json"), splitter);
        leftEditor->setMarginTypeN(0, SC_MARGIN_NUMBER);
        rightEditor->setMarginTypeN(0, SC_MARGIN_NUMBER);
        leftEditor->setMarginWidthN(0, 44);
        rightEditor->setMarginWidthN(0, 44);
        leftEditor->setScrollWidthTracking(true);
        rightEditor->setScrollWidthTracking(true);
        leftEditor->setScrollWidth(1);
        rightEditor->setScrollWidth(1);
        leftEditor->setText(
            "same-0\ndeleted\nanchor-1\n  \"type\": \"bugs\",\nanchor-2\nanchor-3\n");
        rightEditor->setText(
            "leading\nsame-0\nanchor-1\n  \"type\": \"feature\",\nanchor-2\nadded\nanchor-3\n");
        splitter->addWidget(leftEditor);
        splitter->addWidget(rightEditor);
        window.setCentralWidget(splitter);

        Session session;
        QAction previousAction(&window);
        QAction nextAction(&window);
        QAction refreshAction(&window);
        QAction cancelAction(&window);
        QAction clearAction(&window);
        Compare::CompareToolBar toolBar(
            &session,
            &previousAction,
            &nextAction,
            &refreshAction,
            &cancelAction,
            &clearAction,
            &window);
        window.addToolBar(Qt::TopToolBarArea, &toolBar);
        window.resize(1500, 760);
        window.show();

        session.start(leftEditor, rightEditor);
        QTRY_COMPARE_WITH_TIMEOUT(session.state(), Session::State::Ready, 5000);
        session.nextDifference();
        session.nextDifference();
        session.nextDifference();
        QTRY_COMPARE(toolBar.findChild<QLabel *>(QStringLiteral("compareStatusLabel"))->text(),
                 QStringLiteral("3 / 4"));
        QLabel *statusLabel = toolBar.findChild<QLabel *>(QStringLiteral("compareStatusLabel"));
        QTRY_VERIFY(statusLabel->width() >= statusLabel->sizeHint().width());
        QCOMPARE(toolBar.findChild<QLabel *>(QStringLiteral("compareAddedCountLabel"))->text(),
                 QStringLiteral("2 added"));
        QCOMPARE(toolBar.findChild<QLabel *>(QStringLiteral("compareDeletedCountLabel"))->text(),
                 QStringLiteral("1 deleted"));
        QCOMPARE(toolBar.findChild<QLabel *>(QStringLiteral("compareModifiedCountLabel"))->text(),
                 QStringLiteral("1 changed"));
        QVERIFY(leftEditor->marginWidthN(3) >= 10);
        QVERIFY(rightEditor->marginWidthN(3) >= 10);

        const QString wideSnapshot = qEnvironmentVariable("COMPARE_VISUAL_SNAPSHOT");
        if (!wideSnapshot.isEmpty()) {
            QVERIFY2(window.grab().save(wideSnapshot), "Could not save the wide Compare visual snapshot.");
        }

        window.resize(920, 620);
        QTest::qWait(100);
        QVERIFY(statusLabel->isVisible());
        QVERIFY(statusLabel->width() >= statusLabel->sizeHint().width());
        QVERIFY(toolBar.findChild<QLabel *>(QStringLiteral("compareAddedCountLabel"))->isVisible());
        QVERIFY(toolBar.findChild<QLabel *>(QStringLiteral("compareDeletedCountLabel"))->isVisible());
        QVERIFY(toolBar.findChild<QLabel *>(QStringLiteral("compareModifiedCountLabel"))->isVisible());
        const QString narrowSnapshot = qEnvironmentVariable("COMPARE_VISUAL_NARROW_SNAPSHOT");
        if (!narrowSnapshot.isEmpty()) {
            QVERIFY2(window.grab().save(narrowSnapshot), "Could not save the narrow Compare visual snapshot.");
        }
    }

    void toolbarHidesMetricsWhenInputsMatch()
    {
        ScintillaNext leftEditor(QStringLiteral("left"));
        ScintillaNext rightEditor(QStringLiteral("right"));
        leftEditor.setText("same\n");
        rightEditor.setText("same\n");

        Session session;
        QAction previousAction;
        QAction nextAction;
        QAction refreshAction;
        QAction cancelAction;
        QAction clearAction;
        Compare::CompareToolBar toolBar(
            &session,
            &previousAction,
            &nextAction,
            &refreshAction,
            &cancelAction,
            &clearAction);

        session.start(&leftEditor, &rightEditor);
        QTRY_COMPARE_WITH_TIMEOUT(session.state(), Session::State::Ready, 5000);
        QCOMPARE(toolBar.findChild<QLabel *>(QStringLiteral("compareStatusLabel"))->text(),
                 QStringLiteral("No changes"));
        QVERIFY(!toolBar.findChild<QAction *>(
            QStringLiteral("compareAddedMetricAction"))->isVisible());
        QVERIFY(!toolBar.findChild<QAction *>(
            QStringLiteral("compareDeletedMetricAction"))->isVisible());
        QVERIFY(!toolBar.findChild<QAction *>(
            QStringLiteral("compareModifiedMetricAction"))->isVisible());
        QAction *metricsSeparator = toolBar.findChild<QAction *>(
            QStringLiteral("compareMetricsSeparator"));
        QVERIFY(metricsSeparator != nullptr);
        QVERIFY(!metricsSeparator->isVisible());
    }
};

QTEST_MAIN(CompareSessionTests)

#include "CompareSessionTests.moc"