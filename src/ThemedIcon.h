#pragma once

#include <QColor>
#include <QIcon>
#include <QPainter>
#include <QPixmap>
#include <QString>

namespace ThemedIcon {

inline QPixmap tintedPixmap(const QIcon &source, int size, const QColor &color)
{
    QPixmap pixmap = source.pixmap(size, size, QIcon::Normal, QIcon::Off);
    QPainter painter(&pixmap);
    painter.setCompositionMode(QPainter::CompositionMode_SourceIn);
    painter.fillRect(pixmap.rect(), color);
    return pixmap;
}

inline QIcon monochrome(const QString &resource, const QColor &normal,
                        const QColor &disabled)
{
    const QIcon source(resource);
    QIcon result;
    for (const int size : {16, 20, 24, 32}) {
        result.addPixmap(tintedPixmap(source, size, normal), QIcon::Normal);
        result.addPixmap(tintedPixmap(source, size, disabled), QIcon::Disabled);
    }
    return result;
}

} // namespace ThemedIcon