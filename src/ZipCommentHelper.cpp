// SPDX-License-Identifier: LGPL-3.0-or-later
// A narrow frontend to the unmodified official 7-Zip ZIP handler. MainAr.o
// supplies main(); archive serialization/codecs/crypto remain upstream code.
#include "CPP/Common/MyInitGuid.h"
#include "CPP/Common/MyCom.h"
#include "CPP/Common/StringConvert.h"
#include "CPP/Common/UTFConvert.h"
#include "CPP/Windows/PropVariant.h"
#include "CPP/7zip/Archive/Zip/ZipHandler.h"
#include "NativeZipMetadata.h"
#include "CPP/7zip/Common/FileStreams.h"
#include "CPP/7zip/IPassword.h"
#include "CPP/7zip/UI/Common/ArchiveExtractCallback.h"
#include <cstdio>
#include <string>
#include <fstream>
#include <iterator>

namespace {
using NWindows::NCOM::CPropVariant;
std::string utf8(const UString &s) { AString a; ConvertUnicodeToUTF8(s, a); return std::string(a.Ptr(), a.Len()); }
bool unicode(const char *s, UString &value) { return ConvertUTF8ToUnicode(AString(s), value); }
std::string quoted(const std::string &s) {
    std::string out = "\"";
    for (const char value : s) { const auto c = static_cast<unsigned char>(value); if (c == '\\' || c == '"') { out += '\\'; out += char(c); } else if (c < 32) { char escaped[7]; std::snprintf(escaped, sizeof(escaped), "\\u%04x", c); out += escaped; } else out += char(c); }
    return out + '"';
}
UString stringProperty(IInArchive *archive, UInt32 index, PROPID id) {
    CPropVariant p; if (archive->GetProperty(index, id, &p) != S_OK || p.vt != VT_BSTR) return UString(); return UString(p.bstrVal);
}
int fail(const char *message, HRESULT code = E_FAIL) { std::fprintf(stderr, "%s (HRESULT 0x%08x)\n", message, unsigned(code)); return 2; }
class Callback final : public IArchiveUpdateCallback, public CMyUnknownImp {
    Z7_COM_UNKNOWN_IMP_1(IArchiveUpdateCallback)
    Z7_IFACE_COM7_IMP(IProgress)
    Z7_IFACE_COM7_IMP(IArchiveUpdateCallback)
public:
    CMyComPtr<IInArchive> archive;
    UInt32 target = 0;
    UString comment;
};
Z7_COM7F_IMF(Callback::SetTotal(UInt64)) { return S_OK; }
Z7_COM7F_IMF(Callback::SetCompleted(const UInt64 *)) { return S_OK; }
Z7_COM7F_IMF(Callback::GetUpdateItemInfo(UInt32 index, Int32 *data, Int32 *props, UInt32 *inArchive)) { *data = 0; *props = index == target; *inArchive = index; return S_OK; }
Z7_COM7F_IMF(Callback::GetProperty(UInt32 index, PROPID id, PROPVARIANT *value)) {
    if (index == target && id == kpidComment) { CPropVariant p(comment); p.Detach(value); return S_OK; }
    return archive->GetProperty(index, id, value);
}
Z7_COM7F_IMF(Callback::GetStream(UInt32, ISequentialInStream **stream)) { *stream = nullptr; return E_NOTIMPL; }
Z7_COM7F_IMF(Callback::SetOperationResult(Int32)) { return S_OK; }
int metadata(IInArchive *archive, UInt32 count) {
    std::string result = "[";
    for (UInt32 i = 0; i < count; ++i) {
        if (i) result += ',';
        result += "{\"path\":" + quoted(utf8(stringProperty(archive, i, kpidPath))) + ",\"comment\":" + quoted(utf8(stringProperty(archive, i, kpidComment))) + '}';
    }
    result += "]\n";
    return std::fwrite(result.data(), 1, result.size(), stdout) == result.size() ? 0 : fail("Cannot write ZIP metadata");
}
}
int Main2(int argc, char **argv) {
    MY_SetLocale();
    const bool reading = argc == 3 && std::string(argv[1]) == "read";
    const bool writing = argc == 6 && std::string(argv[1]) == "write";
    if (!reading && !writing) return fail("Usage: 7zip-comment read archive | write archive output entry comment-file");
    CMyComPtr2_Create<IInStream, CInFileStream> input;
    if (!input->Open(argv[2])) return fail("Cannot open ZIP input");
    CMyComPtr2_Create<IInArchive, NArchive::NZip::CHandler> archive;
    IInArchive *reader = archive;
    auto status = reader->Open(input, nullptr, nullptr); if (status != S_OK) return fail("Cannot open ZIP archive", status);
    UInt32 count = 0; if (reader->GetNumberOfItems(&count) != S_OK) return fail("Cannot read ZIP items");
    if (reading) return metadata(archive, count);
    UString entry, comment;
    if (!unicode(argv[4], entry)) return fail("ZIP item is not UTF-8");
    std::ifstream source(argv[5], std::ios::binary); if (!source) return fail("Cannot read comment input");
    std::string bytes((std::istreambuf_iterator<char>(source)), std::istreambuf_iterator<char>());
    if (bytes.size() > 65535 || bytes.find('\0') != std::string::npos || !unicode(bytes.c_str(), comment)) return fail("Invalid UTF-8 or oversized comment");
    UInt32 target = count; unsigned found = 0;
    for (UInt32 i = 0; i < count; ++i) if (stringProperty(archive, i, kpidPath) == entry) { target = i; ++found; }
    if (found != 1) return fail("Comment target must be one real ZIP entry");
    CMyComPtr<IOutArchive> writer; status = reader->QueryInterface(IID_IOutArchive, reinterpret_cast<void **>(&writer));
    if (status != S_OK || !writer) return fail("ZIP archive is not updateable", status);
    status = retainZipMetadataTimes(reader); if (status != S_OK) return fail("Cannot retain ZIP metadata timestamps", status);
    CMyComPtr2_Create<IOutStream, COutFileStream> output;
    if (!output->Create_NEW(argv[3])) return fail("Cannot create ZIP comment output");
    CMyComPtr2_Create<IArchiveUpdateCallback, Callback> callback; callback->archive = archive; callback->target = target; callback->comment = comment;
    status = writer->UpdateItems(output, count, callback); const auto closed = output->Close();
    if (status != S_OK || closed != S_OK) return fail("ZIP comment update failed", status != S_OK ? status : closed);
    return 0;
}
