#ifndef JSONINDEX_H
#define JSONINDEX_H

#include <QByteArray>
#include <QString>
#include <QVector>

namespace JsonTools {

enum class Kind { Object, Array, String, Number, Boolean, Null, Unknown };

struct Node
{
    int parent = -1;
    QVector<int> children;
    QString key;
    qsizetype keyStart = -1;
    qsizetype keyEnd = -1;
    qsizetype start = 0;
    qsizetype end = 0;
    int arrayIndex = -1;
    int record = 1;
    Kind kind = Kind::Unknown;
    bool complete = false;
    bool duplicate = false;
};

struct Diagnostic
{
    qsizetype offset = 0;
    QString message;
    bool warning = false;
    int record = 1;
};

class Index
{
public:
    explicit Index(const QByteArray &text = {}, bool jsonLines = false);

    QByteArray source;
    QVector<Node> nodes;
    QVector<int> roots;
    QVector<Diagnostic> diagnostics;
    bool lines = false;

    int nodeAt(qsizetype position) const;
    QString path(int node) const;
    QString pointer(int node) const;
    QVector<int> findPointer(const QString &pointer, int record = -1) const;
    bool ambiguous(int node) const;
    QByteArray raw(int node) const;
    QString decoded(int node) const;
    int lineAt(qsizetype position) const;
    bool recordValid(int record) const;
    static QString kindName(Kind kind);

private:
    QVector<qsizetype> lineStarts;
    qsizetype cursor = 0;
    qsizetype limit = 0;
    int currentRecord = 1;

    void parseRecord(qsizetype begin, qsizetype end, int record);
    int parseValue(int parent, const QString &key, qsizetype keyStart, qsizetype keyEnd,
                   int arrayIndex, int depth);
    void skipSpace();
    bool scanString();
};

}

#endif