// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once
#include "Overwrite.h"
#include <QDialog>
class OverwriteDialog final : public QDialog {
    Q_OBJECT
public:
    explicit OverwriteDialog(const OverwriteConflict &conflict, QWidget *parent = nullptr, bool extraButtons = true, bool defaultNo = false);
    OverwriteAnswer answer() const { return selected; }
protected:
    void reject() override;
private:
    OverwriteAnswer selected = OverwriteAnswer::Cancel;
};
