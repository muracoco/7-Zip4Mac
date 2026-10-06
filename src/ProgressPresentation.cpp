// SPDX-License-Identifier: LGPL-3.0-or-later
#include "ProgressPresentation.h"
#include "UiLanguage.h"
#include <algorithm>
#include <cwchar>
#include <limits>
#include <string>
#include <vector>

namespace {
using UInt64 = quint64; using UInt32 = quint32; using Int32 = qint32; using UINT = unsigned; using Byte = unsigned char; using LPCWSTR = const wchar_t *; using HWND = void *;
constexpr UInt64 UNDEFINED_VAL = std::numeric_limits<UInt64>::max();
#define IS_DEFINED_VAL(v) ((v) != UNDEFINED_VAL)
#define IS_UNDEFINED_VAL(v) ((v) == UNDEFINED_VAL)
#define INIT_AS_UNDEFINED(v) (v) = UNDEFINED_VAL;
#define FOR_VECTOR(i, v) for (unsigned i = 0; i < (v).Size(); ++i)
constexpr unsigned IDT_PROGRESS_ELAPSED_VAL = 120, IDT_PROGRESS_REMAINING_VAL = 121;
constexpr unsigned IDT_PROGRESS_FILES_VAL = 111, IDT_PROGRESS_FILES_TOTAL = 112, IDT_PROGRESS_ERRORS = 3906, IDT_PROGRESS_ERRORS_VAL = 126;
constexpr unsigned IDT_PROGRESS_TOTAL_VAL = 122, IDT_PROGRESS_SPEED_VAL = 123, IDT_PROGRESS_PROCESSED_VAL = 124, IDT_PROGRESS_PACKED_VAL = 110, IDT_PROGRESS_RATIO_VAL = 125;
constexpr unsigned IDT_PROGRESS_STATUS = 103, IDT_PROGRESS_FILE_NAME = 102, IDL_PROGRESS_MESSAGES = 101, IDB_PAUSE = 446, IDB_PROGRESS_BACKGROUND = 444;
struct UString {
    std::wstring value;
    UString() = default;
    UString(const wchar_t *s) : value(s ? s : L"") {}
    UString(const char *s) : value(QString::fromLatin1(s).toStdWString()) {}
    UString(QString s) : value(s.toStdWString()) {}
    operator const wchar_t *() const { return value.c_str(); }
    bool operator==(const UString &other) const { return value == other.value; }
    bool operator!=(const UString &other) const { return !(*this == other); }
    bool operator==(const wchar_t *other) const { return value == other; }
    bool operator!=(const wchar_t *other) const { return !(*this == other); }
    bool IsEmpty() const { return value.empty(); }
    unsigned Len() const { return unsigned(value.size()); }
    const wchar_t *Ptr(unsigned offset = 0) const { return value.c_str() + offset; }
    void Empty() { value.clear(); }
    void Delete(unsigned start, unsigned count) { value.erase(start, count); }
    void Insert(unsigned offset, const wchar_t *s) { value.insert(offset, s); }
    void DeleteFrom(unsigned start) { value.resize(start); }
    void DeleteFrontal(unsigned count) { value.erase(0, count); }
    void SetFrom(const UString &s, unsigned count) { value = s.value.substr(0, count); }
    UString Left(unsigned count) const { UString s; s.value = value.substr(0, count); return s; }
    int Find(wchar_t c) const { const auto n = value.find(c); return n == std::wstring::npos ? -1 : int(n); }
    int ReverseFind_PathSepar() const { const auto n = value.find_last_of(L'/'); return n == std::wstring::npos ? -1 : int(n); }
    UString &operator+=(const UString &s) { value += s.value; return *this; }
    void Add_Colon() { value += L':'; }
    void Add_LF() { value += L'\n'; }
    void Add_Space() { value += L' '; }
    void Add_Char(wchar_t c) { value += c; }
    void Add_UInt64(UInt64 n) { value += std::to_wstring(n); }
    void Add_UInt32(UInt32 n) { Add_UInt64(n); }
    void Replace(const wchar_t *from, const UString &to) { auto s = text(); s.replace(QString::fromWCharArray(from), to.text()); value = s.toStdWString(); }
    QString text() const { return QString::fromStdWString(value); }
};
template<class T> struct Vector : std::vector<T> {
    unsigned Size() const { return unsigned(this->size()); }
    bool IsEmpty() const { return this->empty(); }
    void Add(const T &value) { this->push_back(value); }
    void AddInReserved(const T &value) { this->push_back(value); }
    T &AddNew() { this->emplace_back(); return this->back(); }
    void ClearAndReserve(unsigned count) { this->clear(); this->reserve(count); }
};
using UStringVector = Vector<UString>;
using CUIntVector = Vector<UInt32>;
void ConvertUInt64ToString(UInt64 value, wchar_t *destination) { const auto s = std::to_wstring(value); std::copy(s.cbegin(), s.cend(), destination); destination[s.size()] = 0; }
void ConvertUInt32ToString(UInt32 value, wchar_t *destination) { ConvertUInt64ToString(value, destination); }
unsigned MyStringLen(const wchar_t *s) { return unsigned(std::wcslen(s)); }
void MyStringCopy(wchar_t *destination, const wchar_t *s) { std::wcscpy(destination, s); }
void MyStringCat(wchar_t *destination, const wchar_t *s) { std::wcscat(destination, s); }
void ConvertUInt64ToString(UInt64 value, char *destination) { const auto text = std::to_string(value); std::copy(text.begin(), text.end(), destination); destination[text.size()] = 0; }
#include "upstream/ProgressFinalMessage.inc"
namespace NSynchronization { struct CCriticalSectionLock { explicit CCriticalSectionLock(int &) {} }; }
struct SyncState {
    int _cs = 0;
    bool _filesProgressMode = false, _isDir = false, paused = false;
    UInt64 _totalBytes = UNDEFINED_VAL, _completedBytes = 0, _totalFiles = UNDEFINED_VAL, _curFiles = 0, _inSize = UNDEFINED_VAL, _outSize = UNDEFINED_VAL;
    UString _titleFileName, _filePath, _status;
    UStringVector Messages;
    CProgressFinalMessage FinalMessage;
    bool Get_Paused() const { return paused; }
};
#include "upstream/ProgressConverter.inc"
struct CProgressDialog {
    ProgressPresentation presentation;
    SyncState Sync;
    bool CompressingMode = false, _isDir = false, _background = false, _errorsWereDisplayed = false;
    unsigned _numPostedMessages = 0, _numAutoSizeMessages = 0, _numMessages = 0, _numReduceSymbols = 82, _prevSpeed_MoveBits = 0;
    UInt32 _prevTime = 0, now = 0;
    UInt64 _elapsedTime = 0, _prevPercentValue = UNDEFINED_VAL, _prevElapsedSec = UNDEFINED_VAL, _prevRemainingSec = UNDEFINED_VAL;
    UInt64 _totalBytes_Prev = UNDEFINED_VAL, _processed_Prev = UNDEFINED_VAL, _packed_Prev = UNDEFINED_VAL, _ratio_Prev = UNDEFINED_VAL, _prevSpeed = UNDEFINED_VAL;
    UInt64 _progressBar_Range = UNDEFINED_VAL, _progressBar_Pos = UNDEFINED_VAL;
    UString _titleFileName, _filePath, _status, _title, MainAddTitle;
    UString _filesStr_Prev, _filesTotStr_Prev, _paused_String, _pause_String, _continue_String, _background_String, _backgrounded_String, _foreground_String;
    UStringVector _messageStrings;
    CU64ToI32Converter _progressConv;
    struct Bar {
        ProgressPresentation *view;
        void SetRange32(int, int maximum) { view->barMaximum = maximum; }
        void SetPos(int value) { view->barPosition = value; }
    } m_ProgressBar{&presentation};
    struct Taskbar { void SetProgressValue(void *, UInt64, UInt64) {} } *_taskbarList = nullptr;
    void *_hwndForTaskbar = nullptr;
    struct MessageList {
        ProgressPresentation *view;
        QVector<unsigned> selected;
        int InsertItem(unsigned index, LPCWSTR number) { if (index != unsigned(view->messages.size())) return -1; view->messages.append({QString::fromWCharArray(number), {}}); return int(index); }
        void SetSubItem(unsigned index, int, LPCWSTR message) { view->messages[int(index)].second = QString::fromWCharArray(message); }
        int GetItemCount() const { return int(view->messages.size()); }
        void SetColumnWidthAuto(int) { ++view->columnRevision; }
    } _messageList{&presentation};
    UInt32 GetTickCount() const { return now; }
    void SetItemText(unsigned id, const UString &s) { presentation.text[id] = s.text(); }
    void ShowItem_Bool(unsigned, bool visible) { presentation.errorsVisible = visible; }
    void SetText(const UString &s) { presentation.title = s.text(); }
    void AddToTitle(const UString &s) { presentation.parentTitlePrefix = s.text(); }
    void SetTaskbarProgressState(int = 0) {} // Windows taskbar API has no Qt/macOS counterpart.
    void EnableErrorsControls(bool);
    void SetProgressRange(UInt64);
    void SetProgressPos(UInt64);
    void ShowSize(unsigned, UInt64, UInt64 &);
    void UpdateStatInfo(bool);
    void SetTitleText();
    void SetPauseText();
    void SetPriorityText();
    void AddMessageDirect(LPCWSTR, bool);
    void AddMessage(LPCWSTR);
    void UpdateMessagesDialog();
    void CopyToClipboard();
    bool OnExternalCloseMessage();
    bool _cancelWasPressed = false, _waitCloseByCancelButton = false, MessagesDisplayed = false;
    void ProcessWasFinished_GuiVirt() {}
    void HideItem(unsigned id) { if (id == IDB_PAUSE) presentation.hidePause = true; if (id == IDB_PROGRESS_BACKGROUND) presentation.hideBackground = true; }
    ProgressPresentation *GetItem(unsigned) { return &presentation; }
    void End(int) { presentation.ended = true; }
};
void ListView_GetSelected(CProgressDialog::MessageList &list, CUIntVector &indexes) { for (auto index : list.selected) if (index < unsigned(list.GetItemCount())) indexes.Add(index); std::sort(indexes.begin(), indexes.end()); }
void ClipboardSetText(CProgressDialog &dialog, const UString &text) { dialog.presentation.clipboard = text.text(); }
#include "upstream/ProgressText.inc"
#include "upstream/ProgressPresentation.inc"
#undef UINT_TO_STR_2
UString LangString(unsigned id) { return UString(UiLanguage::resource(id)); }
void LangString(unsigned id, UString &s) { s = LangString(id); }
void LangString_OnlyFromLangFile(unsigned id, UString &s) { s = UString(UiLanguage::localizedResource(id)); }
void AddLangString(UString &s, unsigned id) { s += LangString(id); }
#include "upstream/ProgressOperationText.inc"
constexpr unsigned IDCANCEL = 2, IDS_CLOSE = 408, BM_SETSTYLE = 1, BS_DEFPUSHBUTTON = 1, TRUE = 1, TBPF_NOPROGRESS = 0, MB_ICONERROR = 1, MB_OK = 0;
constexpr bool g_DisableUserQuestions = false;
unsigned MAKELPARAM(unsigned, unsigned) { return 0; }
void SendMessage(ProgressPresentation *view, unsigned, unsigned, unsigned) { view->cancelDefault = true; }
void MessageBoxW(CProgressDialog &dialog, const UString &text, const UString &title, unsigned flags) { dialog.presentation.finalNotices.append({title.text(), text.text(), flags == MB_ICONERROR}); }
#include "upstream/ProgressCompletion.inc"

using CDecompressStat = DecompressStatistics;
struct CProperty { UString Name, Value; };
using CPropNameValPairs = Vector<CProperty>;
constexpr unsigned k_HashCalc_DigestSize_Max = 64, k_HashCalc_ExtraSize = 8;
constexpr unsigned k_HashCalc_Index_DataSum = 1, k_HashCalc_Index_NamesSum = 2, k_HashCalc_Index_StreamsSum = 3;
struct CHasherState {
    UString Name; std::array<QString, 3> groups;
    void WriteToString(unsigned group, char *text) const { const auto bytes = groups[group - 1].toLatin1(); std::copy(bytes.begin(), bytes.end(), text); text[bytes.size()] = 0; }
};
struct CHashBundle {
    UInt64 NumDirs = 0, NumFiles = 0, NumAltStreams = 0, FilesSize = 0, AltStreamsSize = 0, NumErrors = 0;
    UString MainName, FirstFileName;
    Vector<CHasherState> Hashers;
};
struct CListViewDialog {
    UStringVector Strings, Values; UString Title;
    bool DeleteIsAllowed = false, SelectFirst = true; unsigned NumColumns = 1;
    struct List { QVector<unsigned> selected; } _listView;
    QString clipboard;
    void Create(void *target) { auto &view = *static_cast<ChecksumPresentation *>(target); view.title = Title.text(); view.deleteAllowed = DeleteIsAllowed; view.selectFirst = SelectFirst; view.columns = NumColumns; for (unsigned i = 0; i < Strings.Size(); ++i) view.rows.append({Strings[i].text(), i < Values.Size() ? Values[i].text() : QString()}); }
    void CopyToClipboard();
};
void ListView_GetSelected(CListViewDialog::List &list, CUIntVector &indexes) { for (auto index : list.selected) indexes.Add(index); std::sort(indexes.begin(), indexes.end()); }
void ClipboardSetText(CListViewDialog &dialog, const UString &text) { dialog.clipboard = text.text(); }
UString MyFormatNew(unsigned id, const wchar_t *argument) { return MyFormatNew(LangString(id), UString(argument)); }
#include "upstream/ProgressResultText.inc"
#include "upstream/ListViewCopy.inc"
CHashBundle hashBundle(const HashStatistics &s) {
    CHashBundle h; h.NumDirs = s.NumDirs; h.NumFiles = s.NumFiles; h.NumAltStreams = s.NumAltStreams; h.FilesSize = s.FilesSize; h.AltStreamsSize = s.AltStreamsSize; h.NumErrors = s.NumErrors;
    h.MainName = UString(s.MainName); h.FirstFileName = UString(s.FirstFileName); for (const auto &v : s.hashers) h.Hashers.Add({UString(v.name), v.groups}); return h;
}

UInt64 value(std::optional<quint64> number, UInt64 undefined = UNDEFINED_VAL) { return number.value_or(undefined); }
}
QString officialProgressStatus(QString token) {
    if (token == "compress") return UiLanguage::resource(3301);
    if (token == "extract") return UiLanguage::resource(3300);
    if (token == "test") return UiLanguage::resource(3302);
    if (token == "skip") return UiLanguage::resource(IDS_PROGRESS_SKIPPING);
    if (token == "read") return "Reading"; // The original callback keeps this literal.
    if (token == "scan") return UiLanguage::resource(3304);
    if (token == "hash") return UiLanguage::resource(7500);
    if (token.startsWith("update:")) { bool ok = false; const auto n = token.mid(7).toUInt(&ok); return ok && n < sizeof(k_UpdNotifyLangs) / sizeof(k_UpdNotifyLangs[0]) ? UiLanguage::resource(k_UpdNotifyLangs[n]) : QString(); }
    return token == "unknown" ? QString() : token;
}
QString officialProgressError(const ArchiveOperationError &error) {
    if (error.code < 0) return error.fileName.isEmpty() ? error.message : error.fileName + '\n' + error.message;
    UString s; const auto file = error.fileName.toStdWString(); SetExtractErrorMessage(error.code, error.encrypted, file.c_str(), s); return s.text();
}
struct OfficialProgressState::Impl : CProgressDialog {};
OfficialProgressState::OfficialProgressState() : impl(std::make_unique<Impl>()) { impl->EnableErrorsControls(false); }
OfficialProgressState::~OfficialProgressState() = default;
void OfficialProgressState::update(const ArchiveProgress &snapshot, quint64 milliseconds) {
    auto &s = impl->Sync;
    impl->CompressingMode = snapshot.mode == "compress";
    s._totalBytes = value(snapshot.total); s._completedBytes = value(snapshot.completed, 0);
    s._totalFiles = value(snapshot.files); s._curFiles = value(snapshot.doneFiles, 0);
    s._inSize = value(snapshot.input); s._outSize = value(snapshot.output);
    s._filesProgressMode = snapshot.filesProgressMode; s._isDir = snapshot.directory;
    s._filePath = UString(snapshot.current); s._titleFileName = UString(snapshot.titleFileName); s._status = UString(snapshot.status);
    tick(milliseconds, true);
}
void OfficialProgressState::tick(quint64 milliseconds, bool all) {
    impl->now = UInt32(milliseconds);
    if (!impl->Sync.paused) impl->UpdateStatInfo(all);
}
void OfficialProgressState::setPaused(bool paused) { impl->Sync.paused = paused; impl->SetPauseText(); }
void OfficialProgressState::setBackground(bool background) { impl->_background = background; impl->SetPriorityText(); }
void OfficialProgressState::setLanguage(QString title, QString pause, QString resume, QString paused, QString background, QString foreground) {
    impl->_title = UString(title); impl->_pause_String = UString(pause); impl->_continue_String = UString(resume); impl->_paused_String = UString(paused);
    impl->_background_String = UString(background); background.remove('&'); impl->_backgrounded_String = UString(background); impl->_foreground_String = UString(foreground);
    impl->SetPauseText(); impl->SetPriorityText();
}
void OfficialProgressState::setFileNameCapacity(unsigned capacity) {
    if (impl->_numReduceSymbols == capacity) return;
    impl->_numReduceSymbols = capacity;
    impl->_filePath.Empty(); impl->_status.Empty(); impl->UpdateStatInfo(false);
}
void OfficialProgressState::addError(QString message, quint64 milliseconds) {
    impl->Sync.Messages.Add(UString(message)); tick(milliseconds, true);
}
QString OfficialProgressState::copyMessages(const QVector<unsigned> &selected) { impl->_messageList.selected = selected; impl->CopyToClipboard(); return impl->presentation.clipboard; }
const ProgressPresentation &OfficialProgressState::view() const { return impl->presentation; }

void OfficialProgressState::finish(QString error, QString ok, QString title, bool cancelled, quint64 milliseconds) {
    impl->now = UInt32(milliseconds); impl->_cancelWasPressed = cancelled;
    impl->Sync.FinalMessage.ErrorMessage = {UString(), UString(error)};
    impl->Sync.FinalMessage.OkMessage = {UString(title), UString(ok)};
    impl->OnExternalCloseMessage(); impl->presentation.waitForClose = impl->_waitCloseByCancelButton; impl->presentation.messagesDisplayed = impl->MessagesDisplayed;
}
QString officialTestResult(const DecompressStatistics &statistics) { return FormatTestResult(statistics).text(); }
QString officialInsideTestResult(const HashStatistics &statistics) { UString text; AddHashBundleRes(text, hashBundle(statistics)); return text.text(); }
QString officialInsideTestResult(const DecompressStatistics &statistics, QString fileName) {
    HashStatistics s; s.NumDirs = statistics.NumFolders; s.NumFiles = statistics.NumFiles; s.FilesSize = statistics.UnpackSize; s.NumAltStreams = statistics.NumAltStreams; s.AltStreamsSize = statistics.AltStreams_UnpackSize; s.FirstFileName = fileName;
    UString text; AddHashBundleRes(text, hashBundle(s)); return text.text();
}
ChecksumPresentation officialHashResults(const HashStatistics &statistics, const std::optional<DecompressStatistics> &archiveStatistics) {
    CPropNameValPairs pairs;
    if (archiveStatistics) { AddValuePair(pairs, IDS_ARCHIVES_COLON, archiveStatistics->NumArchives); AddSizeValuePair(pairs, IDS_PROP_PACKED_SIZE, archiveStatistics->PackSize); }
    AddHashBundleRes(pairs, hashBundle(statistics)); ChecksumPresentation view; ShowHashResults(pairs, &view); return view;
}
QString officialChecksumCopy(const QVector<QPair<QString, QString>> &rows, const QVector<unsigned> &selected) {
    CListViewDialog list; list.NumColumns = 2;
    for (const auto &row : rows) { list.Strings.Add(UString(row.first)); list.Values.Add(UString(row.second)); }
    for (auto index : selected) if (index < unsigned(rows.size())) list._listView.selected.append(index);
    list.CopyToClipboard(); return list.clipboard;
}
