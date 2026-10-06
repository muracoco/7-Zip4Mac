// SPDX-License-Identifier: LGPL-3.0-or-later
#include "ResourceDialogs.h"
#include <QDialog>
#include <QDialogButtonBox>
#include <QEvent>
#include <QFontMetricsF>
#include <QGroupBox>
#include <QComboBox>
#include <QLineEdit>
#include <QLabel>
#include <QLayout>
#include <QPushButton>
#include <QScopedValueRollback>
#include <QStylePainter>
#include <QStyleOptionButton>
#include <QAbstractButton>
#include <QRadioButton>
#include <algorithm>

enum class ResourceKind { LTEXT, RTEXT, CTEXT, GROUPBOX, COMBOBOX, EDITTEXT, PUSHBUTTON, DEFPUSHBUTTON, CONTROL, ICON };
struct ResourceControl { int id; ResourceKind kind; QRect units; bool multiline; };
struct ResourceDialog { int id; QSize size; QList<ResourceControl> controls; QMap<QString, int> names; };
namespace {
#include "upstream/DialogGeometry.inc"
const ResourceDialog *resourceDialog(int id) { for (auto item : dialogs) if (item->id == id) return item; qFatal("Unknown original dialog resource: %d", id); }
using UINT = unsigned; using WPARAM = int; using HWND = QWidget *;
struct RECT { int left, top, right, bottom; };
#define RECT_SIZE_Y(r) ((r).bottom - (r).top)
#define RECT_SIZE_X(r) ((r).right - (r).left)
#define Z7_ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
constexpr int IDT_PROGRESS_ELAPSED = 3900, IDT_PROGRESS_REMAINING = 3901, IDT_PROGRESS_FILES = 1032, IDT_PROGRESS_ERRORS = 3906;
constexpr int IDT_PROGRESS_TOTAL = 3902, IDT_PROGRESS_SPEED = 3903, IDT_PROGRESS_PROCESSED = 3904, IDT_PROGRESS_PACKED = 1008, IDT_PROGRESS_RATIO = 3905;
constexpr int IDT_PROGRESS_ELAPSED_VAL = 120, IDT_PROGRESS_REMAINING_VAL = 121, IDT_PROGRESS_FILES_VAL = 111, IDT_PROGRESS_FILES_TOTAL = 112, IDT_PROGRESS_ERRORS_VAL = 126;
constexpr int IDT_PROGRESS_TOTAL_VAL = 122, IDT_PROGRESS_SPEED_VAL = 123, IDT_PROGRESS_PROCESSED_VAL = 124, IDT_PROGRESS_PACKED_VAL = 110, IDT_PROGRESS_RATIO_VAL = 125;
constexpr int IDT_PROGRESS_STATUS = 103, IDT_PROGRESS_FILE_NAME = 102, IDC_PROGRESS1 = 100, IDL_PROGRESS_MESSAGES = 101;
constexpr int IDOK = 1, IDCANCEL = 2, IDCLOSE = 8, IDB_PAUSE = 446, IDB_PROGRESS_BACKGROUND = 444;
constexpr int MY_PROGRESS_LABEL_UNITS_MIN = 60, MY_PROGRESS_VAL_UNITS = 72, MY_PROGRESS_PAD_UNITS = 4;
struct CProgressDialog {
    const QMap<int, QPointer<QWidget>> &widgets; qreal scale;
    int _buttonSizeY = 0, _buttonSizeX = 0; unsigned _numReduceSymbols = 0;
    struct Window {
        QWidget *widget = nullptr;
        void Move(int x, int y, int width, int height) { if (widget) widget->setGeometry(x, y, width, height); }
    } _messageList;
    int Units_To_Pixels_X(int value) const { return qRound(value * scale); }
    HWND GetItem(int id) { return widgets.value(id); }
    void GetClientRectOfItem(int id, RECT &rect) { const auto r = widgets.value(id)->geometry(); rect = {r.x(), r.y(), r.x() + r.width(), r.y() + r.height()}; }
    void InvalidateRect(void *) {}
    void MoveItem(int id, int x, int y, int width, int height) { if (auto widget = GetItem(id)) widget->setGeometry(x, y, width, height); }
    bool OnSize(WPARAM, int, int);
};
void ChangeSubWindowSizeX(HWND window, int width) { if (window) window->resize(width, window->height()); }
#include "upstream/ProgressLayout.inc"
struct CListViewDialog {
    const QMap<int, QPointer<QWidget>> &widgets; qreal horizontal, vertical;
    CProgressDialog::Window _listView;
    void GetMargins(int value, int &x, int &y) { x = qRound(value * horizontal); y = qRound(value * vertical); }
    void GetItemSizes(unsigned id, int &width, int &height) { const auto w = widgets.value(id); width = w ? w->width() : 0; height = w ? w->height() : 0; }
    void InvalidateRect(void *) {}
    void MoveItem(unsigned id, int x, int y, int width, int height) { if (auto w = widgets.value(id)) w->setGeometry(x, y, width, height); }
    bool OnSize(WPARAM, int, int);
};
#include "upstream/ListViewLayout.inc"
struct CComboDialog : CListViewDialog {
    HWND _comboBox = nullptr;
    bool OnSize(WPARAM, int, int);
};
#include "upstream/ComboLayout.inc"
constexpr unsigned IDB_COPY_SET_PATH = 102, IDT_COPY_INFO = 103;
struct CCopyDialog : CListViewDialog {
    HWND _path = nullptr;
    HWND GetItem(unsigned id) { return widgets.value(id); }
    void GetClientRectOfItem(unsigned id, RECT &rect) { const auto r = widgets.value(id)->geometry(); rect = {r.x(), r.y(), r.x() + r.width(), r.y() + r.height()}; }
    bool OnSize(WPARAM, int, int);
};
namespace NControl {
struct CStatic { QWidget *widget = nullptr; void Attach(QWidget *value) { widget = value; } void Move(int x, int y, int w, int h) { if (widget) widget->setGeometry(x, y, w, h); } };
}
#include "upstream/CopyLayout.inc"
struct CEditDialog : CListViewDialog {
    CProgressDialog::Window _edit;
    bool OnSize(WPARAM, int, int);
};
#include "upstream/EditLayout.inc"

constexpr unsigned IDS_BUTTON_DELETE = 7205, IDT_BROWSE2_FOLDER = 101, IDC_BROWSE2_FILTER = 103, IDL_BROWSE2 = 100, IDHELP = 9;
struct CBrowseDialog2 : CListViewDialog {
    CProgressDialog::Window _filterCombo, _list;
    QMap<unsigned, QRect> footer;
    void GetClientRectOfItem(unsigned id, RECT &rect) { const auto r = widgets.value(id)->geometry(); rect = {r.x(), r.y(), r.x() + r.width(), r.y() + r.height()}; }
    void MoveItem(unsigned id, int x, int y, int width, int height) {
        // The Win32 controls are direct children. Qt's standard buttons live
        // in a button box, so collect their dialog-relative rectangles first.
        if (id == IDCLOSE || id == IDHELP) footer.insert(id, {x, y, width, height});
        else if (auto w = widgets.value(id)) w->setGeometry(x, y, width, height);
    }
    bool OnSize(WPARAM, int, int);
};
#include "upstream/TempBrowseLayout.inc"

#undef Z7_ARRAY_SIZE
#undef RECT_SIZE_Y
#undef RECT_SIZE_X
}
void ResourceCheckBox::paintEvent(QPaintEvent *event) {
    if (!property("resourceMultiline").toBool()) { QCheckBox::paintEvent(event); return; }
    QStyleOptionButton option; initStyleOption(&option); QStylePainter painter(this);
    const auto content = style()->subElementRect(QStyle::SE_CheckBoxContents, &option, this);
    const auto text = option.text; option.text.clear(); painter.drawControl(QStyle::CE_CheckBox, option);
    painter.drawItemText(content, Qt::AlignVCenter | Qt::TextWordWrap | Qt::TextShowMnemonic, palette(), isEnabled(), text, QPalette::WindowText);
}
bool ResourceCheckBox::hitButton(const QPoint &point) const { return property("resourceMultiline").toBool() ? rect().contains(point) : QCheckBox::hitButton(point); }
ResourceDialogLayout::ResourceDialogLayout(QDialog *window, int resource) : QObject(window), dialog(window), definition(resourceDialog(resource)) { setObjectName("officialDialogLayout"); measure(); }
void ResourceDialogLayout::measure() {
    const QFontMetricsF metrics(dialog->font(), dialog);
    // Preserve original dialog-unit positions. The font substitute determines
    // the unit size; translated text/fallback glyphs can require larger units.
    horizontal = qMax(qreal(1), std::floor((metrics.horizontalAdvance("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz") / 26 + 1) / 2) / 4);
    vertical = qMax(qreal(1), metrics.height() / 8);
    struct Text { QString value; QRect units; QFont font; QWidget *device; int padding; bool wrap; };
    QList<Text> texts;
    for (const auto &control : definition->controls) if (auto widget = widgets.value(control.id)) {
        QString value; int padding = 2; bool wrap = control.multiline;
        if (auto label = qobject_cast<QLabel *>(widget.data())) { value = label->text(); wrap = control.units.height() > 10; }
        else if (auto button = qobject_cast<QAbstractButton *>(widget.data())) {
            value = button->text(); padding = 16;
            if (qobject_cast<QCheckBox *>(button) || qobject_cast<QRadioButton *>(button)) {
                const auto metric = qobject_cast<QRadioButton *>(button) ? QStyle::PM_ExclusiveIndicatorWidth : QStyle::PM_IndicatorWidth;
                padding = button->style()->pixelMetric(metric, nullptr, button) + button->style()->pixelMetric(QStyle::PM_CheckBoxLabelSpacing, nullptr, button) + 4;
                vertical = qMax(vertical, qreal(button->style()->pixelMetric(QStyle::PM_IndicatorHeight, nullptr, button)) / control.units.height());
            }
        } else if (auto group = qobject_cast<QGroupBox *>(widget.data())) { value = group->title(); padding = 16; }
        if (value.isEmpty()) continue;
        const auto label = qobject_cast<QLabel *>(widget.data());
        if (!label || label->buddy()) { value.replace("&&", QString(QChar(0xfffc))); value.remove('&'); value.replace(QChar(0xfffc), '&'); }
        const QFontMetricsF font(widget->font(), widget);
        // A literal path/value must not make the dialog arbitrarily wide, but
        // its fallback font still needs sufficient row height.
        if (qobject_cast<QLabel *>(widget.data())) vertical = qMax(vertical, (font.boundingRect(value.section('\n', 0, 0)).height() + 1) / 8);
        if (widget->property("uiLiteral").toBool() && !widget->property("resourceTextFit").toBool()) continue;
        const bool translatable = widget->property("uiResourceId").isValid() || widget->property("uiText").isValid() || widget->property("resourceTextFit").toBool();
        if (!translatable) continue;
        auto units = control.units;
        // Original Progress OnSize guarantees a 60-unit label column.
        if (definition->id == 97 && control.id >= 3900 && control.id <= 3906) units.setWidth(60);
        texts.append({value, units, widget->font(), widget, padding, wrap});
    }
    for (const auto &text : texts) if (!text.wrap && text.units.width() > 0) {
        const QFontMetricsF font(text.font, text.device);
        horizontal = qMax(horizontal, (font.horizontalAdvance(text.value) + text.padding) / text.units.width());
    }
    for (const auto &text : texts) if (text.wrap && text.units.width() > 0 && text.units.height() > 0) {
        const QFontMetricsF font(text.font, text.device);
        const auto fits = [&](qreal unit) {
            const qreal width = qMax(qreal(1), std::floor(text.units.width() * unit) - text.padding);
            return font.boundingRect(QRectF(0, 0, width, 100000), Qt::TextWordWrap, text.value).height() <= std::floor(text.units.height() * vertical);
        };
        // Find the smallest uniform horizontal unit that fits the source's
        // multiline rectangle within this bounded search interval.
        if (!fits(horizontal)) {
            qreal low = horizontal, high = qMax(horizontal, qreal(8));
            if (fits(high)) { for (int n = 0; n < 20; ++n) { const auto middle = (low + high) / 2; if (fits(middle)) high = middle; else low = middle; } horizontal = high; }
        }
    }
}
QRect ResourceDialogLayout::toPixels(QRect units) const { return {qRound(units.x() * horizontal), qRound(units.y() * vertical), qRound(units.width() * horizontal), qRound(units.height() * vertical)}; }
QSize ResourceDialogLayout::dialogSize() const { return toPixels(QRect({}, definition->size)).size(); }
QRect ResourceDialogLayout::controlRect(QString name) const { const auto id = definition->names.value(name, -1); for (const auto &control : definition->controls) if (control.id == id) return toPixels(control.units); return {}; }
void ResourceDialogLayout::bind(QString name, QWidget *widget) {
    if (!widget || !definition->names.contains(name)) qFatal("Unknown original control: %s", qPrintable(name));
    widgets.insert(definition->names.value(name), widget);
}
void ResourceDialogLayout::bindButtons(QDialogButtonBox *box) {
    buttons = box;
    box->installEventFilter(this);
    for (const auto &pair : {qMakePair(QDialogButtonBox::Ok, "IDOK"), qMakePair(QDialogButtonBox::Cancel, "IDCANCEL"), qMakePair(QDialogButtonBox::Close, "IDCLOSE"), qMakePair(QDialogButtonBox::Help, "IDHELP")}) if (auto button = box->button(pair.first)) bind(pair.second, button);
}
void ResourceDialogLayout::container(QWidget *widget, QStringList controls) {
    QRect bounds;
    for (const auto &name : controls) for (const auto &control : definition->controls) if (control.id == definition->names.value(name)) bounds = bounds.isNull() ? control.units : bounds.united(control.units);
    containers.insert(widget, bounds);
}
void ResourceDialogLayout::extra(QWidget *widget, QRect units) { extras.insert(widget, units); }
void ResourceDialogLayout::install() {
    for (auto widget : dialog->findChildren<QWidget *>()) {
        const auto id = widget->property("uiResourceId").toInt(); if (!id || widgets.contains(id)) continue;
        for (const auto &control : definition->controls) if (control.id == id) widgets.insert(id, widget);
    }
    // Delete only the layouts constructed by these dialogs. Keep the private
    // Qt layouts inside comboboxes and QDialogButtonBox intact.
    delete dialog->layout();
    for (const auto &pointer : widgets) if (auto group = qobject_cast<QGroupBox *>(pointer.data())) delete group->layout();
    for (auto widget : containers.keys()) delete widget->layout();
    for (const auto &control : definition->controls) if (auto widget = widgets.value(control.id)) {
        widget->setProperty("resourceControlId", control.id); widget->setProperty("resourceMultiline", control.multiline);
        widget->setMinimumSize(0, 0); widget->setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
        if (qobject_cast<QAbstractButton *>(widget.data()) || qobject_cast<QComboBox *>(widget.data()) || qobject_cast<QLineEdit *>(widget.data())) widget->setStyleSheet("min-width: 0px; min-height: 0px;");
        if (auto combo = qobject_cast<QComboBox *>(widget.data())) if (combo->lineEdit()) combo->lineEdit()->setStyleSheet("min-height: 0px;");
        if (auto label = qobject_cast<QLabel *>(widget.data())) { label->setTextFormat(Qt::PlainText); label->setWordWrap(control.kind != ResourceKind::ICON && control.units.height() > 10); label->setAlignment((control.kind == ResourceKind::RTEXT ? Qt::AlignRight : control.kind == ResourceKind::CTEXT ? Qt::AlignHCenter : Qt::AlignLeft) | Qt::AlignTop); }
        if (control.kind == ResourceKind::DEFPUSHBUTTON) if (auto button = qobject_cast<QPushButton *>(widget.data())) button->setDefault(true);
    }
    dialog->ensurePolished(); for (const auto &widget : widgets) if (widget) widget->ensurePolished();
    installed = true; measure(); dialog->installEventFilter(this);
    if (definition->id == 97) { dialog->setMinimumSize(toPixels({0, 0, 248, definition->size.height()}).size()); dialog->resize(dialogSize()); }
    else if ((definition->id == 99 || definition->id == 94 || definition->id == 93 || definition->id == 98 || definition->id == 96)) { dialog->setMinimumSize(dialogSize() / 2); dialog->resize(dialogSize()); }
    else dialog->setFixedSize(dialogSize());
    QWidget *previous = nullptr;
    for (const auto &control : definition->controls) if (auto widget = widgets.value(control.id)) if (widget->focusPolicy() != Qt::NoFocus) { if (previous) QWidget::setTabOrder(previous, widget); previous = widget; }
    refresh();
}
void ResourceDialogLayout::refresh() {
    if (!installed || refreshing) return; QScopedValueRollback guard(refreshing, true);
    auto place = [this](QWidget *widget, QRect units) { const auto bounds = toPixels(units); widget->setGeometry(QRect(widget->parentWidget() == dialog ? bounds.topLeft() : widget->parentWidget()->mapFrom(dialog, bounds.topLeft()), bounds.size())); };
    for (const auto &control : definition->controls) if (control.kind == ResourceKind::GROUPBOX) if (auto widget = widgets.value(control.id)) place(widget, control.units);
    for (auto it = containers.cbegin(); it != containers.cend(); ++it) place(it.key(), it.value());
    QRect buttonBounds;
    for (const auto &control : definition->controls) if (auto widget = widgets.value(control.id)) {
        if (buttons && widget->parentWidget() == buttons) {
            const auto bounds = toPixels(control.units); widget->setFixedSize(bounds.size()); buttonBounds = buttonBounds.isNull() ? bounds : buttonBounds.united(bounds);
        } else if (control.kind != ResourceKind::GROUPBOX) {
            place(widget, control.units);
            // AboutDialog.rc uses SS_REALSIZEIMAGE: its 110x63 icon is larger
            // than the template's nominal 32x32-DLU placeholder.
            if (control.kind == ResourceKind::ICON) if (auto label = qobject_cast<QLabel *>(widget.data())) if (!label->pixmap().isNull()) label->resize(label->pixmap().deviceIndependentSize().toSize());
        }
    }
    for (auto it = extras.cbegin(); it != extras.cend(); ++it) place(it.key(), it.value());
    if (definition->id == 2900) if (auto port = dialog->findChild<QLabel *>("aboutPortInfo")) {
        const auto logo = widgets.value(-1002); const auto margin = toPixels({0, 0, 8, 8}).size();
        const auto left = logo ? logo->geometry().right() + 1 + margin.width() : toPixels({48, 0, 0, 0}).x();
        port->setGeometry(left, margin.height(), qMax(0, dialog->width() - left - margin.width()), toPixels({0, 0, 0, 40}).height());
    }
    if (buttons) {
        buttons->layout()->setContentsMargins(0, 0, 0, 0); buttons->layout()->setSpacing(toPixels({0, 0, 8, 8}).width()); buttons->setGeometry(buttonBounds); buttons->layout()->activate();
        // Round each source coordinate once. A Qt box layout distributes
        // fractional spacing differently, introducing a one-pixel offset.
        for (const auto &control : definition->controls) if (auto widget = widgets.value(control.id)) if (widget->parentWidget() == buttons)
            widget->setGeometry(toPixels(control.units).translated(-buttonBounds.topLeft()));
    }
    if (definition->id == 97) {
        CProgressDialog layout{widgets, horizontal}; layout._buttonSizeX = controlRect("IDB_PAUSE").width(); layout._buttonSizeY = controlRect("IDB_PAUSE").height(); layout._messageList.widget = widgets.value(101);
        layout.OnSize(0, dialog->width(), dialog->height());
        dialog->setProperty("progressFileNameCapacity", layout._numReduceSymbols);
    }
    if (definition->id == 98) { CComboDialog layout{{widgets, horizontal, vertical, {}}, widgets.value(101)}; layout.OnSize(0, dialog->width(), dialog->height()); }
    if (definition->id == 96) { CCopyDialog layout{{widgets, horizontal, vertical, {}}, widgets.value(101)}; layout.OnSize(0, dialog->width(), dialog->height()); }
    if (definition->id == 99) { CListViewDialog layout{widgets, horizontal, vertical, {widgets.value(100)}}; layout.OnSize(0, dialog->width(), dialog->height()); }
    if (definition->id == 93) { CBrowseDialog2 layout{{widgets, horizontal, vertical, {}}, {widgets.value(103)}, {widgets.value(100)}, {}}; layout.OnSize(0, dialog->width(), dialog->height());
        if (buttons) {
            QRect bounds; for (const auto &rect : layout.footer) bounds = bounds.isNull() ? rect : bounds.united(rect);
            buttons->setGeometry(bounds); buttons->layout()->activate();
            for (auto it = layout.footer.cbegin(); it != layout.footer.cend(); ++it) if (auto widget = widgets.value(it.key())) widget->setGeometry(it.value().translated(-bounds.topLeft()));
        }
        if (auto status = dialog->findChild<QLabel *>("temporaryStatus")) { const auto left = toPixels({152, 8, 0, 0}).x(); status->setGeometry(left, toPixels({0, 8, 0, 0}).y(), qMax(0, dialog->width() - left - toPixels({0, 0, 8, 0}).width()), toPixels({0, 0, 0, 18}).height()); }
    }
    if (definition->id == 94) { CEditDialog layout{{widgets, horizontal, vertical, {}}, {widgets.value(100)}}; layout.OnSize(0, dialog->width(), dialog->height()); }
}
void ResourceDialogLayout::refreshText() {
    if (!installed) return; const auto previous = dialogSize(); measure();
    if (definition->id == 97) dialog->setMinimumSize(toPixels({0, 0, 248, definition->size.height()}).size());
    else if (definition->id == 99 || definition->id == 94 || definition->id == 93 || definition->id == 98 || definition->id == 96) dialog->setMinimumSize(dialogSize() / 2);
    else dialog->setFixedSize(dialogSize());
    if ((definition->id == 97 || definition->id == 99 || definition->id == 94 || definition->id == 93 || definition->id == 98 || definition->id == 96) && previous != dialogSize())
        dialog->resize(qRound(dialog->width() * qreal(dialogSize().width()) / previous.width()), qRound(dialog->height() * qreal(dialogSize().height()) / previous.height()));
    refresh();
}
bool ResourceDialogLayout::eventFilter(QObject *object, QEvent *event) {
    if (object == buttons) { if (event->type() == QEvent::LayoutRequest || event->type() == QEvent::Resize) refresh(); return false; }
    if (event->type() == QEvent::FontChange || event->type() == QEvent::ApplicationFontChange) refreshText();
    if (event->type() == QEvent::Resize || event->type() == QEvent::Show || event->type() == QEvent::FontChange || event->type() == QEvent::ApplicationFontChange) refresh();
    return false;
}
