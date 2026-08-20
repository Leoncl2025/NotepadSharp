/*
 * This file is part of Notepad Next.
 *
 * Notepad Next is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#pragma once

#include <QByteArray>
#include <QString>
#include <QVector>

#include <atomic>
#include <memory>

namespace Compare
{

enum class ChangeKind
{
    Added,
    Deleted,
    Modified,
};

struct InlineSpan
{
    qsizetype lineOffset = 0;
    qsizetype byteStart = 0;
    qsizetype byteLength = 0;

    bool operator==(const InlineSpan &) const = default;
};

struct DiffHunk
{
    qsizetype leftStart = 0;
    qsizetype leftCount = 0;
    qsizetype rightStart = 0;
    qsizetype rightCount = 0;
    ChangeKind kind = ChangeKind::Modified;
    QVector<InlineSpan> leftInlineSpans;
    QVector<InlineSpan> rightInlineSpans;

    bool operator==(const DiffHunk &) const = default;
};

enum class Status
{
    Completed,
    Cancelled,
    UnsupportedInput,
    LimitExceeded,
};

struct Limits
{
    qsizetype maxBytesPerInput = 32 * 1024 * 1024;
    qsizetype maxLinesPerInput = 250000;
    quint64 maxWorkUnits = 50000000;
};

class CancellationToken
{
public:
    CancellationToken();

    bool isCancellationRequested() const;

private:
    explicit CancellationToken(std::shared_ptr<std::atomic_bool> state);

    std::shared_ptr<std::atomic_bool> state;

    friend class CancellationSource;
};

class CancellationSource
{
public:
    CancellationSource();

    CancellationToken token() const;
    void cancel() const;

private:
    std::shared_ptr<std::atomic_bool> state;
};

struct Result
{
    Status status = Status::Completed;
    QVector<DiffHunk> hunks;
    QString message;
};

}