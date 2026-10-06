// SPDX-License-Identifier: LGPL-3.0-or-later
#include "MainWindow.h"
#include "FileListItem.h"
#include "PanelIcons.h"
#include <QApplication>
#include <QFutureWatcher>
#include <QScrollBar>
#include <QSignalBlocker>
#include <QThreadPool>
#include <QtConcurrent>
#include <atomic>
#include <numeric>

namespace {
using Cancellation = std::shared_ptr<std::atomic_bool>;
// Drain native icon requests while the platform integration is still alive.
// A panel can be destroyed without waiting for an OS icon lookup to return.
QThreadPool *listingPool() {
    static QPointer<QThreadPool> pool;
    if (!pool) {
        pool = new QThreadPool(qApp); pool->setMaxThreadCount(2);
        QObject::connect(qApp, &QCoreApplication::aboutToQuit, pool, [p = pool.data()] { p->waitForDone(); });
    }
    return pool;
}
struct SortCancelled {};
struct IconResult { int row; QImage image; };
}
struct FileList::Work {
    QTimer deferredSort;
    Cancellation sortCancel, iconCancel;
    quint64 sortGeneration = 0, iconGeneration = 0;
    bool enabled = false, applying = false, sorting = false;
    QList<PanelIconRequest> iconRequests;
    QHash<int, QTreeWidgetItem *> iconItems;
    int nextIcon = 0;
};
FileList::FileList(QWidget *parent) : QTreeWidget(parent), work(std::make_unique<Work>()) {
    // Qt's item model always stays unsorted: large comparisons run on immutable
    // copies, and Qt widgets/model indexes never cross the worker boundary.
    QTreeWidget::setSortingEnabled(false);
    work->deferredSort.setSingleShot(true);
    connect(&work->deferredSort, &QTimer::timeout, this, [this] { if (work->enabled) sortItems(header()->sortIndicatorSection(), header()->sortIndicatorOrder()); });
    connect(qApp, &QCoreApplication::aboutToQuit, this, [this] {
        if (work->iconCancel) work->iconCancel->store(true); cancelSorting();
    });
    auto changed = [this] {
        if (work->applying) return;
        cancelSorting();
        if (work->enabled) work->deferredSort.start(0);
    };
    connect(model(), &QAbstractItemModel::rowsInserted, this, changed);
    connect(model(), &QAbstractItemModel::rowsRemoved, this, [this, changed] {
        if (!work->applying) { ++work->iconGeneration; if (work->iconCancel) work->iconCancel->store(true); work->iconItems.clear(); work->iconRequests.clear(); }
        changed();
    });
    connect(model(), &QAbstractItemModel::dataChanged, this, [changed](const QModelIndex &, const QModelIndex &, const QList<int> &roles) {
        if (roles.isEmpty() || roles.contains(Qt::DisplayRole) || roles.contains(Qt::EditRole)) changed();
    });
}
FileList::~FileList() {
    disconnect(model(), nullptr, this, nullptr); disconnect(qApp, nullptr, this, nullptr);
    if (work->sortCancel) work->sortCancel->store(true);
    if (work->iconCancel) work->iconCancel->store(true);
}
bool FileList::isSortingEnabled() const { return work->enabled; }
bool FileList::sortingBusy() const { return work->sorting; }
bool FileList::nativeIconsBusy() const { return !work->iconRequests.isEmpty(); }
void FileList::cancelSorting() {
    work->deferredSort.stop(); ++work->sortGeneration;
    if (work->sortCancel) work->sortCancel->store(true);
    work->sortCancel.reset();
    if (work->sorting) { work->sorting = false; emit sortingStateChanged(false); }
}
void FileList::setSortingEnabled(bool enabled) {
    if (work->enabled == enabled) return;
    work->enabled = enabled; header()->setSectionsClickable(enabled);
    if (!enabled) cancelSorting();
    else sortItems(header()->sortIndicatorSection(), header()->sortIndicatorOrder());
}
void FileList::clear() {
    cancelSorting(); ++work->iconGeneration;
    if (work->iconCancel) work->iconCancel->store(true);
    work->iconCancel.reset(); work->iconRequests.clear(); work->iconItems.clear();
    work->applying = true; QTreeWidget::clear(); work->applying = false;
}
void FileList::sortItems(int column, Qt::SortOrder order) {
    cancelSorting(); header()->setSortIndicator(column, order);
    if (topLevelItemCount() < 1024) {
        work->applying = true; QTreeWidget::sortItems(column, order); work->applying = false;
        return;
    }
    const auto property = this->property("virtualSortProperty");
    const PanelSortState state{property.isValid() ? property.toUInt() : headerItem()->data(column, ColumnPropertyRole).toUInt(), order == Qt::AscendingOrder, headerItem()->data(column, ColumnRawRole).toBool()};
    QList<PanelSortItem> snapshot; snapshot.reserve(topLevelItemCount());
    for (int row = 0; row < topLevelItemCount(); ++row) {
        const auto item = topLevelItem(row); auto original = dynamic_cast<const FileItem *>(item);
        auto node = original ? original->sortInfo : panelSortItem(item->text(0), {}, {}, row, false, archiveView);
        node.parent = item->data(0, Qt::UserRole + 2).toBool(); node.directory = item->data(0, Qt::UserRole + 1).toBool(); snapshot.append(std::move(node));
    }
    const auto generation = work->sortGeneration;
    const auto cancel = work->sortCancel = std::make_shared<std::atomic_bool>(false);
    work->sorting = true; emit sortingStateChanged(true);
    auto watcher = new QFutureWatcher<QList<int>>(this);
    connect(watcher, &QFutureWatcherBase::finished, this, [this, watcher, generation] {
        const auto indexes = watcher->result(); watcher->deleteLater();
        if (generation != work->sortGeneration || indexes.size() != topLevelItemCount()) return;
        const auto focused = currentItem(); const int focusedColumn = currentColumn(), position = verticalScrollBar()->value();
        const auto selected = QTreeWidget::selectedItems();
        {
            QSignalBlocker notifications(this); work->applying = true; setUpdatesEnabled(false);
            const auto rows = invisibleRootItem()->takeChildren(); QList<QTreeWidgetItem *> ordered; ordered.reserve(rows.size());
            for (const auto index : indexes) ordered.append(rows[index]);
            addTopLevelItems(ordered);
            for (const auto item : selected) item->setSelected(true);
            if (focused) setCurrentItem(focused, focusedColumn, QItemSelectionModel::NoUpdate);
            verticalScrollBar()->setValue(position); setUpdatesEnabled(true); work->applying = false;
        }
        work->sortCancel.reset(); work->sorting = false; emit sortingStateChanged(false);
    });
    watcher->setFuture(QtConcurrent::run(listingPool(), [snapshot = std::move(snapshot), state, cancel] {
        QList<int> indexes(snapshot.size()); std::iota(indexes.begin(), indexes.end(), 0);
        try {
            std::sort(indexes.begin(), indexes.end(), [&](int a, int b) { if (cancel->load()) throw SortCancelled{}; return comparePanelItems(snapshot[a], snapshot[b], state) < 0; });
        } catch (const SortCancelled &) { return QList<int>{}; }
        return cancel->load() ? QList<int>{} : indexes;
    }));
}
void FileList::loadNativeIcons(QList<PanelIconRequest> requests) {
    ++work->iconGeneration; if (work->iconCancel) work->iconCancel->store(true);
    work->iconCancel = std::make_shared<std::atomic_bool>(false);
    work->iconRequests = std::move(requests); work->nextIcon = 0; work->iconItems.clear();
    for (int row = 0; row < topLevelItemCount(); ++row) { auto item = topLevelItem(row); if (!item->data(0, Qt::UserRole + 2).toBool()) work->iconItems.insert(item->data(0, PanelIconIdentityRole).toInt(), item); }
    loadNextIcons();
}
void FileList::loadNextIcons() {
    if (work->nextIcon >= work->iconRequests.size()) { work->iconRequests.clear(); work->iconCancel.reset(); return; }
    const auto generation = work->iconGeneration; const auto cancel = work->iconCancel;
    QList<PanelIconRequest> batch; const int end = qMin(work->nextIcon + 64, int(work->iconRequests.size()));
    for (; work->nextIcon < end; ++work->nextIcon) batch.append(work->iconRequests[work->nextIcon]);
    // IDs are stable across sorting; neither pointers nor model indexes enter a worker.
    const qreal ratio = devicePixelRatioF();
    auto watcher = new QFutureWatcher<QList<IconResult>>(this);
    connect(watcher, &QFutureWatcherBase::finished, this, [this, watcher, generation, ratio] {
        const auto results = watcher->result(); watcher->deleteLater();
        if (generation != work->iconGeneration) return;
        for (const auto &result : results) if (!result.image.isNull() && work->iconItems.contains(result.row)) { auto pixmap = QPixmap::fromImage(result.image); pixmap.setDevicePixelRatio(ratio); work->iconItems[result.row]->setIcon(0, QIcon(pixmap)); }
        loadNextIcons();
    });
    watcher->setFuture(QtConcurrent::run(listingPool(), [batch = std::move(batch), cancel, ratio] {
        QList<IconResult> results;
        for (const auto &request : batch) {
            if (cancel->load()) break;
            const auto image = nativePanelIcon(request, QSize(qRound(48 * ratio), qRound(48 * ratio)));
            if (!image.isNull()) results.append({request.identity, image});
        }
        return results;
    }));
}
