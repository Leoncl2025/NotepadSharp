/*
 * This file is part of Notepad Next.
 *
 * Notepad Next is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "CompareSession.h"

#include "CompareEngine.h"
#include "ScintillaNext.h"

#include <QMetaObject>
#include <QRunnable>

#include <algorithm>

namespace Compare
{

Session::Session(QObject *parent) :
    QObject(parent)
{
    workerPool.setMaxThreadCount(1);
    workerPool.setExpiryTimeout(-1);
}

Session::~Session()
{
    disconnectEditors();
    stopWorker();
    workerPool.waitForDone();
    overlay.clear();
}

qsizetype Session::addedCount() const
{
    return changeCount(ChangeKind::Added);
}

qsizetype Session::deletedCount() const
{
    return changeCount(ChangeKind::Deleted);
}

qsizetype Session::modifiedCount() const
{
    return changeCount(ChangeKind::Modified);
}

QString Session::leftName() const
{
    return leftEditor ? leftEditor->getName() : QString();
}

QString Session::rightName() const
{
    return rightEditor ? rightEditor->getName() : QString();
}

void Session::start(ScintillaNext *newLeftEditor, ScintillaNext *newRightEditor)
{
    if (newLeftEditor == nullptr || newRightEditor == nullptr || newLeftEditor == newRightEditor) {
        emit message(tr("Choose two different documents to compare."));
        return;
    }

    clear();
    leftEditor = newLeftEditor;
    rightEditor = newRightEditor;
    leftRevision = 0;
    rightRevision = 0;
    connectEditors();
    beginComputation();
}

void Session::refresh()
{
    if (!hasPair()) {
        return;
    }

    beginComputation();
}

void Session::cancel()
{
    if (!isRunning()) {
        return;
    }

    stopWorker();
    ++generation;
    applyingOverlay = true;
    overlay.clear();
    applyingOverlay = false;
    navigator.reset();
    currentResult = {};
    setState(State::Cancelled);
    emit message(tr("Comparison cancelled."));
}

void Session::clear()
{
    stopWorker();
    ++generation;
    applyingOverlay = true;
    overlay.clear();
    applyingOverlay = false;
    navigator.reset();
    currentResult = {};
    disconnectEditors();
    leftEditor.clear();
    rightEditor.clear();
    leftRevision = 0;
    rightRevision = 0;
    setState(State::None);
}

void Session::nextDifference()
{
    navigateTo(navigator.next());
}

void Session::previousDifference()
{
    navigateTo(navigator.previous());
}

void Session::beginComputation()
{
    stopWorker();
    ++generation;
    cancellationSource = CancellationSource();

    applyingOverlay = true;
    overlay.clear();
    applyingOverlay = false;
    navigator.reset();
    currentResult = {};

    const QByteArray leftSnapshot = leftEditor->get_text_range(0, leftEditor->length());
    const QByteArray rightSnapshot = rightEditor->get_text_range(0, rightEditor->length());
    const CancellationToken cancellationToken = cancellationSource.token();
    const quint64 requestGeneration = generation;
    const quint64 requestLeftRevision = leftRevision;
    const quint64 requestRightRevision = rightRevision;
    const quintptr requestLeftIdentity = reinterpret_cast<quintptr>(leftEditor.data());
    const quintptr requestRightIdentity = reinterpret_cast<quintptr>(rightEditor.data());

    setState(State::Running);
    emit message(tr("Comparing %1 and %2...").arg(leftEditor->getName(), rightEditor->getName()));

    workerPool.start(QRunnable::create([
        this,
        leftSnapshot,
        rightSnapshot,
        cancellationToken,
        requestGeneration,
        requestLeftRevision,
        requestRightRevision,
        requestLeftIdentity,
        requestRightIdentity
    ]() mutable {
        Result result = Engine::compare(leftSnapshot, rightSnapshot, cancellationToken);
        QMetaObject::invokeMethod(this, [
            this,
            requestGeneration,
            requestLeftRevision,
            requestRightRevision,
            requestLeftIdentity,
            requestRightIdentity,
            result = std::move(result)
        ]() mutable {
            complete(requestGeneration,
                     requestLeftRevision,
                     requestRightRevision,
                     requestLeftIdentity,
                     requestRightIdentity,
                     std::move(result));
        }, Qt::QueuedConnection);
    }));
}

void Session::complete(quint64 requestGeneration,
                       quint64 requestLeftRevision,
                       quint64 requestRightRevision,
                       quintptr requestLeftIdentity,
                       quintptr requestRightIdentity,
                       Result result)
{
    if (requestGeneration != generation
        || requestLeftRevision != leftRevision
        || requestRightRevision != rightRevision
        || requestLeftIdentity != reinterpret_cast<quintptr>(leftEditor.data())
        || requestRightIdentity != reinterpret_cast<quintptr>(rightEditor.data())) {
        return;
    }

    if (result.status == Status::Cancelled) {
        setState(State::Cancelled);
        emit message(tr("Comparison cancelled."));
        return;
    }
    if (result.status != Status::Completed) {
        setState(State::Failed);
        emit message(result.message);
        return;
    }

    currentResult = std::move(result);
    navigator.reset(currentResult.hunks);
    applyingOverlay = true;
    overlay.apply(leftEditor, rightEditor, currentResult.hunks);
    applyingOverlay = false;
    setState(State::Ready);

    if (currentResult.hunks.isEmpty()) {
        emit message(tr("No differences found."));
    }
    else {
        emit message(tr("Found %Ln difference(s).", "", currentResult.hunks.size()));
    }
}

void Session::connectEditors()
{
    auto connectEditor = [this](ScintillaNext *editor) {
        editorConnections.append(connect(editor, &ScintillaNext::notify, this,
            [this, editor](Scintilla::NotificationData *notification) {
                if (applyingOverlay || notification->nmhdr.code != Scintilla::Notification::Modified) {
                    return;
                }

                const auto textChanges = Scintilla::ModificationFlags::InsertText
                    | Scintilla::ModificationFlags::DeleteText;
                if (Scintilla::FlagSet(notification->modificationType, textChanges)) {
                    invalidate(editor);
                }
            }));
        editorConnections.append(connect(editor, &ScintillaNext::updateUi, this,
            [this, editor](Scintilla::Update updated) { editorUpdated(editor, updated); }));
        editorConnections.append(connect(editor, &ScintillaNext::reloaded, this,
            [this, editor]() { invalidate(editor); }));
        editorConnections.append(connect(editor, &ScintillaNext::renamed, this,
            [this]() { emit stateChanged(); }));
        editorConnections.append(connect(editor, &ScintillaNext::closed, this,
            [this]() {
                clear();
                emit message(tr("Comparison cleared because an input was closed."));
            }));
    };

    connectEditor(leftEditor);
    connectEditor(rightEditor);
}

void Session::disconnectEditors()
{
    for (const QMetaObject::Connection &connection : std::as_const(editorConnections)) {
        QObject::disconnect(connection);
    }
    editorConnections.clear();
}

void Session::editorUpdated(ScintillaNext *editor, Scintilla::Update updated)
{
    if (applyingOverlay || !hasPair()) {
        return;
    }

    if (Scintilla::FlagSet(updated, Scintilla::Update::VScroll)) {
        synchronizeScroll(editor);
    }
}

void Session::invalidate(ScintillaNext *editor)
{
    if (!hasPair()) {
        return;
    }

    if (editor == leftEditor) {
        ++leftRevision;
    }
    else if (editor == rightEditor) {
        ++rightRevision;
    }
    else {
        return;
    }

    stopWorker();
    ++generation;
    applyingOverlay = true;
    overlay.clear();
    applyingOverlay = false;
    navigator.reset();
    currentResult = {};
    setState(State::Stale);
    emit message(tr("Comparison is out of date. Refresh to compare the current text."));
}

void Session::stopWorker()
{
    cancellationSource.cancel();
    workerPool.clear();
}

void Session::setState(State state)
{
    if (currentState == state) {
        return;
    }

    currentState = state;
    emit stateChanged();
}

void Session::navigateTo(const DiffHunk *hunk)
{
    if (hunk == nullptr || !hasPair()) {
        return;
    }

    applyingOverlay = true;
    overlay.setCurrent(hunk);
    applyingOverlay = false;

    auto reveal = [](ScintillaNext *editor, qsizetype line) {
        const qsizetype lastLine = std::max<qsizetype>(0, editor->lineCount() - 1);
        line = std::min(line, lastLine);
        editor->ensureVisibleEnforcePolicy(line);
        editor->gotoLine(line);
    };

    reveal(leftEditor, hunk->leftStart);
    reveal(rightEditor, hunk->rightStart);
    emit navigationChanged();
}

void Session::synchronizeScroll(ScintillaNext *source)
{
    if (synchronizingScroll || currentState != State::Ready || !hasPair()) {
        return;
    }

    ScintillaNext *target = source == leftEditor ? rightEditor.data() : leftEditor.data();
    if (target == nullptr || (source != leftEditor && source != rightEditor)) {
        return;
    }

    synchronizingScroll = true;
    const qsizetype sourceDisplayLine = source->firstVisibleLine();
    const qsizetype sourceLeadingGaps = overlay.leadingGapLineCount(source);
    const qsizetype sourceVisibleLeadingGaps = overlay.visibleLeadingGapLineCount(source);
    const qsizetype targetLeadingGaps = overlay.leadingGapLineCount(target);
    const qsizetype alignedDisplayLine = sourceDisplayLine
        + sourceLeadingGaps
        - sourceVisibleLeadingGaps;

    const qsizetype sourceDesiredVisibleGaps = std::clamp<qsizetype>(
        sourceLeadingGaps - alignedDisplayLine, 0, sourceLeadingGaps);
    const qsizetype sourceDesiredDisplayLine = std::max<qsizetype>(
        0, alignedDisplayLine - sourceLeadingGaps);
    overlay.setLeadingGapVisibleLines(source, sourceDesiredVisibleGaps);
    if (source->firstVisibleLine() != sourceDesiredDisplayLine) {
        source->setFirstVisibleLine(sourceDesiredDisplayLine);
    }

    const qsizetype targetVisibleGaps = std::clamp<qsizetype>(
        targetLeadingGaps - alignedDisplayLine, 0, targetLeadingGaps);
    const qsizetype targetDisplayLine = std::max<qsizetype>(
        0, alignedDisplayLine - targetLeadingGaps);
    overlay.setLeadingGapVisibleLines(target, targetVisibleGaps);
    target->setFirstVisibleLine(targetDisplayLine);
    synchronizingScroll = false;
}

qsizetype Session::changeCount(ChangeKind kind) const
{
    return std::count_if(currentResult.hunks.cbegin(), currentResult.hunks.cend(),
        [kind](const DiffHunk &hunk) { return hunk.kind == kind; });
}

}