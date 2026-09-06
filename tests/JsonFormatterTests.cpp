#include <QtTest>

#include "JsonFormatter.h"

class JsonFormatterTests : public QObject
{
    Q_OBJECT

private slots:
    void prettyAndCompact();
    void toggle();
    void preserveValues();
    void serializedJson_data();
    void serializedJson();
    void serializedToggle();
    void scalarValues_data();
    void scalarValues();
    void malformedInput_data();
    void malformedInput();
    void preserveComments();
    void customLayout();
};

void JsonFormatterTests::prettyAndCompact()
{
    const QString compact = QStringLiteral(R"({"items":[1,true,null],"empty":{},"list":[]})");
    const QString pretty = QStringLiteral("{\n    \"items\": [\n        1,\n        true,\n        null\n    ],\n    \"empty\": {},\n    \"list\": []\n}");
    const auto expanded = JsonFormatter::format(compact);
    QVERIFY2(expanded.isValid(), qPrintable(expanded.error));
    QCOMPARE(expanded.text, pretty);
    const auto compressed = JsonFormatter::format(pretty, JsonFormatter::Mode::Compact);
    QVERIFY(compressed.isValid());
    QCOMPARE(compressed.text, compact);
    QCOMPARE(JsonFormatter::format(pretty).text, pretty);
}

void JsonFormatterTests::toggle()
{
    const QString compact = QStringLiteral(R"({"value":1})");
    const auto expanded = JsonFormatter::format(compact + '\n', JsonFormatter::Mode::Toggle);
    QCOMPARE(expanded.mode, JsonFormatter::Mode::Pretty);
    QVERIFY(expanded.text.contains('\n'));
    const auto compressed = JsonFormatter::format(expanded.text, JsonFormatter::Mode::Toggle);
    QCOMPARE(compressed.mode, JsonFormatter::Mode::Compact);
    QCOMPARE(compressed.text, compact);
}

void JsonFormatterTests::preserveValues()
{
    const QString input = QStringLiteral(R"({"z":9007199254740993,"a":-0.0e+10,"z":1,"path":"C:\\tmp","text":"a,b:{[\"x\"]}","escaped":"\u4f60\n","nested":"{\"a\":1}"})");
    const auto expanded = JsonFormatter::format(input);
    QVERIFY2(expanded.isValid(), qPrintable(expanded.error));
    QVERIFY(!expanded.decodedString);
    QCOMPARE(JsonFormatter::format(expanded.text, JsonFormatter::Mode::Compact).text, input);
}

void JsonFormatterTests::serializedJson_data()
{
    QTest::addColumn<QString>("input");
    QTest::addColumn<QString>("decoded");
    QTest::addColumn<bool>("valid");
    const auto serialize = [](const QString &text) {
        const QByteArray array = QJsonDocument(QJsonArray{text}).toJson(QJsonDocument::Compact);
        return QString::fromUtf8(array.mid(1, array.size() - 2));
    };
    const QString object = QStringLiteral(R"({"a":[1,2]})");
    const QString serialized = serialize(object);
    QTest::newRow("quoted") << serialized << object << true;
    QTest::newRow("unquoted") << serialized.mid(1, serialized.size() - 2) << object << true;
    QTest::newRow("unquoted-multiline") << QStringLiteral("{\\\"a\\\":[1,\n2]}") << object << true;
    QTest::newRow("double-serialized") << serialize(serialized) << object << true;
    QTest::newRow("triple-serialized") << serialize(serialize(serialized)) << object << true;
    const QString escaped = QStringLiteral(R"({"path":"C:\\tmp\\file.json","quote":"he said \"hello\"","line":"one\ntwo","unicode":"\u4f60"})");
    const QString escapedSerialized = serialize(escaped);
    QTest::newRow("preserve-inner-escapes") << escapedSerialized << escaped << true;
    QTest::newRow("preserve-unquoted-inner-escapes")
        << escapedSerialized.mid(1, escapedSerialized.size() - 2) << escaped << true;
    const QString incomplete = QStringLiteral(R"({"a":1,"b":[2,3])");
    QTest::newRow("incomplete-object") << serialize(incomplete) << incomplete << false;
    const QString incompleteSerialized = serialize(incomplete);
    QTest::newRow("incomplete-unquoted-object")
        << incompleteSerialized.mid(1, incompleteSerialized.size() - 2) << incomplete << false;
    const QString unterminated = QStringLiteral("{\"a\":\"unfinished  ");
    QTest::newRow("unterminated-inner-string") << serialize(unterminated) << unterminated << false;
}

void JsonFormatterTests::serializedJson()
{
    QFETCH(QString, input);
    QFETCH(QString, decoded);
    QFETCH(bool, valid);
    const auto result = JsonFormatter::format(input);
    QCOMPARE(result.isValid(), valid);
    QVERIFY(result.decodedString);
    QCOMPARE(result.text, JsonFormatter::format(decoded).text);
    QCOMPARE(JsonFormatter::format(input, JsonFormatter::Mode::Compact).text, decoded);
    QVERIFY(result.text.contains('\n'));
}

void JsonFormatterTests::serializedToggle()
{
    const auto result = JsonFormatter::format(QStringLiteral(R"("{\n  \"a\": 1\n}")"), JsonFormatter::Mode::Toggle);
    QVERIFY(result.isValid());
    QVERIFY(result.decodedString);
    QCOMPARE(result.mode, JsonFormatter::Mode::Pretty);
    QCOMPARE(result.text, QStringLiteral("{\n    \"a\": 1\n}"));
    const auto compact = JsonFormatter::format(result.text, JsonFormatter::Mode::Toggle);
    QCOMPARE(compact.mode, JsonFormatter::Mode::Compact);
    QCOMPARE(compact.text, QStringLiteral(R"({"a":1})"));
}

void JsonFormatterTests::scalarValues_data()
{
    QTest::addColumn<QString>("input");
    QTest::newRow("string") << QStringLiteral(R"("hello\nworld")");
    QTest::newRow("number") << QStringLiteral("42");
    QTest::newRow("boolean") << QStringLiteral("true");
    QTest::newRow("null") << QStringLiteral("null");
}

void JsonFormatterTests::scalarValues()
{
    QFETCH(QString, input);
    const auto result = JsonFormatter::format(input);
    QVERIFY2(result.isValid(), qPrintable(result.error));
    QCOMPARE(result.text, input);
}

void JsonFormatterTests::malformedInput_data()
{
    QTest::addColumn<QString>("input");
    QTest::addColumn<QString>("expected");
    QTest::newRow("missing-close") << QStringLiteral(R"({"a":1,"b":[2,3])")
        << QStringLiteral("{\n    \"a\": 1,\n    \"b\": [\n        2,\n        3\n    ]");
    QTest::newRow("trailing-comma") << QStringLiteral(R"({"a":1,})")
        << QStringLiteral("{\n    \"a\": 1,\n}");
    QTest::newRow("missing-colon") << QStringLiteral(R"({"a" 1})")
        << QStringLiteral("{\n    \"a\" 1\n}");
    QTest::newRow("missing-quote") << QStringLiteral("{\"a\":\"unterminated, [  ")
        << QStringLiteral("{\n    \"a\": \"unterminated, [  ");
    QTest::newRow("empty") << QString() << QString();
    QTest::newRow("multiple-values") << QStringLiteral("true false") << QStringLiteral("true false");
    QTest::newRow("multiple-comma-values") << QStringLiteral("1,2") << QStringLiteral("1,\n2");
    QTest::newRow("unmatched-close") << QStringLiteral("}") << QStringLiteral("}");
}

void JsonFormatterTests::malformedInput()
{
    QFETCH(QString, input);
    QFETCH(QString, expected);
    const auto result = JsonFormatter::format(input);
    QVERIFY(!result.isValid());
    QVERIFY(result.errorOffset >= 0);
    QVERIFY(result.errorOffset <= result.text.size());
    QCOMPARE(result.text, expected);
    const auto compact = JsonFormatter::format(result.text, JsonFormatter::Mode::Compact);
    QVERIFY(!compact.isValid());
}

void JsonFormatterTests::preserveComments()
{
    const QString input = QStringLiteral("{\"a\":1,// keep , }\n\"b\":/* keep [ */2}");
    const auto result = JsonFormatter::format(input);
    QVERIFY(!result.isValid());
    QVERIFY(result.text.contains("// keep , }\n"));
    QVERIFY(result.text.contains("/* keep [ */"));
    const auto compact = JsonFormatter::format(result.text, JsonFormatter::Mode::Compact);
    QVERIFY(compact.text.contains("// keep , }\n"));
    QVERIFY(compact.text.contains("\"b\":/* keep [ */ 2"));
}

void JsonFormatterTests::customLayout()
{
    const auto result = JsonFormatter::format(QStringLiteral(R"({"a":[1]})"),
                                              JsonFormatter::Mode::Pretty,
                                              QStringLiteral("\t"), QStringLiteral("\r\n"));
    QCOMPARE(result.text, QStringLiteral("{\r\n\t\"a\": [\r\n\t\t1\r\n\t]\r\n}"));
}

QTEST_APPLESS_MAIN(JsonFormatterTests)

#include "JsonFormatterTests.moc"