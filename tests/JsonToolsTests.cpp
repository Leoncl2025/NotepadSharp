#include "json/JsonIndex.h"
#include "json/JsonOperations.h"

#include <QTest>
#include <QFile>
#include <jsoncons/json.hpp>
#include <jsoncons_ext/jsonpath/jsonpath.hpp>

using namespace JsonTools;

class JsonToolsTests : public QObject
{
    Q_OBJECT

private slots:
    void pathsAndSourceRanges();
    void escapedKeysAndUnicode();
    void duplicateKeysAreAmbiguous();
    void malformedPrefixRemainsNavigable();
    void independentJsonLines();
    void jsonPathEngine();
    void querySourceLocations();
    void searchKeysAndValues();
    void queryLinesWithBadRecords();
    void schemaErrorsAndOfflineReferences();
    void semanticComparison();
    void documentedExamples();
    void largeNumbersRemainDistinct();
};

void JsonToolsTests::pathsAndSourceRanges()
{
    const Index index(R"({"users":[{"name":"Alice","age":21}],"empty":{}})");
    QVERIFY(index.diagnostics.isEmpty());
    const int name = index.nodeAt(index.source.indexOf("Alice") + 1);
    QCOMPARE(index.path(name), QStringLiteral("$.users[0].name"));
    QCOMPARE(index.pointer(name), QStringLiteral("/users/0/name"));
    QCOMPARE(index.raw(name), QByteArray("\"Alice\""));
    QCOMPARE(index.decoded(name), QStringLiteral("Alice"));
    QCOMPARE(index.nodeAt(index.nodes[name].keyStart), name);
    QCOMPARE(index.findPointer(QStringLiteral("/users/0/name")), QVector<int>{name});
    QCOMPARE(index.path(index.nodeAt(index.source.size() - 1)), QStringLiteral("$"));
}

void JsonToolsTests::escapedKeysAndUnicode()
{
    const Index index(QStringLiteral("{\"a.b\":{\"/~\":{\"\\u4f60\":\"\u4f60\"}}}").toUtf8());
    QVERIFY(index.diagnostics.isEmpty());
    const int value = index.nodes.size() - 1;
    QCOMPARE(index.pointer(value), QStringLiteral("/a.b/~1~0/\u4f60"));
    QVERIFY(index.path(value).startsWith(QStringLiteral("$[\"a.b\"][\"/~\"]")));
    QCOMPARE(index.decoded(value), QStringLiteral("\u4f60"));
    QCOMPARE(index.nodeAt(index.nodes[value].start + 2), value);
}

void JsonToolsTests::duplicateKeysAreAmbiguous()
{
    const Index index(R"({"key":{"id":1},"key":{"id":2}})");
    QCOMPARE(index.diagnostics.size(), 1);
    QVERIFY(index.diagnostics.first().warning);
    const auto matches = index.findPointer(QStringLiteral("/key/id"));
    QCOMPARE(matches.size(), 2);
    QVERIFY(index.ambiguous(matches[0]));
    QVERIFY(index.ambiguous(matches[1]));
}

void JsonToolsTests::malformedPrefixRemainsNavigable()
{
    const Index index(R"({"good":1,"items":[2,{"unfinished":"value)");
    QVERIFY(!index.diagnostics.isEmpty());
    QCOMPARE(index.path(index.nodeAt(index.source.indexOf('1'))), QStringLiteral("$.good"));
    QCOMPARE(index.path(index.nodes.size() - 1), QStringLiteral("$.items[1].unfinished"));
    QVERIFY(!index.nodes.last().complete);
    QVERIFY(!index.recordValid(1));
}

void JsonToolsTests::independentJsonLines()
{
    const Index index("{\"id\":1}\r\n{\"id\":\r\n\n{\"id\":3}\n", true);
    QCOMPARE(index.roots.size(), 3);
    QVERIFY(index.recordValid(1));
    QVERIFY(!index.recordValid(2));
    QVERIFY(index.recordValid(4));
    const auto matches = index.findPointer(QStringLiteral("/id"));
    QCOMPARE(matches.size(), 2);
    QCOMPARE(index.nodes[matches.last()].record, 4);
    QCOMPARE(index.lineAt(index.nodes[matches.last()].start), 4);
}

void JsonToolsTests::jsonPathEngine()
{
    const auto document = jsoncons::json::parse(R"({"users":[{"age":15},{"age":21}]})");
    const auto result = jsoncons::jsonpath::json_query(document, "$.users[?(@.age >= 18)].age");
    QCOMPARE(result.size(), size_t(1));
    QCOMPARE(result[0].as<int>(), 21);
}

void JsonToolsTests::querySourceLocations()
{
    const Index index(R"({"users":[{"age":15,"name":"A"},{"age":21,"name":"B"}],"a.b":{"/~":7}})");
    const auto adults = query(index, QStringLiteral("$.users[?(@.age >= 18)].name"));
    QVERIFY2(adults.error.isEmpty(), qPrintable(adults.error));
    QCOMPARE(adults.matches.size(), 1);
    QCOMPARE(index.decoded(adults.matches[0]), QStringLiteral("B"));
    QCOMPARE(query(index, QStringLiteral("$.users[*].name")).matches.size(), 2);
    QCOMPARE(query(index, QStringLiteral("$..age")).matches.size(), 2);
    const auto special = query(index, QStringLiteral("$['a.b']['/~']"));
    QCOMPARE(special.matches.size(), 1);
    QCOMPARE(index.raw(special.matches[0]), QByteArray("7"));
    QVERIFY(!query(index, QStringLiteral("$[?")).error.isEmpty());
    const Index duplicates(R"({"key":1,"key":2})");
    QCOMPARE(query(duplicates, QStringLiteral("$.key")).matches.size(), 2);
    QCOMPARE(query(duplicates, QStringLiteral("$..key")).skippedRecords, 1);
}

void JsonToolsTests::searchKeysAndValues()
{
    const Index index(R"({"Name":"Alice","nested":{"name":"Bob"},"text":"name"})");
    QCOMPARE(query(index, QStringLiteral("name"), SearchTarget::Key).matches.size(), 2);
    QCOMPARE(query(index, QStringLiteral("name"), SearchTarget::Value).matches.size(), 1);
    QCOMPARE(query(index, QStringLiteral("name"), SearchTarget::Key, Qt::CaseSensitive).matches.size(), 1);
}

void JsonToolsTests::queryLinesWithBadRecords()
{
    const Index index("{\"id\":1}\n{\"id\":\n{\"id\":3}\n", true);
    const auto result = query(index, QStringLiteral("$..id"));
    QVERIFY(result.error.isEmpty());
    QCOMPARE(result.skippedRecords, 1);
    QCOMPARE(result.matches.size(), 2);
    QCOMPARE(index.nodes[result.matches.last()].record, 3);
}

void JsonToolsTests::schemaErrorsAndOfflineReferences()
{
    const Index index(R"({"age":12,"name":7})");
    const QByteArray schema = R"({"$schema":"https://json-schema.org/draft/2020-12/schema","type":"object","required":["name"],"properties":{"age":{"minimum":18},"name":{"type":"string"}}})";
    const auto result = validate(index, schema);
    QVERIFY2(result.error.isEmpty(), qPrintable(result.error));
    QCOMPARE(result.checkedRecords, 1);
    QCOMPARE(result.issues.size(), 2);
    for (const auto &issue : result.issues) {
        QVERIFY(index.nodeAt(issue.offset) > 0);
    }
    const auto external = validate(index, "{\"$ref\":\"https://example.invalid/schema\"}");
    QVERIFY(!external.error.isEmpty());
    const auto valid = validate(Index(R"({"age":22,"name":"A"})"), schema);
    QVERIFY(valid.error.isEmpty());
    QVERIFY(valid.issues.isEmpty());
}

void JsonToolsTests::semanticComparison()
{
    const Index left(R"({"a":1,"b":[1,2],"remove":true})");
    const Index right(R"({"b":[1,3],"a":1.0,"add":false})");
    const auto result = compare(left, right);
    QVERIFY2(result.error.isEmpty(), qPrintable(result.error));
    QCOMPARE(result.differences.size(), 3);
    const Index reordered(R"({"remove":true,"b":[1,2],"a":1})");
    QVERIFY(compare(left, reordered).differences.isEmpty());
    QCOMPARE(compare(left, reordered, false).differences.size(), 1);
    QVERIFY(!compare(left, Index("{")).error.isEmpty());
}

void JsonToolsTests::documentedExamples()
{
    QFile sample(QFINDTESTDATA("../doc/json/sample.json"));
    QFile schema(QFINDTESTDATA("../doc/json/schema.json"));
    QFile other(QFINDTESTDATA("../doc/json/compare.json"));
    QFile lines(QFINDTESTDATA("../doc/json/sample.jsonl"));
    QVERIFY(sample.open(QIODevice::ReadOnly));
    QVERIFY(schema.open(QIODevice::ReadOnly));
    QVERIFY(other.open(QIODevice::ReadOnly));
    QVERIFY(lines.open(QIODevice::ReadOnly));
    const Index index(sample.readAll());
    const auto result = query(index, QStringLiteral("$.users[?(@.age >= 18)].name"));
    QCOMPARE(result.matches.size(), 1);
    QCOMPARE(index.decoded(result.matches.first()), QStringLiteral("Alice"));
    QCOMPARE(validate(index, schema.readAll()).issues.size(), 1);
    QCOMPARE(compare(index, Index(other.readAll())).differences.size(), 3);
    const auto log = query(Index(lines.readAll(), true), QStringLiteral("$..name"));
    QCOMPARE(log.matches.size(), 2);
    QCOMPARE(log.skippedRecords, 1);
}

void JsonToolsTests::largeNumbersRemainDistinct()
{
    const Index left("{\"value\":18446744073709551616}");
    const Index right("{\"value\":18446744073709551617}");
    const auto result = compare(left, right);
    QVERIFY(result.error.isEmpty());
    QCOMPARE(result.differences.size(), 1);
    const Index decimalLeft("1.000000000000000000000000001");
    const Index decimalRight("1.000000000000000000000000002");
    QCOMPARE(compare(decimalLeft, decimalRight).differences.size(), 1);
    const Index scientific("123e-2");
    const Index decimal("1.2300");
    QVERIFY(compare(scientific, decimal).differences.isEmpty());
    const Index negativeZero("-0.000");
    const Index zero("0e100");
    QVERIFY(compare(negativeZero, zero).differences.isEmpty());
}

QTEST_APPLESS_MAIN(JsonToolsTests)

#include "JsonToolsTests.moc"