/*
 * This file is part of Notepad Next.
 *
 * Notepad Next is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "CompareEngine.h"

#include <QElapsedTimer>
#include <QRandomGenerator>
#include <QTest>

#include <chrono>
#include <thread>

using Compare::ChangeKind;
using Compare::DiffHunk;
using Compare::Engine;
using Compare::Limits;
using Compare::Status;

class CompareEngineTests : public QObject
{
    Q_OBJECT

private:
    static void compareHunk(const DiffHunk &actual, const DiffHunk &expected)
    {
        QCOMPARE(actual.leftStart, expected.leftStart);
        QCOMPARE(actual.leftCount, expected.leftCount);
        QCOMPARE(actual.rightStart, expected.rightStart);
        QCOMPARE(actual.rightCount, expected.rightCount);
        QCOMPARE(actual.kind, expected.kind);
    }

    static void compareInlineSpan(const Compare::InlineSpan &actual,
                                  qsizetype lineOffset,
                                  qsizetype byteStart,
                                  qsizetype byteLength)
    {
        QCOMPARE(actual.lineOffset, lineOffset);
        QCOMPARE(actual.byteStart, byteStart);
        QCOMPARE(actual.byteLength, byteLength);
    }

    static QByteArray encodeLines(const QVector<QByteArray> &lines)
    {
        QByteArray encoded;
        for (const QByteArray &line : lines) {
            encoded.append(line);
            encoded.append('\n');
        }
        return encoded;
    }

    static int shortestEditCost(const QVector<QByteArray> &left, const QVector<QByteArray> &right)
    {
        QVector<int> previous(right.size() + 1);
        QVector<int> current(right.size() + 1);

        for (qsizetype leftIndex = 0; leftIndex < left.size(); ++leftIndex) {
            for (qsizetype rightIndex = 0; rightIndex < right.size(); ++rightIndex) {
                current[rightIndex + 1] = left.at(leftIndex) == right.at(rightIndex)
                    ? previous.at(rightIndex) + 1
                    : std::max(current.at(rightIndex), previous.at(rightIndex + 1));
            }
            std::swap(previous, current);
            current.fill(0);
        }

        const int longestCommonSubsequence = previous.constLast();
        return left.size() + right.size() - 2 * longestCommonSubsequence;
    }

    static void verifyReplay(const QVector<QByteArray> &left,
                             const QVector<QByteArray> &right,
                             const QVector<DiffHunk> &hunks)
    {
        QVector<QByteArray> replayed;
        qsizetype leftCursor = 0;
        qsizetype rightCursor = 0;

        for (const DiffHunk &hunk : hunks) {
            const qsizetype unchanged = hunk.leftStart - leftCursor;
            QCOMPARE(unchanged, hunk.rightStart - rightCursor);
            for (qsizetype index = 0; index < unchanged; ++index) {
                QCOMPARE(left.at(leftCursor + index), right.at(rightCursor + index));
                replayed.append(left.at(leftCursor + index));
            }

            for (qsizetype index = 0; index < hunk.rightCount; ++index) {
                replayed.append(right.at(hunk.rightStart + index));
            }
            leftCursor = hunk.leftStart + hunk.leftCount;
            rightCursor = hunk.rightStart + hunk.rightCount;
        }

        while (leftCursor < left.size()) {
            QCOMPARE(left.at(leftCursor), right.at(rightCursor));
            replayed.append(left.at(leftCursor));
            ++leftCursor;
            ++rightCursor;
        }
        QCOMPARE(rightCursor, right.size());
        QCOMPARE(replayed, right);
    }

    static QByteArray representativeCorpus(int lineCount, bool changed)
    {
        static const QByteArray payload(
            " 0123456789 abcdefghijklmnopqrstuvwxyz ABCDEFGHIJKLMNOPQRSTUVWXYZ payload");
        QByteArray corpus;
        corpus.reserve(lineCount * 90);

        for (int line = 0; line < lineCount; ++line) {
            corpus.append(QByteArray::number(line).rightJustified(8, '0'));
            corpus.append(payload);
            if (changed && ((lineCount == 10000 && line % 100 == 50)
                            || (lineCount == 100000 && line % 1000 >= 400 && line % 1000 < 420))) {
                corpus.append(" changed");
            }
            corpus.append('\n');
        }
        return corpus;
    }

private slots:
    void equalAndEmptyInputs()
    {
        const auto empty = Engine::compare({}, {});
        QCOMPARE(empty.status, Status::Completed);
        QVERIFY(empty.hunks.isEmpty());

        const auto equal = Engine::compare("alpha\nbeta\n", "alpha\nbeta\n");
        QCOMPARE(equal.status, Status::Completed);
        QVERIFY(equal.hunks.isEmpty());
    }

    void reportsAddDeleteAndModify()
    {
        const auto added = Engine::compare("alpha\ngamma\n", "alpha\nbeta\ngamma\n");
        QCOMPARE(added.status, Status::Completed);
        QCOMPARE(added.hunks.size(), 1);
        compareHunk(added.hunks.constFirst(), {1, 0, 1, 1, ChangeKind::Added});

        const auto deleted = Engine::compare("alpha\nbeta\ngamma\n", "alpha\ngamma\n");
        QCOMPARE(deleted.status, Status::Completed);
        QCOMPARE(deleted.hunks.size(), 1);
        compareHunk(deleted.hunks.constFirst(), {1, 1, 1, 0, ChangeKind::Deleted});

        const auto modified = Engine::compare("alpha\nbeta\ngamma\n", "alpha\nBETA\ngamma\n");
        QCOMPARE(modified.status, Status::Completed);
        QCOMPARE(modified.hunks.size(), 1);
        compareHunk(modified.hunks.constFirst(), {1, 1, 1, 1, ChangeKind::Modified});
    }

    void reportsSeparatedChangesInOrder()
    {
        const auto result = Engine::compare(
            "zero\none\ntwo\nthree\nfive\n",
            "zero\nONE\ntwo\nthree\nfour\nfive\n");

        QCOMPARE(result.status, Status::Completed);
        QCOMPARE(result.hunks.size(), 2);
        compareHunk(result.hunks.at(0), {1, 1, 1, 1, ChangeKind::Modified});
        compareHunk(result.hunks.at(1), {4, 0, 4, 1, ChangeKind::Added});
    }

    void reportsTheUserJsonShapeAtExactLines()
    {
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

        const auto result = Engine::compare(encodeLines(leftLines), encodeLines(rightLines));
        QCOMPARE(result.status, Status::Completed);
        QCOMPARE(result.hunks.size(), 3);
        compareHunk(result.hunks.at(0), {7, 1, 7, 1, ChangeKind::Modified});
        compareHunk(result.hunks.at(1), {12, 0, 12, 1, ChangeKind::Added});
        compareHunk(result.hunks.at(2), {203, 8, 204, 0, ChangeKind::Deleted});
        QCOMPARE(result.hunks.at(0).leftInlineSpans.size(), 1);
        QCOMPARE(result.hunks.at(0).rightInlineSpans.size(), 1);
        compareInlineSpan(result.hunks.at(0).leftInlineSpans.constFirst(), 0, 11, 4);
        compareInlineSpan(result.hunks.at(0).rightInlineSpans.constFirst(), 0, 11, 7);
    }

    void normalizesLineEndingsButPreservesFinalTerminator()
    {
        const auto normalized = Engine::compare("alpha\r\nbeta\r\n", "alpha\nbeta\n");
        QCOMPARE(normalized.status, Status::Completed);
        QVERIFY(normalized.hunks.isEmpty());

        const auto finalTerminator = Engine::compare("alpha\n", "alpha");
        QCOMPARE(finalTerminator.status, Status::Completed);
        QCOMPARE(finalTerminator.hunks.size(), 1);
        compareHunk(finalTerminator.hunks.constFirst(), {0, 1, 0, 1, ChangeKind::Modified});
        QVERIFY(finalTerminator.hunks.constFirst().leftInlineSpans.isEmpty());
        QVERIFY(finalTerminator.hunks.constFirst().rightInlineSpans.isEmpty());
    }

    void remainsDeterministicForAmbiguousLines()
    {
        const QByteArray left("same\nleft\nsame\nleft\nsame\n");
        const QByteArray right("same\nsame\nright\nsame\n");
        const auto expected = Engine::compare(left, right);
        QCOMPARE(expected.status, Status::Completed);

        for (int iteration = 0; iteration < 100; ++iteration) {
            const auto actual = Engine::compare(left, right);
            QCOMPARE(actual.status, Status::Completed);
            QCOMPARE(actual.hunks, expected.hunks);
        }
    }

    void randomizedResultsAreShortestAndReplayable()
    {
        QRandomGenerator random(0xC0FFEEu);
        for (int iteration = 0; iteration < 1000; ++iteration) {
            QVector<QByteArray> left;
            QVector<QByteArray> right;
            const int leftSize = random.bounded(13);
            const int rightSize = random.bounded(13);
            for (int index = 0; index < leftSize; ++index) {
                left.append(QByteArray::number(random.bounded(5)));
            }
            for (int index = 0; index < rightSize; ++index) {
                right.append(QByteArray::number(random.bounded(5)));
            }

            const auto result = Engine::compare(encodeLines(left), encodeLines(right));
            QCOMPARE(result.status, Status::Completed);
            verifyReplay(left, right, result.hunks);

            int actualCost = 0;
            for (const DiffHunk &hunk : result.hunks) {
                actualCost += hunk.leftCount + hunk.rightCount;
            }
            QCOMPARE(actualCost, shortestEditCost(left, right));
        }
    }

    void handlesLongLines()
    {
        QByteArray left(1024 * 1024, 'a');
        QByteArray right = left;
        right[right.size() / 2] = 'b';

        const auto result = Engine::compare(left, right);
        QCOMPARE(result.status, Status::Completed);
        QCOMPARE(result.hunks.size(), 1);
        compareHunk(result.hunks.constFirst(), {0, 1, 0, 1, ChangeKind::Modified});
    }

    void handlesUtf8AndNonUtf8BytesDeterministically()
    {
        const QByteArray utf8Left = QStringLiteral("alpha\n你好\n").toUtf8();
        const QByteArray utf8Right = QStringLiteral("alpha\n您好\n").toUtf8();
        const auto utf8Result = Engine::compare(utf8Left, utf8Right);
        QCOMPARE(utf8Result.status, Status::Completed);
        QCOMPARE(utf8Result.hunks.size(), 1);
        compareHunk(utf8Result.hunks.constFirst(), {1, 1, 1, 1, ChangeKind::Modified});
        QCOMPARE(utf8Result.hunks.constFirst().leftInlineSpans.size(), 1);
        QCOMPARE(utf8Result.hunks.constFirst().rightInlineSpans.size(), 1);
        compareInlineSpan(utf8Result.hunks.constFirst().leftInlineSpans.constFirst(), 0, 0, 3);
        compareInlineSpan(utf8Result.hunks.constFirst().rightInlineSpans.constFirst(), 0, 0, 3);

        const QByteArray invalidLeft = QByteArray::fromHex("fffe0a61");
        const QByteArray invalidRight = QByteArray::fromHex("fefd0a61");
        const auto invalidResult = Engine::compare(invalidLeft, invalidRight);
        QCOMPARE(invalidResult.status, Status::Completed);
        QCOMPARE(invalidResult.hunks.size(), 1);
        compareHunk(invalidResult.hunks.constFirst(), {0, 1, 0, 1, ChangeKind::Modified});
        QCOMPARE(Engine::compare(invalidLeft, invalidRight).hunks, invalidResult.hunks);
    }

    void rejectsUnsupportedAndLimitedInputs()
    {
        QByteArray binary("alpha\0beta", 10);
        QCOMPARE(Engine::compare(binary, "alpha").status, Status::UnsupportedInput);

        Limits byteLimits;
        byteLimits.maxBytesPerInput = 4;
        QCOMPARE(Engine::compare("12345", "", {}, byteLimits).status, Status::LimitExceeded);

        Limits lineLimits;
        lineLimits.maxLinesPerInput = 2;
        QCOMPARE(Engine::compare("a\nb\nc\n", "", {}, lineLimits).status, Status::LimitExceeded);

        Limits workLimits;
        workLimits.maxWorkUnits = 1;
        QCOMPARE(Engine::compare("a\nb\n", "c\nd\n", {}, workLimits).status, Status::LimitExceeded);
    }

    void honorsCancellation()
    {
        Compare::CancellationSource cancellationSource;
        cancellationSource.cancel();

        const auto result = Engine::compare("alpha\n", "beta\n", cancellationSource.token());
        QCOMPARE(result.status, Status::Cancelled);
        QVERIFY(result.hunks.isEmpty());
    }

    void meetsRepresentativeCorpusBudgets()
    {
        auto verifyBudget = [](int lineCount, qint64 budgetMilliseconds) {
            const QByteArray left = representativeCorpus(lineCount, false);
            const QByteArray right = representativeCorpus(lineCount, true);

            QCOMPARE(Engine::compare(left, right).status, Status::Completed);
            for (int run = 0; run < 5; ++run) {
                QElapsedTimer timer;
                timer.start();
                const auto result = Engine::compare(left, right);
                const qint64 elapsed = timer.elapsed();
                qInfo("Compare corpus: %d lines, run %d, %lld ms", lineCount, run + 1, elapsed);
                QCOMPARE(result.status, Status::Completed);
                QVERIFY2(elapsed <= budgetMilliseconds, "Representative Compare corpus exceeded its budget.");
            }
        };

        verifyBudget(10000, 1000);
        verifyBudget(100000, 3000);
    }

    void cancellationMeetsBudget()
    {
        QByteArray left;
        QByteArray right;
        left.reserve(9 * 1024 * 1024);
        right.reserve(9 * 1024 * 1024);
        for (int line = 0; line < 100000; ++line) {
            left.append("left-");
            left.append(QByteArray::number(line).rightJustified(8, '0'));
            left.append(" 0123456789 abcdefghijklmnopqrstuvwxyz ABCDEFGHIJKLMNOPQRSTUVWXYZ\n");
            right.append("right-");
            right.append(QByteArray::number(line).rightJustified(8, '0'));
            right.append(" 0123456789 abcdefghijklmnopqrstuvwxyz ABCDEFGHIJKLMNOPQRSTUVWXYZ\n");
        }

        for (int run = 0; run < 5; ++run) {
            Compare::CancellationSource cancellationSource;
            Compare::Result result;
            std::thread worker([&]() {
                result = Engine::compare(left, right, cancellationSource.token());
            });

            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            QElapsedTimer timer;
            timer.start();
            cancellationSource.cancel();
            worker.join();
            const qint64 elapsed = timer.elapsed();
            qInfo("Compare cancellation: run %d, %lld ms", run + 1, elapsed);
            QCOMPARE(result.status, Status::Cancelled);
            QVERIFY2(elapsed <= 250, "Compare cancellation exceeded its budget.");
        }
    }
};

QTEST_APPLESS_MAIN(CompareEngineTests)

#include "CompareEngineTests.moc"