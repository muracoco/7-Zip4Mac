// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once
#include "ArchiveBackend.h"
#include "upstream/PanelSortIds.h"
#include <QHash>
#include <QHeaderView>
#include <string>
class QTreeWidget;

constexpr int ColumnPropertyRole = Qt::UserRole + 101, ColumnRawRole = Qt::UserRole + 102, ColumnWidthRole = Qt::UserRole + 103;
struct PanelSortValue {
    quint16 type = 0;
    quint64 unsignedValue = 0;
    qint64 signedValue = 0;
    unsigned nanoseconds = 0;
    std::wstring string;
    QByteArray bytes;
    quint32 rawType = 0, rawSize = 0;
    qint64 retrievalError = 0;
    bool raw = false;
};
struct PanelSortItem {
    std::wstring name, prefix;
    QHash<quint64, PanelSortValue> values;
    qint64 order = 0;
    bool directory = false, parent = false, archive = false;
};
struct PanelSortState { quint32 property = OfficialSort::kpidName; bool ascending = true, raw = false; };
PanelSortItem panelSortItem(QString name, QString prefix, const ArchivePropertyList &values, qint64 order, bool directory, bool archive);
PanelSortState nextOfficialSort(PanelSortState state, quint32 property, bool raw = false);
int comparePanelItems(const PanelSortItem &a, const PanelSortItem &b, PanelSortState state);
int comparePanelNames(const QString &a, const QString &b);
QList<ArchivePropertyDefinition> filesystemColumns(bool flat);
bool officialColumnVisible(quint32 property, bool filesystem);
int officialColumnWidth(quint32 property, quint16 type);
Qt::Alignment officialColumnAlignment(quint32 property, quint16 type);
quint32 officialPosixAttributes(quint32 mode);
QString officialAttributeText(quint32 attributes);
int propertyColumn(const QTreeWidget *files, quint32 property, bool raw = false);

// Qt owns resizing/reordering; a completed ordinary click uses the official
// property sort policy instead of Qt's first-click-always-ascending policy.
class PanelSortHeader : public QHeaderView {
    Q_OBJECT
public:
    explicit PanelSortHeader(QWidget *parent);
signals:
    void propertyClicked(int column);
protected:
    void mousePressEvent(QMouseEvent *) override;
    void mouseReleaseEvent(QMouseEvent *) override;
private:
    QPoint press;
    int pressedColumn = -1;
    int sortSection = 0;
    Qt::SortOrder sortOrder = Qt::AscendingOrder;
    bool resizePress = false;
};
