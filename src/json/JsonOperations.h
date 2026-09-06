#ifndef JSONOPERATIONS_H
#define JSONOPERATIONS_H

#include "JsonIndex.h"

namespace JsonTools {

enum class SearchTarget { Path, Key, Value };

struct QueryResult
{
    QVector<int> matches;
    QString error;
    int skippedRecords = 0;
    bool truncated = false;
};

struct ValidationResult
{
    QVector<Diagnostic> issues;
    QString error;
    int checkedRecords = 0;
};

enum class Change { Added, Removed, Modified, Order };

struct Difference
{
    Change change;
    int left = -1;
    int right = -1;
};

struct ComparisonResult
{
    QVector<Difference> differences;
    QString error;
    bool truncated = false;
};

QueryResult query(const Index &index, const QString &expression, SearchTarget target = SearchTarget::Path,
                  Qt::CaseSensitivity sensitivity = Qt::CaseInsensitive);
ValidationResult validate(const Index &index, const QByteArray &schema);
ComparisonResult compare(const Index &left, const Index &right, bool ignoreObjectOrder = true);

}

#endif