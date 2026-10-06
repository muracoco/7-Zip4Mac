// SPDX-License-Identifier: LGPL-3.0-or-later
#include "CompletionData.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QRegularExpression>

bool readCompletionData(const QString &path, CompletionData &data, QString &error) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly) || file.size() > 1024 * 1024) { error = "Cannot read native operation results."; return false; }
    QJsonParseError parse;
    const auto document = QJsonDocument::fromJson(file.readAll(), &parse);
    bool valid = parse.error == QJsonParseError::NoError && document.isObject();
    const auto object = document.object(); valid &= object.value("version").toInt() == 1;
    CompletionData result;
    auto number = [&](const QJsonObject &o, const char *key, quint64 &value) {
        const auto json = o.value(key); bool ok = false;
        static const QRegularExpression decimal("^[0-9]{1,20}$");
        value = json.toString().toULongLong(&ok); valid &= json.isString() && decimal.match(json.toString()).hasMatch() && ok;
    };
    if (object.value("decompression").isObject()) {
        DecompressStatistics s; const auto o = object.value("decompression").toObject();
        number(o, "NumArchives", s.NumArchives); number(o, "UnpackSize", s.UnpackSize); number(o, "AltStreams_UnpackSize", s.AltStreams_UnpackSize); number(o, "PackSize", s.PackSize);
        number(o, "NumFolders", s.NumFolders); number(o, "NumFiles", s.NumFiles); number(o, "NumAltStreams", s.NumAltStreams); result.decompression = s;
    } else valid &= object.value("decompression").isNull();
    if (object.value("hash").isObject()) {
        HashStatistics s; const auto o = object.value("hash").toObject();
        number(o, "NumDirs", s.NumDirs); number(o, "NumFiles", s.NumFiles); number(o, "NumAltStreams", s.NumAltStreams); number(o, "FilesSize", s.FilesSize); number(o, "AltStreamsSize", s.AltStreamsSize); number(o, "NumErrors", s.NumErrors);
        valid &= o.value("MainName").isString() && o.value("FirstFileName").isString() && o.value("hashers").isArray();
        s.MainName = o.value("MainName").toString(); s.FirstFileName = o.value("FirstFileName").toString(); const auto array = o.value("hashers").toArray(); valid &= array.size() <= 256;
        for (const auto &value : array) {
            HashDigestResult h; const auto item = value.toObject(); valid &= value.isObject() && item.value("name").isString() && item.value("groups").isArray(); h.name = item.value("name").toString();
            const auto groups = item.value("groups").toArray(); valid &= groups.size() == 3;
            for (int i = 0; i < 3; ++i) { const auto text = i < groups.size() ? groups[i] : QJsonValue(); valid &= text.isString() && text.toString().size() < 160 && !text.toString().contains(QChar::Null); h.groups[i] = text.toString(); }
            s.hashers.append(h);
        }
        result.hash = s;
    } else valid &= object.value("hash").isNull();
    if (!valid || (!result.decompression && !result.hash)) { error = "Invalid native operation results."; return false; }
    data = result; return true;
}
