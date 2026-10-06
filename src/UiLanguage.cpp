// SPDX-License-Identifier: LGPL-3.0-or-later
#include "UiLanguage.h"
#include "LanguageDocument.h"
#include <QApplication>
#include <QTranslator>
#include <QFile>
#include <QDir>
#include <QMap>
#include <QLabel>
#include <QAbstractButton>
#include <QGroupBox>
#include <QComboBox>
#include <QTabWidget>
#include <QMenu>
#include <QTreeWidget>
#include <QRegularExpression>
#include <QAction>
#include <QSignalBlocker>
#include <QCoreApplication>
#include <QLocale>
#include <QFormLayout>
#include <algorithm>
#include <QDialogButtonBox>
#include <QPushButton>

static void initLanguageResources() { static const bool initialized = [] { Q_INIT_RESOURCE(languages); return true; }(); (void)initialized; }
namespace {
LanguageDocument load(QString file) {
    QFile source(file); if (!source.open(QIODevice::ReadOnly)) return {}; return readOfficialLanguage(source);
}
QMap<quint32, QString> readLanguage(QString file) { return load(file).entries; }
QString key(QString text) { text.remove('&'); return text.trimmed().toCaseFolded(); }
class Translator : public QTranslator {
public:
    QMap<QString, QString> words;
    QMap<quint32, QString> resources;
    bool isEmpty() const override { return false; }
    QString translate(const char *, const char *source, const char *, int) const override { return words.value(key(QString::fromUtf8(source))); }
};
Translator *translator() {
    static Translator *instance = [] { initLanguageResources(); auto t = new Translator; t->setParent(qApp); qApp->installTranslator(t); return t; }(); return instance;
}
QString original(QObject *object, const char *property, QString current) {
    auto v = object->property(property); if (!v.isValid()) { object->setProperty(property, current); return current; } return v.toString();
}
}
void UiLanguage::set(const QString &language) {
    auto t = translator(); t->words.clear(); t->resources.clear(); if (language.isEmpty() || language == "en") return;
    auto english = readLanguage(":/languages/en.ttt"), localized = readLanguage(directory() + '/' + language + ".txt");
    for (auto it = english.cbegin(); it != english.cend(); ++it) {
        auto value = localized.value(it.key()); if (!value.isEmpty() && it.key() > 3) { t->words[key(it.value())] = value; t->resources[it.key()] = value; }
    }
}
QString UiLanguage::text(const QString &english) {
    if (english == "Name-2") return text("Name") + "-2";
    if (english == "Delete Temporary Files") { auto value = text(english + "..."); value.remove("..."); return value; }
    if (english == "Open Inside *" || english == "Open Inside #") {
        auto base = text("Open &Inside"); base.remove('&'); return base + ' ' + english.back();
    }
    auto value = translator()->words.value(key(english));
    // Resources sometimes omit a trailing colon or use ellipsis elsewhere.
    if (value.isEmpty() && english.endsWith(':')) { value = translator()->words.value(key(english.chopped(1))); if (!value.isEmpty() && !value.endsWith(':')) value += ':'; }
    if (value.isEmpty()) return english;
    // Toolbar labels do not acquire menu mnemonics.
    if (!english.contains('&')) value.remove('&');
    return value;
}
QString UiLanguage::resource(quint32 id, bool localized) {
    auto t = translator(); static const auto english = readLanguage(":/languages/en.ttt");
    return localized && t->resources.contains(int(id)) ? t->resources.value(int(id)) : english.value(int(id));
}
QString UiLanguage::localizedResource(quint32 id) { return translator()->resources.value(id); }
QString UiLanguage::propertyName(quint32 id, const QString &fallback, bool localized) {
    const auto value = id < 1000 ? resource(1000 + id, localized) : QString();
    return value.isEmpty() ? fallback : value;
}
QList<QPair<QString, QString>> UiLanguage::available() {
    translator(); QList<QPair<QString, QString>> values{{"en", "English : English  ---"}};
    const auto locale = QLocale().name().replace('_', '-').toLower(); const auto base = locale.section('-', 0, 0); const auto preferred = defaultLanguage();
    QList<QPair<QString, QString>> others;
    for (const auto &file : QDir(directory()).entryList({"*.txt"}, QDir::Files, QDir::Name)) {
        const auto lang = load(directory() + '/' + file); if (!lang.valid) continue;
        const auto code = file.chopped(4); QString label = lang.entries.value(1, code); const auto native = lang.entries.value(2);
        if (!native.isEmpty()) label += " : " + native;
        if (code.compare(preferred, Qt::CaseInsensitive) == 0 && code != "en") label += "  ***";
        else if (code.section('-', 0, 0).compare(base, Qt::CaseInsensitive) == 0) label += "  +++";
        others.append({code, label});
    }
    std::stable_sort(others.begin(), others.end(), [](const auto &a, const auto &b) { return QString::compare(a.second, b.second, Qt::CaseInsensitive) < 0; });
    values.append(others); return values;
}
QString UiLanguage::directory() {
    const auto path = QCoreApplication::applicationDirPath() + "/../Resources/Lang";
    return QDir(path).exists() ? QDir::cleanPath(path) : QString(":/languages");
}
QString UiLanguage::defaultLanguage(const QLocale &locale) {
    initLanguageResources();
    QStringList candidates{locale.name().replace('_', '-').toLower()};
    // Windows obtains these regional/script variants from its LCID table.
    // QLocale supplies the corresponding macOS language/script/territory.
    if (locale.language() == QLocale::Chinese) candidates << (locale.script() == QLocale::TraditionalHanScript ? "zh-tw" : "zh-cn");
    if (locale.language() == QLocale::Serbian) candidates << (locale.script() == QLocale::LatinScript ? "sr-spl" : "sr-spc");
    if (locale.language() == QLocale::Uzbek && locale.script() == QLocale::CyrillicScript) candidates << "uz-cyrl";
    if (locale.language() == QLocale::Kurdish && locale.script() == QLocale::ArabicScript) candidates << "ku-ckb";
    if (locale.language() == QLocale::Mongolian && locale.script() == QLocale::MongolianScript) candidates << "mng";
    if (locale.name().section('_', 0, 0) == "ckb") candidates << "ku-ckb";
    candidates << locale.bcp47Name().toLower() << locale.name().section('_', 0, 0).toLower(); candidates.removeDuplicates();
    for (const auto &code : candidates) { if (code == "en" || code.startsWith("en-")) return "en"; if (load(directory() + '/' + code + ".txt").valid) return code; }
    return "en";
}
QString UiLanguage::information(const QString &language) {
    translator(); const auto english = load(":/languages/en.ttt");
    return officialLanguageInformation(language, language == "en" ? english : load(directory() + '/' + language + ".txt"), english);
}
QStringList UiLanguage::invalidFiles() {
    translator(); QStringList files; for (const auto &file : QDir(directory()).entryList({"*.txt"}, QDir::Files, QDir::Name)) if (!load(directory() + '/' + file).valid) files << file; return files;
}
void UiLanguage::bind(QObject *object, quint32 id, bool removeMnemonic) {
    if (!object) return; object->setProperty("uiResourceId", id); object->setProperty("uiRemoveMnemonic", removeMnemonic);
}
void UiLanguage::bindTitle(QWidget *widget, quint32 id) { widget->setProperty("uiTitleId", id); }
void UiLanguage::bindFormLabel(QWidget *field, quint32 id) {
    if (!field || !field->parentWidget()) return;
    for (auto layout : field->parentWidget()->findChildren<QFormLayout *>()) if (auto label = layout->labelForField(field)) { bind(label, id); if (auto text = qobject_cast<QLabel *>(label)) label->setProperty("uiAppendColon", text->text().endsWith(':')); return; }
}
void UiLanguage::apply(QWidget *widget) {
    auto translated = [](QObject *object, QString fallback) {
        const auto id = object->property("uiResourceId").toUInt();
        auto result = id ? resource(id) : text(original(object, "uiText", fallback));
        if (result.isEmpty()) result = fallback;
        if (object->property("uiAppendColon").toBool() && !result.endsWith(':')) result += ':';
        if (object->property("uiRemoveMnemonic").toBool()) result.remove('&'); return result;
    };
    QList<QWidget *> widgets = widget->findChildren<QWidget *>(); widgets.prepend(widget);
    for (auto w : widgets) {
        // A parent refresh must not restore cached labels in a separate dialog
        // (notably the finished progress result and its Close button).
        if (w->window() != widget->window()) continue;
        if (w->property("uiLiteral").toBool()) continue;
        if (auto box = qobject_cast<QDialogButtonBox *>(w)) for (const auto &pair : {qMakePair(QDialogButtonBox::Ok, 401u), qMakePair(QDialogButtonBox::Cancel, 402u), qMakePair(QDialogButtonBox::Yes, 406u), qMakePair(QDialogButtonBox::No, 407u), qMakePair(QDialogButtonBox::Close, 408u), qMakePair(QDialogButtonBox::Help, 409u)}) if (auto button = box->button(pair.first)) bind(button, pair.second);
        if (w->isWindow()) {
            if (w->property("uiManagedTitle").toBool()) QMetaObject::invokeMethod(w, "refreshPresentation", Qt::DirectConnection);
            else if (const auto operation = w->property("uiTitleOperation"); operation.isValid()) { const auto prefix = w->property("uiTitlePrefixId").toUInt(); w->setWindowTitle((prefix ? resource(prefix) + " - " : QString()) + text(operation.toString())); }
            else { const auto id = w->property("uiTitleId").toUInt(); w->setWindowTitle(id ? resource(id) : text(original(w, "uiTitle", w->windowTitle()))); }
        }
        if (auto label = qobject_cast<QLabel *>(w)) label->setText(translated(label, label->text()));
        else if (auto button = qobject_cast<QAbstractButton *>(w)) button->setText(translated(button, button->text()));
        else if (auto group = qobject_cast<QGroupBox *>(w)) group->setTitle(translated(group, group->title()));
        if (auto combo = qobject_cast<QComboBox *>(w)) if (combo->objectName() != "uiLanguage") {
            QSignalBlocker blocker(combo);
            const QString editText = combo->currentText(); const bool fixedChoice = editText == combo->itemText(combo->currentIndex());
            for (int i = 0; i < combo->count(); ++i) {
                const auto id = combo->itemData(i, ResourceIdRole).toUInt();
                if (id) combo->setItemText(i, combo->itemData(i, PrefixRole).toString() + resource(id));
                else if (!combo->isEditable()) { auto value = combo->itemData(i, Qt::UserRole + 100); if (!value.isValid()) { value = combo->itemText(i); combo->setItemData(i, value, Qt::UserRole + 100); } combo->setItemText(i, text(value.toString())); }
            }
            if (combo->isEditable() && !fixedChoice) combo->setEditText(editText);
        }
        if (auto tabs = qobject_cast<QTabWidget *>(w)) for (int n = 0; n < tabs->count(); ++n) {
            const auto page = tabs->widget(n);
            const auto fallback = original(page, "uiTab", tabs->tabText(n));
            const auto id = page->property("uiTabId").toUInt();
            const auto label = id ? resource(id) : text(fallback);
            // Untranslated resource captions (notably the fixed "7-Zip"
            // property page) retain their original label, as in the Win32 UI.
            tabs->setTabText(n, label.isEmpty() ? fallback : label);
        }
        if (auto tree = qobject_cast<QTreeWidget *>(w)) {
            for (int n = 0; n < tree->columnCount(); ++n) {
                auto header = tree->headerItem(); auto value = header->data(n, Qt::UserRole + 100); if (!value.isValid()) { value = header->text(n); header->setData(n, Qt::UserRole + 100, value); } header->setText(n, text(value.toString()));
            }
        }
    }
    for (auto action : widget->findChildren<QAction *>()) if (!action->isSeparator() && !action->property("uiLiteral").toBool()) action->setText(translated(action, action->text()));
    for (auto layout : widget->findChildren<QObject *>("officialDialogLayout")) {
        const auto owner = qobject_cast<QWidget *>(layout->parent());
        if (owner && owner->window() == widget->window()) QMetaObject::invokeMethod(layout, "refreshText", Qt::DirectConnection);
    }
    if (widget->property("uiManagedTitle").toBool()) QMetaObject::invokeMethod(widget, "refreshPresentation", Qt::DirectConnection);

}
