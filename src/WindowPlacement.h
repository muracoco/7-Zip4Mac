// Copyright (C) 2026 7-Zip Mac Port contributors
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once
#include <QCursor>
#include <QGuiApplication>
#include <QScreen>
#include <QWidget>

inline void centerPortWindow(QWidget *window) {
    if (!window->isWindow() || window->isMaximized() || window->isFullScreen()) return;
    auto screen = QGuiApplication::screenAt(QCursor::pos());
    if (!screen) screen = window->screen();
    if (!screen) return;
    const QRect available = screen->availableGeometry();
    const QSize decorations = window->frameGeometry().size() - window->size();
    // Keep restored sizes when they fit; disconnected/smaller screens must
    // not leave the title bar or normal-window controls outside the work area.
    window->resize(qMin(window->width(), available.width() - decorations.width()),
                   qMin(window->height(), available.height() - decorations.height()));
    const QSize frame = window->frameGeometry().size();
    window->move(available.x() + qMax(0, (available.width() - frame.width()) / 2),
                 available.y() + qMax(0, (available.height() - frame.height()) / 2));
}
