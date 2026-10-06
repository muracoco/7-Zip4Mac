// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once
#include <QObject>
#include <QDateTime>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QMap>
#include <atomic>
#include <memory>

struct DirectoryEntry {
    QString path, name, prefix, suffix, comment;
    QFileInfo iconInfo;
    QDateTime modified, created, accessed, changed;
    QString modifiedFraction, createdFraction, accessedFraction, changedFraction;
    quint64 size = 0, packed = 0, inode = 0, links = 0;
    quint32 mode = 0;
    bool directory = false, link = false;
};
struct DirectorySnapshot {
    quint64 generation = 0;
    QString path, error;
    QList<DirectoryEntry> entries;
    bool flat = false, cancelled = false;
};
Q_DECLARE_METATYPE(DirectorySnapshot)

// Jobs capture data and cancellation flags only, never GUI/owner pointers.
// Superseded jobs may finish independently without blocking navigation/close.
class DirectoryScanner final : public QObject {
    Q_OBJECT
public:
    explicit DirectoryScanner(QObject *parent = nullptr);
    ~DirectoryScanner() override;
    quint64 start(QString path, bool flat);
    void cancel();
signals:
    void finished(DirectorySnapshot snapshot);
private:
    struct Job { QFutureWatcher<DirectorySnapshot> *watcher; std::shared_ptr<std::atomic_bool> cancelled; };
    QMap<quint64, Job> jobs;
    quint64 generation = 0;
};
