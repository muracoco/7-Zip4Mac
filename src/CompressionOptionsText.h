// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once
#include <QString>
struct ArchiveRequest;
QString compressionOptionsText(const ArchiveRequest &options, const QString &format);
