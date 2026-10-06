// SPDX-License-Identifier: LGPL-3.0-or-later
#include "LanguageDocument.h"
#include <QBuffer>
#include <QStringDecoder>
#include <algorithm>
#include <string>
#include <vector>
namespace {
using Byte = unsigned char; using UInt32 = quint32; using UInt64 = quint64; using Int32 = qint32;
using CFSTR = QIODevice *;
#define UINT64_CONST(a) a##ULL
#define FOR_VECTOR(i, v) for (unsigned i = 0; i < (v).Size(); ++i)
struct AString {
    QByteArray value;
    char *GetBuf(unsigned length) { value.resize(qsizetype(length) + 1); return value.data(); }
    void ReleaseBuf_SetLen(unsigned length) { value.resize(length); }
};
struct UString {
    std::wstring value;
    UString() = default;
    UString(const wchar_t *s) : value(s) {}
    UString(const char *s) : value(QString::fromUtf8(s).toStdWString()) {}
    UString(QString s) : value(s.toStdWString()) {}
    bool IsEmpty() const { return value.empty(); }
    void Empty() { value.clear(); }
    unsigned Len() const { return unsigned(value.size()); }
    operator const wchar_t *() const { return value.c_str(); }
    wchar_t operator[](unsigned i) const { return value[i]; }
    UString &operator+=(const UString &s) { value += s.value; return *this; }
    void Add_LF() { value += L'\n'; }
    void Add_UInt32(UInt32 n) { value += std::to_wstring(n); }
    void DeleteFrontal(unsigned n) { value.erase(0, n); }
    void TrimRight() { const auto n = value.find_last_not_of(L" \t\n"); value.resize(n == std::wstring::npos ? 0 : n + 1); }
    void Trim() { TrimRight(); const auto n = value.find_first_not_of(L" \t\n"); if (n != std::wstring::npos) value.erase(0, n); }
    QString text() const { return QString::fromStdWString(value); }
};
template<class T> struct Vector : std::vector<T> {
    void Clear() { this->clear(); }
    unsigned Size() const { return unsigned(this->size()); }
    bool IsEmpty() const { return this->empty(); }
    void Add(const T &value) { this->push_back(value); }
    int FindInSorted(const T &value) const { auto p = std::lower_bound(this->begin(), this->end(), value); return p != this->end() && *p == value ? int(p - this->begin()) : -1; }
};
using UStringVector = Vector<UString>;
bool ConvertUTF8ToUnicode(const AString &source, UString &destination) { QStringDecoder decoder(QStringDecoder::Utf8); destination = UString(decoder.decode(source.value)); return !decoder.hasError(); }
bool StringsAreEqual_Ascii(const wchar_t *wide, const char *ascii) { while (*wide && *ascii) if (*wide++ != Byte(*ascii++)) return false; return *wide == 0 && *ascii == 0; }
namespace NWindows::NFile::NIO {
struct CInFile {
    QIODevice *device = nullptr;
    bool Open(QIODevice *source) { device = source; return source && source->isOpen() && source->isReadable() && !source->isSequential(); }
    bool GetLength(UInt64 &size) { const auto result = device->size(); if (result < 0) return false; size = UInt64(result); return true; }
    bool ReadFull(char *buffer, unsigned length, size_t &processed) { processed = 0; while (processed < length) { const auto n = device->read(buffer + processed, length - processed); if (n < 0) return false; if (!n) break; processed += size_t(n); } return true; }
    void Close() {}
};
}
struct CLang {
    Vector<UInt32> _ids, _offsets;
    UStringVector Comments;
    wchar_t *_text = nullptr;
    ~CLang() { Clear(); }
    void Clear() throw();
    bool OpenFromString(const AString &);
    bool Open(CFSTR, const char *id);
    const wchar_t *Get(UInt32) const throw();
    const wchar_t *Get_by_index(unsigned i) const { return _text + _offsets[i]; }
    bool IsEmpty() const { return _ids.IsEmpty(); }
};
struct CLangInfo;
struct CLangPage {
    struct Combo { int GetItemData_of_CurSel() const { return 0; } } _langCombo;
    Vector<CLangInfo> _langs;
    unsigned NumLangLines_EN = 0;
    UString rendered;
    void ShowLangInfo();
    void SetItemText(int, const UString &value) { rendered = value; }
};
constexpr int IDT_LANG_INFO = 101;
#include "upstream/Language.inc"
#undef FOR_VECTOR
#undef UINT64_CONST
LanguageDocument document(const CLang &language) {
    LanguageDocument result; result.valid = true;
    for (unsigned i = 0; i < language._ids.Size(); ++i) result.entries.insert(language._ids[i], QString::fromWCharArray(language.Get_by_index(i)));
    for (const auto &line : language.Comments) result.comments << line.text(); return result;
}
void fill(CLang &lang, const LanguageDocument &doc) {
    size_t length = 0; for (const auto &value : doc.entries) length += value.toStdWString().size() + 1;
    lang._text = new wchar_t[length + 1]; size_t offset = 0;
    for (auto it = doc.entries.cbegin(); it != doc.entries.cend(); ++it) {
        const auto value = it.value().toStdWString(); lang._ids.Add(it.key()); lang._offsets.Add(UInt32(offset));
        std::copy(value.cbegin(), value.cend(), lang._text + offset); offset += value.size(); lang._text[offset++] = 0;
    }
    for (const auto &line : doc.comments) lang.Comments.Add(UString(line));
}
}
LanguageDocument readOfficialLanguage(QIODevice &device) { CLang language; return language.Open(&device, "7-Zip") ? document(language) : LanguageDocument(); }
LanguageDocument parseOfficialLanguage(QByteArray data) { QBuffer buffer(&data); buffer.open(QIODevice::ReadOnly); return readOfficialLanguage(buffer); }
QString officialLanguageInformation(QString code, const LanguageDocument &language, const LanguageDocument &english) {
    if (!english.valid || !language.valid) return {};
    CLang localized, reference; fill(localized, language); fill(reference, english);
    CLangInfo info; info.Name = UString(code == "en" ? QString("-") : code); info.Comments = localized.Comments;
    if (code == "en") { info.NumLines = reference._ids.Size(); info.Comments.Clear(); }
    else FillLangInfo(info, localized, reference);
    CLangPage page; page.NumLangLines_EN = reference._ids.Size(); page._langs.Add(info); page.ShowLangInfo(); return page.rendered.text();
}
