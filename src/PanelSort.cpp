// SPDX-License-Identifier: LGPL-3.0-or-later
#include "PanelSort.h"
#include <QApplication>
#include <QMouseEvent>
#include <QSignalBlocker>
#include <QTreeWidget>
#include <cstring>
#include <cstdio>
#include <sys/stat.h>

namespace {
using namespace OfficialSort;
using Byte = unsigned char;
using UInt16 = quint16; using UInt32 = quint32; using UInt64 = quint64;
using PROPID = quint32; using VARTYPE = quint16; using LPARAM = qint64;
constexpr VARTYPE VT_EMPTY=0, VT_I2=2, VT_I4=3, VT_BSTR=8, VT_BOOL=11, VT_I1=16, VT_UI1=17, VT_UI2=18, VT_UI4=19, VT_I8=20, VT_UI8=21, VT_INT=22, VT_UINT=23, VT_FILETIME=64;
constexpr int LVCFMT_LEFT=0, LVCFMT_RIGHT=1, LVCFMT_CENTER=2;
constexpr quint32 FILE_ATTRIBUTE_DIRECTORY=0x10, FILE_ATTRIBUTE_ARCHIVE=0x20, FILE_ATTRIBUTE_READONLY=1, FILE_ATTRIBUTE_UNIX_EXTENSION=0x8000;
constexpr unsigned kParentIndex = unsigned(-1), k_PropVar_TimePrec_1ns = 8;
#define CALLBACK
template<class A, class B> int MyCompare(A a, B b) { return a < b ? -1 : a > b ? 1 : 0; }
wchar_t MyCharUpper(wchar_t c) { return QChar(char16_t(c)).toUpper().unicode(); }
UInt16 GetUi16(const Byte *p) { return UInt16(p[0]) | (UInt16(p[1]) << 8); }
int CompareFileTime(const quint64 *a, const quint64 *b) { return MyCompare(*a, *b); }
void ConvertUInt32ToHex8Digits(UInt32 value, char *buffer) { std::snprintf(buffer, 9, "%08X", value); }
std::wstring unicodeUnits(const QString &s) {
    std::wstring value; value.reserve(size_t(s.size()));
    for (const auto c : s) value.push_back(wchar_t(c.unicode()));
    return value;
}
struct CPropVariant {
    VARTYPE vt=0;
    quint64 filetime=0;
    unsigned wReserved1=0, wReserved2=0, wReserved3=0;
    unsigned bVal=0, uiVal=0; quint32 ulVal=0;
    qint16 iVal=0, boolVal=0; qint32 lVal=0;
    struct { qint64 QuadPart=0; } hVal;
    struct { quint64 QuadPart=0; } uhVal;
    int Compare(const CPropVariant &) throw();
#include "upstream/SortTimePrecision.inc"
    explicit CPropVariant(const PanelSortValue &value) {
        vt=value.type; filetime=value.unsignedValue;
        bVal=quint8(value.unsignedValue); uiVal=quint16(value.unsignedValue); ulVal=quint32(value.unsignedValue);
        iVal=qint16(value.signedValue); lVal=qint32(value.signedValue); hVal.QuadPart=value.signedValue; uhVal.QuadPart=value.unsignedValue;
        boolVal=value.unsignedValue ? -1 : 0; wReserved2=value.nanoseconds;
    }
};
struct CPanel;
int CompareItems2(LPARAM, LPARAM, const CPanel *, PROPID, qint32);
struct ListAdapter {
    void SortItems(int (*)(LPARAM, LPARAM, LPARAM), LPARAM) {}
    int GetFocusedItem() const { return 0; }
    void EnsureVisible(int, bool) {}
};
struct CPanel {
    const PanelSortItem *a=nullptr, *b=nullptr;
    PROPID _sortID=kpidName; bool _ascending=true; qint32 _isRawSortProp=0;
    ListAdapter _listView;
    const PanelSortItem &item(unsigned index) const { return index==quint64(a->order) ? *a : *b; }
    bool IsItem_Folder(unsigned index) const { return item(index).directory; }
    void SetSortRawStatus() {}
    void SortItemsWithPropID(PROPID);
};
#include "upstream/PanelSort.inc"

int CompareItems2(LPARAM left, LPARAM right, const CPanel *panel, PROPID property, qint32 raw) {
    if (property==kpidNoProperty) return MyCompare(left,right);
    const auto &a=panel->item(unsigned(left)), &b=panel->item(unsigned(right));
    if (property==kpidName) return CompareFileNames_ForFolderList(a.name.c_str(),b.name.c_str());
    if (property==kpidPrefix) return CompareFileNames_ForFolderList(a.prefix.c_str(),b.prefix.c_str());
    if (property==kpidExtension) {
        auto extension=[](const std::wstring &name) { const auto dot=name.rfind('.'); return name.c_str()+(dot==std::wstring::npos?name.size():dot); };
        return CompareFileNames_ForFolderList(extension(a.name),extension(b.name));
    }
    const auto key=(quint64(property)<<1)|quint64(raw!=0);
    static const PanelSortValue empty;
    const auto first=a.values.constFind(key), second=b.values.constFind(key);
    const auto &p1=first==a.values.cend()?empty:first.value(), &p2=second==b.values.cend()?empty:second.value();
    if (raw) {
        if (p1.retrievalError || p2.retrievalError) return 0;
        if (!p1.rawSize || !p2.rawSize) return MyCompare(p1.rawSize!=0,p2.rawSize!=0);
        if (p1.rawType!=1 || p2.rawType!=1) return 0;
        if (property==kpidNtReparse) return CompareFileNames_Le16(reinterpret_cast<const Byte *>(p1.bytes.constData()),unsigned(p1.bytes.size()),reinterpret_cast<const Byte *>(p2.bytes.constData()),unsigned(p2.bytes.size()));
        const auto common=qMin(p1.bytes.size(),p2.bytes.size());
        const auto result=common ? std::memcmp(p1.bytes.constData(),p2.bytes.constData(),size_t(common)) : 0;
        return result ? (result<0?-1:1) : MyCompare(p1.bytes.size(),p2.bytes.size());
    }
    // The original Agent/FS comparison uses integer zero for these missing
    // counters, and folder totals come from its already imported properties.
    if (property==kpidSize || property==kpidPackSize || property==kpidCRC || property==kpidNumSubDirs || property==kpidNumSubFiles)
        return MyCompare(p1.unsignedValue,p2.unsignedValue);
    if (p1.type!=p2.type) return MyCompare(p1.type,p2.type);
    if (p1.type==VT_BSTR) return MyStringCompareNoCase(p1.string.c_str(),p2.string.c_str());
    return CPropVariant(p1).Compare(CPropVariant(p2));
}
#define FS_SHOW_LINKS_INFO
#include "upstream/FilesystemColumns.inc"
#undef FS_SHOW_LINKS_INFO
}

PanelSortItem panelSortItem(QString name, QString prefix, const ArchivePropertyList &properties, qint64 order, bool directory, bool archive) {
    PanelSortItem result; result.name=unicodeUnits(name); result.prefix=unicodeUnits(prefix); result.order=order; result.directory=directory; result.archive=archive;
    for (const auto &property : properties) {
        PanelSortValue value; value.type=property.type; value.raw=property.raw; value.string=unicodeUnits(property.value);
        value.unsignedValue=property.number.toULongLong(); value.signedValue=property.number.toLongLong();
        if (property.type==VT_BOOL) value.unsignedValue=property.number.isEmpty() ? quint64(property.value=="+" || property.value.compare("true",Qt::CaseInsensitive)==0) : property.number.toULongLong();
        if (property.type==VT_FILETIME) {
            value.unsignedValue=property.fileTime.toULongLong();
            CPropVariant precision(value);
            if (property.timePrecision.size()==3) { precision.wReserved1=property.timePrecision[0]; precision.wReserved2=property.timePrecision[1]; precision.wReserved3=property.timePrecision[2]; }
            value.nanoseconds=precision.Get_Ns100();
        }
        if (property.raw) { value.bytes=property.sortData; value.rawType=property.rawType; value.rawSize=property.rawSize; value.retrievalError=property.retrievalError; }
        result.values.insert((quint64(property.id)<<1)|quint64(property.raw),std::move(value));
    }
    return result;
}
PanelSortState nextOfficialSort(PanelSortState state, quint32 property, bool raw) {
    CPanel panel; panel._sortID=state.property; panel._ascending=state.ascending; panel.SortItemsWithPropID(property);
    return {panel._sortID,panel._ascending,raw};
}
int comparePanelItems(const PanelSortItem &a, const PanelSortItem &b, PanelSortState state) {
    if (&a==&b || (a.parent && b.parent)) return 0;
    CPanel panel; panel.a=&a; panel.b=&b; panel._sortID=state.property; panel._ascending=state.ascending; panel._isRawSortProp=state.raw;
    return CompareItems(a.parent ? -1 : a.order,b.parent ? -1 : b.order,reinterpret_cast<LPARAM>(&panel));
}
int comparePanelNames(const QString &a, const QString &b) { const auto x=unicodeUnits(a), y=unicodeUnits(b); return CompareFileNames_ForFolderList(x.c_str(),y.c_str()); }
QList<ArchivePropertyDefinition> filesystemColumns(bool flat) {
    QList<ArchivePropertyDefinition> result;
    for (const auto property : kProps) {
        if (property==kpidPrefix && !flat) continue;
        const auto type=property==kpidName || property==kpidComment || property==kpidPrefix ? VT_BSTR : property==kpidMTime || property==kpidCTime || property==kpidATime || property==kpidChangeTime ? VT_FILETIME : property==kpidAttrib || property==kpidNumSubDirs || property==kpidNumSubFiles || property==kpidLinks ? VT_UI4 : VT_UI8;
        result.append({property,quint16(type),{},false});
    }
    // The explicitly requested file-kind column remains an additional port
    // column; Arrange Type always uses the original extension property.
    result.append({kpidExtension,VT_BSTR,"Type",false});
    return result;
}
bool officialColumnVisible(quint32 property,bool filesystem) { return GetColumnVisible(property,filesystem); }
int officialColumnWidth(quint32 property,quint16 type) { return int(GetColumnWidth(property,type)); }
Qt::Alignment officialColumnAlignment(quint32 property,quint16 type) { const auto align=GetColumnAlign(property,type); return (align==LVCFMT_RIGHT?Qt::AlignRight:align==LVCFMT_CENTER?Qt::AlignHCenter:Qt::AlignLeft)|Qt::AlignVCenter; }
quint32 officialPosixAttributes(quint32 mode) { return Get_WinAttribPosix_From_PosixMode(mode); }
QString officialAttributeText(quint32 attributes) { char text[128]{}; ConvertWinAttribToString(text,attributes); return QString::fromLatin1(text); }
int propertyColumn(const QTreeWidget *files,quint32 property,bool raw) {
    for (int n=0;n<files->columnCount();++n) if (files->headerItem()->data(n,ColumnPropertyRole).toUInt()==property && files->headerItem()->data(n,ColumnRawRole).toBool()==raw) return n;
    return -1;
}
PanelSortHeader::PanelSortHeader(QWidget *parent) : QHeaderView(Qt::Horizontal,parent) { setSectionsClickable(true); }
void PanelSortHeader::mousePressEvent(QMouseEvent *event) {
    press=event->position().toPoint(); pressedColumn=logicalIndexAt(press);
    sortSection=sortIndicatorSection(); sortOrder=sortIndicatorOrder();
    resizePress=cursor().shape()==Qt::SplitHCursor || cursor().shape()==Qt::SizeHorCursor;
    QHeaderView::mousePressEvent(event);
}
void PanelSortHeader::mouseReleaseEvent(QMouseEvent *event) {
    const bool click=event->button()==Qt::LeftButton && pressedColumn>=0 && logicalIndexAt(event->position().toPoint())==pressedColumn && !resizePress &&
        (event->position().toPoint()-press).manhattanLength()<QApplication::startDragDistance();
    if (click) {
        { QSignalBlocker sortSignals(this); QHeaderView::mouseReleaseEvent(event); setSortIndicator(sortSection,sortOrder); }
        emit propertyClicked(pressedColumn);
    } else QHeaderView::mouseReleaseEvent(event);
    pressedColumn=-1;
}
