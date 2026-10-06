// SPDX-License-Identifier: LGPL-3.0-or-later
#include "OpenProfile.h"
#include "ArchiveBackend.h"
#include "UiLanguage.h"
#include <QByteArray>
#include <string>

namespace {
struct AString {
    QByteArray value;
    void Empty() { value.clear(); }
    void Add_Char(char character) { value.append(character); }
    const char *Ptr() const { return value.constData(); }
};
char MyCharLower_Ascii(char c) { return c >= 'A' && c <= 'Z' ? char(c + ('a' - 'A')) : c; }
struct UString {
    QString value;
    mutable std::wstring wide;
    UString() = default;
    UString(const QString &text) : value(text) {}
    UString(const wchar_t *text) : value(QString::fromWCharArray(text)) {}
    UString(wchar_t character) : value(QChar(ushort(character))) {}
    unsigned Len() const { return unsigned(value.size()); }
    int ReverseFind_Dot() const { return value.lastIndexOf('.'); }
    UString Ptr(unsigned offset) const { return value.mid(offset); }
    operator const wchar_t *() const { wide = value.toStdWString(); return wide.c_str(); }
    wchar_t operator[](unsigned offset) const { return value.at(offset).unicode(); }
    UString &operator+=(wchar_t character) { value += QChar(ushort(character)); return *this; }
    UString &operator+=(const UString &text) { value += text.value; return *this; }
    int Find(wchar_t character, unsigned offset = 0) const { return value.indexOf(QChar(ushort(character)), offset); }
    void Replace(const UString &from, const UString &to) { value.replace(from.value, to.value); }
    void Replace(wchar_t from, wchar_t to) { value.replace(QChar(ushort(from)), QChar(ushort(to))); }
    void Delete(unsigned offset, unsigned count = 1) { value.remove(offset, count); }
    void Add_LF() { value += '\n'; }
};
struct CStringFinder {
    AString _temp;
    bool FindWord_In_LowCaseAsciiList_NoCase(const char *list, const wchar_t *text);
};
constexpr int IDS_VIRUS = 3012;
struct CPanel {
    QString warning;
    UString LangString(int id) const { return UiLanguage::resource(id); }
    void MessageBox_Error(const UString &message) { warning = message.value; }
    bool IsVirus_Message(const UString &name);
};
#include "upstream/OpenProfile.inc"
}

bool openProfileAlwaysExternal(const QString &name) { return DoItemAlwaysStart(UString(name)); }
QString openProfileFilenameWarning(const QString &name) {
    CPanel panel; panel.IsVirus_Message(UString(name)); return panel.warning;
}
bool openProfileNotArchive(const ArchiveResult &result) {
    if (result.success || result.cancelled || result.passwordRequired || result.exitCode != 2) return false;
    return result.details.contains("Is not archive", Qt::CaseInsensitive) ||
        result.details.contains("Cannot open the file as", Qt::CaseInsensitive) ||
        result.details.contains("Can not open the file as", Qt::CaseInsensitive);
}
