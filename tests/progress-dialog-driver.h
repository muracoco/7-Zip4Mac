// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once
#include "ChecksumResultsDialog.h"
#include <QApplication>
#include <QMessageBox>
#include <QPushButton>
#include <QTimer>
#include <QTreeWidget>

// Only dismiss result dialogs owned by this test application. Cancellation
// questions and other app/user dialogs are never answered by this driver.
class ProgressDialogDriver : public QObject {
    QTimer timer;
public:
    QVector<ProgressFinalNotice> notices;
    QVector<ChecksumPresentation> checksums;
    ProgressDialogDriver() {
        timer.setInterval(10);
        connect(&timer, &QTimer::timeout, this, [this] {
            for (auto widget : QApplication::topLevelWidgets()) {
                if (!widget->isVisible() || widget->property("testResultHandled").toBool()) continue;
                if (widget->objectName() == "progressFinalMessage") {
                    auto message = qobject_cast<QMessageBox *>(widget); if (!message) continue;
                    widget->setProperty("testResultHandled", true);
                    notices.append({message->windowTitle(), message->text(), message->icon() == QMessageBox::Critical});
                    message->button(QMessageBox::Ok)->click();
                } else if (widget->objectName() == "checksumResultsDialog") {
                    auto dialog = qobject_cast<ChecksumResultsDialog *>(widget); if (!dialog) continue;
                    widget->setProperty("testResultHandled", true);
                    ChecksumPresentation view; view.title = dialog->windowTitle();
                    auto tree = dialog->findChild<QTreeWidget *>("checksumResults");
                    for (int i = 0; i < tree->topLevelItemCount(); ++i) { const auto item = tree->topLevelItem(i); view.rows.append({item->text(0), item->data(1, Qt::UserRole).toString()}); }
                    checksums.append(view); dialog->accept();
                }
            }
        });
        timer.start();
    }
    void clear() { notices.clear(); checksums.clear(); }
};
