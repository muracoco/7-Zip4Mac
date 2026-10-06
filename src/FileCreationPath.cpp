// SPDX-License-Identifier: LGPL-3.0-or-later
#include "FileOperations.h"
#include <QDir>
#include <QFileInfo>

namespace {
struct FString {
    QString value;
    void Empty() { value.clear(); }
    FString &operator+=(const QString &text) { value += text; return *this; }
};
bool IsAbsolutePath(const wchar_t *name) { return *name == L'/'; }
QString us2fs(const wchar_t *name) { return QString::fromWCharArray(name); }
struct CFSFolder {
    QString _path;
    void GetAbsPath(const wchar_t *name, FString &absPath);
};
#include "upstream/FilesystemCreationPath.inc"
}
QString FileOperations::creationPath(const QString &folder, const QString &name) {
    if (name.isEmpty() || name.contains(QChar::Null)) return {};
    const auto base = QFileInfo(folder).canonicalFilePath();
    if (!QDir::isAbsolutePath(name) && base.isEmpty()) return {};
    CFSFolder source{base + '/'}; FString result;
    const auto wide = name.toStdWString(); source.GetAbsPath(wide.c_str(), result);
    auto path = QDir::cleanPath(result.value);
    // macOS standard root aliases are valid explicit absolute destinations.
    // Do not canonicalize arbitrary entered symlinks: no-follow installation
    // still guards every entered directory component after lexical resolution.
    for (const auto &alias : {QStringLiteral("/var"), QStringLiteral("/tmp"), QStringLiteral("/etc")}) {
        if (path == alias || path.startsWith(alias + '/')) {
            const auto resolved = QFileInfo(alias).canonicalFilePath();
            if (!resolved.isEmpty()) path = resolved + path.mid(alias.size());
            break;
        }
    }
    return path;
}
