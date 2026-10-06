// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once
#include <QString>
QString officialProgressTime(quint64 seconds);
QString officialProgressSize(quint64 bytes);
QString officialProgressSpeed(quint64 bytesPerSecond);
