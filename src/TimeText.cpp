// SPDX-License-Identifier: LGPL-3.0-or-later
#include "TimeText.h"
#include <QTimeZone>
namespace {
using UInt64 = quint64; using UInt32 = quint32;
struct FILETIME { quint32 dwLowDateTime, dwHighDateTime; };
struct SYSTEMTIME { unsigned wYear, wMonth, wDay, wHour, wMinute, wSecond; };
UInt64 ticks(const FILETIME &time) { return time.dwLowDateTime | (UInt64(time.dwHighDateTime) << 32); }
FILETIME fromTicks(UInt64 time) { return {quint32(time), quint32(time >> 32)}; }
bool BOOLToBool(bool value) { return value; }
bool FileTimeToSystemTime(const FILETIME *time, SYSTEMTIME *system) {
    if (ticks(*time) >> 63) return false;
    const auto date = QDateTime::fromMSecsSinceEpoch(qint64(ticks(*time) / 10000ULL) - 11644473600000LL, QTimeZone::UTC);
    if (!date.isValid() || date.date().year() < 1601) return false;
    *system = {unsigned(date.date().year()), unsigned(date.date().month()), unsigned(date.date().day()), unsigned(date.time().hour()), unsigned(date.time().minute()), unsigned(date.time().second())}; return true;
}
bool FileTimeToLocalFileTime(const FILETIME *utc, FILETIME *local) {
    const auto date = QDateTime::fromMSecsSinceEpoch(qint64(ticks(*utc) / 10000ULL) - 11644473600000LL, QTimeZone::UTC);
    if (!date.isValid()) return false;
    *local = fromTicks(UInt64(qint64(ticks(*utc)) + qint64(date.toLocalTime().offsetFromUtc()) * 10000000LL)); return true;
}
thread_local bool g_Timestamp_Show_UTC = false;
#include "upstream/TimeText.inc"
}
QString officialTimeText(QDateTime time, QString fraction, int precision, bool utc) {
    if (!time.isValid() || time.date().year() < 1601) return {};
    const auto seconds = time.toSecsSinceEpoch();
    if (fraction.isEmpty()) fraction = QString::number(time.time().msec()).rightJustified(3, '0');
    const auto nanoseconds = fraction.leftJustified(9, '0').left(9).toUInt();
    const auto value = fromTicks(UInt64(seconds + 11644473600LL) * 10000000ULL + nanoseconds / 100);
    const int level = precision == 0 ? kTimestampPrintLevel_DAY : precision == 1 ? kTimestampPrintLevel_MIN : precision == 2 ? kTimestampPrintLevel_SEC : precision;
    g_Timestamp_Show_UTC = utc; char output[64];
    if (!ConvertUtcFileTimeToString2(value, nanoseconds % 100, output, level, 0)) return {};
    return QString::fromLatin1(output);
}
QStringList officialTimeMenuSamples(QDateTime now, bool utc) {
    QStringList labels;
    for (const int level : k_TimeLevels) labels << officialTimeText(now, {}, level == kTimestampPrintLevel_DAY ? 0 : level == kTimestampPrintLevel_MIN ? 1 : level == kTimestampPrintLevel_SEC ? 2 : level, utc);
    return labels;
}
