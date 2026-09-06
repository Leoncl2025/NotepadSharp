#include "ScintillaNext.h"

#include <QAction>
#include <QFile>
#include <QKeySequence>
#include <QMainWindow>
#include <QMap>
#include <QTest>
#include <QXmlStreamReader>

namespace {

QByteArray contents(ScintillaNext &editor)
{
    return QByteArray(reinterpret_cast<const char *>(editor.characterPointer()), editor.textLength());
}

void prepare(ScintillaNext &editor, const QByteArray &text)
{
    editor.setCodePage(SC_CP_UTF8);
    editor.setEOLMode(SC_EOL_LF);
    editor.setUseTabs(false);
    editor.setIndent(2);
    editor.setText(text.constData());
    editor.emptyUndoBuffer();
    editor.setSavePoint();
}

}

class JsonEditorTests : public QObject
{
    Q_OBJECT

private slots:
    void wholeDocumentAndUndo();
    void selectedSerializedJson();
    void malformedJsonStillUpdates();
    void respectsLineEndingsAndIndentation();
    void noOpDoesNotDirtyDocument();
    void protectsReadOnlyAndMultipleSelections();
    void defaultShortcutsWithEditorFocus();
};

void JsonEditorTests::wholeDocumentAndUndo()
{
    ScintillaNext editor(QStringLiteral("json"));
    const QByteArray input = R"({"a":[1,2]})";
    prepare(editor, input);
    const auto pretty = editor.formatJson(JsonFormatter::Mode::Toggle);
    QVERIFY(pretty.has_value());
    QVERIFY(pretty->isValid());
    QCOMPARE(contents(editor), QByteArray("{\n  \"a\": [\n    1,\n    2\n  ]\n}"));
    QVERIFY(editor.selectionEmpty());
    QVERIFY(editor.canUndo());
    editor.undo();
    QCOMPARE(contents(editor), input);
    QVERIFY(!editor.canUndo());
    editor.redo();
    QCOMPARE(contents(editor), pretty->text.toUtf8());
    const auto compact = editor.formatJson(JsonFormatter::Mode::Toggle);
    QVERIFY(compact.has_value());
    QCOMPARE(compact->mode, JsonFormatter::Mode::Compact);
    QCOMPARE(contents(editor), input);
}

void JsonEditorTests::selectedSerializedJson()
{
    ScintillaNext editor(QStringLiteral("log"));
    const QByteArray prefix = QStringLiteral("before \u4f60: ").toUtf8();
    const QByteArray json = R"("{\"name\":\"\u4f60\",\"a\":[1,2]}")";
    const QByteArray suffix = " after\n";
    const QByteArray input = prefix + json + suffix;
    prepare(editor, input);
    editor.setSelection(prefix.size(), prefix.size() + json.size());
    const auto pretty = editor.formatJson(JsonFormatter::Mode::Pretty);
    QVERIFY(pretty.has_value());
    QVERIFY(pretty->isValid());
    QVERIFY(pretty->decodedString);
    QCOMPARE(contents(editor), prefix + pretty->text.toUtf8() + suffix);
    QCOMPARE(editor.selectionStart(), prefix.size());
    QCOMPARE(editor.selectionEnd(), prefix.size() + pretty->text.toUtf8().size());
    QCOMPARE(editor.selectionNCaret(editor.mainSelection()), prefix.size());
    const auto compact = editor.formatJson(JsonFormatter::Mode::Toggle);
    QVERIFY(compact.has_value());
    QCOMPARE(compact->mode, JsonFormatter::Mode::Compact);
    QCOMPARE(contents(editor), prefix + compact->text.toUtf8() + suffix);
    editor.undo();
    QCOMPARE(contents(editor), prefix + pretty->text.toUtf8() + suffix);
    editor.undo();
    QCOMPARE(contents(editor), input);
    QVERIFY(!editor.canUndo());
}

void JsonEditorTests::malformedJsonStillUpdates()
{
    ScintillaNext editor(QStringLiteral("json"));
    const QByteArray input = R"({"a":1,"b":[2,3])";
    prepare(editor, input);
    const auto result = editor.formatJson(JsonFormatter::Mode::Pretty);
    QVERIFY(result.has_value());
    QVERIFY(!result->isValid());
    QCOMPARE(contents(editor), result->text.toUtf8());
    QVERIFY(contents(editor).contains('\n'));
    QVERIFY(result->errorOffset >= 0);
    QVERIFY(result->errorOffset <= result->text.size());
    editor.undo();
    QCOMPARE(contents(editor), input);
}

void JsonEditorTests::respectsLineEndingsAndIndentation()
{
    ScintillaNext editor(QStringLiteral("json"));
    const QByteArray input = "{\"a\":1}\r\n";
    prepare(editor, input);
    editor.setEOLMode(SC_EOL_CRLF);
    editor.setUseTabs(true);
    const auto result = editor.formatJson(JsonFormatter::Mode::Pretty);
    QVERIFY(result.has_value());
    QCOMPARE(contents(editor), QByteArray("{\r\n\t\"a\": 1\r\n}\r\n"));
    QVERIFY(editor.formatJson(JsonFormatter::Mode::Toggle).has_value());
    QCOMPARE(contents(editor), input);
}

void JsonEditorTests::noOpDoesNotDirtyDocument()
{
    ScintillaNext editor(QStringLiteral("json"));
    prepare(editor, R"({"a":1})");
    QVERIFY(editor.formatJson(JsonFormatter::Mode::Compact).has_value());
    QVERIFY(!editor.modify());
    QVERIFY(!editor.canUndo());
}

void JsonEditorTests::protectsReadOnlyAndMultipleSelections()
{
    ScintillaNext editor(QStringLiteral("json"));
    const QByteArray input = R"({"a":1,"b":2})";
    prepare(editor, input);
    editor.setReadOnly(true);
    QVERIFY(!editor.formatJson(JsonFormatter::Mode::Pretty).has_value());
    QCOMPARE(contents(editor), input);
    editor.setReadOnly(false);
    editor.setSelectionMode(SC_SEL_RECTANGLE);
    QVERIFY(!editor.formatJson(JsonFormatter::Mode::Pretty).has_value());
    QCOMPARE(contents(editor), input);
    editor.setSelectionMode(SC_SEL_STREAM);
    editor.setMultipleSelection(true);
    editor.setSelection(3, 1);
    editor.addSelection(9, 7);
    QCOMPARE(editor.selections(), 2);
    QVERIFY(!editor.formatJson(JsonFormatter::Mode::Pretty).has_value());
    QCOMPARE(contents(editor), input);
    QVERIFY(!editor.canUndo());
}

void JsonEditorTests::defaultShortcutsWithEditorFocus()
{
    QFile uiFile(QFINDTESTDATA("../src/dialogs/MainWindow.ui"));
    QVERIFY(uiFile.open(QIODevice::ReadOnly));
    QXmlStreamReader xml(&uiFile);
    QMap<QString, QKeySequence> shortcuts;
    QString actionName;
    QString propertyName;
    while (!xml.atEnd()) {
        xml.readNext();
        if (xml.isStartElement()) {
            if (xml.name() == QStringLiteral("action")) {
                actionName = xml.attributes().value(QStringLiteral("name")).toString();
            }
            else if (xml.name() == QStringLiteral("property")) {
                propertyName = xml.attributes().value(QStringLiteral("name")).toString();
            }
            else if (xml.name() == QStringLiteral("string") && propertyName == QStringLiteral("shortcut") && !actionName.isEmpty()) {
                shortcuts.insert(actionName, QKeySequence(xml.readElementText()));
            }
        }
        else if (xml.isEndElement() && xml.name() == QStringLiteral("action")) {
            actionName.clear();
        }
    }
    QVERIFY2(!xml.hasError(), qPrintable(xml.errorString()));
    const QKeySequence toggleShortcut(QStringLiteral("Ctrl+Alt+J"));
    const QKeySequence compactShortcut(QStringLiteral("Ctrl+Alt+Shift+J"));
    QCOMPARE(shortcuts.value(QStringLiteral("actionJsonToggle")), toggleShortcut);
    QCOMPARE(shortcuts.value(QStringLiteral("actionJsonCompact")), compactShortcut);
    for (auto shortcut = shortcuts.cbegin(); shortcut != shortcuts.cend(); ++shortcut) {
        if (shortcut.key() != QStringLiteral("actionJsonToggle")) {
            QVERIFY(shortcut.value() != toggleShortcut);
        }
        if (shortcut.key() != QStringLiteral("actionJsonCompact")) {
            QVERIFY(shortcut.value() != compactShortcut);
        }
    }

    QMainWindow window;
    auto *editor = new ScintillaNext(QStringLiteral("json"), &window);
    window.setCentralWidget(editor);
    const QByteArray input = R"({"a":1})";
    prepare(*editor, input);
    QAction toggleAction(&window);
    toggleAction.setShortcut(toggleShortcut);
    window.addAction(&toggleAction);
    connect(&toggleAction, &QAction::triggered, editor, [editor]() {
        editor->formatJson(JsonFormatter::Mode::Toggle);
    });
    QAction compactAction(&window);
    compactAction.setShortcut(compactShortcut);
    window.addAction(&compactAction);
    connect(&compactAction, &QAction::triggered, editor, [editor]() {
        editor->formatJson(JsonFormatter::Mode::Compact);
    });

    window.resize(640, 480);
    window.show();
    window.activateWindow();
    editor->QWidget::setFocus(Qt::OtherFocusReason);
    QVERIFY(QTest::qWaitForWindowActive(&window));
    QTest::keyClick(editor, Qt::Key_J, Qt::ControlModifier | Qt::AltModifier);
    QTRY_VERIFY(contents(*editor).contains('\n'));
    QTest::keyClick(editor, Qt::Key_J, Qt::ControlModifier | Qt::AltModifier);
    QTRY_COMPARE(contents(*editor), input);
    QTest::keyClick(editor, Qt::Key_J, Qt::ControlModifier | Qt::AltModifier);
    QTRY_VERIFY(contents(*editor).contains('\n'));
    QTest::keyClick(editor, Qt::Key_J, Qt::ControlModifier | Qt::AltModifier | Qt::ShiftModifier);
    QTRY_COMPARE(contents(*editor), input);
}

QTEST_MAIN(JsonEditorTests)

#include "JsonEditorTests.moc"