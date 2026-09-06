#include "JsonIndex.h"

#include <QCoreApplication>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QRegularExpression>
#include <QStringList>
#include <algorithm>

namespace JsonTools {
namespace {

QString stringValue(const QByteArray &text)
{
    const auto document = QJsonDocument::fromJson("[" + text + "]");
    return document.isArray() && document.array().size() == 1
        ? document.array().first().toString() : QString();
}

QString propertyPath(const QString &key)
{
    static const QRegularExpression identifier(QStringLiteral("^[A-Za-z_][A-Za-z0-9_]*$"));
    if (identifier.match(key).hasMatch()) {
        return '.' + key;
    }
    const QByteArray encoded = QJsonDocument(QJsonArray{key}).toJson(QJsonDocument::Compact);
    return QString::fromUtf8(encoded);
}

bool space(char character)
{
    return character == ' ' || character == '\t' || character == '\r' || character == '\n';
}

}

Index::Index(const QByteArray &text, bool jsonLines) : source(text), lines(jsonLines)
{
    lineStarts.append(0);
    for (qsizetype offset = 0; offset < source.size(); ++offset) {
        if (source[offset] == '\r' || source[offset] == '\n') {
            if (source[offset] == '\r' && offset + 1 < source.size() && source[offset + 1] == '\n') {
                ++offset;
            }
            lineStarts.append(offset + 1);
        }
    }
    if (lines) {
        for (qsizetype line = 0; line < lineStarts.size(); ++line) {
            const qsizetype end = line + 1 < lineStarts.size() ? lineStarts[line + 1] : source.size();
            if (!source.mid(lineStarts[line], end - lineStarts[line]).trimmed().isEmpty()) {
                parseRecord(lineStarts[line], end, line + 1);
            }
        }
    }
    else {
        parseRecord(0, source.size(), 1);
    }
}

void Index::skipSpace()
{
    while (cursor < limit) {
        if (space(source[cursor])) {
            ++cursor;
        }
        else if (cursor + 1 < limit && source[cursor] == '/' && source[cursor + 1] == '/') {
            cursor += 2;
            while (cursor < limit && source[cursor] != '\n' && source[cursor] != '\r') {
                ++cursor;
            }
        }
        else if (cursor + 1 < limit && source[cursor] == '/' && source[cursor + 1] == '*') {
            cursor += 2;
            while (cursor + 1 < limit && !(source[cursor] == '*' && source[cursor + 1] == '/')) {
                ++cursor;
            }
            cursor = qMin(cursor + 2, limit);
        }
        else {
            break;
        }
    }
}

bool Index::scanString()
{
    ++cursor;
    while (cursor < limit) {
        const char character = source[cursor++];
        if (character == '\\' && cursor < limit) {
            ++cursor;
        }
        else if (character == '"') {
            return true;
        }
    }
    return false;
}

void Index::parseRecord(qsizetype begin, qsizetype end, int record)
{
    cursor = begin;
    limit = end;
    currentRecord = record;
    skipSpace();
    const int root = parseValue(-1, {}, -1, -1, -1, 0);
    if (root >= 0) {
        roots.append(root);
    }

    QJsonParseError error;
    const QByteArray recordText = source.mid(begin, end - begin);
    const auto document = QJsonDocument::fromJson("[" + recordText + "\n]", &error);
    if (error.error != QJsonParseError::NoError) {
        diagnostics.append({begin + qBound(qsizetype(0), qsizetype(error.offset) - 1, end - begin),
                            error.errorString(), false, record});
    }
    else if (document.array().size() != 1) {
        diagnostics.append({begin, QCoreApplication::translate("JsonTools", "Expected a single JSON value."), false, record});
    }
}

int Index::parseValue(int parent, const QString &key, qsizetype keyStart, qsizetype keyEnd,
                      int arrayIndex, int depth)
{
    skipSpace();
    if (cursor >= limit || source[cursor] == '}' || source[cursor] == ']' || source[cursor] == ',') {
        return -1;
    }
    if (depth > 256 || nodes.size() >= 500000) {
        diagnostics.append({cursor, QCoreApplication::translate("JsonTools", "JSON navigation limit exceeded."), false, currentRecord});
        cursor = limit;
        return -1;
    }

    const int id = nodes.size();
    Node node;
    node.parent = parent;
    node.key = key;
    node.keyStart = keyStart;
    node.keyEnd = keyEnd;
    node.start = cursor;
    node.arrayIndex = arrayIndex;
    node.record = currentRecord;
    nodes.append(node);
    if (parent >= 0) {
        nodes[parent].children.append(id);
    }

    const char opening = source[cursor];
    if (opening == '{' || opening == '[') {
        nodes[id].kind = opening == '{' ? Kind::Object : Kind::Array;
        const char closing = opening == '{' ? '}' : ']';
        ++cursor;
        QHash<QString, int> seen;
        int element = 0;
        while (cursor < limit) {
            skipSpace();
            if (cursor >= limit) {
                break;
            }
            if (source[cursor] == closing) {
                ++cursor;
                nodes[id].complete = true;
                break;
            }

            QString childKey;
            qsizetype childKeyStart = -1;
            qsizetype childKeyEnd = -1;
            if (opening == '{') {
                if (source[cursor] != '"') {
                    break;
                }
                childKeyStart = cursor;
                if (!scanString()) {
                    break;
                }
                childKeyEnd = cursor;
                const QByteArray token = source.mid(childKeyStart, cursor - childKeyStart);
                QJsonParseError keyError;
                QJsonDocument::fromJson("[" + token + "]", &keyError);
                if (keyError.error != QJsonParseError::NoError) {
                    break;
                }
                childKey = stringValue(token);
                skipSpace();
                if (cursor >= limit || source[cursor] != ':') {
                    break;
                }
                ++cursor;
            }
            const int child = parseValue(id, childKey, childKeyStart, childKeyEnd,
                                         opening == '[' ? element++ : -1, depth + 1);
            if (child < 0) {
                break;
            }
            if (opening == '{') {
                if (seen.contains(childKey)) {
                    nodes[child].duplicate = true;
                    nodes[seen.value(childKey)].duplicate = true;
                    diagnostics.append({childKeyStart, QCoreApplication::translate("JsonTools", "Duplicate key: %1").arg(childKey), true, currentRecord});
                }
                seen.insert(childKey, child);
            }
            skipSpace();
            if (cursor < limit && source[cursor] == ',') {
                ++cursor;
            }
            else if (cursor >= limit || source[cursor] != closing) {
                break;
            }
        }
    }
    else if (opening == '"') {
        nodes[id].kind = Kind::String;
        nodes[id].complete = scanString();
    }
    else {
        while (cursor < limit && !space(source[cursor]) && source[cursor] != ','
               && source[cursor] != ']' && source[cursor] != '}') {
            ++cursor;
        }
        const QByteArray token = source.mid(nodes[id].start, cursor - nodes[id].start);
        QJsonParseError error;
        const auto value = QJsonDocument::fromJson("[" + token + "]", &error);
        if (error.error == QJsonParseError::NoError && value.array().size() == 1) {
            const auto scalar = value.array().first();
            nodes[id].kind = scalar.isNull() ? Kind::Null : scalar.isBool() ? Kind::Boolean : Kind::Number;
            nodes[id].complete = true;
        }
    }
    nodes[id].end = cursor;
    return id;
}

int Index::nodeAt(qsizetype position) const
{
    const auto after = std::upper_bound(nodes.cbegin(), nodes.cend(), position, [](qsizetype offset, const Node &node) {
        return offset < (node.keyStart >= 0 ? node.keyStart : node.start);
    });
    int candidate = after == nodes.cbegin() ? -1 : int(after - nodes.cbegin()) - 1;
    while (candidate >= 0) {
        const Node &node = nodes[candidate];
        if (position < node.end || (position == source.size() && position == node.end)) {
            return candidate;
        }
        candidate = node.parent;
    }
    return -1;
}

QString Index::path(int id) const
{
    QStringList parts;
    while (id >= 0 && nodes[id].parent >= 0) {
        const Node &node = nodes[id];
        parts.prepend(node.arrayIndex >= 0 ? QStringLiteral("[%1]").arg(node.arrayIndex) : propertyPath(node.key));
        id = node.parent;
    }
    return '$' + parts.join(QString());
}

QString Index::pointer(int id) const
{
    QStringList parts;
    while (id >= 0 && nodes[id].parent >= 0) {
        const Node &node = nodes[id];
        QString part = node.arrayIndex >= 0 ? QString::number(node.arrayIndex) : node.key;
        part.replace("~", "~0");
        part.replace("/", "~1");
        parts.prepend('/' + part);
        id = node.parent;
    }
    return parts.join(QString());
}

QVector<int> Index::findPointer(const QString &value, int record) const
{
    QVector<int> matches;
    for (int id = 0; id < nodes.size(); ++id) {
        if ((record < 0 || nodes[id].record == record) && pointer(id) == value) {
            matches.append(id);
        }
    }
    return matches;
}

bool Index::ambiguous(int id) const
{
    while (id >= 0) {
        if (nodes[id].duplicate) {
            return true;
        }
        id = nodes[id].parent;
    }
    return false;
}

QByteArray Index::raw(int id) const
{
    return id >= 0 && id < nodes.size() ? source.mid(nodes[id].start, nodes[id].end - nodes[id].start) : QByteArray();
}

QString Index::decoded(int id) const
{
    return id >= 0 && id < nodes.size() && nodes[id].kind == Kind::String ? stringValue(raw(id)) : QString::fromUtf8(raw(id));
}

int Index::lineAt(qsizetype position) const
{
    return std::upper_bound(lineStarts.cbegin(), lineStarts.cend(), position) - lineStarts.cbegin();
}

bool Index::recordValid(int record) const
{
    return std::none_of(diagnostics.cbegin(), diagnostics.cend(), [record](const Diagnostic &diagnostic) {
        return diagnostic.record == record && !diagnostic.warning;
    });
}

QString Index::kindName(Kind kind)
{
    switch (kind) {
    case Kind::Object: return QCoreApplication::translate("JsonTools", "Object");
    case Kind::Array: return QCoreApplication::translate("JsonTools", "Array");
    case Kind::String: return QCoreApplication::translate("JsonTools", "String");
    case Kind::Number: return QCoreApplication::translate("JsonTools", "Number");
    case Kind::Boolean: return QCoreApplication::translate("JsonTools", "Boolean");
    case Kind::Null: return QStringLiteral("null");
    default: return QCoreApplication::translate("JsonTools", "Incomplete");
    }
}

}