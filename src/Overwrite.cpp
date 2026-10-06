// SPDX-License-Identifier: LGPL-3.0-or-later
#include "Overwrite.h"
#include <atomic>

OverwriteBroker::OverwriteBroker(QObject *parent) : QObject(parent) { qRegisterMetaType<OverwriteConflict>(); qRegisterMetaType<OverwriteAnswer>(); }
void OverwriteBroker::reset() { std::lock_guard<std::mutex> lock(mutex); pending = 0; stopped = answered = false; response = OverwriteAnswer::Cancel; }
OverwriteAnswer OverwriteBroker::ask(OverwriteConflict conflict) {
    static std::atomic<quint64> next{0};
    {
        std::lock_guard<std::mutex> lock(mutex);
        if (stopped || pending) return OverwriteAnswer::Cancel;
        conflict.id = ++next; pending = conflict.id; answered = false;
    }
    emit waitingChanged(true); emit requested(conflict);
    OverwriteAnswer result;
    {
        std::unique_lock<std::mutex> lock(mutex); changed.wait(lock, [this] { return stopped || answered; });
        result = stopped ? OverwriteAnswer::Cancel : response; pending = 0;
    }
    emit waitingChanged(false); return result;
}
bool OverwriteBroker::answer(quint64 id, OverwriteAnswer value) {
    if (value != OverwriteAnswer::Yes && value != OverwriteAnswer::No && value != OverwriteAnswer::YesToAll && value != OverwriteAnswer::NoToAll && value != OverwriteAnswer::AutoRename && value != OverwriteAnswer::Cancel) return false;
    { std::lock_guard<std::mutex> lock(mutex); if (stopped || !pending || pending != id || answered) return false; response = value; answered = true; }
    changed.notify_all(); return true;
}
void OverwriteBroker::cancel() { { std::lock_guard<std::mutex> lock(mutex); stopped = true; } changed.notify_all(); }
bool OverwriteBroker::waiting() const { std::lock_guard<std::mutex> lock(mutex); return pending != 0 && !stopped && !answered; }
bool OverwriteBroker::current(quint64 id) const { std::lock_guard<std::mutex> lock(mutex); return pending == id && id != 0 && !stopped && !answered; }
