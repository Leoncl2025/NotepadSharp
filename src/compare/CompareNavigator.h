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

namespace Compare
{

class Navigator
{
public:
    void reset(const QVector<DiffHunk> &hunks = {});

    bool hasDifferences() const;
    qsizetype currentNumber() const { return currentIndex + 1; }
    const DiffHunk *current() const;
    const DiffHunk *next();
    const DiffHunk *previous();

    static qsizetype mapLine(qsizetype sourceLine,
                             const QVector<DiffHunk> &hunks,
                             bool leftToRight);

private:
    QVector<DiffHunk> hunks;
    qsizetype currentIndex = -1;
};

}