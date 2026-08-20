/*
 * This file is part of Notepad Next.
 *
 * Notepad Next is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "CompareEngine.h"

#include <algorithm>
#include <cstring>

namespace Compare
{

CancellationToken::CancellationToken() :
    state(std::make_shared<std::atomic_bool>(false))
{
}

CancellationToken::CancellationToken(std::shared_ptr<std::atomic_bool> state) :
    state(std::move(state))
{
}

bool CancellationToken::isCancellationRequested() const
{
    return state->load(std::memory_order_relaxed);
}

CancellationSource::CancellationSource() :
    state(std::make_shared<std::atomic_bool>(false))
{
}

CancellationToken CancellationSource::token() const
{
    return CancellationToken(state);
}

void CancellationSource::cancel() const
{
    state->store(true, std::memory_order_relaxed);
}

namespace
{

constexpr qsizetype WorkByteInterval = 4096;

struct Line
{
    qsizetype start;
    qsizetype length;
    bool terminated;
};

enum class EditKind
{
    Equal,
    Delete,
    Insert,
};

struct Edit
{
    EditKind kind;
    qsizetype count;
};

struct Split
{
    qsizetype left;
    qsizetype right;
};

class EngineImpl
{
public:
    EngineImpl(const QByteArray &leftBytes,
               const QByteArray &rightBytes,
               const CancellationToken &cancellationToken,
               const Limits &limits) :
        leftBytes(leftBytes),
        rightBytes(rightBytes),
        cancellationToken(cancellationToken),
        limits(limits)
    {
    }

    Result run()
    {
        if (leftBytes.size() > limits.maxBytesPerInput || rightBytes.size() > limits.maxBytesPerInput) {
            return stopped(Status::LimitExceeded, QStringLiteral("Compare input exceeds the byte limit."));
        }

        if (leftBytes.contains('\0') || rightBytes.contains('\0')) {
            return stopped(Status::UnsupportedInput, QStringLiteral("Binary input is not supported."));
        }

        if (!splitLines(leftBytes, leftLines) || !splitLines(rightBytes, rightLines)) {
            return result;
        }

        diffRange(0, leftLines.size(), 0, rightLines.size());
        if (result.status != Status::Completed) {
            result.hunks.clear();
            return result;
        }

        QVector<DiffHunk> hunks = makeHunks();
        if (result.status != Status::Completed) {
            result.hunks.clear();
            return result;
        }

        result.hunks = std::move(hunks);
        return result;
    }

private:
    bool consumeWork()
    {
        if (cancellationToken.isCancellationRequested()) {
            stopped(Status::Cancelled, QStringLiteral("Compare was cancelled."));
            return false;
        }

        if (workUnits >= limits.maxWorkUnits) {
            stopped(Status::LimitExceeded, QStringLiteral("Compare exceeded the work limit."));
            return false;
        }

        ++workUnits;
        return true;
    }

    Result stopped(Status status, const QString &message)
    {
        result.status = status;
        result.message = message;
        result.hunks.clear();
        return result;
    }

    bool splitLines(const QByteArray &bytes, QVector<Line> &lines)
    {
        qsizetype lineStart = 0;
        qsizetype position = 0;

        while (position < bytes.size()) {
            const char value = bytes.at(position);
            if (value != '\r' && value != '\n') {
                ++position;
                if ((position - lineStart) % WorkByteInterval == 0 && !consumeWork()) {
                    return false;
                }
                continue;
            }

            lines.append({lineStart, position - lineStart, true});
            if (lines.size() > limits.maxLinesPerInput) {
                stopped(Status::LimitExceeded, QStringLiteral("Compare input exceeds the line limit."));
                return false;
            }

            if (value == '\r' && position + 1 < bytes.size() && bytes.at(position + 1) == '\n') {
                position += 2;
            }
            else {
                ++position;
            }
            lineStart = position;

            if (!consumeWork()) {
                return false;
            }
        }

        if (lineStart < bytes.size()) {
            lines.append({lineStart, bytes.size() - lineStart, false});
            if (lines.size() > limits.maxLinesPerInput) {
                stopped(Status::LimitExceeded, QStringLiteral("Compare input exceeds the line limit."));
                return false;
            }
        }

        return true;
    }

    bool linesEqual(qsizetype leftIndex, qsizetype rightIndex)
    {
        if (!consumeWork()) {
            return false;
        }

        const Line &left = leftLines.at(leftIndex);
        const Line &right = rightLines.at(rightIndex);
        if (left.length != right.length || left.terminated != right.terminated) {
            return false;
        }

        for (qsizetype offset = 0; offset < left.length; offset += WorkByteInterval) {
            if (offset > 0 && !consumeWork()) {
                return false;
            }

            const qsizetype chunkLength = std::min(WorkByteInterval, left.length - offset);
            if (std::memcmp(leftBytes.constData() + left.start + offset,
                            rightBytes.constData() + right.start + offset,
                            static_cast<size_t>(chunkLength)) != 0) {
                return false;
            }
        }

        return true;
    }

    void appendEdit(EditKind kind, qsizetype count)
    {
        if (count == 0 || result.status != Status::Completed) {
            return;
        }

        if (!edits.isEmpty() && edits.constLast().kind == kind) {
            edits.last().count += count;
        }
        else {
            edits.append({kind, count});
        }
    }

    void diffRange(qsizetype leftStart,
                   qsizetype leftEnd,
                   qsizetype rightStart,
                   qsizetype rightEnd)
    {
        if (result.status != Status::Completed || !consumeWork()) {
            return;
        }

        qsizetype prefix = 0;
        while (leftStart + prefix < leftEnd && rightStart + prefix < rightEnd) {
            const bool equal = linesEqual(leftStart + prefix, rightStart + prefix);
            if (result.status != Status::Completed || !equal) {
                break;
            }
            ++prefix;
        }

        appendEdit(EditKind::Equal, prefix);
        leftStart += prefix;
        rightStart += prefix;

        qsizetype suffix = 0;
        while (leftStart < leftEnd - suffix && rightStart < rightEnd - suffix) {
            const bool equal = linesEqual(leftEnd - suffix - 1, rightEnd - suffix - 1);
            if (result.status != Status::Completed || !equal) {
                break;
            }
            ++suffix;
        }

        const qsizetype leftMiddleEnd = leftEnd - suffix;
        const qsizetype rightMiddleEnd = rightEnd - suffix;
        const qsizetype leftLength = leftMiddleEnd - leftStart;
        const qsizetype rightLength = rightMiddleEnd - rightStart;

        if (result.status != Status::Completed) {
            return;
        }
        if (leftLength == 0) {
            appendEdit(EditKind::Insert, rightLength);
        }
        else if (rightLength == 0) {
            appendEdit(EditKind::Delete, leftLength);
        }
        else if (leftLength == 1 || rightLength == 1) {
            diffSmall(leftStart, leftMiddleEnd, rightStart, rightMiddleEnd);
        }
        else {
            Split split;
            if (!findSplit(leftStart, leftMiddleEnd, rightStart, rightMiddleEnd, split)) {
                if (result.status == Status::Completed) {
                    appendEdit(EditKind::Delete, leftLength);
                    appendEdit(EditKind::Insert, rightLength);
                }
            }
            else if ((split.left == leftStart && split.right == rightStart)
                     || (split.left == leftMiddleEnd && split.right == rightMiddleEnd)) {
                appendEdit(EditKind::Delete, leftLength);
                appendEdit(EditKind::Insert, rightLength);
            }
            else {
                diffRange(leftStart, split.left, rightStart, split.right);
                diffRange(split.left, leftMiddleEnd, split.right, rightMiddleEnd);
            }
        }

        appendEdit(EditKind::Equal, suffix);
    }

    void diffSmall(qsizetype leftStart,
                   qsizetype leftEnd,
                   qsizetype rightStart,
                   qsizetype rightEnd)
    {
        const qsizetype leftLength = leftEnd - leftStart;
        const qsizetype rightLength = rightEnd - rightStart;

        if (leftLength == 1) {
            qsizetype match = -1;
            for (qsizetype right = rightStart; right < rightEnd; ++right) {
                if (linesEqual(leftStart, right)) {
                    match = right;
                    break;
                }
                if (result.status != Status::Completed) {
                    return;
                }
            }

            if (match == -1) {
                appendEdit(EditKind::Delete, 1);
                appendEdit(EditKind::Insert, rightLength);
            }
            else {
                appendEdit(EditKind::Insert, match - rightStart);
                appendEdit(EditKind::Equal, 1);
                appendEdit(EditKind::Insert, rightEnd - match - 1);
            }
            return;
        }

        qsizetype match = -1;
        for (qsizetype left = leftEnd; left > leftStart; --left) {
            if (linesEqual(left - 1, rightStart)) {
                match = left - 1;
                break;
            }
            if (result.status != Status::Completed) {
                return;
            }
        }

        if (match == -1) {
            appendEdit(EditKind::Delete, leftLength);
            appendEdit(EditKind::Insert, 1);
        }
        else {
            appendEdit(EditKind::Delete, match - leftStart);
            appendEdit(EditKind::Equal, 1);
            appendEdit(EditKind::Delete, leftEnd - match - 1);
        }
    }

    bool findSplit(qsizetype leftStart,
                   qsizetype leftEnd,
                   qsizetype rightStart,
                   qsizetype rightEnd,
                   Split &split)
    {
        const qsizetype leftLength = leftEnd - leftStart;
        const qsizetype rightLength = rightEnd - rightStart;
        const qsizetype maxDistance = (leftLength + rightLength + 1) / 2;
        const qsizetype offset = maxDistance + 1;
        const qsizetype vectorLength = 2 * maxDistance + 3;
        QVector<qsizetype> forward(vectorLength, -1);
        QVector<qsizetype> reverse(vectorLength, -1);
        forward[offset + 1] = 0;
        reverse[offset + 1] = 0;

        const qsizetype delta = leftLength - rightLength;
        const bool oddDelta = (delta & 1) != 0;

        for (qsizetype distance = 0; distance <= maxDistance; ++distance) {
            for (qsizetype diagonal = -distance; diagonal <= distance; diagonal += 2) {
                if (!consumeWork()) {
                    return false;
                }

                const qsizetype index = offset + diagonal;
                qsizetype left = 0;
                if (diagonal == -distance
                    || (diagonal != distance && forward.at(index - 1) < forward.at(index + 1))) {
                    left = forward.at(index + 1);
                }
                else {
                    left = forward.at(index - 1) + 1;
                }
                qsizetype right = left - diagonal;

                while (left < leftLength && right < rightLength
                       && linesEqual(leftStart + left, rightStart + right)) {
                    ++left;
                    ++right;
                }
                if (result.status != Status::Completed) {
                    return false;
                }
                forward[index] = left;

                if (oddDelta) {
                    const qsizetype reverseDiagonal = delta - diagonal;
                    const qsizetype reverseIndex = offset + reverseDiagonal;
                    if (reverseIndex >= 0 && reverseIndex < vectorLength && reverse.at(reverseIndex) != -1
                        && left >= leftLength - reverse.at(reverseIndex)) {
                        split = {leftStart + left, rightStart + right};
                        return true;
                    }
                }
            }

            for (qsizetype diagonal = -distance; diagonal <= distance; diagonal += 2) {
                if (!consumeWork()) {
                    return false;
                }

                const qsizetype index = offset + diagonal;
                qsizetype left = 0;
                if (diagonal == -distance
                    || (diagonal != distance && reverse.at(index - 1) < reverse.at(index + 1))) {
                    left = reverse.at(index + 1);
                }
                else {
                    left = reverse.at(index - 1) + 1;
                }
                qsizetype right = left - diagonal;

                while (left < leftLength && right < rightLength
                       && linesEqual(leftEnd - left - 1, rightEnd - right - 1)) {
                    ++left;
                    ++right;
                }
                if (result.status != Status::Completed) {
                    return false;
                }
                reverse[index] = left;

                if (!oddDelta) {
                    const qsizetype forwardDiagonal = delta - diagonal;
                    const qsizetype forwardIndex = offset + forwardDiagonal;
                    if (forwardIndex >= 0 && forwardIndex < vectorLength && forward.at(forwardIndex) != -1
                        && forward.at(forwardIndex) >= leftLength - left) {
                        const qsizetype forwardLeft = forward.at(forwardIndex);
                        split = {leftStart + forwardLeft,
                                 rightStart + forwardLeft - forwardDiagonal};
                        return true;
                    }
                }
            }
        }

        return false;
    }

    static bool isUtf8Continuation(char value)
    {
        return (static_cast<unsigned char>(value) & 0xC0) == 0x80;
    }

    void appendInlineSpans(DiffHunk &hunk)
    {
        if (hunk.kind != ChangeKind::Modified) {
            return;
        }

        const qsizetype pairedLines = std::min(hunk.leftCount, hunk.rightCount);
        for (qsizetype lineOffset = 0; lineOffset < pairedLines; ++lineOffset) {
            if (!consumeWork()) {
                return;
            }

            const Line &left = leftLines.at(hunk.leftStart + lineOffset);
            const Line &right = rightLines.at(hunk.rightStart + lineOffset);
            const char *leftData = leftBytes.constData() + left.start;
            const char *rightData = rightBytes.constData() + right.start;

            qsizetype prefix = 0;
            while (prefix < left.length && prefix < right.length
                   && leftData[prefix] == rightData[prefix]) {
                ++prefix;
                if (prefix % WorkByteInterval == 0 && !consumeWork()) {
                    return;
                }
            }
            while (prefix > 0
                   && ((prefix < left.length && isUtf8Continuation(leftData[prefix]))
                       || (prefix < right.length && isUtf8Continuation(rightData[prefix])))) {
                --prefix;
            }

            qsizetype suffix = 0;
            while (suffix < left.length - prefix
                   && suffix < right.length - prefix
                   && leftData[left.length - suffix - 1] == rightData[right.length - suffix - 1]) {
                ++suffix;
                if (suffix % WorkByteInterval == 0 && !consumeWork()) {
                    return;
                }
            }
            while (suffix > 0
                   && (isUtf8Continuation(leftData[left.length - suffix])
                       || isUtf8Continuation(rightData[right.length - suffix]))) {
                --suffix;
            }

            const qsizetype leftLength = left.length - prefix - suffix;
            const qsizetype rightLength = right.length - prefix - suffix;
            if (leftLength > 0) {
                hunk.leftInlineSpans.append({lineOffset, prefix, leftLength});
            }
            if (rightLength > 0) {
                hunk.rightInlineSpans.append({lineOffset, prefix, rightLength});
            }
        }
    }

    QVector<DiffHunk> makeHunks()
    {
        QVector<DiffHunk> hunks;
        qsizetype leftLine = 0;
        qsizetype rightLine = 0;
        qsizetype editIndex = 0;

        while (editIndex < edits.size()) {
            const Edit &edit = edits.at(editIndex);
            if (edit.kind == EditKind::Equal) {
                leftLine += edit.count;
                rightLine += edit.count;
                ++editIndex;
                continue;
            }

            DiffHunk hunk;
            hunk.leftStart = leftLine;
            hunk.rightStart = rightLine;
            while (editIndex < edits.size() && edits.at(editIndex).kind != EditKind::Equal) {
                const Edit &change = edits.at(editIndex);
                if (change.kind == EditKind::Delete) {
                    hunk.leftCount += change.count;
                    leftLine += change.count;
                }
                else {
                    hunk.rightCount += change.count;
                    rightLine += change.count;
                }
                ++editIndex;
            }

            if (hunk.leftCount == 0) {
                hunk.kind = ChangeKind::Added;
            }
            else if (hunk.rightCount == 0) {
                hunk.kind = ChangeKind::Deleted;
            }
            else {
                hunk.kind = ChangeKind::Modified;
            }
            appendInlineSpans(hunk);
            if (result.status != Status::Completed) {
                return {};
            }
            hunks.append(hunk);
        }

        return hunks;
    }

    const QByteArray &leftBytes;
    const QByteArray &rightBytes;
    const CancellationToken &cancellationToken;
    const Limits &limits;
    QVector<Line> leftLines;
    QVector<Line> rightLines;
    QVector<Edit> edits;
    quint64 workUnits = 0;
    Result result;
};

}

Result Engine::compare(const QByteArray &left,
                       const QByteArray &right,
                       const CancellationToken &cancellationToken,
                       const Limits &limits)
{
    return EngineImpl(left, right, cancellationToken, limits).run();
}

}