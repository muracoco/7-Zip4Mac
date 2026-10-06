// SPDX-License-Identifier: LGPL-3.0-or-later
// Qt data adapter around the original App.cpp summary routines.
#include "CopyDialog.h"
#include "PanelDisplay.h"
#include "UiLanguage.h"
#include <QFileInfo>
namespace {
using UInt64 = quint64; using UInt32 = quint32; using Int64 = qint64; using UINT = unsigned; using PROPID = unsigned;
constexpr unsigned IDS_PROP_FOLDERS = 1031, IDS_PROP_FILES = 1032, IDS_PROP_SIZE = 1007, IDS_FILE_SIZE = 3504, kpidSize = 7;
constexpr int kCopyDialog_NumInfoLines = 11;
struct UString {
    QString value;
    UString() = default; UString(QString value) : value(std::move(value)) {} UString(const wchar_t *value) : value(QString::fromWCharArray(value)) {}
    void Add_LF() { value += '\n'; } void Add_PathSepar() { value += '/'; }
    UString &operator+=(const char *text) { value += QString::fromUtf8(text); return *this; }
    UString &operator+=(const UString &other) { value += other.value; return *this; }
};
template<class T> struct CRecordVector : QList<T> { unsigned Size() const { return unsigned(this->size()); } };
UString ConvertSizeToString(UInt64 value) { return officialPanelSize(value); }
UString MyFormatNew(unsigned id, const UString &value) { return UiLanguage::resource(id).replace("{0}", value.value); }
void AddLangString(UString &value, unsigned id) { value.value += UiLanguage::resource(id); }
void AddPropValueToSum(const QList<CopySummaryItem> *folder, UInt32 index, PROPID, UInt64 &sum) {
    if (sum == quint64(-1)) return; const auto &size = folder->at(index).size; if (size) sum += *size; else sum = quint64(-1);
}
struct CPanel {
    const QList<CopySummaryItem> *_folder;
    UString _currentFolderPrefix;
    bool IsItem_Folder(unsigned index) const { return _folder->at(index).directory; }
    UString GetItemRelPath(unsigned index) const { return _folder->at(index).name; }
    UString GetItemsInfoString(const CRecordVector<UInt32> &);
};
#include "upstream/CopyInfo.inc"
}
QString copyItemsInfo(const QList<CopySummaryItem> &items, const QString &folder) {
    CPanel panel{&items, folder}; CRecordVector<UInt32> indices; for (qsizetype i = 0; i < items.size(); ++i) indices.append(UInt32(i)); return panel.GetItemsInfoString(indices).value;
}
QString combineItemsInfo(const QStringList &volumes, quint64 size, const QString &folder) {
    UString info; AddValuePair2(info, IDS_PROP_FILES, volumes.size(), size); info.Add_LF(); info.value += folder;
    qsizetype i = 0; for (; i < volumes.size() && i < 2; ++i) AddInfoFileName(info, UString(QFileInfo(volumes[i]).fileName()));
    if (i != volumes.size()) { if (i + 1 != volumes.size()) AddInfoFileName(info, UString(L"...")); AddInfoFileName(info, UString(QFileInfo(volumes.last()).fileName())); }
    return info.value;
}
