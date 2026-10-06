// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once
#include <QByteArray>
#include <QMap>
#include <QStringList>
#include <QIODevice>
struct LanguageDocument {
    bool valid = false;
    QMap<quint32, QString> entries;
    QStringList comments;
};
LanguageDocument readOfficialLanguage(QIODevice &device);
LanguageDocument parseOfficialLanguage(QByteArray data);
QString officialLanguageInformation(QString code, const LanguageDocument &language, const LanguageDocument &english);
