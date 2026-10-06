// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once
#include <QComboBox>
#include <functional>

class AddressCombo : public QComboBox {
    Q_OBJECT
public:
    explicit AddressCombo(QWidget *parent = nullptr);
    std::function<QString()> currentLocation;
    void showPopup() override;
    void hidePopup() override;
};
