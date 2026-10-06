#pragma once
#include "ArchiveBackend.h"
#include "ArchiveProperties.h"
#include "FileOperations.h"
#include "VersionControl.h"
#include "FolderStatistics.h"
#include "DirectoryScanner.h"
#include "FileComments.h"
#include "PanelIcons.h"
#include "Dialogs.h"
#include "OptionsDialog.h"
#include <QMainWindow>
#include <QTreeWidget>
#include <QLineEdit>
#include <QPointer>
#include <QPersistentModelIndex>
#include <QToolBar>
#include <QTemporaryDir>
#include <QListView>
#include <QFileSystemWatcher>
#include <QTimer>
#include <optional>
class QStackedWidget;
class QSplitter;
class QVBoxLayout;
class QHBoxLayout;
class QMenu;
class QDateTime;
class ExternalProcess;
class QInputDialog;
class ComboDialog;
class AddressCombo;
class OverwriteDialog;

class PanelSelection;
class PanelDragController;
class FileList : public QTreeWidget {
    Q_OBJECT
public:
    explicit FileList(QWidget *parent = nullptr);
    ~FileList() override;
    void setSortingEnabled(bool);
    bool isSortingEnabled() const;
    void sortItems(int, Qt::SortOrder);
    bool sortingBusy() const;
    bool nativeIconsBusy() const;
    void cancelSorting();
    void clear();
    void loadNativeIcons(QList<PanelIconRequest>);
    bool archiveView = false;
    PanelSelection *selection = nullptr;
    QList<QTreeWidgetItem *> markedItems() const;
    QList<QTreeWidgetItem *> operatedItems() const;
    void setItemSelected(QTreeWidgetItem *, bool);
    void notifyMarksChanged() { emit marksChanged(); }
signals:
    void filesDropped(QStringList paths);
    void archiveDragRequested();
    void filesystemDragRequested();
    void marksChanged();
    void sortingStateChanged(bool);
    void renameAccepted(QPersistentModelIndex index, QString name);
    void renameEditingChanged();
protected:
    QItemSelectionModel::SelectionFlags selectionCommand(const QModelIndex &, const QEvent *) const override;
    void dragEnterEvent(QDragEnterEvent *) override;
    void dragMoveEvent(QDragMoveEvent *) override;
    void dropEvent(QDropEvent *) override;
    void startDrag(Qt::DropActions) override;
private:
    struct Work;
    std::unique_ptr<Work> work;
    void loadNextIcons();
};
class IconFileList : public QListView {
    Q_OBJECT
public:
    using QListView::QListView;
    FileList *fileList = nullptr;
signals:
    void filesDropped(QStringList paths);
    void archiveDragRequested();
    void filesystemDragRequested();
protected:
    QItemSelectionModel::SelectionFlags selectionCommand(const QModelIndex &, const QEvent *) const override;
    void dragEnterEvent(QDragEnterEvent *) override;
    void dragMoveEvent(QDragMoveEvent *) override;
    void dropEvent(QDropEvent *) override;
    void startDrag(Qt::DropActions) override;
};
class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QString backendPath, QWidget *parent = nullptr, bool embedded = false);
    ~MainWindow() override;
    void openPath(QString path, QString readMode = {}, bool reopen = false);
    void executeOpenWith(QString command, QStringList paths);
    QString currentArchivePath() const { return archivePath; }
    QString currentDirectory() const { return fsPath; }
    bool operationBusy() const;
signals:
    void directoryLoaded(QString path, bool success);
private:
    friend class OpenWithController;
    friend class PanelDragController;
    PanelDragController *dragController = nullptr;
    bool panelBusy() const { return copyDialogActive || transferOperation || (files && (files->property("renameEditing").toBool() || files->property("renamePending").toBool())) || openCommandActive || !openCommands.isEmpty() || commentInProgress || backend.busy() || nestedOpen || nestedUpdate || afterNestedExit || externalUpdating || externalDecision || fileOps.busy() || versionControl.busy() || folderStatistics.busy(); }
    bool dataOperationBusy() const;
    void buildMenus();
    void buildToolbar();
    void askOverwrite(OverwriteConflict conflict, bool filesystem);
    void showFileContextMenu(QPoint globalPosition);
    void buildViewMenus(QMenu *view);
    void setupViews(QVBoxLayout *, QHBoxLayout *);
    void finishBrowse();
    void saveViewColumns();
    void sortByProperty(quint32 property, bool raw = false);
    void applyViewSettings();
    void setViewMode(int mode);
    void setTwoPanels(bool enabled);
    void updateToolbarSettings();
    QString timestamp(QDateTime value, QString fraction = {}) const;
    QString timestamp(QDateTime value, QString fraction, int precision, bool utc) const;
    QString viewKey() const;
    MainWindow *otherPanel() const;
    void setOtherPanelFolder(bool same);
    void focusList();
    void focusAddress(int panel);
    void submitAddress(QString path);
    void options();
    void applySettings();
    void launchExternal(const QStringList &paths, const QString &command = {});
    void diffFiles();
    void setupVersionControl();
    void versionCommand(VersionCommand command);
    void splitFile();
    void combineFiles();
    void deleteTemporaryFiles();
    QStringList activeTemporaryPaths() const;
    void link();
    void selectByType(bool select);
    void copyNamesToClipboard();
    void calculateHash(const QString &method);
    void saveBookmark(int index);
    void openBookmark(int index);
    void updateBookmarks();
    void fileProgress(QString operation, QString target);
    void showFilesystem(QString path, std::function<void()> continuation = {});
    void setupFilesystem();
    void cancelFilesystemRead();
    void prepareFilesystemRows();
    void installFilesystemRows();
    QString requestedDirectory() const;
    void showArchive();
    void setArchiveListing(const ArchiveResult &result);
    void activateItem(QTreeWidgetItem *item, bool insideOnly = false, QString readMode = {});
    void openSelectedItems(bool tryInternal);
    void continueOpenCommands();
    void openExternalItems(const QList<QTreeWidgetItem *> &items, bool drag, const QString &command);
    void startExternalExtraction(ArchiveRequest request, bool drag, const QString &command);
    bool externalNameBlocked(const QString &path);
    QString archiveLocation() const;
    QString archiveWorkingFolder() const;
    void navigateArchive(QString relative);
    void bindArchiveDirectory(int id);
    int alternateArchiveDirectory() const;
    void openAlternateStreams();
    void openNestedArchive(QString entry, bool externalFallback, QString tail = {}, QString readMode = {}, QTreeWidgetItem *source = nullptr);
    bool finishNestedOpen(const ArchiveResult &result);
    bool finishNestedUpdate(const ArchiveResult &result);
    void leaveNestedArchives(QString keepVirtual, std::function<void()> continuation);
    void continueNestedExit();
    void restoreParentArchive();
    bool canUpdateArchive() const;
    void selectArchiveItem(const QString &entry, qint64 archiveIndex = -1);
    void up();
    void refresh();
    void add(QStringList sources = {}, QString destination = {});
    void copyIntoArchive(QStringList sources, bool move = false);
    void extract();
    void test();
    void transfer(bool move, bool copyToSame = false);
    bool finishTransfer(const ArchiveResult &);
    bool finishFilesystemTransfer(const QString &error);
    void remove();
    void rename();
    void commitRename(QPersistentModelIndex index, QString name);
    void newFolder();
    bool canCreateArchiveFolder() const;
    void newFile();
    void info();
    void setupComments();
    void comment();
    void cancelComment();
    bool canComment() const;
    void finishComment(QString error = {});
    void setupFolderStatistics();
    void viewFocusedItem();
    QStringList operatedFolders() const;
    void openExternal(bool drag = false, const QString &command = {}, bool focusedOnly = false);
    struct ExternalTempGroup {
        std::shared_ptr<QTemporaryDir> temporary;
        bool preserve = false;
        ~ExternalTempGroup() { temporary->setAutoRemove(!preserve); }
    };
    struct ExternalEdit {
        enum State { Running, Complex, Ready, Updating, Done, Closed } state = Running;
        std::shared_ptr<ExternalTempGroup> group;
        QPointer<ExternalProcess> process;
        QString path, archive, location, entry, format, password;
        ArchiveRequest selection;
        struct stat stamp{};
        bool readOnly = false;
        ~ExternalEdit() { password.fill(QChar::Null); }
    };
    void launchArchiveExternal(std::shared_ptr<QTemporaryDir>, const QStringList &entries, const QString &command, const QString &password, const ArchiveRequest &selection);
    static bool rebindArchiveSelection(ArchiveRequest &, const ArchiveResult &, bool requireAll = true);
    void rebindArchiveSessions(const ArchiveResult &, bool other = true);
    void showUpdatedArchive(const ArchiveResult &);
    void considerExternalEdits();
    bool finishExternalUpdate(const ArchiveResult &);
    void closeExternalSessions();
    static bool externalFileChanged(const ExternalEdit &);
    void retainExternalEdit(const std::shared_ptr<ExternalEdit> &, QString error);
    void run(ArchiveRequest request);
    void onFinished(ArchiveResult result);
    void updateState();
    void updateListInteraction(bool busy);
    QStringList selectedPaths() const;
    QStringList markedPaths() const;
    struct BrowseSelection { QStringList marked; QString focused; int focusedRow = -1; bool focusedSelected = false; };
    struct TransferOperation {
        enum Phase { Filesystem, Extract, Add, Refresh } phase = Filesystem;
        QPointer<MainWindow> source, destination;
        BrowseSelection sourceSelection, destinationSelection;
        std::shared_ptr<QTemporaryDir> temporary;
        ArchiveRequest addRequest;
        QStringList copyNames;
        bool move = false, copyToSame = false;
        ~TransferOperation() { addRequest.password.fill(QChar::Null); }
    };
    std::shared_ptr<TransferOperation> transferOperation;
    bool copyDialogActive = false;
    void completeTransfer();
    void reloadTransferPanel(BrowseSelection selection, std::function<void()> continuation = {});
    std::optional<BrowseSelection> fileOperationSelection;
    QString fileOperationFocus;
    BrowseSelection saveBrowseSelection() const;
    void restoreBrowseSelection(const BrowseSelection &);
    std::optional<BrowseSelection> refreshSelection;
    void setArchiveSelection(ArchiveRequest &request, const QList<QTreeWidgetItem *> &items, bool preservePaths = false) const;
    bool canUseNativeArchiveUpdate() const;
    QList<QTreeWidgetItem *> selectedItems() const;
    QAction *action(QString name, QString text, QString shortcut, std::function<void()> callback);
    void closeEvent(QCloseEvent *) override;
    void showEvent(QShowEvent *) override;
    void syncPanelFonts();
    bool initialWindowPlacement = true;
    bool eventFilter(QObject *, QEvent *) override;
    SevenZipProcessBackend backend;
    FileOperations fileOps;
    VersionControl versionControl;
    FolderStatistics folderStatistics;
    DirectoryScanner directoryScanner;
    bool commentInProgress = false;
    QString commentName, commentFocus;
    QStringList commentSelection;
    BrowseSelection commentBrowseSelection;
    QList<ArchiveRequest::SelectionItem> commentNativeSelection;
    QFutureWatcher<FileCommentDocument> commentReader;
    QFutureWatcher<QString> commentWriter;
    std::shared_ptr<OperationControl> commentControl;
    QPointer<ComboDialog> commentDialog;
    QPointer<OverwriteDialog> overwriteDialog;
    struct FilesystemRead {
        quint64 generation = 0;
        QString path;
        DirectorySnapshot snapshot;
        qsizetype next = 0;
        int timePrecision = 1;
        bool utc = false;
        std::shared_ptr<const QList<ArchivePropertyDefinition>> columns;
        QList<QTreeWidgetItem *> rows;
        std::function<void()> continuation;
        ~FilesystemRead() { qDeleteAll(rows); }
    };
    std::unique_ptr<FilesystemRead> filesystemRead;
    QTimer filesystemChunks;
    FileList *files = nullptr;
    IconFileList *icons = nullptr;
    QStackedWidget *viewStack = nullptr;
    QSplitter *panels = nullptr;
    MainWindow *secondPanel = nullptr;
    bool embedded = false, loadingView = true, flatView = false, autoRefresh = true;
    bool hasBrowse = false, uiReady = false;
    int viewMode = 3, activePanel = 0;
    QString backendExecutable;
    QFileSystemWatcher watcher;
    QTimer watchDebounce;
    QLineEdit *pathBox = nullptr;
    AddressCombo *pathCombo = nullptr;
    bool listFocusPending = false;
    QToolBar *toolbar = nullptr;
    QPointer<ProgressDialog> progressDialog;
    QPointer<MainWindow> archiveTransferSource;
    QMap<QString, QAction *> actions;
    bool rightControlDown = false;
    QPointer<QWidget> browseFocus, browseFocusDisplaced;
    FileManagerSettings settings;
    QString fsPath, archivePath, archivePrefix, archivePassword, pendingArchive;
    QString archiveVirtualPath, pendingArchiveTail;
    bool pendingReopen = false;
    QString pendingDefaultOpen;
    QList<ArchiveEntry> archiveEntries;
    QString archiveType, archiveReadMode;
    QMap<QString, QString> archiveProperties;
    QList<ArchiveLayer> archiveLayers;
    ArchiveFolderIndex archiveFolderIndex;
    int archiveDirectory = 0;
    struct ArchiveFrame {
        QString path, virtualPath, prefix, password, type, child;
        QList<ArchiveEntry> entries;
        QMap<QString, QString> properties;
        std::shared_ptr<QTemporaryDir> temporary;
        qint64 childSize = -1;
        struct stat childStamp{};
        QString readMode;
        QList<ArchiveLayer> layers;
        ArchiveFolderIndex folderIndex;
        int directory = 0;
        ArchiveRequest childSelection;
    };
    struct NestedOpen {
        ArchiveFrame parent;
        std::shared_ptr<QTemporaryDir> temporary;
        QString entry, path, virtualPath, tail, readMode;
        bool listing = false, externalFallback = false;
    };
    QList<ArchiveFrame> parentArchives;
    std::shared_ptr<QTemporaryDir> archiveTemporary;
    std::unique_ptr<NestedOpen> nestedOpen;
    struct NestedUpdate { QString path; std::shared_ptr<QTemporaryDir> temporary; };
    std::unique_ptr<NestedUpdate> nestedUpdate;
    QString exitKeepVirtual;
    std::function<void()> afterNestedExit;
    ArchiveRequest lastRequest;
    struct OpenCommand {
        enum Kind { BrowseFolder, ExternalFolder, OpenFile } kind;
        QString path;
        bool parent = false, tryInternal = false;
        int directory = -1;
        ArchiveRequest selection;
    };
    QList<OpenCommand> openCommands;
    bool openCommandActive = false;
    QList<ArchiveRequest> openWithBatch;
    bool externalPending = false, dragPending = false;
    QString externalCommand;
    QList<std::shared_ptr<ExternalEdit>> externalEdits;
    std::shared_ptr<ExternalEdit> externalUpdating;
    QTimer externalDecisions;
    bool externalDecision = false;
    bool closingRequested = false;
    std::unique_ptr<QTemporaryDir> externalTemp;
    QList<std::shared_ptr<QTemporaryDir>> retainedTemps;
};
