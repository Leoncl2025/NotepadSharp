/*
 * This file is part of Notepad Next.
 *
 * Notepad Next is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "CompareToolBar.h"

#include "CompareSession.h"

#include <QAction>
#include <QHBoxLayout>
#include <QLabel>
#include <QSizePolicy>

#include <algorithm>

namespace Compare
{

CompareToolBar::CompareToolBar(Session *session,
                               QAction *previousAction,
                               QAction *nextAction,
                               QAction *refreshAction,
                               QAction *cancelAction,
                               QAction *clearAction,
                               QWidget *parent) :
    QToolBar(tr("Compare"), parent),
    session(session),
    previousAction(previousAction),
    nextAction(nextAction),
    refreshAction(refreshAction),
    cancelAction(cancelAction),
    clearAction(clearAction),
    pairLabel(new QLabel(this)),
    statusLabel(new QLabel(this))
{
    setObjectName(QStringLiteral("compareToolBar"));
    setMovable(false);
    setFloatable(false);
    setIconSize(QSize(16, 16));
    setToolButtonStyle(Qt::ToolButtonIconOnly);
    setStyleSheet(QStringLiteral(
        "QToolBar#compareToolBar { background: #f8fafc; border-top: 1px solid #e2e8f0; "
        "border-bottom: 1px solid #cbd5e1; padding: 5px 8px; spacing: 3px; }"
        "QToolBar#compareToolBar QToolButton { border: 1px solid transparent; border-radius: 5px; "
        "min-width: 26px; min-height: 26px; padding: 1px; }"
        "QToolBar#compareToolBar QToolButton:hover { background: #e8edf2; border-color: #d8e0e7; }"
        "QToolBar#compareToolBar QToolButton:pressed { background: #dce4ea; }"
        "QToolBar#compareToolBar QToolButton:disabled { opacity: 0.38; }"
        "QToolBar#compareToolBar::separator { background: #d9e1e7; width: 1px; margin: 5px 6px; }"
        "QLabel#comparePairLabel { color: #1f2937; font-weight: 600; padding: 0 10px 0 2px; }"));

    previousAction->setIcon(QIcon(QStringLiteral(":/icons/chevron-up.svg")));
    previousAction->setShortcut(QKeySequence(QStringLiteral("Shift+F7")));
    previousAction->setToolTip(tr("Previous Difference (Shift+F7)"));
    nextAction->setIcon(QIcon(QStringLiteral(":/icons/chevron-down.svg")));
    nextAction->setShortcut(QKeySequence(QStringLiteral("F7")));
    nextAction->setToolTip(tr("Next Difference (F7)"));
    refreshAction->setIcon(QIcon(QStringLiteral(":/icons/rotate-cw.svg")));
    refreshAction->setToolTip(tr("Refresh Compare"));
    cancelAction->setIcon(QIcon(QStringLiteral(":/icons/cross.svg")));
    cancelAction->setToolTip(tr("Cancel Compare"));
    clearAction->setIcon(QIcon(QStringLiteral(":/icons/trash-2.svg")));
    clearAction->setToolTip(tr("Clear Compare"));

    pairLabel->setObjectName(QStringLiteral("comparePairLabel"));
    pairLabel->setMinimumWidth(120);
    pairLabel->setMaximumWidth(320);
    pairLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    statusLabel->setObjectName(QStringLiteral("compareStatusLabel"));
    statusLabel->setMinimumWidth(74);
    statusLabel->setAlignment(Qt::AlignCenter);
    statusLabel->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Preferred);

    auto createMetric = [this](const QString &name, const QString &color, QLabel **textLabel) {
        QWidget *metric = new QWidget(this);
        metric->setObjectName(name + QStringLiteral("Metric"));
        metric->setFixedHeight(24);
        metric->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Preferred);

        QHBoxLayout *layout = new QHBoxLayout(metric);
        layout->setContentsMargins(6, 0, 6, 0);
        layout->setSpacing(5);

        QLabel *dot = new QLabel(metric);
        dot->setFixedSize(7, 7);
        dot->setStyleSheet(QStringLiteral("background: %1; border-radius: 3px;").arg(color));

        QLabel *label = new QLabel(metric);
        label->setObjectName(name);
        label->setStyleSheet(QStringLiteral("color: #475569; font-weight: 500;"));
        label->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Preferred);

        layout->addWidget(dot);
        layout->addWidget(label);
        *textLabel = label;
        return metric;
    };
    addedMetric = createMetric(
        QStringLiteral("compareAddedCountLabel"), QStringLiteral("#2f9e44"), &addedCountLabel);
    deletedMetric = createMetric(
        QStringLiteral("compareDeletedCountLabel"), QStringLiteral("#e03131"), &deletedCountLabel);
    modifiedMetric = createMetric(
        QStringLiteral("compareModifiedCountLabel"), QStringLiteral("#d97706"), &modifiedCountLabel);

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
    updateState();
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
    QString statusBackground = QStringLiteral("#eef2f6");
    QString statusBorder = QStringLiteral("#d8e0e7");
    QString statusForeground = QStringLiteral("#475569");
    switch (session->state()) {
    case Session::State::Running:
        statusText = tr("Comparing");
        statusBackground = QStringLiteral("#e8f1ff");
        statusBorder = QStringLiteral("#bfd5f5");
        statusForeground = QStringLiteral("#1d4ed8");
        break;
    case Session::State::Ready:
        if (session->differenceCount() == 0) {
            statusText = tr("No changes");
            statusBackground = QStringLiteral("#eaf7ee");
            statusBorder = QStringLiteral("#b9dec5");
            statusForeground = QStringLiteral("#287a3e");
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
        statusBackground = QStringLiteral("#fff6e5");
        statusBorder = QStringLiteral("#efd39a");
        statusForeground = QStringLiteral("#8a5a00");
        break;
    case Session::State::Cancelled:
        statusText = tr("Cancelled");
        statusBackground = QStringLiteral("#eef2f6");
        break;
    case Session::State::Failed:
        statusText = tr("Compare failed");
        statusBackground = QStringLiteral("#fdecec");
        statusBorder = QStringLiteral("#efb8b8");
        statusForeground = QStringLiteral("#a22626");
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
        .arg(statusForeground, statusBackground, statusBorder));
    statusLabel->setMinimumWidth(std::max(74, statusLabel->sizeHint().width()));
}

}