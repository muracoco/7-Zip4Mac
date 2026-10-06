// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once
#include <QDateTime>
#include <QStringList>
QString officialTimeText(QDateTime time, QString fraction, int precision, bool utc);
QStringList officialTimeMenuSamples(QDateTime now, bool utc);
