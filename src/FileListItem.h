// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once
#include <QTreeWidget>
#include <QHeaderView>
#include "PanelSort.h"
#include "PanelDisplay.h"

class FileItem : public QTreeWidgetItem {
public:
    using QTreeWidgetItem::QTreeWidgetItem;
    PanelSortItem sortInfo;
    void setDeferredProperties(const ArchivePropertyList &values,std::shared_ptr<const QList<ArchivePropertyDefinition>> columns,int precision,bool utc) {
        properties=values; displayCache.clear(); definitions=std::move(columns); timePrecision=precision; showUTC=utc;
    }
    QVariant data(int column,int role) const override {
        auto result=QTreeWidgetItem::data(column,role);
        if(role!=Qt::DisplayRole || result.isValid() || !definitions || column<0 || column>=definitions->size()) return result;
        if (displayCache.contains(column)) return displayCache.value(column);
        const auto &definition=(*definitions)[column];
        for(const auto &property:properties) if(property.id==definition.id && property.raw==definition.raw) { const auto text=panelPropertyText(property,timePrecision,showUTC); displayCache.insert(column,text); return text; }
        return {};
    }
    void setSortProperties(const ArchivePropertyList &values, qint64 order, QString prefix = {}, bool archive = false) {
        QString name=panelItemName(this); for(const auto &property:values) if(!data(0,PanelNameRole).isValid() && property.id==OfficialSort::kpidName && !property.raw) { name=property.value; break; }
        sortInfo = panelSortItem(name, prefix, values, order, data(0, Qt::UserRole + 1).toBool(), archive);
        sortInfo.parent = data(0, Qt::UserRole + 2).toBool();
    }
    void updateSortProperty(const ArchiveProperty &property) {
        const auto value = panelSortItem({}, {}, {property}, 0, false, false);
        sortInfo.values.insert((quint64(property.id)<<1)|quint64(property.raw), value.values.value((quint64(property.id)<<1)|quint64(property.raw)));
    }
    bool operator<(const QTreeWidgetItem &other) const override {
        if (this == &other) return false;
        const bool asc = treeWidget()->header()->sortIndicatorOrder() == Qt::AscendingOrder;
        const auto item = dynamic_cast<const FileItem *>(&other);
        if (!item) return QTreeWidgetItem::operator<(other);
        for (const auto role : {Qt::UserRole+2,Qt::UserRole+1}) {
            const auto a=data(0,role).toBool(), b=other.data(0,role).toBool();
            if (a!=b) return asc ? a : b;
        }
        const int column=treeWidget()->sortColumn();
        const auto property=treeWidget()->property("virtualSortProperty").isValid() ? treeWidget()->property("virtualSortProperty").toUInt() : treeWidget()->headerItem()->data(column,ColumnPropertyRole).toUInt();
        const bool raw=treeWidget()->headerItem()->data(column,ColumnRawRole).toBool();
        const auto order=comparePanelItems(sortInfo,item->sortInfo,{property,asc,raw});
        // Qt reverses operator< for descending order; the original comparator
        // has already applied its direction while keeping parent/folders first.
        return asc ? order<0 : order>0;
    }
private:
    ArchivePropertyList properties;
    mutable QHash<int,QString> displayCache;
    std::shared_ptr<const QList<ArchivePropertyDefinition>> definitions;
    int timePrecision=1;
    bool showUTC=false;
};
