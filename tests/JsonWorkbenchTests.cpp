#include "json/JsonSession.h"
#include "json/JsonTreeModel.h"
#include "json/JsonWorkbench.h"

#include <QAbstractItemModelTester>
#include <QApplication>
#include <QClipboard>
#include <QComboBox>
#include <QDir>
#include <QHeaderView>
#include <QLineEdit>
#include <QMainWindow>
#include <QMenu>
#include <QTableWidget>
#include <QLabel>
#include <QSignalSpy>
#include <QScopeGuard>
#include <QStatusBar>
#include <QTemporaryFile>
#include <QToolButton>
#include <QTranslator>
#include <QRegularExpression>
#include <QXmlStreamReader>
#include <QTest>

using namespace JsonTools;

class JsonWorkbenchTests : public QObject
{
    Q_OBJECT

private slots:
    void cachedIndexAndEditorSwitch();
    void treeModelRanges();
    void rightClickUsesClickedNodeAndRejectsStale();
    void querySelectsKeyOrValue();
    void validationAndComparison();
    void jsonLinesPreviewAndUndo();
    void layoutAndLivePath_data();
    void layoutAndLivePath();
    void jsonTranslationsAreComplete();
};

void JsonWorkbenchTests::cachedIndexAndEditorSwitch()
{
    ScintillaNext first(QStringLiteral("first.json"));
    first.setText("{\"a\":1}");
    ScintillaNext second(QStringLiteral("second.jsonl"));
    second.setText("{\"id\":1}\n{\"id\":2}\n");
    Session session;
    session.setEditor(&first);
    QTRY_VERIFY(session.snapshot());
    const auto original = session.snapshot();
    first.setSelection(5, 5);
    QCoreApplication::processEvents();
    QCOMPARE(session.snapshot(), original);
    first.setText("{\"b\":2}");
    QVERIFY(!session.snapshot());
    QTRY_VERIFY(session.snapshot());
    QCOMPARE(session.snapshot()->nodes[1].key, QStringLiteral("b"));
    session.setEditor(&second);
    QVERIFY(!session.snapshot());
    QTRY_VERIFY(session.snapshot());
    QVERIFY(session.jsonLines());
    QCOMPARE(session.snapshot()->roots.size(), 2);
    first.setText("{}");
    QCOMPARE(session.snapshot()->roots.size(), 2);
    session.setEditor(nullptr);
    QVERIFY(!session.snapshot());
}

void JsonWorkbenchTests::treeModelRanges()
{
    TreeModel model;
    QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::QtTest);
    auto document = std::make_shared<Index>("{\"a\":[1,{\"b\":2}]}");
    model.setSnapshot(document);
    QCOMPARE(model.rowCount(), 1);
    const auto root = model.index(0, 0);
    QCOMPARE(model.rowCount(root), 1);
    const auto array = model.index(0, 0, root);
    QCOMPARE(model.rowCount(array), 2);
    for (int id = 0; id < document->nodes.size(); ++id) {
        const auto nodeIndex = model.forNode(id);
        QCOMPARE(model.node(nodeIndex), id);
        QCOMPARE(model.node(model.parent(nodeIndex)), document->nodes[id].parent);
    }
    model.setSnapshot(nullptr);
    QCOMPARE(model.rowCount(), 0);
}

void JsonWorkbenchTests::rightClickUsesClickedNodeAndRejectsStale()
{
    ScintillaNext editor(QStringLiteral("test.json"));
    editor.setText("{\"a\":1,\"b\":\"text\\nvalue\"}");
    Workbench workbench;
    workbench.setEditor(&editor);
    QTRY_VERIFY(workbench.session()->snapshot());
    editor.setSelection(5, 5);
    QMenu menu;
    workbench.addContextMenu(&menu, &editor, 14);
    auto *copyPath = menu.findChild<QAction *>(QStringLiteral("jsonCopyPath"));
    QVERIFY(copyPath);
    copyPath->trigger();
    QCOMPARE(QApplication::clipboard()->text(), QStringLiteral("$.b"));
    menu.findChild<QAction *>(QStringLiteral("jsonCopyDecoded"))->trigger();
    QCOMPARE(QApplication::clipboard()->text(), QStringLiteral("text\nvalue"));
    editor.setText("{}");
    QApplication::clipboard()->setText(QStringLiteral("unchanged"));
    copyPath->trigger();
    QCOMPARE(QApplication::clipboard()->text(), QStringLiteral("unchanged"));
}

void JsonWorkbenchTests::querySelectsKeyOrValue()
{
    ScintillaNext editor(QStringLiteral("test.json"));
    editor.setText("{\"users\":[{\"name\":\"Alice\"}]}");
    Workbench workbench;
    workbench.setEditor(&editor);
    QTRY_VERIFY(workbench.session()->snapshot());
    workbench.findChild<QLineEdit *>(QStringLiteral("jsonExpression"))->setText(QStringLiteral("$..name"));
    workbench.runQuery();
    auto *results = workbench.findChild<QTableWidget *>(QStringLiteral("jsonQueryResults"));
    QTRY_COMPARE(results->rowCount(), 1);
    auto *selection = workbench.findChild<QComboBox *>(QStringLiteral("jsonSelectionTarget"));
    selection->setCurrentIndex(1);
    emit results->cellClicked(0, 0);
    QCOMPARE(editor.getSelText(), QByteArray("\"name\""));
    selection->setCurrentIndex(0);
    emit results->cellClicked(0, 0);
    QCOMPARE(editor.getSelText(), QByteArray("\"Alice\""));
    QCOMPARE(workbench.pathWidget()->toolTip(), QStringLiteral("$.users[0].name"));
}

void JsonWorkbenchTests::validationAndComparison()
{
    ScintillaNext editor(QStringLiteral("test.json"));
    editor.setText("{\"age\":12}");
    Workbench workbench;
    workbench.setEditor(&editor);
    QTRY_VERIFY(workbench.session()->snapshot());
    QTemporaryFile schema;
    QVERIFY(schema.open());
    schema.write("{\"properties\":{\"age\":{\"minimum\":18}}}");
    schema.flush();
    workbench.findChild<QLineEdit *>(QStringLiteral("jsonSchemaFile"))->setText(schema.fileName());
    workbench.runValidation();
    auto *issues = workbench.findChild<QTableWidget *>(QStringLiteral("jsonIssues"));
    QTRY_COMPARE(issues->rowCount(), 1);
    emit issues->cellClicked(0, 0);
    QCOMPARE(editor.currentPos(), 7);
    QTemporaryFile other;
    QVERIFY(other.open());
    other.write("{\"age\":21,\"name\":\"A\"}");
    other.flush();
    workbench.findChild<QLineEdit *>(QStringLiteral("jsonComparisonFile"))->setText(other.fileName());
    workbench.runComparison();
    auto *differences = workbench.findChild<QTableWidget *>(QStringLiteral("jsonDifferences"));
    QTRY_COMPARE(differences->rowCount(), 2);
    emit differences->cellClicked(0, 2);
    QCOMPARE(editor.getSelText(), QByteArray("12"));
    editor.setText("{}");
    QCOMPARE(issues->rowCount(), 0);
    QCOMPARE(differences->rowCount(), 0);
}

void JsonWorkbenchTests::jsonLinesPreviewAndUndo()
{
    ScintillaNext editor(QStringLiteral("test.jsonl"));
    const QByteArray original = "{ \"id\": 1 }\n{\"bad\":\n{ \"id\": 3 }\n";
    editor.setText(original.constData());
    editor.emptyUndoBuffer();
    Workbench workbench;
    QSignalSpy preview(&workbench, &Workbench::openDocument);
    workbench.setEditor(&editor);
    QTRY_VERIFY(workbench.session()->snapshot());
    workbench.formatLines(JsonFormatter::Mode::Pretty);
    QCOMPARE(preview.size(), 1);
    QCOMPARE(QByteArray(reinterpret_cast<const char *>(editor.characterPointer()), editor.textLength()), original);
    workbench.formatLines(JsonFormatter::Mode::Compact);
    const QByteArray compact(reinterpret_cast<const char *>(editor.characterPointer()), editor.textLength());
    QCOMPARE(compact, QByteArray("{\"id\":1}\n{\"bad\":\n{\"id\":3}\n"));
    editor.undo();
    QCOMPARE(QByteArray(reinterpret_cast<const char *>(editor.characterPointer()), editor.textLength()), original);
    QVERIFY(!editor.canUndo());
}

void JsonWorkbenchTests::layoutAndLivePath_data()
{
    QTest::addColumn<QSize>("size");
    QTest::newRow("wide") << QSize(1200, 760);
    QTest::newRow("compact") << QSize(800, 600);
}

void JsonWorkbenchTests::layoutAndLivePath()
{
    QFETCH(QSize, size);
    QTranslator translator;
    const QString translationFile = qEnvironmentVariable("NOTEPADSHARP_JSON_TRANSLATION");
    if (!translationFile.isEmpty()) {
        QVERIFY(translator.load(translationFile));
        QCoreApplication::installTranslator(&translator);
    }
    const auto removeTranslator = qScopeGuard([&translator]() { QCoreApplication::removeTranslator(&translator); });
    QMainWindow window;
    auto *editor = new ScintillaNext(QStringLiteral("sample.json"), &window);
    editor->setCodePage(SC_CP_UTF8);
    editor->setText("{\n  \"users\": [\n    {\"name\": \"Alice\", \"age\": 21},\n    {\"name\": \"Bob\", \"age\": 17}\n  ]\n}");
    window.setCentralWidget(editor);
    auto *workbench = new Workbench(&window);
    if (!translationFile.isEmpty()) {
        const QString title = translator.translate("JsonTools::Workbench", "JSON Tools");
        QVERIFY(!title.isEmpty());
        QCOMPARE(workbench->windowTitle(), title);
    }
    window.addDockWidget(Qt::RightDockWidgetArea, workbench);
    window.statusBar()->addWidget(workbench->pathWidget(), 1);
    workbench->setEditor(editor);
    window.resize(size);
    window.show();
    QTRY_VERIFY(workbench->session()->snapshot());
    const auto snapshot = workbench->session()->snapshot();
    const int name = snapshot->findPointer(QStringLiteral("/users/0/name")).first();
    workbench->selectNode(name);
    QTRY_VERIFY(workbench->pathWidget()->isVisible());
    QCOMPARE(workbench->pathWidget()->toolTip(), QStringLiteral("$.users[0].name"));
    QTest::mouseClick(workbench->pathWidget(), Qt::LeftButton);
    QCOMPARE(QApplication::clipboard()->text(), QStringLiteral("$.users[0].name"));
    QVERIFY(window.rect().contains(workbench->geometry()));
    QVERIFY(window.statusBar()->rect().contains(workbench->pathWidget()->geometry()));
    auto *input = workbench->findChild<QLineEdit *>(QStringLiteral("jsonExpression"));
    workbench->openQuery();
    input->setText(QStringLiteral("$.users[*].name"));
    workbench->runQuery();
    auto *results = workbench->findChild<QTableWidget *>(QStringLiteral("jsonQueryResults"));
    QTRY_COMPARE(results->rowCount(), 2);
    QTRY_VERIFY(results->horizontalHeader()->length() <= results->viewport()->width());
    const QString prefix = qEnvironmentVariable("NOTEPADSHARP_JSON_SCREENSHOT");
    if (!prefix.isEmpty()) {
        QVERIFY(window.grab().save(prefix + '-' + QString::fromLatin1(QTest::currentDataTag()) + ".png"));
    }
}

void JsonWorkbenchTests::jsonTranslationsAreComplete()
{
    using Messages = QMap<QPair<QString, QString>, QString>;
    const auto readMessages = [](const QString &path) {
        Messages messages;
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) { return messages; }
        QXmlStreamReader xml(&file);
        QString context;
        QString source;
        QString translation;
        bool finished = true;
        while (!xml.atEnd()) {
            xml.readNext();
            if (xml.isStartElement()) {
                if (xml.name() == QStringLiteral("name")) { context = xml.readElementText(); }
                else if (xml.name() == QStringLiteral("message")) { source.clear(); translation.clear(); finished = true; }
                else if (xml.name() == QStringLiteral("source")) { source = xml.readElementText(); }
                else if (xml.name() == QStringLiteral("translation")) {
                    finished = xml.attributes().value(QStringLiteral("type")) != QStringLiteral("unfinished");
                    translation = xml.readElementText(QXmlStreamReader::IncludeChildElements);
                }
            }
            else if (xml.isEndElement() && xml.name() == QStringLiteral("message") && context.startsWith(QStringLiteral("Json"))) {
                messages.insert({context, source}, finished ? translation : QString());
            }
        }
        if (xml.hasError()) { messages.clear(); }
        return messages;
    };
    const QDir directory(QFINDTESTDATA("../i18n"));
    const auto reference = readMessages(directory.filePath(QStringLiteral("NotepadSharp_en.ts")));
    QVERIFY(reference.size() >= 87);
    const auto placeholders = [](const QString &text) {
        QStringList result;
        const QRegularExpression pattern(QStringLiteral("%[0-9]+"));
        auto matches = pattern.globalMatch(text);
        while (matches.hasNext()) { result.append(matches.next().captured()); }
        result.sort();
        return result;
    };
    const auto files = directory.entryList({QStringLiteral("NotepadSharp_*.ts")}, QDir::Files);
    QCOMPARE(files.size(), 21);
    for (const QString &file : files) {
        const auto messages = readMessages(directory.filePath(file));
        for (auto entry = reference.cbegin(); entry != reference.cend(); ++entry) {
            const QString translated = messages.value(entry.key());
            QVERIFY2(!translated.isEmpty(), qPrintable(file + ": " + entry.key().second));
            QCOMPARE(placeholders(translated), placeholders(entry.key().second));
        }
    }
}

QTEST_MAIN(JsonWorkbenchTests)

#include "JsonWorkbenchTests.moc"