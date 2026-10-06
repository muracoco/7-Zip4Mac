// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once
#include "OperationControl.h"
#include <QString>
#include <QByteArray>
#include <QList>
#include <memory>
#include <sys/stat.h>

struct FileCommentPair { QString name, value; };
struct FileCommentDocument {
    QString folder, error;
    QByteArray bytes;
    QList<FileCommentPair> pairs;
    struct stat directory{}, file{};
    bool exists = false, cancelled = false;
    QString value(const QString &name) const;
};

// TextPairs.cpp / FSFolder.cpp semantics, with guarded atomic saving.
// Call on workers: these routines never reference a GUI object.
namespace FileComments {
FileCommentDocument parse(QByteArray bytes, std::shared_ptr<OperationControl> control = {});
FileCommentDocument readAt(int directory, std::shared_ptr<OperationControl> control = {});
FileCommentDocument read(QString folder, std::shared_ptr<OperationControl> control = {});
QString write(const FileCommentDocument &, QString name, QString value, std::shared_ptr<OperationControl> control = {});
QString display(QString value);
}
