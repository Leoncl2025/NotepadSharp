#include "JsonWorkbench.h"
#include "JsonTreeModel.h"
#include "UndoAction.h"

#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
#include <QFileDialog>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QResizeEvent>
#include <QSignalBlocker>
#include <QStyle>
#include <QTableWidget>
#include <QTabWidget>
#include <QTextStream>
#include <QToolButton>
#include <QTreeView>
#include <QVBoxLayout>
#include <QtConcurrent/QtConcurrentRun>

namespace JsonTools {
namespace {

class PathButton : public QToolButton
{
public:
    using QToolButton::QToolButton;
    void showPath(const QString &path) {
        fullText = path;
        setToolTip(path);
        setText(fontMetrics().elidedText(fullText, Qt::ElideMiddle, qMax(0, width() - 16)));
    }
protected:
    void resizeEvent(QResizeEvent *event) override {
        QToolButton::resizeEvent(event);
        showPath(fullText);
    }
private:
    QString fullText;
};

QToolButton *tool(QWidget *parent, QHBoxLayout *layout, QStyle::StandardPixmap icon, const QString &label)
{
    auto *button = new QToolButton(parent);
    button->setIcon(parent->style()->standardIcon(icon));
    button->setToolTip(label);
    button->setAccessibleName(label);
    button->setAutoRaise(true);
    button->setFixedSize(28, 28);
    layout->addWidget(button);
    return button;
}

QTableWidget *table(QWidget *parent, const QStringList &headers)
{
    auto *widget = new QTableWidget(0, headers.size(), parent);
    widget->setHorizontalHeaderLabels(headers);
    widget->setEditTriggers(QAbstractItemView::NoEditTriggers);
    widget->setSelectionBehavior(QAbstractItemView::SelectRows);
    widget->setSelectionMode(QAbstractItemView::SingleSelection);
    widget->setWordWrap(false);
    widget->verticalHeader()->hide();
    widget->horizontalHeader()->setStretchLastSection(true);
    widget->horizontalHeader()->setMinimumSectionSize(45);
    return widget;
}

void cell(QTableWidget *widget, int row, int column, const QString &text)
{
    auto *item = new QTableWidgetItem(text.left(1024));
    item->setToolTip(text.left(4096));
    widget->setItem(row, column, item);
}

QByteArray readJsonFile(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        throw std::runtime_error(file.errorString().toStdString());
    }
    if (file.size() > 32 * 1024 * 1024) {
        throw std::runtime_error("JSON tools are limited to 32 MiB per document.");
    }
    QTextStream stream(&file);
    stream.setEncoding(QStringConverter::Utf8);
    stream.setAutoDetectUnicode(true);
    return stream.readAll().toUtf8();
}

}

Workbench::Workbench(QWidget *parent) : QDockWidget(tr("JSON Tools"), parent), documentSession(this)
{
    setObjectName(QStringLiteral("JsonWorkbench"));
    auto *body = new QWidget(this);
    auto *layout = new QVBoxLayout(body);
    layout->setContentsMargins(6, 6, 6, 6);
    auto *toolbar = new QHBoxLayout;
    toolbar->setSpacing(2);
    mode = new QComboBox(body);
    mode->setObjectName(QStringLiteral("jsonMode"));
    mode->addItems({QStringLiteral("JSON"), QStringLiteral("JSON Lines")});
    toolbar->addWidget(mode);
    auto *parentButton = tool(body, toolbar, QStyle::SP_ArrowUp, tr("Parent Node"));
    auto *previous = tool(body, toolbar, QStyle::SP_ArrowBack, tr("Previous Sibling"));
    auto *next = tool(body, toolbar, QStyle::SP_ArrowForward, tr("Next Sibling"));
    auto *pretty = tool(body, toolbar, QStyle::SP_FileDialogContentsView, tr("Pretty Print Node"));
    auto *compact = tool(body, toolbar, QStyle::SP_TitleBarMinButton, tr("Compact Node"));
    auto *fold = tool(body, toolbar, QStyle::SP_TitleBarShadeButton, tr("Fold / Unfold Node"));
    auto *refresh = tool(body, toolbar, QStyle::SP_BrowserReload, tr("Refresh JSON"));
    toolbar->addStretch();
    layout->addLayout(toolbar);

    documentSummary = new QLabel(body);
    documentSummary->setObjectName(QStringLiteral("jsonDocumentSummary"));
    documentSummary->setWordWrap(true);
    layout->addWidget(documentSummary);
    tabs = new QTabWidget(body);
    tabs->setObjectName(QStringLiteral("jsonTabs"));
    tree = new QTreeView(tabs);
    tree->setObjectName(QStringLiteral("jsonTree"));
    treeModel = new TreeModel(this);
    tree->setModel(treeModel);
    tree->setUniformRowHeights(true);
    tree->setAlternatingRowColors(true);
    tree->header()->setStretchLastSection(false);
    tree->header()->setMinimumSectionSize(35);
    tree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    tree->header()->setSectionResizeMode(1, QHeaderView::Stretch);
    tree->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    tree->header()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    tabs->addTab(tree, tr("Structure"));

    auto *queryPage = new QWidget(tabs);
    auto *queryLayout = new QVBoxLayout(queryPage);
    auto *queryBar = new QHBoxLayout;
    searchTarget = new QComboBox(queryPage);
    searchTarget->setObjectName(QStringLiteral("jsonSearchTarget"));
    searchTarget->addItems({tr("Path"), tr("Key"), tr("Value")});
    queryBar->addWidget(searchTarget);
    expression = new QLineEdit(QStringLiteral("$"), queryPage);
    expression->setObjectName(QStringLiteral("jsonExpression"));
    expression->setMaxLength(4096);
    expression->setClearButtonEnabled(true);
    expression->setAccessibleName(tr("JSONPath or Search Text"));
    queryBar->addWidget(expression, 1);
    queryButton = tool(queryPage, queryBar, QStyle::SP_ArrowForward, tr("Run Query"));
    queryButton->setObjectName(QStringLiteral("jsonRunQuery"));
    queryLayout->addLayout(queryBar);
    auto *queryOptions = new QHBoxLayout;
    caseSensitive = new QCheckBox(tr("Match Case"), queryPage);
    queryOptions->addWidget(caseSensitive);
    selectionTarget = new QComboBox(queryPage);
    selectionTarget->setObjectName(QStringLiteral("jsonSelectionTarget"));
    selectionTarget->addItems({tr("Select Value"), tr("Select Key")});
    queryOptions->addWidget(selectionTarget);
    queryOptions->addStretch();
    auto *copyResults = tool(queryPage, queryOptions, QStyle::SP_DialogSaveButton, tr("Copy Results"));
    copyResults->setIcon(QIcon(QStringLiteral(":/icons/copy.png")));
    queryLayout->addLayout(queryOptions);
    querySummary = new QLabel(queryPage);
    querySummary->setObjectName(QStringLiteral("jsonQuerySummary"));
    querySummary->setWordWrap(true);
    queryLayout->addWidget(querySummary);
    queryTable = table(queryPage, {tr("Line"), tr("Path"), tr("Type"), tr("Value")});
    queryTable->setObjectName(QStringLiteral("jsonQueryResults"));
    queryTable->horizontalHeader()->setStretchLastSection(false);
    queryTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    queryTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    queryTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    queryTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
    queryLayout->addWidget(queryTable);
    tabs->addTab(queryPage, tr("Query"));

    auto *validationPage = new QWidget(tabs);
    auto *validationLayout = new QVBoxLayout(validationPage);
    auto *schemaBar = new QHBoxLayout;
    schemaFile = new QLineEdit(validationPage);
    schemaFile->setObjectName(QStringLiteral("jsonSchemaFile"));
    schemaFile->setPlaceholderText(tr("Schema file"));
    schemaBar->addWidget(schemaFile, 1);
    auto *browseSchema = tool(validationPage, schemaBar, QStyle::SP_DirOpenIcon, tr("Open Schema"));
    validateButton = tool(validationPage, schemaBar, QStyle::SP_DialogApplyButton, tr("Validate JSON"));
    validateButton->setObjectName(QStringLiteral("jsonValidate"));
    auto *copyIssues = tool(validationPage, schemaBar, QStyle::SP_DialogSaveButton, tr("Copy Results"));
    copyIssues->setIcon(QIcon(QStringLiteral(":/icons/copy.png")));
    validationLayout->addLayout(schemaBar);
    validationSummary = new QLabel(validationPage);
    validationSummary->setObjectName(QStringLiteral("jsonValidationSummary"));
    validationSummary->setWordWrap(true);
    validationLayout->addWidget(validationSummary);
    issuesTable = table(validationPage, {tr("Line"), tr("Path"), tr("Issue")});
    issuesTable->setObjectName(QStringLiteral("jsonIssues"));
    issuesTable->horizontalHeader()->setStretchLastSection(false);
    issuesTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    issuesTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    issuesTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    validationLayout->addWidget(issuesTable);
    tabs->addTab(validationPage, tr("Validation"));

    auto *comparisonPage = new QWidget(tabs);
    auto *comparisonLayout = new QVBoxLayout(comparisonPage);
    auto *compareBar = new QHBoxLayout;
    comparisonFile = new QLineEdit(comparisonPage);
    comparisonFile->setObjectName(QStringLiteral("jsonComparisonFile"));
    comparisonFile->setPlaceholderText(tr("Comparison file"));
    compareBar->addWidget(comparisonFile, 1);
    auto *browseComparison = tool(comparisonPage, compareBar, QStyle::SP_DirOpenIcon, tr("Open Comparison File"));
    compareButton = tool(comparisonPage, compareBar, QStyle::SP_ArrowForward, tr("Compare JSON"));
    compareButton->setObjectName(QStringLiteral("jsonCompare"));
    auto *copyDifferences = tool(comparisonPage, compareBar, QStyle::SP_DialogSaveButton, tr("Copy Results"));
    copyDifferences->setIcon(QIcon(QStringLiteral(":/icons/copy.png")));
    comparisonLayout->addLayout(compareBar);
    ignoreOrder = new QCheckBox(tr("Ignore Object Key Order"), comparisonPage);
    ignoreOrder->setChecked(true);
    comparisonLayout->addWidget(ignoreOrder);
    comparisonSummary = new QLabel(comparisonPage);
    comparisonSummary->setObjectName(QStringLiteral("jsonComparisonSummary"));
    comparisonSummary->setWordWrap(true);
    comparisonLayout->addWidget(comparisonSummary);
    comparisonTable = table(comparisonPage, {tr("Change"), tr("Path"), tr("Before"), tr("After")});
    comparisonTable->setObjectName(QStringLiteral("jsonDifferences"));
    comparisonTable->horizontalHeader()->setStretchLastSection(false);
    comparisonTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    for (int column = 1; column < 4; ++column) {
        comparisonTable->horizontalHeader()->setSectionResizeMode(column, QHeaderView::Stretch);
    }
    comparisonLayout->addWidget(comparisonTable);
    tabs->addTab(comparisonPage, tr("Compare"));
    for (QLabel *label : {documentSummary, querySummary, validationSummary, comparisonSummary}) {
        label->setTextFormat(Qt::PlainText);
    }
    layout->addWidget(tabs);
    setWidget(body);

    pathButton = new PathButton(this);
    pathButton->setObjectName(QStringLiteral("jsonPathStatus"));
    pathButton->setAutoRaise(true);
    pathButton->setMinimumWidth(0);
    pathButton->setMaximumWidth(360);
    pathButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    pathButton->setAccessibleName(tr("Copy JSON Path"));
    pathButton->hide();
    connect(pathButton, &QToolButton::clicked, this, [this]() {
        if (!currentPath.isEmpty()) {
            QApplication::clipboard()->setText(currentPath);
        }
    });
    connect(mode, &QComboBox::currentIndexChanged, this, [this](int value) { documentSession.setJsonLines(value == 1); });
    connect(&documentSession, &Session::updated, this, &Workbench::indexUpdated);
    connect(&documentSession, &Session::caretChanged, this, &Workbench::caretUpdated);
    connect(&documentSession, &Session::updated, this, [this, pretty, compact, fold, parentButton, previous, next]() {
        const bool available = documentSession.snapshot() && documentSession.editor();
        const bool writable = available && !documentSession.editor()->readOnly();
        pretty->setEnabled(available && (writable || documentSession.jsonLines()));
        compact->setEnabled(writable);
        for (QToolButton *button : {fold, parentButton, previous, next}) { button->setEnabled(available); }
    });
    connect(parentButton, &QToolButton::clicked, this, [this]() {
        const auto snapshot = documentSession.snapshot();
        const int id = currentNode();
        if (snapshot && id >= 0) { selectNode(snapshot->nodes[id].parent); }
    });
    connect(previous, &QToolButton::clicked, this, [this]() { sibling(-1); });
    connect(next, &QToolButton::clicked, this, [this]() { sibling(1); });
    connect(pretty, &QToolButton::clicked, this, [this]() { formatNode(currentNode(), JsonFormatter::Mode::Pretty); });
    connect(compact, &QToolButton::clicked, this, [this]() { formatNode(currentNode(), JsonFormatter::Mode::Compact); });
    connect(fold, &QToolButton::clicked, this, [this]() { foldNode(currentNode()); });
    connect(refresh, &QToolButton::clicked, &documentSession, &Session::enable);
    connect(tree, &QTreeView::activated, this, [this](const QModelIndex &index) { selectNode(treeModel->node(index), index.column() == 0); });
    connect(queryButton, &QToolButton::clicked, this, &Workbench::runQuery);
    connect(expression, &QLineEdit::returnPressed, this, &Workbench::runQuery);
    connect(queryTable, &QTableWidget::cellClicked, this, [this](int row, int) {
        if (row < queryMatches.size()) { selectNode(queryMatches[row], selectionTarget->currentIndex() == 1); }
    });
    connect(copyResults, &QToolButton::clicked, this, [this]() { copyTable(queryTable); });
    connect(&queryWatcher, &QFutureWatcher<QueryResult>::finished, this, [this]() {
        queryButton->setEnabled(bool(documentSession.snapshot()));
        if (queryRevision == documentSession.revision() && documentSession.snapshot()) {
            showQueryResults(queryWatcher.result());
        }
    });
    connect(browseSchema, &QToolButton::clicked, this, [this]() {
        const QString file = QFileDialog::getOpenFileName(this, tr("Open Schema"), schemaFile->text(), tr("JSON files (*.json);;All files (*)"));
        if (!file.isEmpty()) { schemaFile->setText(file); runValidation(); }
    });
    connect(validateButton, &QToolButton::clicked, this, &Workbench::runValidation);
    connect(schemaFile, &QLineEdit::returnPressed, this, &Workbench::runValidation);
    connect(copyIssues, &QToolButton::clicked, this, [this]() { copyTable(issuesTable); });
    connect(issuesTable, &QTableWidget::cellClicked, this, [this](int row, int) {
        const auto snapshot = documentSession.snapshot();
        ScintillaNext *editor = documentSession.editor();
        if (snapshot && editor && row < shownIssues.size()) {
            const qsizetype position = shownIssues[row].offset;
            editor->ensureVisible(editor->lineFromPosition(position));
            editor->setSelection(position, position);
            editor->scrollCaret();
            editor->QWidget::setFocus(Qt::OtherFocusReason);
        }
    });
    connect(&validationWatcher, &QFutureWatcher<ValidationResult>::finished, this, [this]() {
        validateButton->setEnabled(bool(documentSession.snapshot()));
        if (validationRevision != documentSession.revision() || !documentSession.snapshot()) { return; }
        const auto result = validationWatcher.result();
        showIssues(result.issues);
        validationSummary->setText(result.error.isEmpty()
            ? tr("%1 issues; %2 records validated").arg(result.issues.size()).arg(result.checkedRecords) : result.error);
    });
    connect(browseComparison, &QToolButton::clicked, this, [this]() {
        const QString file = QFileDialog::getOpenFileName(this, tr("Open Comparison File"), comparisonFile->text(),
                                                         tr("JSON files (*.json *.jsonl *.ndjson);;All files (*)"));
        if (!file.isEmpty()) { comparisonFile->setText(file); runComparison(); }
    });
    connect(compareButton, &QToolButton::clicked, this, &Workbench::runComparison);
    connect(comparisonFile, &QLineEdit::returnPressed, this, &Workbench::runComparison);
    connect(copyDifferences, &QToolButton::clicked, this, [this]() { copyTable(comparisonTable); });
    connect(comparisonTable, &QTableWidget::cellClicked, this, [this](int row, int column) {
        if (!documentSession.snapshot() || !comparedDocument || row >= differences.size()) { return; }
        const Difference &difference = differences[row];
        if ((column == 3 || difference.left < 0) && difference.right >= 0) {
            const Node &node = comparedDocument->nodes[difference.right];
            emit openDocument(comparedDocument->source, tr("JSON Comparison"), node.start, node.end);
        }
        else { selectNode(difference.left); }
    });
    connect(&comparisonWatcher, &QFutureWatcher<ComparisonJob>::finished, this, [this]() {
        compareButton->setEnabled(bool(documentSession.snapshot()));
        const auto snapshot = documentSession.snapshot();
        if (comparisonRevision != documentSession.revision() || !snapshot) { return; }
        const auto job = comparisonWatcher.result();
        comparedDocument = job.other;
        differences = job.result.differences;
        comparisonTable->setRowCount(differences.size());
        const QStringList names{tr("Added"), tr("Removed"), tr("Modified"), tr("Key Order")};
        for (int row = 0; row < differences.size(); ++row) {
            const Difference &difference = differences[row];
            const Index &location = difference.left >= 0 ? *snapshot : *comparedDocument;
            const int node = difference.left >= 0 ? difference.left : difference.right;
            QString path = location.path(node);
            if (location.lines) { path = tr("Line %1: %2").arg(location.nodes[node].record).arg(path); }
            cell(comparisonTable, row, 0, names[static_cast<int>(difference.change)]);
            cell(comparisonTable, row, 1, path);
            cell(comparisonTable, row, 2, QString::fromUtf8(snapshot->raw(difference.left).left(4096)));
            cell(comparisonTable, row, 3, QString::fromUtf8(comparedDocument->raw(difference.right).left(4096)));
        }
        QString summary = job.result.error.isEmpty() ? tr("%1 differences").arg(differences.size()) : job.result.error;
        if (job.result.truncated) { summary += ' ' + tr("Results limited to 10000."); }
        comparisonSummary->setText(summary);
    });
}

QWidget *Workbench::pathWidget() const { return pathButton; }

void Workbench::setEditor(ScintillaNext *editor)
{
    documentSession.setEditor(editor);
    const QSignalBlocker blocker(mode);
    mode->setCurrentIndex(documentSession.jsonLines() ? 1 : 0);
}

void Workbench::openTools()
{
    show();
    raise();
    if (!documentSession.snapshot()) { documentSession.enable(); }
}

void Workbench::openQuery()
{
    openTools();
    tabs->setCurrentIndex(1);
    expression->setFocus();
    expression->selectAll();
}

void Workbench::indexUpdated()
{
    const auto snapshot = documentSession.snapshot();
    queryButton->setEnabled(snapshot && !queryWatcher.isRunning());
    validateButton->setEnabled(snapshot && !validationWatcher.isRunning());
    compareButton->setEnabled(snapshot && !comparisonWatcher.isRunning());
    treeModel->setSnapshot(snapshot);
    queryMatches.clear();
    queryTable->setRowCount(0);
    querySummary->clear();
    shownIssues.clear();
    issuesTable->setRowCount(0);
    validationSummary->clear();
    differences.clear();
    comparedDocument.reset();
    comparisonTable->setRowCount(0);
    comparisonSummary->clear();
    documentSummary->setText(snapshot
        ? tr("%1 nodes; %2 issues").arg(snapshot->nodes.size()).arg(snapshot->diagnostics.size()) : documentSession.state());
    if (snapshot) {
        tree->expandToDepth(1);
        showIssues(snapshot->diagnostics);
    }
}

int Workbench::currentNode() const
{
    const auto snapshot = documentSession.snapshot();
    ScintillaNext *editor = documentSession.editor();
    if (!snapshot || !editor) { return -1; }
    sptr_t position = editor->currentPos();
    if (!editor->selectionEmpty() && position == editor->selectionEnd()) {
        position = editor->positionBefore(position);
    }
    return snapshot->nodeAt(position);
}

void Workbench::caretUpdated()
{
    const auto snapshot = documentSession.snapshot();
    const int id = currentNode();
    currentPath = snapshot && id >= 0 ? snapshot->path(id) : QString();
    QString display = currentPath;
    if (snapshot && id >= 0) {
        if (snapshot->ambiguous(id)) { display = tr("Ambiguous: %1").arg(display); }
        if (snapshot->lines) { display = tr("Line %1: %2").arg(snapshot->nodes[id].record).arg(display); }
        tree->setCurrentIndex(treeModel->forNode(id));
    }
    static_cast<PathButton *>(pathButton)->showPath(display);
    pathButton->setVisible(!display.isEmpty());
}

void Workbench::selectNode(int id, bool key)
{
    const auto snapshot = documentSession.snapshot();
    ScintillaNext *editor = documentSession.editor();
    if (!snapshot || !editor || id < 0 || id >= snapshot->nodes.size()) { return; }
    const Node &node = snapshot->nodes[id];
    const qsizetype begin = key && node.keyStart >= 0 ? node.keyStart : node.start;
    const qsizetype end = key && node.keyStart >= 0 ? node.keyEnd : node.end;
    editor->ensureVisible(editor->lineFromPosition(begin));
    editor->setSelection(end, begin);
    editor->scrollRange(end, begin);
    editor->QWidget::setFocus(Qt::OtherFocusReason);
    caretUpdated();
}

void Workbench::addContextMenu(QMenu *menu, ScintillaNext *editor, qsizetype position)
{
    const auto snapshot = documentSession.snapshot();
    if (!snapshot || editor != documentSession.editor()) { return; }
    const int id = snapshot->nodeAt(position);
    if (id < 0) { return; }
    const Node &node = snapshot->nodes[id];
    QMenu *json = menu->addMenu(QStringLiteral("JSON"));
    const quint64 revision = documentSession.revision();
    const auto copy = [this, json, revision](const QString &label, const QString &text, bool enabled = true) {
        QAction *action = json->addAction(label);
        action->setEnabled(enabled);
        connect(action, &QAction::triggered, this, [this, revision, text]() {
            if (revision == documentSession.revision()) { QApplication::clipboard()->setText(text); }
        });
        return action;
    };
    QAction *path = copy(tr("Copy JSON Path"), snapshot->path(id));
    path->setObjectName(QStringLiteral("jsonCopyPath"));
    path->setToolTip(snapshot->ambiguous(id) ? tr("Duplicate keys: this path is not unique.") : snapshot->path(id));
    copy(tr("Copy JSON Pointer"), snapshot->pointer(id))->setObjectName(QStringLiteral("jsonCopyPointer"));
    copy(tr("Copy Key"), node.key, node.keyStart >= 0)->setObjectName(QStringLiteral("jsonCopyKey"));
    copy(tr("Copy JSON Value"), QString::fromUtf8(snapshot->raw(id)), node.complete)->setObjectName(QStringLiteral("jsonCopyValue"));
    copy(tr("Copy Decoded String"), snapshot->decoded(id), node.kind == Kind::String && node.complete)->setObjectName(QStringLiteral("jsonCopyDecoded"));
    json->addSeparator();
    const auto command = [this, json, revision, id](const QString &label, auto callback, bool enabled = true) {
        QAction *action = json->addAction(label);
        action->setEnabled(enabled);
        connect(action, &QAction::triggered, this, [this, revision, id, callback]() {
            if (revision == documentSession.revision()) { callback(id); }
        });
    };
    command(tr("Select Key"), [this](int nodeId) { selectNode(nodeId, true); });
    command(tr("Select Value"), [this](int nodeId) { selectNode(nodeId); });
    command(tr("Preview Node"), [this](int nodeId) { previewNode(nodeId); });
    command(tr("Pretty Print Node"), [this](int nodeId) { formatNode(nodeId, JsonFormatter::Mode::Pretty); }, !editor->readOnly() || snapshot->lines);
    command(tr("Compact Node"), [this](int nodeId) { formatNode(nodeId, JsonFormatter::Mode::Compact); }, !editor->readOnly());
    command(tr("Fold / Unfold Node"), [this](int nodeId) { foldNode(nodeId); });
}

void Workbench::runQuery()
{
    const auto snapshot = documentSession.snapshot();
    if (!snapshot || queryWatcher.isRunning()) { return; }
    queryRevision = documentSession.revision();
    queryButton->setEnabled(false);
    querySummary->setText(tr("Querying..."));
    const QString text = expression->text();
    const auto target = static_cast<SearchTarget>(searchTarget->currentIndex());
    const auto sensitivity = caseSensitive->isChecked() ? Qt::CaseSensitive : Qt::CaseInsensitive;
    queryWatcher.setFuture(QtConcurrent::run([snapshot, text, target, sensitivity]() {
        return query(*snapshot, text, target, sensitivity);
    }));
}

void Workbench::showQueryResults(const QueryResult &result)
{
    const auto snapshot = documentSession.snapshot();
    if (!snapshot) { return; }
    queryMatches = result.matches;
    queryTable->setRowCount(queryMatches.size());
    for (int row = 0; row < queryMatches.size(); ++row) {
        const int id = queryMatches[row];
        cell(queryTable, row, 0, QString::number(snapshot->lineAt(snapshot->nodes[id].start)));
        cell(queryTable, row, 1, snapshot->path(id));
        cell(queryTable, row, 2, Index::kindName(snapshot->nodes[id].kind));
        cell(queryTable, row, 3, QString::fromUtf8(snapshot->raw(id).left(4096)));
    }
    QString summary = result.error.isEmpty() ? tr("%1 matches; %2 invalid or ambiguous records skipped").arg(queryMatches.size()).arg(result.skippedRecords) : result.error;
    if (result.truncated) { summary += ' ' + tr("Results limited to 10000."); }
    querySummary->setText(summary);
}

void Workbench::copyTable(QTableWidget *widget)
{
    QStringList rows;
    const auto snapshot = documentSession.snapshot();
    if (widget == queryTable && snapshot) {
        qsizetype size = 0;
        for (int node : queryMatches) {
            QString row = snapshot->path(node) + '\t' + QString::fromUtf8(snapshot->raw(node));
            row.replace('\n', QStringLiteral("\\n"));
            row.replace('\r', QStringLiteral("\\r"));
            size += row.size();
            if (size > 16 * 1024 * 1024) {
                emit message(tr("Copy limit exceeded; narrow the query."));
                return;
            }
            rows.append(row);
        }
        QApplication::clipboard()->setText(rows.join('\n'));
        return;
    }
    for (int row = 0; row < widget->rowCount(); ++row) {
        QStringList columns;
        for (int column = 0; column < widget->columnCount(); ++column) {
            const auto *item = widget->item(row, column);
            columns.append(item ? item->text().replace('\n', QStringLiteral("\\n")).replace('\r', QStringLiteral("\\r")).replace('\t', QStringLiteral("\\t")) : QString());
        }
        rows.append(columns.join('\t'));
    }
    QApplication::clipboard()->setText(rows.join('\n'));
}

void Workbench::sibling(int direction)
{
    const auto snapshot = documentSession.snapshot();
    const int id = currentNode();
    if (!snapshot || id < 0) { return; }
    const int parent = snapshot->nodes[id].parent;
    const auto &siblings = parent < 0 ? snapshot->roots : snapshot->nodes[parent].children;
    const int row = siblings.indexOf(id) + direction;
    if (row >= 0 && row < siblings.size()) { selectNode(siblings[row]); }
}

void Workbench::formatNode(int id, JsonFormatter::Mode format)
{
    if (documentSession.jsonLines() && format == JsonFormatter::Mode::Pretty) {
        previewNode(id);
        return;
    }
    ScintillaNext *editor = documentSession.editor();
    const auto snapshot = documentSession.snapshot();
    if (!editor || !snapshot || id < 0 || editor->readOnly()) { return; }
    selectNode(id);
    const auto result = editor->formatJson(format);
    if (result && !result->isValid()) { emit message(tr("Best-effort formatting: %1").arg(result->error)); }
}

void Workbench::previewNode(int id)
{
    const auto snapshot = documentSession.snapshot();
    if (!snapshot || id < 0) { return; }
    const auto result = JsonFormatter::format(QString::fromUtf8(snapshot->raw(id)));
    emit openDocument(result.text.toUtf8(), tr("JSON Preview"), 0, 0);
}

void Workbench::foldNode(int id)
{
    const auto snapshot = documentSession.snapshot();
    if (!snapshot || id < 0 || !documentSession.editor()) { return; }
    while (snapshot->nodes[id].kind != Kind::Object && snapshot->nodes[id].kind != Kind::Array) {
        id = snapshot->nodes[id].parent;
        if (id < 0) { return; }
    }
    emit jsonLanguageRequested();
    documentSession.editor()->colourise(0, -1);
    documentSession.editor()->foldLine(documentSession.editor()->lineFromPosition(snapshot->nodes[id].start), SC_FOLDACTION_TOGGLE);
    const auto modelIndex = treeModel->forNode(id);
    tree->setExpanded(modelIndex, !tree->isExpanded(modelIndex));
}

void Workbench::formatLines(JsonFormatter::Mode format)
{
    const auto snapshot = documentSession.snapshot();
    ScintillaNext *editor = documentSession.editor();
    if (!snapshot || !editor) {
        emit message(tr("JSON index is not ready."));
        return;
    }
    if (editor->selectionIsRectangle() || editor->selections() != 1) { return; }
    if (format != JsonFormatter::Mode::Compact) {
        int id = currentNode();
        while (id >= 0 && snapshot->nodes[id].parent >= 0) { id = snapshot->nodes[id].parent; }
        previewNode(id);
        return;
    }
    if (editor->readOnly()) { return; }
    int skipped = 0;
    const bool selected = !editor->selectionEmpty();
    const qsizetype selectionStart = editor->selectionStart();
    const qsizetype selectionEnd = editor->selectionEnd();
    const UndoAction undo(editor);
    for (auto root = snapshot->roots.crbegin(); root != snapshot->roots.crend(); ++root) {
        const Node &node = snapshot->nodes[*root];
        if (selected && (node.start < selectionStart || node.end > selectionEnd)) { continue; }
        if (!snapshot->recordValid(node.record)) { ++skipped; continue; }
        if (node.kind == Kind::String) { continue; }
        const auto result = JsonFormatter::format(QString::fromUtf8(snapshot->raw(*root)), JsonFormatter::Mode::Compact);
        const QByteArray replacement = result.text.toUtf8();
        if (replacement != snapshot->raw(*root)) {
            editor->setTargetRange(node.start, node.end);
            editor->replaceTarget(replacement.size(), replacement.constData());
        }
    }
    emit message(tr("JSON Lines compacted; %1 invalid records unchanged.").arg(skipped));
}

void Workbench::runValidation()
{
    const auto snapshot = documentSession.snapshot();
    if (!snapshot || validationWatcher.isRunning()) { return; }
    validationRevision = documentSession.revision();
    validateButton->setEnabled(false);
    validationSummary->setText(tr("Validating..."));
    const QString path = schemaFile->text().trimmed();
    validationWatcher.setFuture(QtConcurrent::run([snapshot, path]() {
        try {
            return validate(*snapshot, path.isEmpty() ? QByteArray("{}") : readJsonFile(path));
        }
        catch (const std::exception &exception) {
            ValidationResult result;
            result.error = QString::fromUtf8(exception.what());
            return result;
        }
    }));
}

void Workbench::showIssues(const QVector<Diagnostic> &issues)
{
    const auto snapshot = documentSession.snapshot();
    if (!snapshot) { return; }
    shownIssues = issues;
    issuesTable->setRowCount(issues.size());
    for (int row = 0; row < issues.size(); ++row) {
        const Diagnostic &issue = issues[row];
        const int node = snapshot->nodeAt(issue.offset);
        cell(issuesTable, row, 0, QString::number(snapshot->lineAt(issue.offset)));
        cell(issuesTable, row, 1, node >= 0 ? snapshot->path(node) : QString());
        cell(issuesTable, row, 2, (issue.warning ? tr("Warning: %1") : tr("Error: %1")).arg(issue.message));
    }
    validationSummary->setText(tr("%1 issues").arg(issues.size()));
}

void Workbench::runComparison()
{
    const auto snapshot = documentSession.snapshot();
    const QString path = comparisonFile->text().trimmed();
    if (!snapshot || comparisonWatcher.isRunning()) { return; }
    if (path.isEmpty()) { comparisonSummary->setText(tr("Select a comparison file.")); return; }
    comparisonRevision = documentSession.revision();
    const bool ignore = ignoreOrder->isChecked();
    compareButton->setEnabled(false);
    comparisonSummary->setText(tr("Comparing..."));
    comparisonWatcher.setFuture(QtConcurrent::run([snapshot, path, ignore]() {
        ComparisonJob job;
        try {
            const QString suffix = QFileInfo(path).suffix().toLower();
            job.other = std::make_shared<Index>(readJsonFile(path), suffix == QStringLiteral("jsonl") || suffix == QStringLiteral("ndjson"));
            job.result = compare(*snapshot, *job.other, ignore);
        }
        catch (const std::exception &exception) {
            job.result.error = QString::fromUtf8(exception.what());
        }
        return job;
    }));
}

}