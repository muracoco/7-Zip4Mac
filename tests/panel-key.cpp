// SPDX-License-Identifier: LGPL-3.0-or-later
#include "MainWindow.h"
#include "FileListItem.h"
#include "PanelKey.h"
#include "AddressCombo.h"
#include "PortStyle.h"
#include "input-driver.h"
#include "progress-dialog-driver.h"
#include <QApplication>
#include <QClipboard>
#include <QFile>
#include <QKeyEvent>
#include <QLocale>
#include <QMessageBox>
#include <QSignalSpy>
#include <QTest>
#include <sys/stat.h>
#include <unistd.h>

static QString executable;
static bool write(QString path, QByteArray bytes = "owned payload 日本語") { QDir().mkpath(QFileInfo(path).absolutePath()); QFile file(path); return file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size(); }
static QByteArray read(QString path) { QFile file(path); return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray(); }
template<class T> static T *owned(MainWindow &window, const QString &name) {
    for (auto widget : window.findChildren<T *>(name)) { auto parent=widget->parentWidget(); while(parent&&!qobject_cast<MainWindow *>(parent)) parent=parent->parentWidget(); if(parent==&window) return widget; }
    return nullptr;
}
static FileList *list(MainWindow &window) { return owned<FileList>(window,"fileList"); }
static QTreeWidgetItem *find(MainWindow &window, QString name) { auto files = list(window); for (int i=0;i<files->topLevelItemCount();++i) if (panelItemName(files->topLevelItem(i)) == name) return files->topLevelItem(i); return nullptr; }
// Set Qt logical activation for shortcut testing; this does not assert Cocoa/Finder focus.
QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
static void focus(MainWindow &window, QTreeWidgetItem *item) { QApplication::setActiveWindow(window.window()); auto files=list(window); files->clearSelection(); files->setCurrentItem(item, 0, QItemSelectionModel::NoUpdate); files->clearSelection(); files->setFocus(); }
QT_WARNING_POP
static QString address(MainWindow &window) { return owned<QLineEdit>(window,"currentPath")->text(); }

class PanelKeyTests : public QObject {
    Q_OBJECT
    QTemporaryDir temporary;
    ProgressDialogDriver results;
    void settings(QString root, bool two=false, int mode=3) { QSettings().clear(); QSettings().setValue("View/LastPath", root); QSettings().setValue("View/Panel2Path", root); QSettings().setValue("View/AutoRefresh", false); QSettings().setValue("View/TwoPanels", two); QSettings().setValue("View/Mode", mode); QSettings().setValue("Options/AlternativeSelection", true); QSettings().setValue("Options/ShowDots", true); QSettings().setValue("Options/Language", "en"); }
    ArchiveResult execute(ArchiveRequest request) { SevenZipProcessBackend backend(executable); QSignalSpy done(&backend,&ArchiveBackend::finished); backend.start(request); if(done.isEmpty()&&!done.wait(30000)) return {}; return qvariant_cast<ArchiveResult>(done.last()[0]); }
    QString seed(QString root, QString format) {
        if(!write(root+"/input/folder/payload.txt")||!write(root+"/input/keep.txt","keep")) return {};
        ArchiveRequest request; request.operation=ArchiveOperation::Add; request.archive=root+"/archive."+format; request.workingDirectory=root+"/input"; request.files={"folder","keep.txt"}; request.format=format; request.useArchiveDefaults=true; request.method=format=="7z"?QString("LZMA2"):format=="zip"?QString("Deflate"):QString(); request.level=0;
        return execute(request).success ? request.archive : QString();
    }
    void renameTo(MainWindow &window, QWidget *view, QString name) {
        QTest::keyClick(view, Qt::Key_F2);
        auto editor=window.findChild<QLineEdit *>("renameEditor"); QVERIFY(editor); QVERIFY(editor->isVisible()); editor->setText(name); QTest::keyClick(editor,Qt::Key_Return);
    }
private slots:
    void initTestCase() { QVERIFY(temporary.isValid()); QSettings::setDefaultFormat(QSettings::IniFormat); QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,temporary.filePath("preferences")); }
    void init() { results.clear(); }
    void focusedRenameWithoutMarks_data() { QTest::addColumn<int>("mode"); QTest::newRow("details")<<3; QTest::newRow("icons")<<0; }
    void focusedRenameWithoutMarks() {
        QFETCH(int,mode); const auto root=temporary.filePath("focus-"+QString::number(mode)); QVERIFY(write(root+"/focused.txt")); QVERIFY(write(root+"/marked.txt","retain mark")); settings(root,false,mode);
        MainWindow window(executable); window.show(); QTRY_VERIFY(!window.operationBusy()); auto files=list(window); auto row=find(window,"focused.txt"); QVERIFY(row); focus(window,row); QVERIFY(files->operatedItems().isEmpty()); QVERIFY(!window.findChild<QAction *>("renameAction")->isEnabled());
        QWidget *view=mode==3?static_cast<QWidget *>(files):static_cast<QWidget *>(window.findChild<IconFileList *>("iconFileList")); QVERIFY(view); view->setFocus(); renameTo(window,view,"renamed 日本語.txt"); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy()&&QFileInfo::exists(root+"/renamed 日本語.txt"),10000); QVERIFY(!QFileInfo::exists(root+"/focused.txt")); QCOMPARE(read(root+"/renamed 日本語.txt"),QByteArray("owned payload 日本語"));
        row=find(window,"renamed 日本語.txt"); QVERIFY(row); focus(window,row); files->setItemSelected(find(window,"marked.txt"),true); renameTo(window,view,"second.txt"); QTRY_VERIFY(!window.operationBusy()&&QFileInfo::exists(root+"/second.txt")); QCOMPARE(read(root+"/marked.txt"),QByteArray("retain mark")); QVERIFY(files->markedItems().contains(find(window,"marked.txt"))); window.close();
    }
    void relativeFilesystemRename_data() { QTest::addColumn<bool>("flat"); QTest::newRow("ordinary-child-path")<<false; QTest::newRow("flat-parent-relative")<<true; }
    void relativeFilesystemRename() {
        QFETCH(bool,flat); const auto root=temporary.filePath("relative-"+QString::number(flat)); QVERIFY(write(root+"/sub/old.txt")); QVERIFY(QDir().mkpath(root+"/destination")); settings(root); QSettings().setValue("View/Flat",flat);
        MainWindow window(executable); window.show(); if(!flat) window.openPath(root+"/sub"); QTRY_VERIFY(!window.operationBusy()); auto row=find(window,"old.txt"); QVERIFY(row); focus(window,row); renameTo(window,list(window),"../destination/日本語 renamed.txt"); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy()&&QFileInfo::exists(root+"/destination/日本語 renamed.txt"),10000); QVERIFY(!QFileInfo::exists(root+"/sub/old.txt")); QCOMPARE(read(root+"/destination/日本語 renamed.txt"),QByteArray("owned payload 日本語")); QCOMPARE(address(window),flat?root:root+"/sub"); window.close();
    }
    void renameFailuresKeepBothFiles_data() { QTest::addColumn<QString>("name"); QTest::newRow("existing")<<QString("target.txt"); QTest::newRow("missing-parent")<<QString("missing/new.txt"); QTest::newRow("dot")<<QString("."); QTest::newRow("dot-parent")<<QString("destination/.."); }
    void renameFailuresKeepBothFiles() {
        QFETCH(QString,name); const auto root=temporary.filePath("failure-"+QString(QTest::currentDataTag())); QVERIFY(write(root+"/old.txt")); QVERIFY(write(root+"/target.txt","existing independent data")); settings(root); MainWindow window(executable); window.show(); QTRY_VERIFY(!window.operationBusy()); focus(window,find(window,"old.txt"));
        QStringList warnings; QTimer dismiss; dismiss.setInterval(10); connect(&dismiss,&QTimer::timeout,&window,[&]{for(auto message:window.findChildren<QMessageBox *>()) if(message->isVisible()&&message->objectName()!="progressFinalMessage") {warnings<<message->text(); message->accept();}}); dismiss.start(); renameTo(window,list(window),name); QTRY_VERIFY(!window.operationBusy()); QTRY_VERIFY(!warnings.isEmpty()||!results.notices.isEmpty()); dismiss.stop(); QCOMPARE(read(root+"/old.txt"),QByteArray("owned payload 日本語")); QCOMPARE(read(root+"/target.txt"),QByteArray("existing independent data")); QVERIFY(find(window,"old.txt")); window.close();
    }
    void relativeArchiveRename_data() { QTest::addColumn<QString>("format"); QTest::addColumn<bool>("flat"); for(auto format:{"7z","zip","tar","wim"}) for(bool flat:{false,true}) QTest::newRow(qPrintable(QString("%1-%2").arg(format).arg(flat)))<<QString(format)<<flat; }
    void relativeArchiveRename() {
        QFETCH(QString,format); QFETCH(bool,flat); const auto root=temporary.filePath("archive-"+format+QString::number(flat)); const auto archive=seed(root,format); QVERIFY(!archive.isEmpty()); settings(root); QSettings().setValue("View/Flat",flat); MainWindow window(executable); window.show(); window.openPath(archive+(flat?QString():QString("/folder"))); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(),10000);
        auto row=find(window,"payload.txt"); QVERIFY(row); focus(window,row); renameTo(window,list(window),"new folder/日本語 payload.txt"); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(),20000); QString errors; for(const auto &notice:results.notices) errors+=notice.text+"\n"; QVERIFY2(results.notices.isEmpty(),qPrintable(errors));
        ArchiveRequest verify; verify.operation=ArchiveOperation::Test; verify.archive=archive; auto result=execute(verify); QVERIFY2(result.success,qPrintable(result.message+result.details)); verify.operation=ArchiveOperation::Extract; verify.outputDirectory=root+"/output"; verify.overwriteMode="overwrite"; result=execute(verify); QVERIFY2(result.success,qPrintable(result.message+result.details)); QCOMPARE(read(root+"/output/folder/new folder/日本語 payload.txt"),QByteArray("owned payload 日本語")); QCOMPARE(read(root+"/output/keep.txt"),QByteArray("keep")); QVERIFY(!QFileInfo::exists(root+"/output/folder/payload.txt")); window.close();
    }
    void shiftF4AndAltPathFocus() {
        const auto root=temporary.filePath("create-focus"); QVERIFY(QDir().mkpath(root)); settings(root,true); MainWindow window(executable); window.show(); QTRY_VERIFY(!window.operationBusy()); auto other=window.findChild<MainWindow *>("secondPanel"); QVERIFY(other);
        bool shown=false; QTimer fill; fill.setInterval(10); connect(&fill,&QTimer::timeout,&window,[&]{ auto input=testInput(&window); if(input&&input.isVisible()){shown=true; input.setTextValue("日本語 new.txt"); input.accept();}}); fill.start(); QTest::keyClick(list(window),Qt::Key_F4,Qt::ShiftModifier); fill.stop(); QVERIFY(shown); QTRY_VERIFY(!window.operationBusy()&&QFileInfo::exists(root+"/日本語 new.txt"));
        auto rightPath=owned<QLineEdit>(*other,"currentPath"); auto leftPath=owned<QLineEdit>(window,"currentPath");
        QTest::keyClick(list(window),Qt::Key_F2,Qt::AltModifier); QVERIFY(owned<AddressCombo>(*other,"pathCombo")->view()->isVisible()); QCOMPARE(rightPath->selectedText(),rightPath->text()); owned<AddressCombo>(*other,"pathCombo")->hidePopup(); QTest::keyClick(list(*other),Qt::Key_F1,Qt::AltModifier); QVERIFY(owned<AddressCombo>(window,"pathCombo")->view()->isVisible()); QCOMPARE(leftPath->selectedText(),leftPath->text()); owned<AddressCombo>(window,"pathCombo")->hidePopup(); window.close();
    }
    void otherPanelFilesystemNavigation() {
        const auto root=temporary.filePath("two-fs"); QVERIFY(write(root+"/left/folder/payload.txt")); QVERIFY(QDir().mkpath(root+"/right")); settings(root+"/left",true); QSettings().setValue("View/Panel2Path",root+"/right"); MainWindow window(executable); window.show(); QTRY_VERIFY(!window.operationBusy()); auto other=window.findChild<MainWindow *>("secondPanel"); QVERIFY(other);
        QVERIFY(find(window,"folder")); focus(window,find(window,"folder")); QTest::keyClick(list(window),Qt::Key_Right,Qt::AltModifier); QTRY_VERIFY(!window.operationBusy()&&address(*other)==root+"/left/folder"); QCOMPARE(address(window),root+"/left"); QVERIFY(find(*other,"payload.txt"));
        QTest::keyClick(list(window),Qt::Key_Up,Qt::AltModifier); QTRY_VERIFY(!window.operationBusy()&&address(*other)==root+"/left"); QVERIFY(find(window,"..")); focus(window,find(window,"..")); QTest::keyClick(list(window),Qt::Key_Left,Qt::AltModifier); QTRY_VERIFY(!window.operationBusy()&&address(*other)==root); window.close();
    }
    void otherPanelArchiveNavigation_data() { QTest::addColumn<QString>("format"); QTest::newRow("7z")<<QString("7z"); QTest::newRow("zip")<<QString("zip"); }
    void otherPanelArchiveNavigation() {
        QFETCH(QString,format); const auto root=temporary.filePath("two-archive-"+format); const auto archive=seed(root,format); QVERIFY(!archive.isEmpty()); settings(root,true); MainWindow window(executable); window.show(); window.openPath(archive); QTRY_VERIFY(!window.operationBusy()); auto other=window.findChild<MainWindow *>("secondPanel"); QVERIFY(other); QVERIFY(find(window,"folder")); focus(window,find(window,"folder")); QTest::keyClick(list(window),Qt::Key_Right,Qt::AltModifier); QTRY_VERIFY(!window.operationBusy()&&address(*other)==archive+"/folder/"); QVERIFY(find(*other,"payload.txt"));
        QVERIFY(find(*other,"..")); focus(*other,find(*other,"..")); QTest::keyClick(list(*other),Qt::Key_Left,Qt::AltModifier); QTRY_COMPARE(address(window),archive+"/"); QVERIFY(find(window,"keep.txt")); QVERIFY(find(window,"folder")); focus(window,find(window,"folder")); QTest::keyClick(list(window),Qt::Key_Up,Qt::AltModifier); QTRY_COMPARE(address(*other),archive+"/"); QVERIFY(find(*other,"keep.txt")); window.close();
    }
    void sharedNestedLifetimeAndParent() {
        const auto root=temporary.filePath("nested-panels"); const auto inner=seed(root+"/inside","zip"); QVERIFY(!inner.isEmpty()); QVERIFY(QFile::copy(inner,root+"/inner.zip")); ArchiveRequest request; request.operation=ArchiveOperation::Add; request.archive=root+"/outer.7z"; request.format="7z"; request.level=0; request.workingDirectory=root; request.files={"inner.zip"}; QVERIFY(execute(request).success);
        settings(root,true); MainWindow window(executable); window.show(); window.openPath(request.archive+"/inner.zip/folder"); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(),20000); auto other=window.findChild<MainWindow *>("secondPanel"); QVERIFY(other); const auto nested=request.archive+"/inner.zip/folder/"; QCOMPARE(address(window),nested); QTest::keyClick(list(window),Qt::Key_Up,Qt::AltModifier); QTRY_COMPARE(address(*other),nested); QVERIFY(find(*other,"payload.txt"));
        window.openPath(root); QTRY_VERIFY(!window.operationBusy()&&address(window)==root); QVERIFY(find(*other,"payload.txt")); QVERIFY(find(*other,"..")); focus(*other,find(*other,"..")); QTest::keyClick(list(*other),Qt::Key_Left,Qt::AltModifier); QTRY_VERIFY(!window.operationBusy()&&address(window)==request.archive+"/inner.zip/"); QVERIFY(find(window,"keep.txt")); QVERIFY(find(window,"..")); focus(window,find(window,"..")); QTest::keyClick(list(window),Qt::Key_Right,Qt::AltModifier); QTRY_COMPARE(address(*other),request.archive+"/"); QVERIFY(find(*other,"inner.zip")); window.close();
    }
    void scanDoesNotStealExplicitAddressFocus() {
        const auto root=temporary.filePath("focus-scan"); QVERIFY(write(root+"/folder/payload.txt")); settings(root); MainWindow window(executable); window.show(); QTRY_VERIFY(!window.operationBusy()); focus(window,find(window,"folder"));
        window.openPath(root+"/folder"); auto path=owned<QLineEdit>(window,"currentPath"); path->setFocus(); QTest::keyClick(path,Qt::Key_X); const auto draft=path->text(); QTRY_VERIFY(!window.operationBusy()); QVERIFY(window.focusWidget()==path || window.focusWidget()==owned<AddressCombo>(window,"pathCombo")); QCOMPARE(path->text(),draft);
        focus(window,find(window,"payload.txt")); QTest::keyClick(list(window),Qt::Key_F2,Qt::AltModifier); QVERIFY(owned<AddressCombo>(window,"pathCombo")->view()->isVisible()); QCOMPARE(path->selectedText(),path->text()); owned<AddressCombo>(window,"pathCombo")->hidePopup(); window.close();
    }
    void clipboardUsesExactNames() {
        const auto root=temporary.filePath("clipboard-names"); const QString name="literal\\name. "; QVERIFY(write(root+'/'+name)); settings(root); MainWindow window(executable); window.show(); QTRY_VERIFY(!window.operationBusy()); auto row=find(window,name); QVERIFY(row); focus(window,row); list(window)->setItemSelected(row,true);
        QTest::keyClick(list(window),Qt::Key_Insert,Qt::ControlModifier); QCOMPARE(QApplication::clipboard()->text(),name); QTest::keyClick(list(window),Qt::Key_Insert,Qt::ShiftModifier); QVERIFY(!window.operationBusy()); QCOMPARE(read(root+'/'+name),QByteArray("owned payload 日本語")); window.close();
    }
    void symbolicFolderBrowseAndRename() {
        const auto root=temporary.filePath("symbolic-browse"); QVERIFY(write(root+"/target/payload.txt")); QVERIFY(::symlink("target",QFile::encodeName(root+"/alias").constData())==0); settings(root); MainWindow window(executable); window.show(); QTRY_VERIFY(!window.operationBusy()); auto alias=find(window,"alias"); QVERIFY(alias); QVERIFY(alias->data(0,Qt::UserRole+1).toBool()); focus(window,alias); list(window)->setItemSelected(alias,true); QTest::keyClick(list(window),Qt::Key_Return); QTRY_VERIFY(!window.operationBusy()&&address(window)==root+"/alias"); QVERIFY(find(window,"payload.txt")); QTest::keyClick(list(window),Qt::Key_Backspace); QTRY_VERIFY(!window.operationBusy()&&address(window)==root); focus(window,find(window,"alias")); renameTo(window,list(window),"renamed alias"); QTRY_VERIFY(!window.operationBusy()&&QFileInfo(root+"/renamed alias").isSymLink()); QCOMPARE(read(root+"/renamed alias/payload.txt"),QByteArray("owned payload 日本語")); QVERIFY(QFileInfo(root+"/target").isDir()); QVERIFY(!QFileInfo(root+"/alias").isSymLink()); window.close();
    }
};
int main(int argc,char **argv) {
    QLocale::setDefault(QLocale(QLocale::English)); if(argc<2) return 1; executable=QString::fromLocal8Bit(argv[1]);
    if(!qEnvironmentVariable("PORT_TEST_PLUGIN_ROOT").isEmpty()) QCoreApplication::setLibraryPaths({qEnvironmentVariable("PORT_TEST_PLUGIN_ROOT")}); QCoreApplication::setAttribute(Qt::AA_MacDontSwapCtrlAndMeta); QCoreApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
    QApplication app(argc,argv); applyPortAppearance(app); app.setOrganizationName("SevenZipMacPortTests"); app.setApplicationName("PanelKeys"); app.setQuitOnLastWindowClosed(false);
    PanelKeyTests tests; QList<char *> args{argv[0]}; for(int n=2;n<argc;++n) args<<argv[n]; args<<nullptr; return QTest::qExec(&tests,args.size()-1,args.data());
}
#include "panel-key.moc"
