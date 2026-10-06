// SPDX-License-Identifier: LGPL-3.0-or-later
#include "MainWindow.h"
#include "FileListItem.h"
#include "PortStyle.h"
#include "ArchiveMetadata.h"
#include <QApplication>
#include <QAction>
#include <QFile>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
#include <QSignalSpy>
#include <QTest>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

using namespace OfficialSort;
static QString executable;
static bool write(QString path, QByteArray bytes) {
    QDir().mkpath(QFileInfo(path).absolutePath()); QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(bytes)==bytes.size();
}
static ArchiveProperty value(quint32 id,quint16 type,QString number={},QString text={}) {
    ArchiveProperty property; property.id=id; property.type=type; property.number=number; property.value=text; return property;
}
static QStringList fileNames(FileList *tree) {
    QStringList names; for(int n=0;n<tree->topLevelItemCount();++n) names<<tree->topLevelItem(n)->text(0); return names;
}
static FileList *ownedTree(MainWindow &window) {
    for(auto tree:window.findChildren<FileList *>("fileList")) {
        QWidget *parent=tree->parentWidget(); while(parent && !qobject_cast<MainWindow *>(parent)) parent=parent->parentWidget();
        if(parent==&window) return tree;
    }
    return nullptr;
}
static QTreeWidgetItem *item(FileList *tree,QString name) {
    for(int n=0;n<tree->topLevelItemCount();++n) if(tree->topLevelItem(n)->text(0)==name) return tree->topLevelItem(n); return nullptr;
}
class PanelSortTests : public QObject {
    Q_OBJECT
    QTemporaryDir temporary;
    QString root;
    ArchiveResult execute(SevenZipProcessBackend &backend,ArchiveRequest request) {
        QSignalSpy done(&backend,&ArchiveBackend::finished); backend.start(request);
        if(done.isEmpty() && !done.wait(20000)) qFatal("Owned sorting fixture operation timed out");
        return qvariant_cast<ArchiveResult>(done.last().first());
    }
    void clickColumn(FileList *tree,quint32 property) {
        const int column=propertyColumn(tree,property); QVERIFY(column>=0);
        const auto x=tree->header()->sectionViewportPosition(column)+tree->header()->sectionSize(column)/2;
        QTest::mouseMove(tree->header()->viewport(),QPoint(x,tree->header()->height()/2));
        QTest::mouseClick(tree->header()->viewport(),Qt::LeftButton,Qt::NoModifier,QPoint(x,tree->header()->height()/2));
    }
private slots:
    void initTestCase() {
        QVERIFY(temporary.isValid()); QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,temporary.filePath("preferences"));
        root=temporary.filePath("source 日本語 space"); QVERIFY(QDir().mkpath(root+"/folder2/empty")); QVERIFY(QDir().mkpath(root+"/folder10"));
        QVERIFY(write(root+"/file2.txt",QByteArray(100,'a'))); QVERIFY(write(root+"/file10.bin","b"));
        QVERIFY(write(root+"/a-date.txt","early")); QVERIFY(write(root+"/z-date.txt","later"));
        QVERIFY(write(root+"/日本語 space.txt","unicode"));
        const timespec early[2]{{1791000000,123456100},{1791000000,123456100}},later[2]{{1791000000,123456900},{1791000000,123456900}};
        QVERIFY(::utimensat(AT_FDCWD,QFile::encodeName(root+"/a-date.txt").constData(),early,0)==0);
        QVERIFY(::utimensat(AT_FDCWD,QFile::encodeName(root+"/z-date.txt").constData(),later,0)==0);
        qInfo()<<"Owned panel sorting fixtures:"<<temporary.path();
    }
    void init() {
        QSettings().clear(); QSettings().setValue("View/LastPath",root); QSettings().setValue("View/Panel2Path",root); QSettings().setValue("View/AutoRefresh",false);
        FileManagerSettings settings; settings.realIcons=false; settings.showDots=true; settings.alternativeSelection=true; QVERIFY(settings.save());
    }
    void naturalNames_data() {
        QTest::addColumn<QString>("first"); QTest::addColumn<QString>("second"); QTest::addColumn<int>("comparison");
        QTest::newRow("numbers")<<"file2.txt"<<"file10.txt"<<-1;
        QTest::newRow("leading-zero-tie")<<"file002.txt"<<"file2.txt"<<0;
        QTest::newRow("all-zero")<<"file00"<<"file0"<<0;
        QTest::newRow("case-tie")<<"abc.ZIP"<<"ABC.zip"<<0;
        QTest::newRow("ordinal-punctuation")<<"_a"<<"Z"<<1;
        QTest::newRow("unicode-numbers")<<"日本語 9"<<"日本語 10"<<-1;
        QTest::newRow("long-number")<<"a99999999999999999999999999999"<<"a100000000000000000000000000000"<<-1;
        QTest::newRow("empty")<<""<<"a"<<-1;
    }
    void naturalNames() { QFETCH(QString,first); QFETCH(QString,second); QFETCH(int,comparison); QCOMPARE(comparePanelNames(first,second),comparison); }
    void typedProperties_data() {
        QTest::addColumn<int>("type"); QTest::addColumn<QString>("first"); QTest::addColumn<QString>("second");
        QTest::newRow("ui1")<<17<<"2"<<"200";
        QTest::newRow("i2")<<2<<"-32000"<<"100";
        QTest::newRow("ui2")<<18<<"2"<<"65535";
        QTest::newRow("i4")<<3<<"-2147483648"<<"2147483647";
        QTest::newRow("ui4")<<19<<"2"<<"4294967295";
        QTest::newRow("i8")<<20<<"-9223372036854775808"<<"9223372036854775807";
        QTest::newRow("ui8")<<21<<"9007199254740993"<<"18446744073709551615";
        QTest::newRow("bool")<<11<<"0"<<"1";
    }
    void typedProperties() {
        QFETCH(int,type); QFETCH(QString,first); QFETCH(QString,second);
        const auto a=panelSortItem("z",{},{value(kpidOffset,quint16(type),first)},0,false,true),b=panelSortItem("a",{},{value(kpidOffset,quint16(type),second)},1,false,true);
        QCOMPARE(comparePanelItems(a,b,{kpidOffset,true,false}),-1); QCOMPARE(comparePanelItems(a,b,{kpidOffset,false,false}),1);
    }
    void tiesMissingAndVariantTypes() {
        auto a=panelSortItem("file002","dir2",{value(kpidSize,21,"10")},0,false,true),b=panelSortItem("file2","dir10",{value(kpidSize,21,"10")},1,false,true);
        QCOMPARE(comparePanelItems(a,b,{kpidSize,true,false}),-1); QCOMPARE(comparePanelItems(a,b,{kpidSize,false,false}),1);
        b.prefix=a.prefix; QCOMPARE(comparePanelItems(a,b,{kpidSize,true,false}),-1); // load order breaks an otherwise equal tie
        a.values.clear(); b.values.clear(); b.values.insert(quint64(kpidSize)<<1,PanelSortValue{});
        QCOMPARE(comparePanelItems(a,b,{kpidSize,true,false}),-1); // absent size and zero compare equal, then names/prefix/order
        a=panelSortItem("z",{},{value(kpidOffset,19,"100")},0,false,true); b=panelSortItem("a",{},{value(kpidOffset,21,"1")},1,false,true);
        QCOMPARE(comparePanelItems(a,b,{kpidOffset,true,false}),-1); // VT_UI4 before VT_UI8, without loss to double
        a=panelSortItem("z",{},{value(kpidComment,8,{},"note2")},0,false,true); b=panelSortItem("a",{},{value(kpidComment,8,{},"NOTE10")},1,false,true);
        QCOMPARE(comparePanelItems(a,b,{kpidComment,true,false}),1); // comments use ordinal no-case, names use natural order
    }
    void timePrecisionAndRawProperties() {
        auto p=value(kpidMTime,64); p.fileTime="134354736001234561"; p.timePrecision={0,20,0}; auto q=p; q.timePrecision={0,90,0};
        auto a=panelSortItem("z",{},{p},0,false,true),b=panelSortItem("a",{},{q},1,false,true);
        QCOMPARE(comparePanelItems(a,b,{kpidMTime,true,false}),-1);
        q.fileTime="134354736001234562"; b=panelSortItem("a",{},{q},1,false,true); QCOMPARE(comparePanelItems(a,b,{kpidMTime,true,false}),-1);
        p=value(kpidChecksum,0); p.raw=true; p.rawType=1; p.rawSize=2; p.sortData=QByteArray::fromHex("007f"); q=p; q.sortData=QByteArray::fromHex("0080");
        a=panelSortItem("z",{},{p},0,false,true); b=panelSortItem("a",{},{q},1,false,true); QCOMPARE(comparePanelItems(a,b,{kpidChecksum,true,true}),-1);
        p.rawSize=0; p.sortData.clear(); a=panelSortItem("z",{},{p},0,false,true); QCOMPARE(comparePanelItems(a,b,{kpidChecksum,true,true}),-1);
        p=q; p.rawType=9; a=panelSortItem("z",{},{p},0,false,true); QCOMPARE(comparePanelItems(a,b,{kpidChecksum,true,true}),1); // unsupported raw type falls through to name
        p=value(kpidNtReparse,0); p.raw=true; p.rawType=1; p.rawSize=4; p.sortData=QByteArray::fromHex("32000000"); q=p; q.sortData=QByteArray::fromHex("31003000");
        a=panelSortItem("z",{},{p},0,false,true); b=panelSortItem("a",{},{q},1,false,true); QCOMPARE(comparePanelItems(a,b,{kpidNtReparse,true,true}),1); // reparse target uses UTF-16 ordinal, not natural-number order
    }
    void parentFoldersAndUnsorted() {
        auto folder=panelSortItem("z",{},{},0,true,false),file=panelSortItem("a",{},{},1,false,false),parent=panelSortItem("..",{},{},2,true,false); parent.parent=true;
        for(bool ascending:{true,false}) {
            QCOMPARE(comparePanelItems(parent,file,{kpidName,ascending,false}),-1); QCOMPARE(comparePanelItems(folder,file,{kpidName,ascending,false}),-1);
            QCOMPARE(comparePanelItems(parent,parent,{kpidName,ascending,false}),0);
        }
        auto first=panelSortItem("z",{},{},0,false,false),second=panelSortItem("a",{},{},1,false,false);
        QCOMPARE(comparePanelItems(first,second,{kpidNoProperty,true,false}),-1); QCOMPARE(comparePanelItems(first,second,{kpidNoProperty,false,false}),1);
        auto state=nextOfficialSort({},kpidSize); QVERIFY(!state.ascending); state=nextOfficialSort(state,kpidSize); QVERIFY(state.ascending);
        QVERIFY(!nextOfficialSort(state,kpidMTime).ascending); QVERIFY(!nextOfficialSort(state,kpidPackSize).ascending);
        QVERIFY(nextOfficialSort(state,kpidExtension).ascending); QVERIFY(nextOfficialSort(state,kpidNoProperty).ascending);
    }
    void filesystemSchemaAndStatValues() {
        const QList<quint32> expected{kpidName,kpidSize,kpidMTime,kpidCTime,kpidATime,kpidChangeTime,kpidAttrib,kpidPackSize,kpidINode,kpidLinks,kpidComment,kpidNumSubDirs,kpidNumSubFiles,kpidExtension};
        const auto columns=filesystemColumns(false); QCOMPARE(columns.size(),expected.size());
        for(int n=0;n<columns.size();++n) QCOMPARE(columns[n].id,expected[n]);
        const auto flat=filesystemColumns(true); QCOMPARE(flat[flat.size()-2].id,quint32(kpidPrefix));
        QCOMPARE(officialColumnWidth(kpidName,8),160); QCOMPARE(officialColumnWidth(kpidSize,21),100);
        QVERIFY(!officialColumnVisible(kpidATime,true)); QVERIFY(!officialColumnVisible(kpidINode,true)); QVERIFY(officialColumnVisible(kpidComment,true)); QVERIFY(officialColumnVisible(kpidATime,false));
        QCOMPARE(officialColumnAlignment(kpidMTime,64),Qt::AlignLeft|Qt::AlignVCenter); QCOMPARE(officialColumnAlignment(kpidEncrypted,11),Qt::AlignRight|Qt::AlignVCenter);
        const auto path=temporary.filePath("stat/readonly.txt"),link=temporary.filePath("stat/hard link"); QVERIFY(write(path,"owned bytes"));
        QVERIFY(::chmod(QFile::encodeName(path).constData(),0444)==0); QVERIFY(::link(QFile::encodeName(path).constData(),QFile::encodeName(link).constData())==0);
        struct stat stamp{}; QVERIFY(::lstat(QFile::encodeName(path).constData(),&stamp)==0);
        DirectoryScanner scanner; QSignalSpy done(&scanner,&DirectoryScanner::finished); scanner.start(QFileInfo(path).absolutePath(),false); if(done.isEmpty()) QVERIFY(done.wait(10000));
        const auto snapshot=qvariant_cast<DirectorySnapshot>(done.first()[0]); QVERIFY(snapshot.error.isEmpty()); QCOMPARE(snapshot.entries.size(),2);
        for(const auto &entry:snapshot.entries) {
            QCOMPARE(entry.inode,quint64(stamp.st_ino)); QCOMPARE(entry.links,quint64(2)); QCOMPARE(entry.packed,quint64(stamp.st_blocks)*512); QCOMPARE(entry.mode,quint32(stamp.st_mode));
            QVERIFY(entry.accessed.isValid()); QVERIFY(entry.changed.isValid()); QCOMPARE(entry.accessedFraction.size(),9);
            QVERIFY(officialAttributeText(officialPosixAttributes(entry.mode)).contains("-r--r--r--"));
        }
    }
    void headerMenuAndSelection() {
        MainWindow window(executable); window.show(); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(),10000); auto tree=ownedTree(window); QVERIFY(tree);
        QVERIFY(tree->header()->sectionsClickable()); auto first=item(tree,"file2.txt"),second=item(tree,"file10.bin"); QVERIFY(first && second);
        QVERIFY(tree->indexOfTopLevelItem(first)<tree->indexOfTopLevelItem(second));
        tree->setItemSelected(first,true); tree->setCurrentItem(second,0,QItemSelectionModel::NoUpdate);
        clickColumn(tree,kpidSize); QCOMPARE(tree->header()->sortIndicatorOrder(),Qt::DescendingOrder); QCOMPARE(tree->property("sortProperty").toUInt(),quint32(kpidSize));
        QVERIFY(tree->indexOfTopLevelItem(first)<tree->indexOfTopLevelItem(second)); QVERIFY(tree->markedItems().contains(first)); QCOMPARE(tree->currentItem(),second); QCOMPARE(tree->topLevelItem(0)->text(0),QString(".."));
        clickColumn(tree,kpidSize); QCOMPARE(tree->header()->sortIndicatorOrder(),Qt::AscendingOrder); QVERIFY(tree->indexOfTopLevelItem(second)<tree->indexOfTopLevelItem(first));
        tree->header()->moveSection(tree->header()->visualIndex(propertyColumn(tree,kpidSize)),1); clickColumn(tree,kpidSize); QCOMPARE(tree->header()->sortIndicatorOrder(),Qt::DescendingOrder);
        for(int mode=0;mode<4;++mode) { window.findChild<QAction *>("mode"+QString::number(mode)+"Action")->trigger(); window.findChild<QAction *>("sort1Action")->trigger(); QCOMPARE(tree->property("sortProperty").toUInt(),quint32(kpidExtension)); QVERIFY(tree->markedItems().contains(first)); QCOMPARE(tree->currentItem(),second); }
        window.findChild<QAction *>("sort2Action")->trigger(); QCOMPARE(tree->header()->sortIndicatorOrder(),Qt::DescendingOrder);
        const auto early=item(tree,"a-date.txt"),late=item(tree,"z-date.txt"); QCOMPARE(early->text(propertyColumn(tree,kpidMTime)),late->text(propertyColumn(tree,kpidMTime))); QVERIFY(tree->indexOfTopLevelItem(late)<tree->indexOfTopLevelItem(early));
        window.findChild<QAction *>("unsortedAction")->trigger(); QCOMPARE(tree->property("sortProperty").toUInt(),quint32(kpidNoProperty)); QVERIFY(!tree->header()->isSortIndicatorShown());
        const auto ascending=fileNames(tree); window.findChild<QAction *>("unsortedAction")->trigger(); QCOMPARE(tree->header()->sortIndicatorOrder(),Qt::DescendingOrder); QVERIFY(fileNames(tree)!=ascending); QCOMPARE(tree->topLevelItem(0)->text(0),QString(".."));
    }
    void propertySettingsSurviveFlatAndPanels() {
        QSettings().setValue("View/TwoPanels",true);
        {
            MainWindow window(executable); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(),10000); auto tree=ownedTree(window); QVERIFY(tree);
            auto child=window.findChild<MainWindow *>("secondPanel"); QVERIFY(child); auto other=ownedTree(*child); QVERIFY(other);
            const int type=propertyColumn(tree,kpidExtension); tree->setColumnWidth(type,177); tree->setColumnHidden(type,true); tree->setColumnWidth(0,301);
            child->findChild<QAction *>("sort3Action")->trigger(); window.findChild<QAction *>("sort1Action")->trigger();
            QCOMPARE(other->property("sortProperty").toUInt(),quint32(kpidSize)); QCOMPARE(tree->property("sortProperty").toUInt(),quint32(kpidExtension));
            window.findChild<QAction *>("flatAction")->trigger(); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(),10000);
            QCOMPARE(tree->headerItem()->data(propertyColumn(tree,kpidExtension),ColumnWidthRole).toInt(),177); QVERIFY(tree->isColumnHidden(propertyColumn(tree,kpidExtension))); QCOMPARE(tree->columnWidth(0),301);
            const int prefix=propertyColumn(tree,kpidPrefix); QVERIFY(prefix>=0); tree->setColumnWidth(prefix,181); tree->header()->moveSection(tree->header()->visualIndex(prefix),1);
            window.findChild<QAction *>("flatAction")->trigger(); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(),10000); QCOMPARE(propertyColumn(tree,kpidPrefix),-1);
            window.findChild<QAction *>("flatAction")->trigger(); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(),10000);
            QCOMPARE(tree->columnWidth(propertyColumn(tree,kpidPrefix)),181); QCOMPARE(tree->header()->visualIndex(propertyColumn(tree,kpidPrefix)),1);
            window.close();
        }
        MainWindow reopened(executable); QTRY_VERIFY_WITH_TIMEOUT(!reopened.operationBusy(),10000); auto tree=ownedTree(reopened); QCOMPARE(tree->columnWidth(0),301);
        QCOMPARE(tree->property("sortProperty").toUInt(),quint32(kpidExtension)); QCOMPARE(tree->header()->visualIndex(propertyColumn(tree,kpidPrefix)),1); QCOMPARE(tree->headerItem()->data(propertyColumn(tree,kpidExtension),ColumnWidthRole).toInt(),177);
        auto child=reopened.findChild<MainWindow *>("secondPanel"); QVERIFY(child); QCOMPARE(ownedTree(*child)->property("sortProperty").toUInt(),quint32(kpidSize));
    }
    void legacyColumnSettingsMigration() {
        QTreeWidget old; old.setColumnCount(9); old.header()->setStretchLastSection(false); old.setColumnWidth(4,213); old.setColumnHidden(4,true); old.header()->moveSection(4,1); old.header()->setSortIndicator(2,Qt::DescendingOrder);
        QSettings settings; settings.setValue("View/FS/Header",old.header()->saveState()); settings.setValue("View/FS/Width4",213);
        MainWindow window(executable); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(),10000); auto tree=ownedTree(window); QVERIFY(tree);
        QCOMPARE(tree->headerItem()->data(propertyColumn(tree,kpidComment),ColumnWidthRole).toInt(),213); QVERIFY(tree->isColumnHidden(propertyColumn(tree,kpidComment))); QCOMPARE(tree->header()->visualIndex(propertyColumn(tree,kpidComment)),1);
        QCOMPARE(tree->property("sortProperty").toUInt(),quint32(kpidMTime)); QCOMPARE(tree->header()->sortIndicatorOrder(),Qt::DescendingOrder); QCOMPARE(QSettings().value("View/FS/ColumnSchema").toInt(),2);
    }
    void archivePropertyMenu_data() { QTest::addColumn<QString>("format"); QTest::newRow("7z")<<"7z"; QTest::newRow("zip")<<"zip"; }
    void archivePropertyMenu() {
        QFETCH(QString,format); const auto path=temporary.filePath("sorting."+format);
        SevenZipProcessBackend backend(executable); ArchiveRequest request; request.operation=ArchiveOperation::Add; request.archive=path; request.workingDirectory=root; request.files={"file2.txt","file10.bin","a-date.txt","z-date.txt","日本語 space.txt","folder2","folder10"}; request.format=format; request.method=format=="zip"?"Deflate":"LZMA2"; request.dictionary="4m"; request.level=1; request.threads=2;
        auto result=execute(backend,request); QVERIFY2(result.success,qPrintable(result.message+result.details));
        MainWindow window(executable); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(),10000); window.openPath(path); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(),10000);
        auto tree=ownedTree(window); QVERIFY(tree->property("nativeMetadata").toBool()); QVERIFY(propertyColumn(tree,kpidSize)>=0);
        window.findChild<QAction *>("sort1Action")->trigger(); QCOMPARE(tree->property("sortProperty").toUInt(),quint32(kpidExtension)); QVERIFY(tree->property("virtualSortProperty").isValid());
        QVERIFY(tree->indexOfTopLevelItem(item(tree,"file10.bin"))<tree->indexOfTopLevelItem(item(tree,"file2.txt"))); QCOMPARE(tree->topLevelItem(0)->text(0),QString(".."));
        window.findChild<QAction *>("sort3Action")->trigger(); QCOMPARE(tree->header()->sortIndicatorSection(),propertyColumn(tree,kpidSize)); QCOMPARE(tree->header()->sortIndicatorOrder(),Qt::DescendingOrder);
        QVERIFY(tree->indexOfTopLevelItem(item(tree,"file2.txt"))<tree->indexOfTopLevelItem(item(tree,"file10.bin")));
        window.findChild<QAction *>("sort2Action")->trigger(); QCOMPARE(tree->header()->sortIndicatorSection(),propertyColumn(tree,kpidMTime));
        window.findChild<QAction *>("sort1Action")->trigger(); window.findChild<QAction *>("refreshAction")->trigger(); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(),10000); QCOMPARE(tree->property("sortProperty").toUInt(),quint32(kpidExtension));
        request.operation=ArchiveOperation::Test; result=execute(backend,request); QVERIFY(result.success);
    }
};
int main(int argc,char **argv) {
    if(argc<2) return 1; executable=QString::fromLocal8Bit(argv[1]); QLocale::setDefault(QLocale(QLocale::English));
    if(!qEnvironmentVariable("PORT_TEST_PLUGIN_ROOT").isEmpty()) QCoreApplication::setLibraryPaths({qEnvironmentVariable("PORT_TEST_PLUGIN_ROOT")});
    QCoreApplication::setAttribute(Qt::AA_MacDontSwapCtrlAndMeta); QCoreApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
    QApplication app(argc,argv); applyPortAppearance(app); app.setOrganizationName("SevenZipMacPortTests"); app.setApplicationName("PanelSort"); app.setQuitOnLastWindowClosed(false);
    PanelSortTests tests; QList<char *> args{argv[0]}; for(int n=2;n<argc;++n) args<<argv[n]; args<<nullptr; return QTest::qExec(&tests,args.size()-1,args.data());
}
#include "panel-sort.moc"
