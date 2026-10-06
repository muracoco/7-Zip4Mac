// SPDX-License-Identifier: LGPL-3.0-or-later
#include "ExternalProcess.h"
#include "ExternalProcessDiscovery.h"
#include <QProcess>
#include <QFile>
#include <QFileInfo>
#include <QVector>
#include <QSet>
#include <QStandardPaths>
#include <spawn.h>
#include <crt_externs.h>
#include <sys/wait.h>
#include <sys/proc.h>
#include <libproc.h>
#include <fcntl.h>
#include <cerrno>
#include <cstring>

ExternalProcess::ExternalProcess(QObject *parent) : QObject(parent) {
    timer.setInterval(100); connect(&timer, &QTimer::timeout, this, &ExternalProcess::poll);
}
ExternalProcess::~ExternalProcess() {
    // A running child is left alive and will be reparented when the Manager
    // exits. QProcess's owning/destructor-kill semantics are unsuitable here.
    if (child > 0 && !reaped) { int status; ::waitpid(child, &status, WNOHANG); }
}
bool ExternalProcess::start(QString command, QString file, QString working, QString *error) {
    if (child > 0) { *error = "The application observer has already been started."; return false; }
    QStringList parts = QProcess::splitCommand(command); QString program;
    if (parts.isEmpty()) { program = "/usr/bin/open"; parts = {"-W", file}; }
    else {
        program = parts.takeFirst();
        if (program.endsWith(".app", Qt::CaseInsensitive)) {
            if (!QFileInfo(program).isDir()) { *error = "Application does not exist: " + program; return false; }
            const auto applicationArguments = parts; parts = {"-W", "-a", program, file};
            if (!applicationArguments.isEmpty()) parts << "--args" << applicationArguments;
            program = "/usr/bin/open";
        } else parts << file;
    }
    openedFile = file;
    hasInitialFile = ::lstat(QFile::encodeName(file).constData(), &initialFile) == 0;
    // LaunchServices' open -W already observes the selected application. Its
    // helper name must not make us wait for unrelated /usr/bin/open jobs.
    if (program != "/usr/bin/open") {
        mainPath = program.contains('/') ? QFileInfo(program).absoluteFilePath() : QStandardPaths::findExecutable(program);
        if (mainPath.isEmpty()) mainPath = program;
    }
    QList<QByteArray> bytes{QFile::encodeName(program)}; for (const auto &part : parts) bytes << part.toUtf8();
    QVector<char *> arguments; for (auto &part : bytes) arguments << part.data(); arguments << nullptr;
    posix_spawn_file_actions_t files; posix_spawnattr_t attributes;
    int code = ::posix_spawn_file_actions_init(&files); if (code) { *error = QString::fromLocal8Bit(std::strerror(code)); return false; }
    code = ::posix_spawnattr_init(&attributes);
    if (code) { ::posix_spawn_file_actions_destroy(&files); *error = QString::fromLocal8Bit(std::strerror(code)); return false; }
    // A separate session/group lets us observe ordinary forked descendants,
    // including children reparented after a short-lived launcher exits.
    code = ::posix_spawnattr_setflags(&attributes, POSIX_SPAWN_SETSID | POSIX_SPAWN_CLOEXEC_DEFAULT);
    for (int descriptor = 0; descriptor < 3 && !code; ++descriptor)
        code = ::posix_spawn_file_actions_addopen(&files, descriptor, "/dev/null", descriptor ? O_WRONLY : O_RDONLY, 0);
    if (!code && !working.isEmpty()) {
        // The replacement without _np is only available on macOS 26; retain 15.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
        code = ::posix_spawn_file_actions_addchdir_np(&files, QFile::encodeName(working).constData());
#pragma clang diagnostic pop
    }
    if (!code) code = ::posix_spawnp(&child, bytes[0].constData(), &files, &attributes, arguments.data(), *_NSGetEnviron());
    ::posix_spawn_file_actions_destroy(&files); ::posix_spawnattr_destroy(&attributes);
    if (code) { child = -1; *error = "Cannot start application: " + program + '\n' + QString::fromLocal8Bit(std::strerror(code)); return false; }
    clock.start(); timer.start(); return true;
}
void ExternalProcess::poll() {
    bool unknown = false, alive = false;
    auto list = [&](uint32_t kind, pid_t id) {
        errno = 0; const auto required = ::proc_listpids(kind, uint32_t(id), nullptr, 0);
        if (required < 0 || (!required && errno)) { unknown = true; return QVector<pid_t>(); }
        QVector<pid_t> result(required / int(sizeof(pid_t)) + 64);
        errno = 0; const auto size = ::proc_listpids(kind, uint32_t(id), result.data(), int(result.size() * sizeof(pid_t)));
        if (size < 0 || (!size && errno) || size == int(result.size() * sizeof(pid_t))) { unknown = true; return QVector<pid_t>(); }
        result.resize(size / int(sizeof(pid_t))); return result;
    };
    auto pending = list(PROC_PGRP_ONLY, child);
    for (auto pid : descendants.keys()) pending << pid;
    if (!reaped) pending << child;
    QSet<pid_t> visited;
    while (!pending.isEmpty()) {
        const auto pid = pending.takeLast(); if (pid <= 0 || visited.contains(pid)) continue; visited.insert(pid);
        struct proc_bsdinfo info{}; errno = 0;
        if (::proc_pidinfo(pid, PROC_PIDTBSDINFO, 0, &info, sizeof(info)) != sizeof(info)) {
            if (errno == ESRCH) descendants.remove(pid); else unknown = true;
            continue;
        }
        const auto identity = qMakePair(quint64(info.pbi_start_tvsec), quint64(info.pbi_start_tvusec));
        if (descendants.contains(pid) && descendants.value(pid) != identity) { descendants.remove(pid); continue; }
        descendants[pid] = identity;
        if (info.pbi_status == SZOMB) continue;
        if (pid != child) alive = true;
        // Continue following observed children even after setsid()/setpgid().
        // Birth times prevent a reused PID from becoming a different editor.
        pending << list(PROC_PPID_ONLY, pid);
    }
    if (!reaped) {
        int status = 0; const auto result = ::waitpid(child, &status, WNOHANG);
        if (result == child) { reaped = true; exitCode = WIFEXITED(status) ? WEXITSTATUS(status) : WIFSIGNALED(status) ? 128 + WTERMSIG(status) : -1; }
        else if (result < 0 && errno == ECHILD) reaped = true;
    }
    if (reaped && !discoveredHandoff) {
        discoveredHandoff = true;
        struct stat current{};
        const bool changed = !hasInitialFile || ::lstat(QFile::encodeName(openedFile).constData(), &current) != 0 ||
            current.st_size != initialFile.st_size || current.st_mtimespec.tv_sec != initialFile.st_mtimespec.tv_sec ||
            current.st_mtimespec.tv_nsec != initialFile.st_mtimespec.tv_nsec;
        // PanelItemOpen.cpp searches matching executable names only when the
        // first process exits within two seconds without changing the file.
        bool uncertain = false;
        const auto found = discoverExternalProcesses(descendants, child, mainPath,
            !mainPath.isEmpty() && clock.elapsed() < 2000 && !changed, &uncertain);
        if (uncertain) { discoveredHandoff = false; unknown = true; }
        for (auto it = found.cbegin(); it != found.cend(); ++it) { descendants.insert(it.key(), it.value()); alive = true; }
    }
    if (!reaped || alive || unknown) return;
    timer.stop(); emit finished(exitCode);
}
