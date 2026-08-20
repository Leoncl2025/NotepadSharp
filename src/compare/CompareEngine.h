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

class Engine
{
public:
    static Result compare(const QByteArray &left,
                          const QByteArray &right,
                          const CancellationToken &cancellationToken = {},
                          const Limits &limits = {});
};

}