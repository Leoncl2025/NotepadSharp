#ifndef JSONFORMATTER_H
#define JSONFORMATTER_H

#include <QString>

class JsonFormatter
{
public:
    enum class Mode { Pretty, Compact, Toggle };

    struct Result
    {
        QString text;
        QString error;
        qsizetype errorOffset = -1;
        Mode mode = Mode::Pretty;
        bool decodedString = false;

        bool isValid() const { return error.isEmpty(); }
    };

    static Result format(const QString &text, Mode mode = Mode::Pretty,
                        const QString &indent = QStringLiteral("    "),
                        const QString &eol = QStringLiteral("\n"));
};

#endif