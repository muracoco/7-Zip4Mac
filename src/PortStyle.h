#pragma once
#include <QProxyStyle>
#include <QDialogButtonBox>
#include <QApplication>
#include <QStyleFactory>
#include <QStyleHints>
#include <QKeySequence>
// Public Qt function documented in qkeysequence.cpp; Qt's installed headers
// require the application to declare this prototype before use.
QT_BEGIN_NAMESPACE
void Q_GUI_EXPORT qt_set_sequence_auto_mnemonic(bool enabled);
QT_END_NAMESPACE
class PortStyle : public QProxyStyle {
public:
    explicit PortStyle(QStyle *style) : QProxyStyle(style) {}
    int styleHint(StyleHint hint, const QStyleOption *option = nullptr, const QWidget *widget = nullptr, QStyleHintReturn *data = nullptr) const override {
        if (hint == SH_DialogButtonLayout) return QDialogButtonBox::WinLayout;
        return QProxyStyle::styleHint(hint, option, widget, data);
    }
};

inline void applyPortAppearance(QApplication &app) {
    // Qt disables resource/menu access keys on macOS by default. Preserve the
    // original Windows ampersand mnemonics for menus, labels and buttons.
    qt_set_sequence_auto_mnemonic(true);
    // Fusion's checkbox outlines also consult the color scheme. Keep the
    // light Windows-like client area readable when macOS uses dark appearance.
    app.styleHints()->setColorScheme(Qt::ColorScheme::Light);
    app.setStyle(new PortStyle(QStyleFactory::create("Fusion")));
    QPalette palette = app.style()->standardPalette(); palette.setColor(QPalette::Window, QColor(240, 240, 240)); palette.setColor(QPalette::Base, Qt::white); palette.setColor(QPalette::Text, Qt::black); palette.setColor(QPalette::WindowText, Qt::black); palette.setColor(QPalette::Button, QColor(240, 240, 240)); palette.setColor(QPalette::ButtonText, Qt::black); palette.setColor(QPalette::Highlight, QColor(0, 120, 215)); palette.setColor(QPalette::HighlightedText, Qt::white);
    // The role-only overload above also sets the Disabled group. Restore its
    // Windows-like grey so unavailable menu commands do not appear clickable.
    for (auto role : {QPalette::Text, QPalette::WindowText, QPalette::ButtonText})
        palette.setColor(QPalette::Disabled, role, QColor(128, 128, 128));
    app.setPalette(palette);
    app.setFont(QFont("Arial", 9));
    app.setStyleSheet("QTreeView { background: white; color: black; } QTreeView::item { height: 22px; } QToolBar { border-bottom: 1px solid #a0a0a0; spacing: 2px; } QHeaderView::section { padding: 3px 5px; } QLineEdit, QComboBox { min-height: 20px; } QDialog QPushButton { min-width: 65px; }");
}
