// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once
#include <QObject>
#include <QTimer>
#include <QElapsedTimer>
#include <QStringList>
#include <QMap>
#include <sys/types.h>
#include <sys/stat.h>

// Own only observation. Destruction must never terminate the user's editor.
class ExternalProcess final : public QObject {
    Q_OBJECT
public:
    explicit ExternalProcess(QObject *parent = nullptr);
    ~ExternalProcess() override;
    bool start(QString command, QString file, QString working, QString *error);
    bool running() const { return timer.isActive(); }
    qint64 elapsed() const { return clock.isValid() ? clock.elapsed() : 0; }
    qint64 processId() const { return child; }
signals:
    void finished(int exitCode);
private:
    void poll();
    QTimer timer;
    QElapsedTimer clock;
    pid_t child = -1;
    bool reaped = false;
    int exitCode = -1;
    QMap<pid_t, QPair<quint64, quint64>> descendants;
    QString mainPath, openedFile;
    struct stat initialFile{};
    bool hasInitialFile = false, discoveredHandoff = false;
};
