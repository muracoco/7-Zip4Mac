// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once
#include <QObject>
#include <QDateTime>
#include <condition_variable>
#include <mutex>
#include <optional>

enum class OverwriteAnswer { Yes, No, YesToAll, NoToAll, AutoRename, Cancel };
struct OverwriteFileInfo {
    QString path;
    quint64 size = 0;
    QDateTime modified;
    bool sizeDefined = false, directory = false;
    std::optional<quint64> fileTime; // Original 100 ns FILETIME; never round through a JSON double.
};
struct OverwriteConflict {
    quint64 id = 0;
    OverwriteFileInfo existing, incoming;
};
Q_DECLARE_METATYPE(OverwriteConflict)
Q_DECLARE_METATYPE(OverwriteAnswer)

// The worker waits here, never the GUI thread. IDs are globally unique so an
// answer from a closed dialog cannot authorize a later operation's collision.
class OverwriteBroker final : public QObject {
    Q_OBJECT
public:
    explicit OverwriteBroker(QObject *parent = nullptr);
    void reset();
    OverwriteAnswer ask(OverwriteConflict conflict);
    bool answer(quint64 id, OverwriteAnswer answer);
    void cancel();
    bool waiting() const;
    bool current(quint64 id) const;
signals:
    void requested(OverwriteConflict conflict);
    void waitingChanged(bool waiting);
private:
    mutable std::mutex mutex;
    std::condition_variable changed;
    quint64 pending = 0;
    bool stopped = false, answered = false;
    OverwriteAnswer response = OverwriteAnswer::Cancel;
};
