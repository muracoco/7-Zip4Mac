// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once
#include <QString>
struct ArchiveResult;
bool openProfileAlwaysExternal(const QString &name);
QString openProfileFilenameWarning(const QString &name);
bool openProfileNotArchive(const ArchiveResult &result);
