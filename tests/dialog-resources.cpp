// SPDX-License-Identifier: LGPL-3.0-or-later
#include "Dialogs.h"
#include "ResourceDialogs.h"
#include "CompressionOptionsText.h"
#include "ProgressText.h"
#include "OptionsDialog.h"
#include "UiLanguage.h"
#include "Help.h"
#include "TemporaryFilesDialog.h"
#include "PortStyle.h"
#include <QDialogButtonBox>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>
#include <QPixmap>
#include <QDir>
#include <QLabel>
#include <QFileInfo>
#include <QFontMetricsF>
#include <QAbstractButton>
#include <QStyleOptionButton>
#include <QStyle>
#include <QPointer>
#include <QLibraryInfo>
#include <limits>
static QString executable;
class DialogResourceTests : public QObject {
    Q_OBJECT
    QTemporaryDir temporary;
    static QRect rectangle(QWidget *widget, QWidget &dialog) { return {widget->mapTo(&dialog, QPoint()), widget->size()}; }
    static QWidget *control(QDialog &dialog, int id) { for (auto widget : dialog.findChildren<QWidget *>()) if (widget->property("resourceControlId").toInt() == id) return widget; return nullptr; }
    static void render(QDialog &dialog, QString name) {
        const auto root = qEnvironmentVariable("PORT_DIALOG_RENDER_ROOT"); if (root.isEmpty()) return;
        QDir().mkpath(root); dialog.grab().save(root + '/' + name + ".png");
    }
    static void verifyTextFits(QDialog &dialog, const QString &language) {
        for (auto widget : dialog.findChildren<QWidget *>()) {
            if (!widget->property("resourceControlId").isValid() || (widget->property("uiLiteral").toBool() && !widget->property("resourceTextFit").toBool())) continue;
            QString text; QRect contents;
            if (auto label = qobject_cast<QLabel *>(widget)) { text = label->text(); contents = label->contentsRect().adjusted(label->margin(), label->margin(), -label->margin(), -label->margin()); }
            else if (auto button = qobject_cast<QAbstractButton *>(widget)) {
                text = button->text(); QStyleOptionButton option; option.initFrom(button); option.text = text;
                contents = button->style()->subElementRect(qobject_cast<QCheckBox *>(button) ? QStyle::SE_CheckBoxContents : QStyle::SE_PushButtonContents, &option, button);
            } else continue;
            if (text.isEmpty()) continue;
            const auto label = qobject_cast<QLabel *>(widget);
            if (!label || label->buddy()) { text.replace("&&", QString(QChar(0xfffc))); text.remove('&'); text.replace(QChar(0xfffc), '&'); }
            const QFontMetricsF font(widget->font(), widget);
            const bool wrap = widget->property("resourceMultiline").toBool() || (label && label->wordWrap());
            const auto ink = wrap ? font.boundingRect(QRectF(0, 0, contents.width(), 100000), Qt::TextWordWrap, text) : font.boundingRect(text);
            const auto context = language + '/' + dialog.objectName() + '/' + widget->objectName() + QString(" (id=%1): %2; content=%3x%4 ink=%5x%6").arg(widget->property("resourceControlId").toInt()).arg(text).arg(contents.width()).arg(contents.height()).arg(ink.width()).arg(ink.height());
            QVERIFY2(ink.height() <= contents.height() + 1, qPrintable(context));
            if (!wrap) QVERIFY2(font.horizontalAdvance(text) <= contents.width() + 1, qPrintable(context));
            QVERIFY2(dialog.rect().contains(rectangle(widget, dialog)), qPrintable(context + " is outside the dialog"));
        }
    }
private slots:
    void initTestCase() {
        QVERIFY(temporary.isValid()); QSettings::setDefaultFormat(QSettings::IniFormat); QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, temporary.filePath("prefs"));
    }
    void init() { QSettings().clear(); QLocale::setDefault(QLocale(QLocale::English)); UiLanguage::set("en"); }
    void cleanup() { QLocale::setDefault(QLocale(QLocale::English)); UiLanguage::set("en"); }
    void sourceCoordinatesAndButtonOrder() {
        AddDialog add(temporary.filePath("日本語 archive.7z")); add.show(); QTest::qWait(10);
        auto layout = add.findChild<ResourceDialogLayout *>(); QVERIFY(layout); QCOMPARE(add.size(), layout->dialogSize());
        QCOMPARE(rectangle(add.findChild<QComboBox *>("archiveFormat"), add), layout->toPixels({112, 39, 88, 14}));
        QCOMPARE(rectangle(add.findChild<QComboBox *>("compressionThreads"), add), layout->toPixels({112, 165, 48, 14}));
        QCOMPARE(rectangle(add.findChild<QLabel *>("compressionMemoryRequired"), add), layout->toPixels({8, 194, 140, 8}));
        auto box = add.findChild<QDialogButtonBox *>(); QVERIFY(box);
        QCOMPARE(rectangle(box->button(QDialogButtonBox::Ok), add), layout->toPixels({200, 312, 64, 16}));
        QCOMPARE(rectangle(box->button(QDialogButtonBox::Cancel), add), layout->toPixels({272, 312, 64, 16}));
        QCOMPARE(rectangle(box->button(QDialogButtonBox::Help), add), layout->toPixels({344, 312, 64, 16}));
        QStringList names; auto formats = add.findChild<QComboBox *>("archiveFormat"); for (int n = 0; n < formats->count(); ++n) names << formats->itemText(n);
        QCOMPARE(names, QStringList({"7z", "bzip2", "gzip", "Hash", "tar", "wim", "xz", "zip"}));
        const auto original = add.size(); add.resize(1200, 1000); QCOMPARE(add.size(), original); render(add, "add-en");
        UiLanguage::set("ja"); UiLanguage::apply(&add); render(add, "add-ja"); QCOMPARE(formats->itemText(3), QString("Hash"));
    }
    void extractionVisibilityAndGeometry() {
        ExtractDialog extract(temporary.filePath("output"), true); extract.show(); QTest::qWait(10);
        auto layout = extract.findChild<ResourceDialogLayout *>(); QVERIFY(layout);
        QCOMPARE(rectangle(extract.findChild<QComboBox *>("extractPathMode"), extract), layout->toPixels({8, 72, 160, 14}));
        QCOMPARE(rectangle(extract.findChild<QComboBox *>("extractOverwriteMode"), extract), layout->toPixels({8, 124, 160, 14}));
        auto name = extract.findChild<QLineEdit *>("extractSubfolder"); auto enable = extract.findChild<QCheckBox *>("extractNameEnabled"); QVERIFY(name && enable);
        QVERIFY(name->isVisible()); enable->setChecked(false); QVERIFY(!name->isVisible()); QCOMPARE(extract.options().outputDirectory, temporary.path());
        enable->setChecked(true); QVERIFY(name->isVisible()); QCOMPARE(rectangle(name, extract), layout->toPixels({22, 40, 146, 14})); QVERIFY(extract.selectedOnly());
        auto box = extract.findChild<QDialogButtonBox *>(); QCOMPARE(rectangle(box->button(QDialogButtonBox::Ok), extract), layout->toPixels({136, 160, 64, 16}));
        render(extract, "extract-en"); UiLanguage::set("ja"); UiLanguage::apply(&extract); render(extract, "extract-ja");
    }
    void originalProgressResizeAndState() {
        ProgressDialog progress("Extract", "日本語 archive.7z"); progress.show(); QTest::qWait(10);
        auto layout = progress.findChild<ResourceDialogLayout *>(); QVERIFY(layout);
        for (auto size : {layout->dialogSize(), QSize(1100, 650), QSize(480, 430)}) {
            progress.resize(size); QTest::qWait(10);
            auto cancel = control(progress, 2), pause = control(progress, 446), background = control(progress, 444);
            QVERIFY(cancel && pause && background); const auto margin = layout->toPixels({0, 0, 8, 8}).size();
            QCOMPARE(cancel->geometry().right() + 1, progress.width() - margin.width()); QCOMPARE(cancel->geometry().bottom() + 1, progress.height() - margin.height());
            QVERIFY(background->geometry().right() < pause->geometry().left()); QVERIFY(pause->geometry().right() < cancel->geometry().left());
            QVERIFY(control(progress, 101)->geometry().bottom() < cancel->geometry().top());
            QVERIFY(control(progress, 3900)->geometry().right() < control(progress, 120)->geometry().left());
            QCOMPARE(control(progress, 122)->geometry().right() + 1, progress.width() - margin.width());
        }
        ArchiveProgress update; update.mode = "extract"; update.doneFiles = 3; update.files = 4; update.total = 1000; update.completed = 750; update.output = 750; update.input = 300; progress.updateDetails(update);
        QCOMPARE(progress.findChild<QLabel *>("progressFiles")->text(), QString("3")); QCOMPARE(progress.findChild<QLabel *>("progressFilesTotal")->text(), QString(" / 4"));
        progress.setPauseAvailable(true); progress.setPaused(true); UiLanguage::set("ja"); UiLanguage::apply(&progress); QVERIFY(progress.windowTitle().startsWith(UiLanguage::resource(447) + ' '));
        progress.setPaused(false); QVERIFY(!progress.windowTitle().startsWith(UiLanguage::resource(447) + ' ')); render(progress, "progress-ja");
    }
    void originalProgressFormatting() {
        QCOMPARE(officialProgressTime(0), QString("00:00:00")); QCOMPARE(officialProgressTime(359999), QString("99:59:59")); QCOMPARE(officialProgressTime(360000), QString("100:00:00"));
        QCOMPARE(officialProgressSize(99999), QString("99999")); QCOMPARE(officialProgressSize(100000), QString("97 KB")); QCOMPARE(officialProgressSize(quint64(100000) << 10), QString("97 MB"));
        QCOMPARE(officialProgressSize(quint64(100000) << 20), QString("97 GB")); QCOMPARE(officialProgressSize(std::numeric_limits<quint64>::max()), QString("17179869183 GB"));
        QCOMPARE(officialProgressSpeed(9999), QString("9999 B/s")); QCOMPARE(officialProgressSpeed(10000), QString("9 KB/s")); QCOMPARE(officialProgressSpeed(quint64(10000) << 10), QString("9 MB/s"));
    }
    void originalOptionsSummaryAndModal() {
        ArchiveRequest request; request.timestampPrecision = 2; request.modificationTime = 0; request.creationTime = 1; request.accessTime = 0; request.latestArchiveTimeSpecified = true; request.latestArchiveTime = false; request.storeSymbolicLinks = true; request.storeHardLinks = true;
        QCOMPARE(compressionOptionsText(request, "tar"), QString("tp2 tm- tc ta- -stl- SL HL"));
        AddDialog add(temporary.filePath("options.7z")); bool seen = false; QTimer answer; answer.setInterval(10);
        connect(&answer, &QTimer::timeout, &add, [&] { auto dialog = add.findChild<QDialog *>("compressOptionsDialog"); if (!dialog || !dialog->isVisible() || seen) return; seen = true; QTimer::singleShot(1000, dialog, &QDialog::reject);
            auto layout = dialog->findChild<ResourceDialogLayout *>(); QVERIFY(layout); QCOMPARE(dialog->size(), layout->dialogSize());
            auto specified = dialog->findChild<QCheckBox *>("storeModificationTimeSet"); auto modified = dialog->findChild<QCheckBox *>("storeModificationTime"); QVERIFY(specified && modified); specified->setChecked(true); modified->setChecked(false);
            render(*dialog, "compression-options-en"); dialog->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok)->click();
        }); answer.start(); add.findChild<QPushButton *>("compressionOptions")->click(); answer.stop(); QVERIFY(seen); QCOMPARE(add.options().modificationTime, 0);
        QVERIFY(add.findChild<QLabel *>("compressionOptionsSummary")->text().contains("tm-"));
    }
    void originalAboutAndTemporaryLayout() {
        AboutDialog about; auto geometry = about.findChild<ResourceDialogLayout *>(); QVERIFY(geometry); QCOMPARE(about.size(), geometry->dialogSize());
        auto logo = about.findChild<QLabel *>("aboutLogo"); QVERIFY(!logo->pixmap().isNull()); QCOMPARE(rectangle(logo, about).topLeft(), geometry->toPixels({8, 8, 32, 32}).topLeft()); QCOMPARE(logo->size(), logo->pixmap().deviceIndependentSize().toSize());
        QCOMPARE(rectangle(about.findChild<QLabel *>("aboutVersion"), about), geometry->toPixels({8, 54, 144, 8})); QCOMPARE(about.findChild<QLabel *>("aboutDate")->text(), QString("2026-09-03"));
        QCOMPARE(rectangle(about.findChild<QPushButton *>("aboutOk"), about), geometry->toPixels({88, 136, 64, 16})); QCOMPARE(rectangle(about.findChild<QPushButton *>("aboutHomePage"), about), geometry->toPixels({16, 136, 64, 16})); QVERIFY(about.findChild<QPushButton *>("aboutOk")->isDefault());
        QVERIFY(about.findChild<QLabel *>("aboutPortInfo")->text().contains("Unofficial")); render(about, "about-en");
        const auto helper = QFileInfo(executable).dir().filePath("7zz-progress"); TemporaryFilesDialog temp(helper, nullptr, temporary.path()); temp.show(); QTest::qWait(10);
        geometry = temp.findChild<ResourceDialogLayout *>(); QVERIFY(geometry); QCOMPARE(temp.size(), geometry->dialogSize());
        QCOMPARE(rectangle(temp.findChild<QPushButton *>("deleteTemporary"), temp), geometry->toPixels({8, 8, 64, 16})); QCOMPARE(rectangle(temp.findChild<QPushButton *>("refreshTemporary"), temp), geometry->toPixels({80, 8, 64, 16}));
        auto box = temp.findChild<QDialogButtonBox *>(); auto close = box->button(QDialogButtonBox::Close), help = box->button(QDialogButtonBox::Help); QVERIFY(close && help);
        for (const auto size : {geometry->dialogSize(), QSize(1000, 700), QSize(500, 420)}) {
            temp.resize(size); QTest::qWait(10); const auto margin = geometry->toPixels({0, 0, 8, 8}).size();
            const auto closeRect = rectangle(close, temp), helpRect = rectangle(help, temp);
            QCOMPARE(helpRect.right() + 1, temp.width() - margin.width()); QCOMPARE(helpRect.bottom() + 1, temp.height() - margin.height()); QCOMPARE(helpRect.left() - closeRect.right() - 1, margin.width());
            auto filter = temp.findChild<QComboBox *>("temporaryFilter"); auto files = temp.findChild<QWidget *>("temporaryList");
            QCOMPARE(filter->geometry().bottom() + 1, helpRect.top() - margin.height()); QCOMPARE(files->geometry().bottom() + 1, filter->geometry().top() - margin.height());
            QVERIFY(!close->isDefault()); QVERIFY(!help->isDefault()); QVERIFY(!temp.findChild<QPushButton *>("deleteTemporary")->isDefault());
        }
        temp.resize(geometry->dialogSize()); render(temp, "temporary-en"); close->click(); QCOMPARE(temp.result(), int(QDialog::Rejected));
    }
    void officialLanguageTextFits() {
        AddDialog add(temporary.filePath("日本語 archive.7z")); ExtractDialog extract(temporary.filePath("output"), true); AboutDialog about; ProgressDialog progress("Extract", "日本語 archive.7z");
        const QList<QDialog *> dialogs{&add, &extract, &about, &progress};
        for (auto dialog : dialogs) dialog->show(); QTest::qWait(10);
        const auto all = UiLanguage::available(); QCOMPARE(all.size(), 93);
        for (const auto &language : all) {
            UiLanguage::set(language.first);
            for (auto dialog : dialogs) { UiLanguage::apply(dialog); verifyTextFits(*dialog, language.first); }
            auto logo = about.findChild<QLabel *>("aboutLogo"), port = about.findChild<QLabel *>("aboutPortInfo"); QVERIFY(logo->geometry().right() < port->geometry().left());
            const QFontMetricsF notice(port->font(), port); QVERIFY(notice.boundingRect(QRectF(0, 0, port->width(), 10000), Qt::TextWordWrap, port->text()).height() <= port->height());
        }
        UiLanguage::set("ja"); for (auto dialog : dialogs) UiLanguage::apply(dialog);
        render(add, "add-ja-fit"); render(extract, "extract-ja-fit"); render(about, "about-ja-fit"); render(progress, "progress-ja-fit");
        add.setFont(QFont("Helvetica", 12)); verifyTextFits(add, "ja/Helvetica12");
        bool seen = false; QTimer timer; timer.setInterval(10);
        connect(&timer, &QTimer::timeout, &add, [&] {
            auto dialog = add.findChild<QDialog *>("compressOptionsDialog"); if (!dialog || !dialog->isVisible() || seen) return; seen = true;
            for (const auto &language : all) { UiLanguage::set(language.first); UiLanguage::apply(dialog); verifyTextFits(*dialog, language.first); }
            UiLanguage::set("ja"); UiLanguage::apply(dialog); render(*dialog, "compression-options-ja-fit"); dialog->reject();
        }); timer.start(); add.findChild<QPushButton *>("compressionOptions")->click(); timer.stop(); QVERIFY(seen);
    }
    void defaultLanguageAndSavedChoice() {
        const QList<QPair<QString, QString>> cases{{"ja_JP", "ja"}, {"en_US", "en"}, {"pt_BR", "pt-br"}, {"pt_PT", "pt"}, {"zh_HK", "zh-tw"}, {"zh_CN", "zh-cn"}, {"sr_Latn_RS", "sr-spl"}, {"sr_Cyrl_RS", "sr-spc"}, {"uz_Cyrl_UZ", "uz-cyrl"}, {"nb_NO", "nb"}, {"pa_IN", "pa-in"}, {"C", "en"}};
        for (const auto &item : cases) QCOMPARE(UiLanguage::defaultLanguage(QLocale(item.first)), item.second);
        QLocale::setDefault(QLocale("ja_JP")); QCOMPARE(FileManagerSettings::load().language, QString("ja")); QVERIFY(!QSettings().contains("Options/Language"));
        OptionsDialog options; QCOMPARE(options.findChild<QComboBox *>("uiLanguage")->currentData().toString(), QString("ja"));
        auto settings = FileManagerSettings::load(); settings.language = "fr"; QVERIFY(settings.save()); QCOMPARE(FileManagerSettings::load().language, QString("fr"));
        QSettings().setValue("Options/Language", "-"); QCOMPARE(FileManagerSettings::load().language, QString("en"));
    }
};
int main(int argc, char **argv) {
    if (argc < 2) return 2; executable = argv[1]; QLocale::setDefault(QLocale(QLocale::English)); auto plugins = qEnvironmentVariable("PORT_TEST_PLUGIN_ROOT"); if (!plugins.isEmpty()) QCoreApplication::setLibraryPaths({plugins, QLibraryInfo::path(QLibraryInfo::PluginsPath)});
    QCoreApplication::setAttribute(Qt::AA_MacDontSwapCtrlAndMeta); QCoreApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
    QApplication app(argc, argv); applyPortAppearance(app); app.setOrganizationName("SevenZipMacPortTests"); app.setApplicationName("DialogResources"); app.setQuitOnLastWindowClosed(false);
    DialogResourceTests tests; QList<char *> args{argv[0]}; for (int i = 2; i < argc; ++i) args << argv[i]; args << nullptr; return QTest::qExec(&tests, args.size() - 1, args.data());
}
#include "dialog-resources.moc"
