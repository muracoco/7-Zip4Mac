// SPDX-License-Identifier: LGPL-3.0-or-later
// Adapter around official Bench/IBenchCallback and imported Windows GUI math.
#include "NativeBenchmark.h"
#include "NativeProgress.h"
#include "CPP/7zip/UI/Console/ConsoleClose.h"
#include "CPP/Common/IntToString.h"
#include "CPP/Common/UTFConvert.h"
#include "CPP/Windows/SystemInfo.h"
#include "CPP/7zip/MyVersion.h"
#include <chrono>
#include <cerrno>
#include <cstdlib>
#include <mutex>
#include <vector>

namespace {
#include "upstream/BenchmarkGuiMath.inc"
using Clock = std::chrono::steady_clock;
std::string quoted(const std::string &value) {
    std::string result = "\"";
    for (unsigned char c : value) {
        if (c == '"' || c == '\\') { result += '\\'; result += char(c); }
        else if (c < 32) { const char *hex = "0123456789abcdef"; result += "\\u00"; result += hex[c >> 4]; result += hex[c & 15]; }
        else result += char(c);
    }
    return result + '"';
}
std::string utf8(const UString &value) { AString result; ConvertUnicodeToUTF8(value, result); return {result.Ptr(), result.Len()}; }
std::string ratingText(UInt64 value) { WCHAR text[64]; NumberToDot3(GetMips(value), text); return utf8(UString(text)) + " GIPS"; }
struct Pass { CTotalBenchRes enc, dec; };
class Callback final : public IBenchCallback, public IBenchFreqCallback {
    std::mutex mutex;
    Clock::time_point sent{};
    CTotalBenchRes2 encCurrent, decCurrent, encResult, decResult;
    std::vector<Pass> history;
    int deletedIndex = -1;
    UString frequency;
    std::string system;
    unsigned previousFreqThreads = 0;
    UInt64 dictionary;
    UInt32 completed = 0;
    bool finished = false;
    std::string log() const {
        UString text = frequency;
        if (!history.empty()) {
            if (!text.IsEmpty()) text.Add_LF();
            text += "Compr Decompr Total   CPU"; text.Add_LF();
            for (unsigned i = 0; i < history.size(); ++i) {
                if (i) text.Add_LF();
                if (int(i) == deletedIndex) { text += "..."; text.Add_LF(); }
                AddRatingsLine(text, history[i].enc, history[i].dec);
            }
        }
        return utf8(text);
    }
    void send(bool force) {
        const auto now = Clock::now();
        if (!force && now - sent < std::chrono::milliseconds(200)) return;
        CTotalBenchRes total = encResult; total.Update_With_Res(decResult);
        const std::string value = "{\"version\":1,\"mode\":\"benchmark\",\"completed\":" + quoted(std::to_string(completed)) +
            ",\"finished\":" + (finished ? "true" : "false") +
            ",\"encCurrent\":" + PortBenchmark::ResultValue(encCurrent, encCurrent.UnpackSize) +
            ",\"decCurrent\":" + PortBenchmark::ResultValue(decCurrent, decCurrent.UnpackSize) +
            ",\"encResult\":" + PortBenchmark::ResultValue(encResult, encResult.UnpackSize) +
            ",\"decResult\":" + PortBenchmark::ResultValue(decResult, decResult.UnpackSize) +
            ",\"total\":" + PortBenchmark::ResultValue(total, encResult.UnpackSize + decResult.UnpackSize) +
            ",\"log\":" + quoted(log()) + ",\"system\":" + system + '}';
        PortProgress::Record(value); sent = now;
    }
public:
    explicit Callback(UInt64 size) : dictionary(size) {
        encCurrent.Init(); decCurrent.Init(); encResult.Init(); decResult.Init();
        AString cpu, registers, first, second, features;
        GetCpuName_MultiLine(cpu, registers); GetSysInfo(first, second);
        GetOsInfoText(features); features += " : "; AddCpuFeatures(features);
        NWindows::NSystem::CProcessAffinity affinity; affinity.InitST();
        // The Win32 convenience method is absent in upstream's POSIX class.
        UInt32 processThreads = affinity.Get() ? affinity.GetNumProcessThreads() : 0;
        if (!processThreads) processThreads = NWindows::NSystem::GetNumberOfProcessors();
        if (!processThreads) processThreads = 1;
        AString hardware("/ "); hardware.Add_UInt32(processThreads); hardware += GetProcessThreadsInfo(affinity);
        auto text = [](const AString &value) { return quoted(std::string(value.Ptr(), value.Len())); };
        system = "{\"cpu\":" + text(cpu) + ",\"features\":" + text(features) +
            ",\"first\":" + text(first) + ",\"second\":" + text(second) +
            ",\"hardware\":" + text(hardware) + ",\"version\":" + quoted("7-Zip " MY_VERSION_CPU) + '}';
        send(true);
    }
    HRESULT SetEncodeResult(const CBenchInfo &info, bool final) override {
        std::lock_guard<std::mutex> lock(mutex);
        if (NConsoleClose::TestBreakSignal()) return E_ABORT;
        UInt64 size = dictionary;
        if (!final && size > info.UnpackSize) size = info.UnpackSize;
        encCurrent.Rating = info.GetRating_LzmaEnc(size);
        encCurrent.SetFrom_BenchInfo(info);
        if (final) encResult.Update_With_Res2(encCurrent);
        send(final); return S_OK;
    }
    HRESULT SetDecodeResult(const CBenchInfo &info, bool final) override {
        std::lock_guard<std::mutex> lock(mutex);
        if (NConsoleClose::TestBreakSignal()) return E_ABORT;
        decCurrent.Rating = info.GetRating_LzmaDec();
        decCurrent.SetFrom_BenchInfo(info);
        if (final) decResult.Update_With_Res2(decCurrent);
        send(final); return S_OK;
    }
    HRESULT AddCpuFreq(unsigned threads, UInt64 freq, UInt64 usage) override {
        std::lock_guard<std::mutex> lock(mutex);
        if (previousFreqThreads != threads) {
            previousFreqThreads = threads;
            if (!frequency.IsEmpty()) frequency.Add_LF();
            frequency.Add_UInt32(threads); frequency += "T Frequency (MHz):"; frequency.Add_LF();
        }
        frequency.Add_Space();
        if (threads != 1) { frequency.Add_UInt64(GetUsagePercents(usage)); frequency.Add_Char('%'); frequency.Add_Space(); }
        frequency.Add_UInt64(GetMips(freq));
        return NConsoleClose::TestBreakSignal() ? E_ABORT : S_OK;
    }
    HRESULT FreqsFinished(unsigned) override {
        std::lock_guard<std::mutex> lock(mutex); send(true);
        return NConsoleClose::TestBreakSignal() ? E_ABORT : S_OK;
    }
    void passFinished(bool last) {
        std::lock_guard<std::mutex> lock(mutex);
        ++completed; finished = last;
        history.push_back({encCurrent, decCurrent});
        if (history.size() > 20) { deletedIndex = 5; history.erase(history.begin() + deletedIndex); }
        send(true);
    }
};
}
bool PortBenchmark::Requested() { return std::getenv("SEVENZIP_PORT_BENCHMARK_DICT") != nullptr; }
std::string PortBenchmark::ResultValue(const CTotalBenchRes &result, UInt64 size) {
    if (!result.NumIterations2) return "{\"defined\":false}";
    const UInt64 n = result.NumIterations2;
    const auto speed = (result.Speed >> 10) / n;
    const auto usage = GetUsagePercents(result.Usage / n);
    const auto rating = GetMips(result.Rating / n), rpu = GetMips(result.RPU / n);
    const bool gb = size >= (UInt64(1) << 40);
    return "{\"defined\":true,\"speed\":" + quoted(std::to_string(speed)) +
        ",\"usage\":" + quoted(std::to_string(usage)) + ",\"rating\":" + quoted(std::to_string(rating)) +
        ",\"rpu\":" + quoted(std::to_string(rpu)) + ",\"size\":" + quoted(std::to_string(size)) +
        ",\"speedText\":" + quoted(std::to_string(speed) + " KB/s") +
        ",\"usageText\":" + quoted(std::to_string(usage) + '%') +
        ",\"ratingText\":" + quoted(ratingText(result.Rating / n)) +
        ",\"rpuText\":" + quoted(ratingText(result.RPU / n)) +
        ",\"sizeText\":" + quoted(std::to_string(size >> (gb ? 30 : 20)) + (gb ? " GB" : " MB")) + '}';
}
HRESULT PortBenchmark::Run(DECL_EXTERNAL_CODECS_LOC_VARS const CObjectVector<CProperty> &props, UInt32 passes) {
    const char *text = std::getenv("SEVENZIP_PORT_BENCHMARK_DICT");
    if (!text || !*text || !PortProgress::Enabled() || !passes) return E_INVALIDARG;
    for (const char *p = text; *p; ++p) if (*p < '0' || *p > '9') return E_INVALIDARG;
    errno = 0; char *end = nullptr;
    const UInt64 dictionary = std::strtoull(text, &end, 10);
    if (errno || *end || dictionary < (UInt64(1) << 18) || dictionary > (UInt64(1) << 32) || (dictionary & 1023)) return E_INVALIDARG;
    Callback callback(dictionary);
    for (UInt32 pass = 0; pass < passes; ++pass) {
        if (NConsoleClose::TestBreakSignal()) return E_ABORT;
        // Same single-dictionary, callback-driven call as CThreadBenchmark.
        RINOK(Bench(EXTERNAL_CODECS_LOC_VARS nullptr, &callback, props, 1, false, pass == 0 ? &callback : nullptr))
        callback.passFinished(pass + 1 == passes);
    }
    return S_OK;
}
