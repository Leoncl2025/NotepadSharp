#include "JsonSession.h"

#include <QtConcurrent/QtConcurrentRun>

namespace JsonTools {

Session::Session(QObject *parent) : QObject(parent)
{
    debounce.setSingleShot(true);
    debounce.setInterval(180);
    connect(&debounce, &QTimer::timeout, this, &Session::rebuild);
    connect(&watcher, &QFutureWatcher<Build>::finished, this, [this]() {
        const Build result = watcher.result();
        if (result.revision != generation) {
            if (!debounce.isActive()) {
                debounce.start();
            }
            return;
        }
        currentIndex = result.index;
        status = result.error;
        emit updated();
        emit caretChanged();
    });
}

void Session::setEditor(ScintillaNext *editor)
{
    disconnect(modificationConnection);
    disconnect(positionConnection);
    disconnect(destructionConnection);
    disconnect(languageConnection);
    currentEditor = editor;
    if (editor) {
        modificationConnection = connect(editor, &ScintillaNext::modified, this, [this](Scintilla::ModificationFlags flags) {
            if (Scintilla::FlagSet(flags, Scintilla::ModificationFlags::InsertText)
                || Scintilla::FlagSet(flags, Scintilla::ModificationFlags::DeleteText)) {
                refresh();
            }
        });
        positionConnection = connect(editor, &ScintillaNext::updateUi, this, [this](Scintilla::Update flags) {
            if (Scintilla::FlagSet(flags, Scintilla::Update::Selection)) {
                emit caretChanged();
            }
        });
        destructionConnection = connect(editor, &QObject::destroyed, this, [this]() { setEditor(nullptr); });
        languageConnection = connect(editor, &ScintillaNext::lexerChanged, this, &Session::refresh);
    }
    refresh();
}

bool Session::jsonLines() const
{
    if (!currentEditor) {
        return false;
    }
    const QVariant override = currentEditor->QObject::property("jsonLinesMode");
    if (override.isValid()) {
        return override.toBool();
    }
    const QString suffix = QFileInfo(currentEditor->getName()).suffix().toLower();
    return suffix == QStringLiteral("jsonl") || suffix == QStringLiteral("ndjson");
}

void Session::setJsonLines(bool enabled)
{
    if (currentEditor) {
        currentEditor->QObject::setProperty("jsonLinesMode", enabled);
        enable();
    }
}

void Session::enable()
{
    if (currentEditor) {
        currentEditor->QObject::setProperty("jsonToolsEnabled", true);
    }
    refresh();
}

void Session::refresh()
{
    ++generation;
    currentIndex.reset();
    status.clear();
    emit updated();
    emit caretChanged();
    debounce.start();
}

void Session::rebuild()
{
    if (watcher.isRunning() || !currentEditor) {
        return;
    }
    if (currentEditor->textLength() > 32 * 1024 * 1024) {
        status = tr("JSON tools are limited to 32 MiB per document.");
        emit updated();
        return;
    }
    const QByteArray text(reinterpret_cast<const char *>(currentEditor->characterPointer()), currentEditor->textLength());
    const QByteArray beginning = text.left(256).trimmed();
    const bool looksJson = !beginning.isEmpty() && (beginning.startsWith('{') || beginning.startsWith('[')
        || beginning.startsWith('"') || beginning.startsWith('-') || (beginning[0] >= '0' && beginning[0] <= '9')
        || beginning.startsWith("true") || beginning.startsWith("false") || beginning.startsWith("null"));
    if (!looksJson && !jsonLines() && currentEditor->languageName != QStringLiteral("JSON")
        && !currentEditor->QObject::property("jsonToolsEnabled").toBool()) {
        return;
    }
    status = tr("Indexing JSON...");
    emit updated();
    const quint64 revision = generation;
    const bool lines = jsonLines();
    watcher.setFuture(QtConcurrent::run([text, lines, revision]() {
        Build result;
        result.revision = revision;
        try {
            result.index = std::make_shared<Index>(text, lines);
        }
        catch (const std::exception &exception) {
            result.error = QString::fromUtf8(exception.what());
        }
        return result;
    }));
}

}