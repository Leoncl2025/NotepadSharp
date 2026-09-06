#include "JsonFormatter.h"

#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QVector>

namespace {

enum class TokenType { Open, Close, Colon, Comma, Value, LineComment, BlockComment };

struct Token
{
    TokenType type;
    QString text;
};

bool isJsonSpace(QChar character)
{
    return character == ' ' || character == '\t' || character == '\r' || character == '\n';
}

bool isPunctuation(QChar character)
{
    return character == '{' || character == '}' || character == '[' || character == ']'
        || character == ':' || character == ',' || character == '"';
}

bool startsComment(const QString &text, qsizetype index)
{
    return text[index] == '/' && index + 1 < text.size()
        && (text[index + 1] == '/' || text[index + 1] == '*');
}

QVector<Token> tokenize(const QString &text)
{
    QVector<Token> tokens;
    qsizetype index = 0;
    while (index < text.size()) {
        const QChar character = text[index];
        if (isJsonSpace(character)) {
            ++index;
            continue;
        }

        const qsizetype start = index++;
        TokenType type = TokenType::Value;
        if (character == '{' || character == '[') {
            type = TokenType::Open;
        }
        else if (character == '}' || character == ']') {
            type = TokenType::Close;
        }
        else if (character == ':') {
            type = TokenType::Colon;
        }
        else if (character == ',') {
            type = TokenType::Comma;
        }
        else if (character == '"') {
            while (index < text.size()) {
                const QChar stringCharacter = text[index++];
                if (stringCharacter == '\\' && index < text.size()) {
                    ++index;
                }
                else if (stringCharacter == '"') {
                    break;
                }
            }
        }
        else if (startsComment(text, start)) {
            if (text[index++] == '/') {
                type = TokenType::LineComment;
                while (index < text.size() && text[index] != '\n' && text[index] != '\r') {
                    ++index;
                }
            }
            else {
                type = TokenType::BlockComment;
                while (index < text.size()) {
                    if (text[index++] == '*' && index < text.size() && text[index] == '/') {
                        ++index;
                        break;
                    }
                }
            }
        }
        else {
            while (index < text.size() && !isJsonSpace(text[index])
                   && !isPunctuation(text[index]) && !startsComment(text, index)) {
                ++index;
            }
        }
        tokens.append({type, text.mid(start, index - start)});
    }
    return tokens;
}

bool isEmptyContainer(const Token &opening, const Token &closing)
{
    return opening.type == TokenType::Open && closing.type == TokenType::Close
        && ((opening.text == "{" && closing.text == "}")
            || (opening.text == "[" && closing.text == "]"));
}

QString decodeSerialized(const QString &text)
{
    QString candidate = text;
    QString result = text;
    for (int layer = 0; layer < 16; ++layer) {
        QJsonParseError parseError;
        QJsonDocument document = QJsonDocument::fromJson("[" + candidate.toUtf8() + "\n]", &parseError);
        QString decoded;
        if (parseError.error == QJsonParseError::NoError) {
            if (document.array().size() != 1 || !document.array().first().isString()) {
                break;
            }
            decoded = document.array().first().toString();
        }
        else {
            const QString trimmed = candidate.trimmed();
            if ((!trimmed.startsWith("{") && !trimmed.startsWith("[")) || !candidate.contains("\\\"")) {
                break;
            }
            QString encoded = candidate;
            encoded.replace('\r', QStringLiteral("\\r"));
            encoded.replace('\n', QStringLiteral("\\n"));
            encoded.replace('\t', QStringLiteral("\\t"));
            document = QJsonDocument::fromJson("[\"" + encoded.toUtf8() + "\"]", &parseError);
            if (parseError.error != QJsonParseError::NoError || document.array().size() != 1
                || !document.array().first().isString()) {
                break;
            }
            decoded = document.array().first().toString();
        }

        const QString trimmedDecoded = decoded.trimmed();
        if (trimmedDecoded.startsWith('{') || trimmedDecoded.startsWith('[')) {
            result = decoded;
        }
        else if (!trimmedDecoded.startsWith('"')) {
            break;
        }
        if (decoded == candidate) {
            break;
        }
        candidate = decoded;
    }
    return result;
}

QString layout(const QVector<Token> &tokens, bool pretty, const QString &indent, const QString &eol)
{
    QString output;
    qsizetype depth = 0;
    bool lineStart = true;
    const auto append = [&](const QString &text) {
        if (lineStart && pretty) {
            output += indent.repeated(qMin<qsizetype>(depth, 256));
        }
        output += text;
        lineStart = false;
    };
    const auto newline = [&] {
        if (!lineStart) {
            output += eol;
            lineStart = true;
        }
    };

    for (qsizetype index = 0; index < tokens.size(); ++index) {
        const Token &token = tokens[index];
        if (index > 0 && !lineStart && (token.type == TokenType::Value || token.type == TokenType::Open)) {
            const TokenType previous = tokens[index - 1].type;
            if (previous == TokenType::Value || previous == TokenType::Close || previous == TokenType::BlockComment) {
                append(QStringLiteral(" "));
            }
        }

        switch (token.type) {
        case TokenType::Open:
            append(token.text);
            ++depth;
            if (pretty && (index + 1 == tokens.size() || !isEmptyContainer(token, tokens[index + 1]))) {
                newline();
            }
            break;
        case TokenType::Close:
            depth = qMax<qsizetype>(0, depth - 1);
            if (pretty && (index == 0 || !isEmptyContainer(tokens[index - 1], token))) {
                newline();
            }
            append(token.text);
            break;
        case TokenType::Comma:
            append(token.text);
            if (pretty) {
                newline();
            }
            break;
        case TokenType::Colon:
            append(pretty ? QStringLiteral(": ") : token.text);
            break;
        case TokenType::LineComment:
            append(token.text);
            newline();
            break;
        case TokenType::BlockComment:
        case TokenType::Value:
            append(token.text);
            break;
        }
    }
    if (lineStart && output.endsWith(eol)) {
        output.chop(eol.size());
    }
    return output;
}

}

JsonFormatter::Result JsonFormatter::format(const QString &text, Mode mode, const QString &indent, const QString &eol)
{
    Result result;
    const QString source = decodeSerialized(text);
    result.decodedString = source != text;
    const QString trimmed = source.trimmed();
    result.mode = mode == Mode::Toggle
        ? (!result.decodedString && (trimmed.contains('\n') || trimmed.contains('\r')) ? Mode::Compact : Mode::Pretty)
        : mode;
    result.text = layout(tokenize(source), result.mode == Mode::Pretty, indent, eol);

    const QByteArray utf8 = result.text.toUtf8();
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson("[" + utf8 + "\n]", &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        result.error = parseError.errorString();
        const qsizetype byteOffset = qBound(qsizetype(0), qsizetype(parseError.offset) - 1, utf8.size());
        result.errorOffset = QString::fromUtf8(utf8.left(byteOffset)).size();
    }
    else if (document.array().size() != 1) {
        result.error = QCoreApplication::translate("JsonFormatter", "Expected a single JSON value.");
        result.errorOffset = 0;
    }

    return result;
}