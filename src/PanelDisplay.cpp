// SPDX-License-Identifier: LGPL-3.0-or-later
#include "PanelDisplay.h"
#include "PanelSort.h"
#include "TimeText.h"
#include <QDateTime>
#include <QTimeZone>
#include <QTreeWidget>
#include <vector>
namespace {
using namespace OfficialSort;
using Byte=unsigned char; using UInt32=quint32; using UInt64=quint64; using UINT=quint32;
#include "upstream/PanelDisplay.inc"
#undef INT_TO_STR_SPEC
int copyName(const wchar_t *name,wchar_t *text,unsigned capacity) {
    struct { unsigned cchTextMax; } item{capacity};
#define SPACE_REPLACE_CHAR (wchar_t)(0x2423)
#define SPACE_TERMINATOR_CHAR (wchar_t)(0x9C)
    // The original Windows display markers are text policy, not an OS API.
#define _WIN32
#include "upstream/PanelNameDisplay.inc"
#undef _WIN32
#undef SPACE_REPLACE_CHAR
#undef SPACE_TERMINATOR_CHAR
}
QString fromUnits(const wchar_t *units) {
    QString result; for(;*units;++units) result+=QChar(char16_t(*units)); return result;
}
}
QString panelItemName(const QTreeWidgetItem *item) {
    const auto original=item->data(0,PanelNameRole); return original.isValid()?original.toString():item->text(0);
}
QString officialPanelName(const QString &name) {
    std::vector<wchar_t> input; input.reserve(size_t(name.size()+1));
    for(const auto unit:name) input.push_back(wchar_t(unit.unicode())); input.push_back(0);
    std::vector<wchar_t> output(size_t(name.size()+8)); copyName(input.data(),output.data(),unsigned(output.size())); return fromUnits(output.data());
}
QString officialPanelSize(quint64 size) { wchar_t output[32]; ConvertSizeToString(size,output); return fromUnits(output); }
QString panelPropertyText(const ArchiveProperty &property,int precision,bool utc) {
    if(property.retrievalError) return "Error: ";
    if(property.raw) return property.listValue;
    if(property.id==kpidPrefix) return property.value;
    if((property.type==21 || property.type==19 || property.type==18) && IsSizeProp(property.id) && !property.number.isEmpty()) return officialPanelSize(property.number.toULongLong());
    if(property.type==64 && !property.fileTime.isEmpty()) {
        constexpr quint64 epoch=116444736000000000ULL; const auto ticks=property.fileTime.toULongLong();
        auto time=QDateTime::fromMSecsSinceEpoch(ticks>=epoch?qint64((ticks-epoch)/10000):-qint64((epoch-ticks+9999)/10000),QTimeZone::UTC);
        const auto fraction=property.timeFraction.isEmpty()?property.value.mid(20):property.timeFraction;
        auto display=officialTimeText(time,fraction,precision,utc);
        if(property.native) {
            const auto point=display.indexOf('.');
            if(point>=0) { const bool suffix=display.endsWith('Z'); display.truncate(point+(fraction.isEmpty()?0:1+qMin(display.size()-point-1-int(suffix),fraction.size()))); if(suffix) display+='Z'; }
        }
        return display;
    }
    auto text=property.value; text.replace('\r',' '); text.replace('\n',' '); return text;
}
