// SPDX-License-Identifier: LGPL-3.0-or-later
#include "CompressionOptionsText.h"
#include "ArchiveBackend.h"
#include "ArchiveFormats.h"
namespace {
using UInt32 = quint32;
struct AString {
    QString value;
    void Add_OptSpaced(const char *text) { if (!value.isEmpty()) value += ' '; value += QString::fromLatin1(text); }
    void Add_UInt32(UInt32 n) { value += QString::number(n); }
    AString &operator+=(const char *text) { value += QString::fromLatin1(text); return *this; }
};
QString GetUnicodeString(const AString &text) { return text.value; }
struct CBoolPair { bool Def = false, Val = false; };
struct CBool1 { bool Supported = false, Val = false; };
namespace NCompression {
struct CFormatOptions {
    int TimePrec = -1;
    CBoolPair MTime, CTime, ATime, SetArcMTime;
    bool IsSet_TimePrec() const { return TimePrec >= 0; }
};
}
struct CCompressDialog {
    NCompression::CFormatOptions options;
    CBool1 SymLinks, HardLinks, AltStreams, NtSecurity;
    QString summary;
    NCompression::CFormatOptions &Get_FormatOptions() { return options; }
    void SetItemText(int, QString text) { summary = text; }
    void ShowOptionsString();
};
constexpr int IDT_COMPRESS_OPTIONS = 141;
#include "upstream/CompressOptionsText.inc"
}
QString compressionOptionsText(const ArchiveRequest &request, const QString &format) {
    CCompressDialog dialog;
    dialog.options.TimePrec = request.timestampPrecision;
    dialog.options.MTime = {request.modificationTime >= 0, request.modificationTime != 0};
    dialog.options.CTime = {request.creationTime >= 0, request.creationTime != 0};
    dialog.options.ATime = {request.accessTime >= 0, request.accessTime != 0};
    dialog.options.SetArcMTime = {request.latestArchiveTimeSpecified, request.latestArchiveTime};
    const auto caps = ArchiveFormats::find(format);
    dialog.SymLinks = {caps.symbolicLinks, request.storeSymbolicLinks}; dialog.HardLinks = {caps.hardLinks, request.storeHardLinks};
    dialog.ShowOptionsString(); return dialog.summary;
}
