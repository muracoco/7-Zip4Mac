// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once
#include <QString>
#include <QStringList>
class QMenu;
struct FileMenuState {
    bool readOnly = false, hashFolder = false, filesystem = true;
    bool allFiles = true, alternateStreams = false, largeScreen = true, programMenu = false;
    unsigned count = 0;
    QString diff, versionStore, filePath;
};
void appendOfficialFileMenu(QMenu *destination, QMenu *source, const FileMenuState &state);
QStringList officialVersionMenuItems(const FileMenuState &state);
