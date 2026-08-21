/*
 * This file is part of Notepad Next.
 *
 * Notepad Next is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#pragma once

#include "CompareNavigator.h"
#include "CompareOverlay.h"

#include <QMetaObject>
#include <QObject>
#include <QPointer>
#include <QThreadPool>

class ScintillaNext;
class AppearanceManager;

namespace Scintilla
{
enum class Update;
}

namespace Compare
{

class Session : public QObject
{
    Q_OBJECT

public:
    enum class State
    {
        None,
        Running,
        Ready,
        Stale,
        Cancelled,
        Failed,
    };
    Q_ENUM(State)

    explicit Session(QObject *parent = nullptr);
    Session(AppearanceManager *appearanceManager, QObject *parent);
    ~Session() override;

    void start(ScintillaNext *leftEditor, ScintillaNext *rightEditor);
    void refresh();
    void cancel();
    void clear();
    void nextDifference();
    void previousDifference();
    void refreshAppearance();

    State state() const { return currentState; }
    bool hasPair() const { return !leftEditor.isNull() && !rightEditor.isNull(); }
    bool isRunning() const { return currentState == State::Running; }
    bool hasDifferences() const { return currentState == State::Ready && navigator.hasDifferences(); }
    qsizetype differenceCount() const { return currentResult.hunks.size(); }
    qsizetype currentDifferenceNumber() const { return navigator.currentNumber(); }
    qsizetype addedCount() const;
    qsizetype deletedCount() const;
    qsizetype modifiedCount() const;
    QString leftName() const;
    QString rightName() const;

signals:
    void stateChanged();
    void navigationChanged();
    void message(const QString &text);

private:
    void beginComputation();
    void complete(quint64 requestGeneration,
                  quint64 requestLeftRevision,
                  quint64 requestRightRevision,
                  quintptr requestLeftIdentity,
                  quintptr requestRightIdentity,
                  Result result);
    void connectEditors();
    void disconnectEditors();
    void editorUpdated(ScintillaNext *editor, Scintilla::Update updated);
    void invalidate(ScintillaNext *editor);
    void stopWorker();
    void setState(State state);
    void navigateTo(const DiffHunk *hunk);
    void synchronizeScroll(ScintillaNext *source);
    qsizetype changeCount(ChangeKind kind) const;

    QPointer<ScintillaNext> leftEditor;
    QPointer<ScintillaNext> rightEditor;
    QVector<QMetaObject::Connection> editorConnections;
    QThreadPool workerPool;
    CancellationSource cancellationSource;
    quint64 generation = 0;
    quint64 leftRevision = 0;
    quint64 rightRevision = 0;
    State currentState = State::None;
    Result currentResult;
    Navigator navigator;
    Overlay overlay;
    bool applyingOverlay = false;
    bool synchronizingScroll = false;
};

}