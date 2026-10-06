// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once
#include "ArchiveBackend.h"
class QTreeWidgetItem;
constexpr int PanelNameRole=Qt::UserRole+105;
constexpr int PanelCopyNameRole=Qt::UserRole+115;
QString panelItemName(const QTreeWidgetItem *);
QString officialPanelName(const QString &name);
QString officialPanelSize(quint64 size);
QString panelPropertyText(const ArchiveProperty &,int precision,bool utc);
