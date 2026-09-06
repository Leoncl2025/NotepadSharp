#include "JsonOperations.h"

#include <QCoreApplication>
#include <QHash>
#include <QRegularExpression>
#include <QSet>
#include <jsoncons/json.hpp>
#include <jsoncons/utility/bigint.hpp>
#include <jsoncons_ext/jsonpath/jsonpath.hpp>
#include <jsoncons_ext/jsonschema/jsonschema.hpp>
#include <functional>

namespace JsonTools {
namespace {

using Json = jsoncons::json;
constexpr qsizetype resultLimit = 10000;

QString locationPointer(const jsoncons::jsonpath::json_location &location)
{
    QString result;
    for (const auto &element : location) {
        QString part = element.has_name()
            ? QString::fromUtf8(element.name().data(), element.name().size()) : QString::number(element.index());
        part.replace("~", "~0");
        part.replace("/", "~1");
        result += '/' + part;
    }
    return result;
}

bool hasDuplicate(const Index &index, int record)
{
    return std::any_of(index.diagnostics.cbegin(), index.diagnostics.cend(), [record](const Diagnostic &diagnostic) {
        return diagnostic.record == record && diagnostic.warning;
    });
}

Json parse(const Index &index, int node)
{
    return Json::parse(index.raw(node).toStdString());
}

struct ExactNumber
{
    QString digits;
    jsoncons::bigint exponent;
    bool negative;
};

ExactNumber exactNumber(const QByteArray &source)
{
    static const QRegularExpression pattern(QStringLiteral("^(-?)([0-9]+)(?:\\.([0-9]+))?(?:[eE]([+-]?[0-9]+))?$"));
    const auto match = pattern.match(QString::fromLatin1(source));
    const QString digits = match.captured(2) + match.captured(3);
    qsizetype first = 0;
    while (first < digits.size() && digits[first] == '0') { ++first; }
    if (first == digits.size()) { return {QStringLiteral("0"), jsoncons::bigint(0), false}; }
    qsizetype end = digits.size();
    while (end > first && digits[end - 1] == '0') { --end; }
    QString exponentText = match.captured(4);
    if (exponentText.startsWith('+')) { exponentText.remove(0, 1); }
    jsoncons::bigint exponent = exponentText.isEmpty() ? jsoncons::bigint(0) : jsoncons::bigint(exponentText.toStdString());
    exponent -= jsoncons::bigint(match.captured(3).size());
    exponent += digits.size() - end;
    return {digits.mid(first, end - first), std::move(exponent), !match.captured(1).isEmpty()};
}

}

QueryResult query(const Index &index, const QString &expression, SearchTarget target, Qt::CaseSensitivity sensitivity)
{
    QueryResult result;
    const auto add = [&result](int node) {
        if (result.matches.size() < resultLimit) {
            result.matches.append(node);
        }
        else {
            result.truncated = true;
        }
    };
    if (target != SearchTarget::Path) {
        for (int node = 0; node < index.nodes.size(); ++node) {
            const Node &item = index.nodes[node];
            if (target == SearchTarget::Key && item.keyStart >= 0 && item.key.contains(expression, sensitivity)) {
                add(node);
            }
            else if (target == SearchTarget::Value && item.kind != Kind::Object && item.kind != Kind::Array
                     && index.decoded(node).contains(expression, sensitivity)) {
                add(node);
            }
        }
        return result;
    }

    const QString input = expression.trimmed();
    if (input.isEmpty()) {
        result.error = QCoreApplication::translate("JsonTools", "Enter a JSONPath or JSON Pointer.");
        return result;
    }
    if (input.startsWith('/')) {
        for (int node : index.findPointer(input)) {
            add(node);
        }
        return result;
    }

    for (int node = 0; node < index.nodes.size(); ++node) {
        if (index.path(node) == input) {
            add(node);
        }
    }
    if (!result.matches.isEmpty()) {
        return result;
    }

    try {
        const auto compiled = jsoncons::jsonpath::make_expression<Json>(input.toStdString());
        QHash<QString, QVector<int>> locations;
        for (int node = 0; node < index.nodes.size(); ++node) {
            locations[QString::number(index.nodes[node].record) + ':' + index.pointer(node)].append(node);
        }
        for (int root : index.roots) {
            const int record = index.nodes[root].record;
            if (!index.recordValid(record) || hasDuplicate(index, record)) {
                ++result.skippedRecords;
                continue;
            }
            const auto value = parse(index, root);
            const auto matches = compiled.select_paths(value);
            for (const auto &location : matches) {
                const QString key = QString::number(record) + ':' + locationPointer(location);
                for (int node : locations.value(key)) {
                    add(node);
                }
            }
        }
    }
    catch (const std::exception &exception) {
        result.error = QString::fromUtf8(exception.what());
        result.matches.clear();
    }
    return result;
}

ValidationResult validate(const Index &index, const QByteArray &schema)
{
    ValidationResult result;
    result.issues = index.diagnostics;
    try {
        const Index schemaIndex(schema);
        if (!schemaIndex.diagnostics.isEmpty()) {
            result.error = schemaIndex.diagnostics.first().message;
            return result;
        }
        const auto compiled = jsoncons::jsonschema::make_json_schema(Json::parse(schema.toStdString()),
            [](const jsoncons::uri &) -> Json {
                throw std::runtime_error("External schema references are disabled; use local $defs.");
            });
        for (int root : index.roots) {
            const int record = index.nodes[root].record;
            if (!index.recordValid(record) || hasDuplicate(index, record)) {
                continue;
            }
            ++result.checkedRecords;
            compiled.validate(parse(index, root), [&](const jsoncons::jsonschema::validation_message &message) {
                const auto matches = index.findPointer(QString::fromStdString(message.instance_location().string()), record);
                const qsizetype offset = matches.isEmpty() ? index.nodes[root].start : index.nodes[matches.first()].start;
                result.issues.append({offset, QString::fromStdString(message.message()), false, record});
                return result.issues.size() >= resultLimit ? jsoncons::jsonschema::walk_state::abort : jsoncons::jsonschema::walk_state::advance;
            });
            if (result.issues.size() >= resultLimit) {
                break;
            }
        }
    }
    catch (const std::exception &exception) {
        result.error = QString::fromUtf8(exception.what());
    }
    return result;
}

ComparisonResult compare(const Index &left, const Index &right, bool ignoreObjectOrder)
{
    ComparisonResult result;
    if (!left.diagnostics.isEmpty() || !right.diagnostics.isEmpty()) {
        result.error = QCoreApplication::translate("JsonTools", "Comparison requires valid JSON without duplicate keys.");
        return result;
    }
    const auto add = [&result](Change change, int before, int after) {
        if (result.differences.size() < resultLimit) {
            result.differences.append({change, before, after});
        }
        else {
            result.truncated = true;
        }
    };
    std::function<void(int, int)> visit = [&](int before, int after) {
        if (result.truncated) {
            return;
        }
        if (before < 0 || after < 0) {
            add(before < 0 ? Change::Added : Change::Removed, before, after);
            return;
        }
        const Node &first = left.nodes[before];
        const Node &second = right.nodes[after];
        if (first.kind != second.kind) {
            add(Change::Modified, before, after);
        }
        else if (first.kind == Kind::Object) {
            QHash<QString, int> firstKeys;
            QHash<QString, int> secondKeys;
            QStringList firstOrder;
            QStringList secondOrder;
            for (int child : first.children) {
                firstKeys.insert(left.nodes[child].key, child);
                firstOrder.append(left.nodes[child].key);
            }
            for (int child : second.children) {
                secondKeys.insert(right.nodes[child].key, child);
                secondOrder.append(right.nodes[child].key);
            }
            if (!ignoreObjectOrder && firstOrder != secondOrder && firstKeys.keys().size() == secondKeys.keys().size()
                && QSet<QString>(firstOrder.begin(), firstOrder.end()) == QSet<QString>(secondOrder.begin(), secondOrder.end())) {
                add(Change::Order, before, after);
            }
            for (int child : first.children) {
                visit(child, secondKeys.value(left.nodes[child].key, -1));
            }
            for (int child : second.children) {
                if (!firstKeys.contains(right.nodes[child].key)) {
                    visit(-1, child);
                }
            }
        }
        else if (first.kind == Kind::Array) {
            const qsizetype count = qMax(first.children.size(), second.children.size());
            for (qsizetype element = 0; element < count; ++element) {
                visit(element < first.children.size() ? first.children[element] : -1,
                      element < second.children.size() ? second.children[element] : -1);
            }
        }
        else if (first.kind == Kind::Number) {
            const auto firstNumber = exactNumber(left.raw(before));
            const auto secondNumber = exactNumber(right.raw(after));
            if (firstNumber.digits != secondNumber.digits || firstNumber.exponent != secondNumber.exponent
                || firstNumber.negative != secondNumber.negative) {
                add(Change::Modified, before, after);
            }
        }
        else if (parse(left, before) != parse(right, after)) {
            add(Change::Modified, before, after);
        }
    };
    try {
        const qsizetype count = qMax(left.roots.size(), right.roots.size());
        for (qsizetype record = 0; record < count; ++record) {
            visit(record < left.roots.size() ? left.roots[record] : -1,
                  record < right.roots.size() ? right.roots[record] : -1);
        }
    }
    catch (const std::exception &exception) {
        result.error = QString::fromUtf8(exception.what());
    }
    return result;
}

}