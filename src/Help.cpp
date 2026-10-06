// SPDX-License-Identifier: LGPL-3.0-or-later
#include "Help.h"
#include "UiLanguage.h"
#include "ResourceDialogs.h"
#include "upstream/UiResourceIds.h"
#include <QApplication>
#include <QPointer>
#include <QWindow>
#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QLineEdit>
#include <QComboBox>
#include <QMenu>
#include <QToolButton>
#include <QSettings>
#include <QSignalBlocker>
#include <QPushButton>
#include <QSplitter>
#include <QTabWidget>
#include <QTextBrowser>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QCheckBox>
#include <QLabel>
#include <QFutureWatcher>
#include <QtConcurrent>
#include <QPrintDialog>
#include <QPrinter>
#include <QRadioButton>
#include <QRegularExpression>
#include <QShortcut>
#include <QTextDocument>
#include <QSysInfo>

static void initializeHelp() { Q_INIT_RESOURCE(help); }
static void initializeAboutResources() { static const bool initialized = [] { Q_INIT_RESOURCE(resources); return true; }(); (void)initialized; }
namespace {
QPointer<HelpWindow> helpWindow;
const QJsonObject &manifest() {
    static const QJsonObject value = [] { initializeHelp(); QFile file(":/help/manifest.json"); if (!file.open(QIODevice::ReadOnly)) return QJsonObject(); return QJsonDocument::fromJson(file.readAll()).object(); }(); return value;
}
class Browser : public QTextBrowser {
public:
    using QTextBrowser::QTextBrowser;
    QVariant loadResource(int type, const QUrl &url) override {
        if (type != QTextDocument::HtmlResource && type != QTextDocument::StyleSheetResource) return {};
        return Help::resource(Help::resolve(url, source()));
    }
protected:
    void doSetSource(const QUrl &url, QTextDocument::ResourceType type) override {
        const auto resolved = Help::resolve(url, source().isEmpty() ? QUrl("qrc:/help/fm/index.htm") : source());
        if (!resolved.isEmpty() && resolved.path().endsWith(".htm")) QTextBrowser::doSetSource(resolved, type);
    }
};
class PrintDocument : public QTextDocument {
public:
    using QTextDocument::QTextDocument;
    QVariant loadResource(int type, const QUrl &url) override {
        return type == QTextDocument::StyleSheetResource ? Help::resource(Help::resolve(url)) : QVariant();
    }
};
}
QStringList Help::pages() { QStringList result; for (const auto &path : manifest().value("files").toObject().keys()) if (path.endsWith(".htm")) result << path; return result; }
QUrl Help::resolve(const QUrl &url, const QUrl &base) {
    if (!url.isValid() || !url.userInfo().isEmpty() || !url.host().isEmpty() || url.hasQuery()) return {};
    if (!url.scheme().isEmpty() && url.scheme() != "qrc") return {};
    QUrl resolved = base.resolved(url);
    // Two Add-page links in the original CHM traverse one directory too far.
    const auto text = url.toString();
    if (base.path().compare("/help/fm/plugins/7-zip/add.htm", Qt::CaseInsensitive) == 0 && (text.startsWith("../../../../cmdline/switches/method.htm") || text.startsWith("../../../../cmdline/switches/sfx.htm"))) {
        resolved.setPath("/help/" + url.path().mid(QString("../../../../").size()));
    }
    if (resolved.scheme() != "qrc" || !resolved.host().isEmpty()) return {};
    const QString path = QDir::cleanPath(resolved.path()).toCaseFolded(); if (!path.startsWith("/help/")) return {};
    const QString key = path.mid(6); if (!manifest().value("files").toObject().contains(key) || (!key.endsWith(".htm") && !key.endsWith(".css"))) return {};
    resolved.setPath(path); return resolved;
}
QByteArray Help::resource(const QUrl &url) {
    const auto resolved = resolve(url); if (resolved.isEmpty()) return {};
    QFile file(":" + resolved.path()); return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}
bool Help::external(const QUrl &url) {
    if (url.scheme() != "http" || url.host() != "www.7-zip.org" || !url.userInfo().isEmpty() || url.port() != -1 || url.hasQuery() || url.hasFragment()) return false;
    return QStringList{"", "/", "/support.html", "/recover.html"}.contains(url.path());
}
QList<HelpSearch::Topic> Help::topics() {
    QList<HelpSearch::Topic> result;
    static const QRegularExpression title("<title>(.*?)</title>", QRegularExpression::CaseInsensitiveOption | QRegularExpression::DotMatchesEverythingOption);
    for (const auto &path : pages()) {
        const auto html = QString::fromLatin1(resource(QUrl("qrc:/help/" + path)));
        PrintDocument document; document.setHtml(html);
        QTextDocument heading; heading.setHtml(title.match(html).captured(1));
        result << HelpSearch::Topic{path, heading.toPlainText(), document.toPlainText()};
    }
    return result;
}
QStringList Help::printTopics(const QUrl &source, bool subtopics) {
    const auto resolved = resolve(source); if (resolved.isEmpty() || !resolved.path().endsWith(".htm")) return {};
    const auto path = resolved.path().mid(6); QStringList result{path}; if (!subtopics) return result;
    int depth = -1;
    for (const auto &entry : manifest().value("contents").toArray()) {
        const auto object = entry.toObject(); const auto current = object.value("local").toString().toCaseFolded(); const auto level = object.value("depth").toInt();
        if (depth >= 0) { if (level <= depth) break; if (!result.contains(current) && pages().contains(current)) result << current; }
        else if (current == path) depth = level;
    }
    return result;
}
HelpWindow::HelpWindow(QWidget *parent) : QDialog(parent, Qt::Window) {
    setObjectName("helpWindow"); setWindowTitle("7-Zip Help"); resize(920, 650);
    auto layout = new QVBoxLayout(this); auto navigation = new QHBoxLayout;
    auto back = new QPushButton("Back", this), forward = new QPushButton("Forward", this), printButton = new QPushButton("Print", this); printButton->setObjectName("helpPrint"); navigation->addWidget(back); navigation->addWidget(forward); navigation->addWidget(printButton); navigation->addStretch(); layout->addLayout(navigation);
    auto splitter = new QSplitter(this); auto tabs = new QTabWidget(splitter); browser = new Browser(splitter); browser->setObjectName("helpBrowser"); browser->setOpenLinks(false); browser->setOpenExternalLinks(false);
    auto contents = new QTreeWidget(tabs); contents->setObjectName("helpContents"); contents->setHeaderHidden(true); tabs->addTab(contents, "Contents");
    QList<QTreeWidgetItem *> parents;
    for (const auto &entry : manifest().value("contents").toArray()) {
        const auto object = entry.toObject(); const int depth = qMin(object.value("depth").toInt(), int(parents.size())); while (parents.size() > depth) parents.removeLast();
        auto item = parents.isEmpty() ? new QTreeWidgetItem(contents) : new QTreeWidgetItem(parents.last()); item->setText(0, object.value("name").toString()); item->setData(0, Qt::UserRole, object.value("local").toString()); parents << item;
    }
    contents->expandToDepth(0);
    auto indexPage = new QWidget(tabs); auto indexLayout = new QVBoxLayout(indexPage); auto search = new QLineEdit(indexPage); search->setObjectName("helpIndexSearch"); auto index = new QTreeWidget(indexPage); index->setObjectName("helpIndex"); index->setHeaderHidden(true); indexLayout->addWidget(search); indexLayout->addWidget(index); tabs->addTab(indexPage, "Index");
    for (const auto &entry : manifest().value("index").toArray()) { const auto object = entry.toObject(); auto item = new QTreeWidgetItem(index); item->setText(0, object.value("name").toString()); item->setData(0, Qt::UserRole, object.value("local").toString()); }
    connect(search, &QLineEdit::textChanged, this, [index](const QString &text) { for (int n = 0; n < index->topLevelItemCount(); ++n) index->topLevelItem(n)->setHidden(!index->topLevelItem(n)->text(0).contains(text, Qt::CaseInsensitive)); });
    for (auto tree : {contents, index}) connect(tree, &QTreeWidget::itemClicked, this, [this](QTreeWidgetItem *item) { navigate(QUrl("qrc:/help/" + item->data(0, Qt::UserRole).toString())); });
    auto searchPage = new QWidget(tabs); auto searchLayout = new QVBoxLayout(searchPage);
    searchLayout->addWidget(new QLabel("Type in the keyword to find:", searchPage));
    auto queryRow = new QHBoxLayout;
    auto history = new QComboBox(searchPage); history->setObjectName("helpSearchHistory"); history->setEditable(true); history->setInsertPolicy(QComboBox::NoInsert); history->setProperty("uiLiteral", true);
    auto query = history->lineEdit(); query->setObjectName("helpSearchQuery");
    for (const auto &text : QSettings().value("Help/SearchHistory").toStringList()) {
        if (text.trimmed().isEmpty() || text.size() > 4096 || history->findText(text, Qt::MatchExactly) >= 0) continue;
        history->addItem(text); if (history->count() == 20) break;
    }
    history->setCurrentIndex(-1); query->clear();
    auto operators = new QToolButton(searchPage); operators->setObjectName("helpSearchOperators"); operators->setText(">"); operators->setToolTip("Search operators"); operators->setProperty("uiLiteral", true);
    auto operatorMenu = new QMenu(operators); operatorMenu->setObjectName("helpSearchOperatorMenu");
    for (const auto &text : {QString("AND"), QString("OR"), QString("NOT"), QString("NEAR")}) {
        auto action = operatorMenu->addAction(text); action->setObjectName("helpOperator" + text); action->setProperty("uiLiteral", true);
        connect(action, &QAction::triggered, this, [=] {
            const auto before = query->text().left(query->selectionStart() >= 0 ? query->selectionStart() : query->cursorPosition());
            const int end = query->selectionStart() >= 0 ? query->selectionStart() + query->selectedText().size() : query->cursorPosition();
            const auto after = query->text().mid(end);
            query->insert((before.isEmpty() || before.back().isSpace() ? QString() : QString(" ")) + text + (after.isEmpty() || !after.front().isSpace() ? QString(" ") : QString()));
            query->setFocus();
        });
    }
    connect(operators, &QToolButton::clicked, this, [=] { operatorMenu->popup(operators->mapToGlobal(QPoint(0, operators->height()))); });
    queryRow->addWidget(history, 1); queryRow->addWidget(operators); searchLayout->addLayout(queryRow);
    auto searchButtons = new QHBoxLayout; auto listTopics = new QPushButton("List Topics", searchPage), display = new QPushButton("Display", searchPage);
    listTopics->setObjectName("helpListTopics"); display->setObjectName("helpDisplay"); display->setEnabled(false); searchButtons->addWidget(listTopics); searchButtons->addWidget(display); searchLayout->addLayout(searchButtons);
    auto hits = new QTreeWidget(searchPage); hits->setObjectName("helpSearchResults"); hits->setHeaderLabels({"Title", "Location"}); searchLayout->addWidget(hits);
    auto previous = new QCheckBox("Search previous results", searchPage), similar = new QCheckBox("Match similar words", searchPage), titles = new QCheckBox("Search titles only", searchPage);
    previous->setObjectName("helpSearchPrevious"); similar->setObjectName("helpSearchSimilar"); titles->setObjectName("helpSearchTitles"); similar->setChecked(true); previous->setEnabled(false);
    searchLayout->addWidget(previous); searchLayout->addWidget(similar); searchLayout->addWidget(titles);
    auto searchStatus = new QLabel(searchPage); searchStatus->setObjectName("helpSearchStatus"); searchStatus->setWordWrap(true); searchLayout->addWidget(searchStatus); tabs->addTab(searchPage, "Search");
    auto watcher = new QFutureWatcher<HelpSearch::Results>(this);
    connect(listTopics, &QPushButton::clicked, this, [=] {
        HelpSearch::Options options; options.titlesOnly = titles->isChecked(); options.similarWords = similar->isChecked(); options.withinPrevious = previous->isChecked();
        for (int n = 0; n < hits->topLevelItemCount(); ++n) options.previous << hits->topLevelItem(n)->data(0, Qt::UserRole).toString();
        const auto text = query->text(); const auto allTopics = Help::topics(); watcher->setProperty("searchQuery", text); listTopics->setEnabled(false); history->setEnabled(false); operators->setEnabled(false); searchStatus->setText("Searching...");
        watcher->setFuture(QtConcurrent::run([text, allTopics, options] { return HelpSearch::search(text, allTopics, options); }));
    });
    connect(query, &QLineEdit::returnPressed, listTopics, &QPushButton::click);
    connect(watcher, &QFutureWatcher<HelpSearch::Results>::finished, this, [=] {
        const auto result = watcher->result(); listTopics->setEnabled(true); history->setEnabled(true); operators->setEnabled(true);
        if (!result.error.isEmpty()) { searchStatus->setText(result.error); emit searchFinished(-1); return; }
        const auto searched = watcher->property("searchQuery").toString().trimmed();
        if (!searched.isEmpty()) {
            QSignalBlocker blocker(history); const auto current = query->text();
            const int existing = history->findText(searched, Qt::MatchExactly); if (existing >= 0) history->removeItem(existing);
            history->insertItem(0, searched); while (history->count() > 20) history->removeItem(history->count() - 1);
            history->setCurrentIndex(-1); query->setText(current);
            QStringList saved; for (int n = 0; n < history->count(); ++n) saved << history->itemText(n);
            QSettings().setValue("Help/SearchHistory", saved);
        }
        hits->clear(); for (const auto &hit : result.hits) { auto item = new QTreeWidgetItem(hits, {hit.title, hit.path}); item->setData(0, Qt::UserRole, hit.path); }
        previous->setEnabled(true); searchStatus->setText(QString("%1 topic(s) found").arg(result.hits.size())); if (!result.hits.isEmpty()) hits->setCurrentItem(hits->topLevelItem(0));
        display->setEnabled(!result.hits.isEmpty()); emit searchFinished(result.hits.size());
    });
    connect(display, &QPushButton::clicked, this, [=] { if (auto item = hits->currentItem()) navigate(QUrl("qrc:/help/" + item->data(0, Qt::UserRole).toString())); });
    connect(hits, &QTreeWidget::itemActivated, display, [display] { display->click(); });
    splitter->setStretchFactor(0, 0); splitter->setStretchFactor(1, 1); splitter->setSizes({270, 650}); layout->addWidget(splitter);
    connect(back, &QPushButton::clicked, browser, &QTextBrowser::backward); connect(forward, &QPushButton::clicked, browser, &QTextBrowser::forward);
    connect(browser, &QTextBrowser::backwardAvailable, back, &QPushButton::setEnabled); connect(browser, &QTextBrowser::forwardAvailable, forward, &QPushButton::setEnabled); back->setEnabled(false); forward->setEnabled(false);
    connect(browser, &QTextBrowser::anchorClicked, this, [this](const QUrl &url) { if (Help::external(url)) emit externalLinkRequested(url); else navigate(url); });
    connect(printButton, &QPushButton::clicked, this, [this] {
        QDialog scope(this); scope.setWindowTitle("Print Topics"); auto layout = new QVBoxLayout(&scope);
        auto current = new QRadioButton("Print the selected topic", &scope), children = new QRadioButton("Print the selected heading and all subtopics", &scope); current->setChecked(true); children->setEnabled(Help::printTopics(source(), true).size() > 1); layout->addWidget(current); layout->addWidget(children);
        auto buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &scope); layout->addWidget(buttons); connect(buttons, &QDialogButtonBox::accepted, &scope, &QDialog::accept); connect(buttons, &QDialogButtonBox::rejected, &scope, &QDialog::reject);
        if (scope.exec() != QDialog::Accepted) return; QPrinter printer(QPrinter::HighResolution); QPrintDialog dialog(&printer, this); if (dialog.exec() == QDialog::Accepted) print(&printer, children->isChecked());
    });
    auto buttons = new QDialogButtonBox(QDialogButtonBox::Close, this); connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject); layout->addWidget(buttons);
    navigate(QUrl("qrc:/help/fm/index.htm")); UiLanguage::apply(this);
}
bool HelpWindow::navigate(const QUrl &url) {
    const auto resolved = Help::resolve(url, source().isEmpty() ? QUrl("qrc:/help/fm/index.htm") : source());
    if (resolved.isEmpty() || !resolved.path().endsWith(".htm")) return false;
    browser->setSource(resolved); return true;
}
QUrl HelpWindow::source() const { return browser->source(); }
bool HelpWindow::print(QPrinter *printer, bool subtopics) {
    if (!printer) return false; const auto pages = Help::printTopics(source(), subtopics); if (pages.isEmpty()) return false;
    if (pages.size() == 1) { browser->document()->print(printer); return printer->printerState() != QPrinter::Error; }
    static const QRegularExpression body("<body[^>]*>(.*?)</body>", QRegularExpression::CaseInsensitiveOption | QRegularExpression::DotMatchesEverythingOption);
    QString html; for (const auto &page : pages) { if (!html.isEmpty()) html += "<div style=\"page-break-before:always\"></div>"; html += body.match(QString::fromLatin1(Help::resource(QUrl("qrc:/help/" + page)))).captured(1); }
    PrintDocument document; document.setDefaultStyleSheet(QString::fromLatin1(Help::resource(QUrl("qrc:/help/style.css")))); document.setHtml(html); document.print(printer); return printer->printerState() != QPrinter::Error;
}
void Help::show(QWidget *parent, const QString &topic) {
    // Official HelpUtils.cpp uses HtmlHelp(NULL, ..., HH_DISPLAY_TOPIC, 0):
    // one independent help window survives the calling modal dialog.
    if (!helpWindow) {
        helpWindow = new HelpWindow;
        helpWindow->setAttribute(Qt::WA_DeleteOnClose);
        if (parent) helpWindow->setScreen(parent->screen());
        QObject::connect(helpWindow, &HelpWindow::externalLinkRequested, helpWindow, [](const QUrl &url) { QDesktopServices::openUrl(url); });
        QObject::connect(qApp, &QCoreApplication::aboutToQuit, helpWindow, &QWidget::close);
    }
    // Keep QObject ownership independent, while allowing the help window to
    // receive input when it is opened from an application-modal Qt dialog.
    helpWindow->winId();
    helpWindow->windowHandle()->setTransientParent(parent ? parent->window()->windowHandle() : nullptr);
    helpWindow->navigate(QUrl("qrc:/help/" + topic)); UiLanguage::apply(helpWindow);
    helpWindow->show(); helpWindow->raise();
}
HelpWindow *Help::currentWindow() { return helpWindow.data(); }
AboutDialog::AboutDialog(QWidget *parent) : QDialog(parent) {
    initializeAboutResources();
    setObjectName("aboutDialog"); setWindowTitle("About 7-Zip"); UiLanguage::bindTitle(this, OfficialUi::IDD_ABOUT);
    auto label = [this](const char *name, const QString &text, bool literal = true) { auto widget = new QLabel(text, this); widget->setObjectName(name); widget->setTextFormat(Qt::PlainText); widget->setProperty("uiLiteral", literal); widget->setProperty("resourceTextFit", true); return widget; };
    auto logo = label("aboutLogo", {}); logo->setPixmap(QPixmap(":/icons/7zipLogo.ico"));
    auto version = label("aboutVersion", "7-Zip 26.03 (" + QSysInfo::buildCpuArchitecture() + ")");
    auto date = label("aboutDate", "2026-09-03"), copyright = label("aboutCopyright", "Copyright (c) 1999-2026 Igor Pavlov");
    auto info = label("aboutInfo", "7-Zip is free software", false); UiLanguage::bind(info, OfficialUi::IDT_ABOUT_INFO);
    auto port = label("aboutPortInfo", "Unofficial Mac Port " + QString::fromLatin1(PORT_PROJECT_VERSION) + "\nQt " + QString(qVersion()) + " / LGPL 3.0\n© The Qt Company Ltd.\nand other contributors"); auto font = port->font(); font.setPointSizeF(8); port->setFont(font); port->setWordWrap(true); port->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    port->setToolTip("Qt Copyright (C) The Qt Company Ltd. and other contributors. Qt: LGPL 3.0; Port: LGPL 3.0 or later. GPL/LGPL license texts and corresponding source: Contents/Resources in the application bundle");
    auto ok = new QPushButton("OK", this), home = new QPushButton("www.7-zip.org", this); ok->setObjectName("aboutOk"); home->setObjectName("aboutHomePage"); UiLanguage::bind(ok, 401); home->setProperty("uiLiteral", true); home->setProperty("resourceTextFit", true);
    auto geometry = new ResourceDialogLayout(this, OfficialUi::IDD_ABOUT);
    geometry->bind("IDOK", ok); geometry->bind("IDB_ABOUT_HOMEPAGE", home); geometry->bind("STATIC_2", logo);
    geometry->bind("IDT_ABOUT_VERSION", version); geometry->bind("IDT_ABOUT_DATE", date); geometry->bind("STATIC_5", copyright); geometry->bind("IDT_ABOUT_INFO", info);
    geometry->extra(port, {48, 8, 104, 40}); geometry->install();
    connect(ok, &QPushButton::clicked, this, &QDialog::accept); connect(home, &QPushButton::clicked, this, [this] { emit homePageRequested(QUrl("https://www.7-zip.org/")); });
    connect(new QShortcut(QKeySequence("F1"), this), &QShortcut::activated, this, [this] { Help::show(this, "start.htm"); }); UiLanguage::apply(this);
}
void Help::about(QWidget *parent) { AboutDialog dialog(parent); QObject::connect(&dialog, &AboutDialog::homePageRequested, &dialog, [](const QUrl &url) { QDesktopServices::openUrl(url); }); dialog.exec(); }
