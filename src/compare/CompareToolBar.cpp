/*
 * This file is part of Notepad Next.
 *
 * Notepad Next is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "CompareToolBar.h"

#include "AppearanceManager.h"
#include "AppearanceTrace.h"
#include "CompareSession.h"

#include <QAction>
#include <QApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QSizePolicy>

#include <algorithm>

namespace Compare
{

namespace
{

QString cssColor(const QColor &color)
{
    return QStringLiteral("rgba(%1, %2, %3, %4)")
        .arg(color.red())
        .arg(color.green())
        .arg(color.blue())
        .arg(color.alpha());
}

QIcon monochromeIcon(const QString &resource, const QColor &color)
{
    const QIcon source(resource);
    QIcon result;
    for (const int size : {16, 20, 24, 32}) {
        QPixmap pixmap = source.pixmap(size, size);
        QPainter painter(&pixmap);
        painter.setCompositionMode(QPainter::CompositionMode_SourceIn);
        painter.fillRect(pixmap.rect(), color);
        painter.end();
        result.addPixmap(pixmap);
    }
    return result;
}

}

CompareToolBar::CompareToolBar(Session *session,
                               QAction *previousAction,
                               QAction *nextAction,
                               QAction *refreshAction,
                               QAction *cancelAction,
                               QAction *clearAction,
                               QWidget *parent) :
    CompareToolBar(session, nullptr, previousAction, nextAction, refreshAction,
                   cancelAction, clearAction, parent)
{
}

CompareToolBar::CompareToolBar(Session *session,
                               AppearanceManager *appearanceManager,
                               QAction *previousAction,
                               QAction *nextAction,
                               QAction *refreshAction,
                               QAction *cancelAction,
                               QAction *clearAction,
                               QWidget *parent) :
    QToolBar(tr("Compare"), parent),
    session(session),
    appearanceManager(appearanceManager),
    previousAction(previousAction),
    nextAction(nextAction),
    refreshAction(refreshAction),
    cancelAction(cancelAction),
    clearAction(clearAction),
    pairLabel(new QLabel(this)),
    statusLabel(new QLabel(this))
{
    if (!appearanceManager) {
        const QPalette palette = QApplication::palette();
        fallbackTokens.surfaceShell = palette.color(QPalette::Window);
        fallbackTokens.surfaceRaised = palette.color(QPalette::Button);
        fallbackTokens.surfaceHover = palette.color(QPalette::AlternateBase);
        fallbackTokens.borderDefault = palette.color(QPalette::Mid);
        fallbackTokens.textPrimary = palette.color(QPalette::WindowText);
        fallbackTokens.textSecondary = palette.color(QPalette::PlaceholderText);
        fallbackTokens.stateError = QColor(QStringLiteral("#A1260D"));
        fallbackTokens.stateWarning = QColor(QStringLiteral("#8A6D00"));
        fallbackTokens.stateSuccess = QColor(QStringLiteral("#107C10"));
        fallbackTokens.stateInformation = palette.color(QPalette::Link);
        fallbackTokens.diffAddedMarker = QColor(QStringLiteral("#2F9E44"));
        fallbackTokens.diffModifiedMarker = QColor(QStringLiteral("#0078D4"));
        fallbackTokens.diffDeletedMarker = QColor(QStringLiteral("#E03131"));
    }

    setObjectName(QStringLiteral("compareToolBar"));
    setMovable(false);
    setFloatable(false);
    setIconSize(QSize(16, 16));
    setToolButtonStyle(Qt::ToolButtonIconOnly);

    previousAction->setShortcut(QKeySequence(QStringLiteral("Shift+F7")));
    previousAction->setToolTip(tr("Previous Difference (Shift+F7)"));
    nextAction->setShortcut(QKeySequence(QStringLiteral("F7")));
    nextAction->setToolTip(tr("Next Difference (F7)"));
    refreshAction->setToolTip(tr("Refresh Compare"));
    cancelAction->setToolTip(tr("Cancel Compare"));
    clearAction->setToolTip(tr("Clear Compare"));

    pairLabel->setObjectName(QStringLiteral("comparePairLabel"));
    pairLabel->setMinimumWidth(120);
    pairLabel->setMaximumWidth(320);
    pairLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    statusLabel->setObjectName(QStringLiteral("compareStatusLabel"));
    statusLabel->setMinimumWidth(74);
    statusLabel->setAlignment(Qt::AlignCenter);
    statusLabel->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Preferred);

    auto createMetric = [this](const QString &name, const QString &dotName, QLabel **textLabel) {
        QWidget *metric = new QWidget(this);
        metric->setObjectName(name + QStringLiteral("Metric"));
        metric->setFixedHeight(24);
        metric->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Preferred);

        QHBoxLayout *layout = new QHBoxLayout(metric);
        layout->setContentsMargins(6, 0, 6, 0);
        layout->setSpacing(5);

        QLabel *dot = new QLabel(metric);
        dot->setObjectName(dotName);
        dot->setFixedSize(7, 7);

        QLabel *label = new QLabel(metric);
        label->setObjectName(name);
        label->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Preferred);

        layout->addWidget(dot);
        layout->addWidget(label);
        *textLabel = label;
        return metric;
    };
    addedMetric = createMetric(
        QStringLiteral("compareAddedCountLabel"), QStringLiteral("compareAddedDot"), &addedCountLabel);
    deletedMetric = createMetric(
        QStringLiteral("compareDeletedCountLabel"), QStringLiteral("compareDeletedDot"), &deletedCountLabel);
    modifiedMetric = createMetric(
        QStringLiteral("compareModifiedCountLabel"), QStringLiteral("compareModifiedDot"), &modifiedCountLabel);

    QWidget *spacer = new QWidget(this);
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    addWidget(pairLabel);
    addWidget(statusLabel);
    metricsSeparator = addSeparator();
    metricsSeparator->setObjectName(QStringLiteral("compareMetricsSeparator"));
    addedMetricAction = addWidget(addedMetric);
    deletedMetricAction = addWidget(deletedMetric);
    modifiedMetricAction = addWidget(modifiedMetric);
    addedMetricAction->setObjectName(QStringLiteral("compareAddedMetricAction"));
    deletedMetricAction->setObjectName(QStringLiteral("compareDeletedMetricAction"));
    modifiedMetricAction->setObjectName(QStringLiteral("compareModifiedMetricAction"));
    addWidget(spacer);
    addAction(previousAction);
    addAction(nextAction);
    addSeparator();
    addAction(refreshAction);
    addAction(cancelAction);
    addAction(clearAction);

    connect(session, &Session::stateChanged, this, [this]() { updateState(); });
    connect(session, &Session::navigationChanged, this, [this]() { updateState(); });
    applyAppearance();
}

void CompareToolBar::applyAppearance()
{
    AppearanceTrace::Scope trace(QStringLiteral("compare-toolbar"));
    const AppearanceTokens &tokens = appearanceTokens();
    setStyleSheet(QStringLiteral(
        "QToolBar#compareToolBar { background: %1; border-top: 1px solid %2; "
        "border-bottom: 1px solid %2; padding: 5px 8px; spacing: 3px; }"
        "QToolBar#compareToolBar QToolButton { color: %3; border: 1px solid transparent; "
        "border-radius: 5px; min-width: 26px; min-height: 26px; padding: 1px; }"
        "QToolBar#compareToolBar QToolButton:hover { background: %4; border-color: %2; }"
        "QToolBar#compareToolBar QToolButton:pressed { background: %5; }"
        "QToolBar#compareToolBar::separator { background: %2; width: 1px; margin: 5px 6px; }"
        "QLabel#comparePairLabel { color: %3; font-weight: 600; padding: 0 10px 0 2px; }"
        "QLabel#compareAddedCountLabel, QLabel#compareDeletedCountLabel, "
        "QLabel#compareModifiedCountLabel { color: %6; font-weight: 500; }"
        "QLabel#compareAddedDot { background: %7; border-radius: 3px; }"
        "QLabel#compareDeletedDot { background: %8; border-radius: 3px; }"
        "QLabel#compareModifiedDot { background: %9; border-radius: 3px; }")
        .arg(cssColor(tokens.surfaceShell), cssColor(tokens.borderDefault),
             cssColor(tokens.textPrimary), cssColor(tokens.surfaceHover),
             cssColor(tokens.surfaceRaised), cssColor(tokens.textSecondary),
             cssColor(tokens.diffAddedMarker), cssColor(tokens.diffDeletedMarker),
             cssColor(tokens.diffModifiedMarker)));

    previousAction->setIcon(monochromeIcon(QStringLiteral(":/icons/chevron-up.svg"), tokens.textPrimary));
    nextAction->setIcon(monochromeIcon(QStringLiteral(":/icons/chevron-down.svg"), tokens.textPrimary));
    refreshAction->setIcon(monochromeIcon(QStringLiteral(":/icons/rotate-cw.svg"), tokens.textPrimary));
    cancelAction->setIcon(monochromeIcon(QStringLiteral(":/icons/cross.svg"), tokens.textPrimary));
    clearAction->setIcon(monochromeIcon(QStringLiteral(":/icons/trash-2.svg"), tokens.textPrimary));
    updateState();
}

const AppearanceTokens &CompareToolBar::appearanceTokens() const
{
    return appearanceManager ? appearanceManager->tokens() : fallbackTokens;
}

void CompareToolBar::updateState()
{
    const bool hasPair = session->hasPair();
    const bool running = session->isRunning();
    previousAction->setEnabled(session->hasDifferences());
    nextAction->setEnabled(session->hasDifferences());
    refreshAction->setEnabled(hasPair && !running);
    cancelAction->setEnabled(running);
    clearAction->setEnabled(hasPair);
    setVisible(hasPair);

    const QString pairText = tr("%1  vs  %2").arg(session->leftName(), session->rightName());
    pairLabel->setText(pairLabel->fontMetrics().elidedText(pairText, Qt::ElideMiddle, 300));
    pairLabel->setToolTip(pairText);

    const bool ready = session->state() == Session::State::Ready;
    const bool showCounts = ready && session->differenceCount() > 0;
    metricsSeparator->setVisible(showCounts);
    addedMetricAction->setVisible(showCounts);
    deletedMetricAction->setVisible(showCounts);
    modifiedMetricAction->setVisible(showCounts);
    addedCountLabel->setText(tr("%1 added").arg(session->addedCount()));
    deletedCountLabel->setText(tr("%1 deleted").arg(session->deletedCount()));
    modifiedCountLabel->setText(tr("%1 changed").arg(session->modifiedCount()));

    QString statusText;
    const AppearanceTokens &tokens = appearanceTokens();
    QColor statusBackground = tokens.surfaceRaised;
    QColor statusBorder = tokens.borderDefault;
    QColor statusForeground = tokens.textSecondary;
    auto useStateColor = [&](const QColor &color) {
        statusForeground = color;
        statusBackground = color;
        statusBackground.setAlpha(36);
        statusBorder = color;
        statusBorder.setAlpha(120);
    };
    switch (session->state()) {
    case Session::State::Running:
        statusText = tr("Comparing");
        useStateColor(tokens.stateInformation);
        break;
    case Session::State::Ready:
        if (session->differenceCount() == 0) {
            statusText = tr("No changes");
            useStateColor(tokens.stateSuccess);
        }
        else if (session->currentDifferenceNumber() > 0) {
            statusText = tr("%1 / %2")
                .arg(session->currentDifferenceNumber())
                .arg(session->differenceCount());
        }
        else {
            statusText = tr("%1 changes").arg(session->differenceCount());
        }
        break;
    case Session::State::Stale:
        statusText = tr("Out of date");
        statusLabel->setToolTip(tr("Refresh Compare to update the result."));
        useStateColor(tokens.stateWarning);
        break;
    case Session::State::Cancelled:
        statusText = tr("Cancelled");
        break;
    case Session::State::Failed:
        statusText = tr("Compare failed");
        useStateColor(tokens.stateError);
        break;
    case Session::State::None:
        break;
    }
    if (session->state() != Session::State::Stale) {
        statusLabel->setToolTip(QString());
    }
    statusLabel->setText(statusText);
    statusLabel->setStyleSheet(QStringLiteral(
        "color: %1; background: %2; border: 1px solid %3; border-radius: 10px; "
        "font-weight: 500; padding: 3px 10px;")
        .arg(cssColor(statusForeground), cssColor(statusBackground), cssColor(statusBorder)));
    statusLabel->setMinimumWidth(std::max(74, statusLabel->sizeHint().width()));
}

}