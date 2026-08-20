/*
 * This file is part of Notepad Next.
 *
 * Notepad Next is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "CompareNavigator.h"

#include <algorithm>

namespace Compare
{

void Navigator::reset(const QVector<DiffHunk> &newHunks)
{
    hunks = newHunks;
    currentIndex = -1;
}

bool Navigator::hasDifferences() const
{
    return !hunks.isEmpty();
}

const DiffHunk *Navigator::current() const
{
    return currentIndex >= 0 && currentIndex < hunks.size()
        ? &hunks.at(currentIndex)
        : nullptr;
}

const DiffHunk *Navigator::next()
{
    if (hunks.isEmpty()) {
        return nullptr;
    }

    currentIndex = (currentIndex + 1) % hunks.size();
    return current();
}

const DiffHunk *Navigator::previous()
{
    if (hunks.isEmpty()) {
        return nullptr;
    }

    currentIndex = currentIndex <= 0 ? hunks.size() - 1 : currentIndex - 1;
    return current();
}

qsizetype Navigator::mapLine(qsizetype sourceLine,
                             const QVector<DiffHunk> &hunks,
                             bool leftToRight)
{
    qsizetype offset = 0;

    for (const DiffHunk &hunk : hunks) {
        const qsizetype sourceStart = leftToRight ? hunk.leftStart : hunk.rightStart;
        const qsizetype sourceCount = leftToRight ? hunk.leftCount : hunk.rightCount;
        const qsizetype targetStart = leftToRight ? hunk.rightStart : hunk.leftStart;
        const qsizetype targetCount = leftToRight ? hunk.rightCount : hunk.leftCount;

        if (sourceLine < sourceStart) {
            return std::max<qsizetype>(0, sourceLine + offset);
        }

        if (sourceCount > 0 && sourceLine < sourceStart + sourceCount) {
            if (targetCount == 0) {
                return targetStart;
            }

            const qsizetype relativeLine = sourceLine - sourceStart;
            return targetStart + std::min(relativeLine, targetCount - 1);
        }

        offset = (targetStart + targetCount) - (sourceStart + sourceCount);
    }

    return std::max<qsizetype>(0, sourceLine + offset);
}

}