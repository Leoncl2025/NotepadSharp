#ifndef JSONWORKBENCH_H
#define JSONWORKBENCH_H

#include "JsonOperations.h"
#include "JsonSession.h"

#include <QDockWidget>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QMenu;
class QTableWidget;
class QTabWidget;
class QToolButton;
class QTreeView;

namespace JsonTools {

class TreeModel;

class Workbench : public QDockWidget
{
    Q_OBJECT

public:
    explicit Workbench(QWidget *parent = nullptr);
    Session *session() { return &documentSession; }
    QWidget *pathWidget() const;
    void setEditor(ScintillaNext *editor);
    void addContextMenu(QMenu *menu, ScintillaNext *editor, qsizetype position);
    void selectNode(int node, bool key = false);
    void formatLines(JsonFormatter::Mode mode);

public slots:
    void openTools();
    void openQuery();
    void runQuery();
    void runValidation();
    void runComparison();

signals:
    void message(const QString &text);
    void openDocument(const QByteArray &text, const QString &name, qsizetype start, qsizetype end);
    void jsonLanguageRequested();

private:
    void indexUpdated();
    void caretUpdated();
    int currentNode() const;
    void formatNode(int node, JsonFormatter::Mode mode);
    void previewNode(int node);
    void foldNode(int node);
    void sibling(int direction);
    void showQueryResults(const QueryResult &result);
    void showIssues(const QVector<Diagnostic> &issues);
    void copyTable(QTableWidget *table);

    Session documentSession;
    TreeModel *treeModel;
    QTreeView *tree;
    QTabWidget *tabs;
    QComboBox *mode;
    QToolButton *pathButton;
    QToolButton *queryButton;
    QLineEdit *expression;
    QComboBox *searchTarget;
    QComboBox *selectionTarget;
    QCheckBox *caseSensitive;
    QLabel *querySummary;
    QTableWidget *queryTable;
    QLabel *documentSummary;
    QString currentPath;
    QVector<int> queryMatches;
    QFutureWatcher<QueryResult> queryWatcher;
    quint64 queryRevision = 0;
    QLineEdit *schemaFile;
    QToolButton *validateButton;
    QLabel *validationSummary;
    QTableWidget *issuesTable;
    QVector<Diagnostic> shownIssues;
    QFutureWatcher<ValidationResult> validationWatcher;
    quint64 validationRevision = 0;
    QLineEdit *comparisonFile;
    QCheckBox *ignoreOrder;
    QToolButton *compareButton;
    QLabel *comparisonSummary;
    QTableWidget *comparisonTable;
    Snapshot comparedDocument;
    QVector<Difference> differences;
    struct ComparisonJob
    {
        ComparisonResult result;
        Snapshot other;
    };
    QFutureWatcher<ComparisonJob> comparisonWatcher;
    quint64 comparisonRevision = 0;
};

}

#endif