#ifndef JSONSESSION_H
#define JSONSESSION_H

#include "JsonIndex.h"
#include "ScintillaNext.h"

#include <QFutureWatcher>
#include <QPointer>
#include <QTimer>
#include <memory>

namespace JsonTools {

using Snapshot = std::shared_ptr<const Index>;

class Session : public QObject
{
    Q_OBJECT

public:
    explicit Session(QObject *parent = nullptr);
    void setEditor(ScintillaNext *editor);
    ScintillaNext *editor() const { return currentEditor; }
    Snapshot snapshot() const { return currentIndex; }
    quint64 revision() const { return generation; }
    bool jsonLines() const;
    QString state() const { return status; }
    void setJsonLines(bool enabled);
    void enable();
    void refresh();

signals:
    void updated();
    void caretChanged();

private:
    struct Build
    {
        Snapshot index;
        QString error;
        quint64 revision = 0;
    };

    void rebuild();
    QPointer<ScintillaNext> currentEditor;
    Snapshot currentIndex;
    QTimer debounce;
    QFutureWatcher<Build> watcher;
    quint64 generation = 0;
    QString status;
    QMetaObject::Connection modificationConnection;
    QMetaObject::Connection positionConnection;
    QMetaObject::Connection destructionConnection;
    QMetaObject::Connection languageConnection;
};

}

#endif