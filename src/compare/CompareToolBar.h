/*
 * This file is part of Notepad Next.
 *
 * Notepad Next is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#pragma once

#include "AppearanceManager.h"

#include <QToolBar>

class QAction;
class QLabel;
class QWidget;

namespace Compare
{

class Session;

class CompareToolBar : public QToolBar
{
public:
    CompareToolBar(Session *session,
                   QAction *previousAction,
                   QAction *nextAction,
                   QAction *refreshAction,
                   QAction *cancelAction,
                   QAction *clearAction,
                   QWidget *parent = nullptr);
    CompareToolBar(Session *session,
                   AppearanceManager *appearanceManager,
                   QAction *previousAction,
                   QAction *nextAction,
                   QAction *refreshAction,
                   QAction *cancelAction,
                   QAction *clearAction,
                   QWidget *parent = nullptr);

    void applyAppearance();

private:
    const AppearanceTokens &appearanceTokens() const;
    void updateState();

    Session *session;
    AppearanceManager *appearanceManager;
    AppearanceTokens fallbackTokens;
    QAction *previousAction;
    QAction *nextAction;
    QAction *refreshAction;
    QAction *cancelAction;
    QAction *clearAction;
    QAction *metricsSeparator;
    QAction *addedMetricAction;
    QAction *deletedMetricAction;
    QAction *modifiedMetricAction;
    QLabel *pairLabel;
    QLabel *statusLabel;
    QWidget *addedMetric;
    QWidget *deletedMetric;
    QWidget *modifiedMetric;
    QLabel *addedCountLabel;
    QLabel *deletedCountLabel;
    QLabel *modifiedCountLabel;
};

}