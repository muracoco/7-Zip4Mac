// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once
#include <QMap>
#include <QString>
#include <sys/types.h>

using ExternalProcessIdentity = QPair<quint64, quint64>;

// The official same-name/descendant search, adapted to libproc. Observation
// never grants permission to signal or terminate another application.
QMap<pid_t, ExternalProcessIdentity> discoverExternalProcesses(
    const QMap<pid_t, ExternalProcessIdentity> &known, pid_t mainProcess,
    const QString &mainPath, bool findByName, bool *unknown);
