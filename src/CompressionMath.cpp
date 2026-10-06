// SPDX-License-Identifier: LGPL-3.0-or-later
#include "CompressionMath.h"
#include <QRegularExpression>
#include <QStringList>
#include <sys/sysctl.h>
#include <cstddef>
#include <cstdint>

namespace {
// Adapt only the GUI accessors around unchanged official arithmetic bodies.
using UInt64 = uint64_t; using UInt32 = uint32_t; using Int64 = int64_t; using Int32 = int32_t;
constexpr UInt32 kLzmaMaxDictSize = UInt32(15) << 28;
constexpr UInt32 kSolidLog_NoSolid = 0, kSolidLog_FullSolid = 64;
using LPCSTR = const char *;
#define Z7_ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))
#include "upstream/CompressTables.inc"
unsigned formatIndex(const QString &format) {
    for (unsigned n = 0; n < Z7_ARRAY_SIZE(g_Formats); ++n)
        if (format.compare(QString::fromLatin1(g_Formats[n].Name), Qt::CaseInsensitive) == 0) return n;
    return 0;
}
struct NumericCombo {
    CompressionMath::NumericChoices choices;
    void ResetContent() { choices = {}; }
    void SetCurSel(int index) { choices.selected = index; }
    int GetCount() const { return choices.items.size(); }
    int add(QString text, UInt64 value, bool automatic = false) {
        choices.items.append({std::move(text), value, automatic}); return GetCount() - 1;
    }
};
constexpr size_t k_Auto_Dict = size_t(-1);
struct CArcInfoEx { bool Is_Zstd() const { return false; } };
class CCompressDialog {
public:
    CompressionMath::Input input;
    QString format;
    bool _ramSize_Defined;
    UInt32 _auto_Dict = UInt32(-1), _auto_Order = 1;
    NumericCombo m_Dictionary, m_Order;
    explicit CCompressDialog(CompressionMath::Input value) : input(value), format(value.format), _ramSize_Defined(value.ram != 0) {}
    UInt32 GetLevel2() const { return UInt32(input.level); }
    int GetMethodID() const {
        if (input.level == 0 && input.format != "tar" && input.format != "Hash") return -1;
        if (input.method == "LZMA") return kLZMA;
        if (input.method == "LZMA2") return kLZMA2;
        if (input.method == "PPMd") return IsZipFormat() ? kPPMdZip : kPPMd;
        if (input.method == "BZip2") return kBZip2;
        if (input.method == "Deflate") return kDeflate;
        if (input.method == "Deflate64") return kDeflate64;
        for (unsigned n = 0; n < Z7_ARRAY_SIZE(kMethodsNames); ++n)
            if (input.method.compare(QString::fromLatin1(kMethodsNames[n]), Qt::CaseInsensitive) == 0) return int(n);
        return -1;
    }
    int GetStaticFormatIndex() const { return int(formatIndex(format)); }
    CArcInfoEx Get_ArcInfoEx() const { return {}; }
    bool IsZipFormat() const { return format == "zip"; }
    bool IsXzFormat() const { return format == "xz"; }
    UInt64 GetDict2() const { return input.dictionary; }
    UInt32 GetBlockSizeSpec() const {
        if (input.solid == CompressionMath::Automatic) return UInt32(-1);
        if (input.solid == 0) return kSolidLog_NoSolid;
        if (input.solid == UInt64(-2)) return kSolidLog_FullSolid;
        UInt32 bits = 0; for (auto value = input.solid; value > 1; value >>= 1) ++bits; return bits;
    }
    UInt64 Get_MemUse_Bytes() const { return input.memoryLimit; }
    UInt64 GetMemoryUsage_Threads_Dict_DecompMem(UInt32, UInt64, UInt64 &);
    UInt32 AutoThreads(UInt32 numCPUs, UInt32 numHardwareThreads);
    UInt32 MaximumThreads(UInt32 numHardwareThreads);
    UInt64 AutoSolid(UInt64 dict);
    void SetDictionary2(UInt32 defaultDict);
    void SetOrder2(UInt32 defaultOrder);
    int AddDict2(size_t real, size_t shown) {
        const bool automatic = real == k_Auto_Dict;
        return m_Dictionary.add((automatic ? "*  " : "") + CompressionMath::sizeText(shown), automatic ? CompressionMath::Automatic : real, automatic);
    }
    int AddDict(size_t size) { return AddDict2(size, size); }
    int AddOrder(UInt32 size) { return m_Order.add(QString::number(size), size); }
    int AddOrder_Auto() { return m_Order.add("*  " + QString::number(_auto_Order), CompressionMath::Automatic, true); }
};
#include "upstream/CompressControls.inc"
#include "upstream/CompressMath.inc"
}
quint64 CompressionMath::systemRam() { uint64_t size = 0; size_t length = sizeof(size); return ::sysctlbyname("hw.memsize", &size, &length, nullptr, 0) == 0 ? size : 0; }
quint64 CompressionMath::parseSize(QString text) {
    text.remove(' '); static const QRegularExpression pattern("^([0-9]+)([KMGT]?)(?:B)?$", QRegularExpression::CaseInsensitiveOption); const auto match = pattern.match(text); if (!match.hasMatch()) return Automatic;
    bool ok; const auto value = match.captured(1).toULongLong(&ok); unsigned shift = QString(" KMGT").indexOf(match.captured(2).toUpper()); if (match.captured(2).isEmpty()) shift = 0; else shift *= 10;
    if (!ok || value > (Automatic >> shift)) return Automatic; return value << shift;
}
QString CompressionMath::sizeText(quint64 bytes, bool rounded) {
    if (bytes == Automatic) return "—";
    if (rounded) { unsigned shift = bytes <= (quint64(16) << 30) ? 20 : bytes <= (quint64(64) << 40) ? 30 : 40; return QString::number((bytes >> shift) + bool(bytes & ((quint64(1) << shift) - 1))) + (shift == 20 ? " MB" : shift == 30 ? " GB" : " TB"); }
    unsigned shift = bytes && !(bytes & ((quint64(1) << 30) - 1)) ? 30 : bytes && !(bytes & ((1 << 20) - 1)) ? 20 : bytes && !(bytes & 1023) ? 10 : 0;
    return QString::number(bytes >> shift) + (shift == 30 ? " GB" : shift == 20 ? " MB" : shift == 10 ? " KB" : " B");
}
CompressionMath::Result CompressionMath::calculate(Input input) {
    Result result; input.level = qBound(0, input.level, 9); input.cpus = qMax(1, input.cpus);
    CCompressDialog defaults(input); defaults.SetDictionary2(UInt32(-1)); defaults.SetOrder2(UInt32(-1));
    if (input.dictionary == Automatic) {
        input.dictionary = defaults._auto_Dict == UInt32(-1) ? Automatic : defaults._auto_Dict;
    }
    result.word = defaults._auto_Order;
    const bool multithread = g_Formats[formatIndex(input.format)].MultiThread_();
    input.memoryLimit = input.memoryLimit ? input.memoryLimit : qMax<quint64>(1 << 26, input.ram ? input.ram : quint64(sizeof(size_t)) << 29) / 100 * 80;
    CCompressDialog adapter(input); result.maximumThreads = multithread ? adapter.MaximumThreads(input.cpus) : 1;
    result.dictionary = input.dictionary; result.solid = input.level && hasSolidControl(input.format) ? adapter.AutoSolid(input.dictionary == Automatic ? 1 << 25 : input.dictionary) : 1 << 20;
    result.threads = input.threads > 0 ? unsigned(input.threads) : multithread ? adapter.AutoThreads(input.cpus, input.cpus) : 1;
    UInt64 decompress; result.compressMemory = adapter.GetMemoryUsage_Threads_Dict_DecompMem(result.threads, input.dictionary, decompress); result.decompressMemory = decompress;
    return result;
}
QList<quint64> CompressionMath::dictionaries(QString method, QString format) {
    QList<quint64> result;
    Input input; input.method = method; input.format = format;
    for (auto choice : dictionaryChoices(input).items) if (!choice.automatic) result << choice.value;
    return result;
}
QList<unsigned> CompressionMath::words(QString method, QString format) {
    QList<unsigned> result;
    Input input; input.method = method; input.format = format;
    for (auto choice : wordChoices(input).items) if (!choice.automatic) result << unsigned(choice.value);
    return result;
}
quint32 CompressionMath::levelNameId(int level) { return level >= 0 && unsigned(level) < Z7_ARRAY_SIZE(g_Levels) ? g_Levels[level] : 0; }
QList<int> CompressionMath::levels(QString format) {
    QList<int> result; const auto mask = g_Formats[formatIndex(format)].LevelsMask;
    for (unsigned n = 0; n < 32; ++n) if ((mask >> n) & 1) result << int(n);
    return result;
}
QStringList CompressionMath::methods(QString format, int level) {
    QStringList result; if (level == 0 && format != "tar" && format != "Hash") return result;
    const auto &info = g_Formats[formatIndex(format)];
    for (unsigned n = 0; n < info.NumMethods; ++n) {
        const auto id = info.MethodIDs[n];
        if (format == "7z" && (id == kCopy || id == kDeflate || id == kDeflate64)) continue;
        result << QString::fromLatin1(kMethodsNames[id]);
    }
    return result;
}
CompressionMath::NumericChoices CompressionMath::dictionaryChoices(Input input, quint64 saved) {
    CCompressDialog adapter(input); const UInt32 stored = saved == Automatic ? UInt32(-1) : saved > UInt32(-1) ? UInt32(-2) : UInt32(saved);
    adapter.SetDictionary2(stored); return adapter.m_Dictionary.choices;
}
CompressionMath::NumericChoices CompressionMath::wordChoices(Input input, quint32 saved) {
    CCompressDialog adapter(input); adapter.SetOrder2(saved); return adapter.m_Order.choices;
}
QList<unsigned> CompressionMath::threadChoices(Input input) {
    QList<unsigned> result; if (!g_Formats[formatIndex(input.format)].MultiThread_()) return result;
    input.threads = 0;
    const auto calculated = calculate(input);
    if (calculated.maximumThreads != calculated.threads || calculated.threads != 1)
        for (unsigned n = 1; n <= unsigned(qMax(1, input.cpus)) * 2 && n <= calculated.maximumThreads; ++n) result << n;
    return result;
}
bool CompressionMath::hasMemoryControl(QString format) { return g_Formats[formatIndex(format)].MemUse_(); }
bool CompressionMath::hasSolidControl(QString format) { return g_Formats[formatIndex(format)].Solid_(); }
