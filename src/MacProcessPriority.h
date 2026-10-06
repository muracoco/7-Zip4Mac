// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once
#include <QString>
namespace MacProcessPriority {
// Only callers owning the process may pass its PID. Background state is
// reversible; ordinary nice(19) would require privilege to restore nice(0).
QString set(qint64 ownedProcessId, bool background);
QString lease(const void *owner, bool background);
void release(const void *owner);
}
