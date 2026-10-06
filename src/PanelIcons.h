// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once
#include <QImage>
#include <QString>
constexpr int PanelIconIdentityRole = Qt::UserRole + 104;
struct PanelIconRequest { int identity; QString path; bool directory = false, archive = false; };
QImage nativePanelIcon(const PanelIconRequest &, QSize);
