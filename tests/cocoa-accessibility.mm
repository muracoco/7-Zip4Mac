// Copyright (C) 2026 7-Zip Mac Port contributors
// SPDX-License-Identifier: LGPL-3.0-or-later
#include <QApplication>
#include <QAccessible>
#include <QTreeWidget>
#include <QDebug>
#import <AppKit/AppKit.h>
#import <objc/message.h>

int main(int argc, char **argv) {
    if (!qEnvironmentVariable("PORT_TEST_PLUGIN_ROOT").isEmpty()) QCoreApplication::setLibraryPaths({qEnvironmentVariable("PORT_TEST_PLUGIN_ROOT")});
    QApplication app(argc, argv);
    QTreeWidget tree; tree.setColumnCount(3); tree.setSelectionBehavior(QAbstractItemView::SelectRows);
    for (int row = 0; row < 3; ++row) new QTreeWidgetItem(&tree, {QString::number(row), "file", "data"});
    tree.resize(1200, 400); tree.show(); QAccessible::setActive(true); app.processEvents();
    const auto tableId = QAccessible::uniqueId(QAccessible::queryAccessibleInterface(&tree));
    Class cocoaClass = objc_getClass("QMacAccessibilityElement");
    if (!cocoaClass) { qCritical() << "Cocoa accessibility class was not loaded"; return 1; }
    auto makeElement = [cocoaClass](QAccessible::Id identifier, NSString *role) -> id {
        return ((id (*)(id, SEL, QAccessible::Id, id))objc_msgSend)([cocoaClass alloc], sel_registerName("initWithId:role:"), identifier, role);
    };
    // Both ownership guards are checked before unsafe selected-cell resolution.
    // Unpatched Qt fails here with an ordinary exit instead of a crash popup.
    id placeholder = makeElement(tableId, NSAccessibilityRowRole);
    ((void (*)(id, SEL, id))objc_msgSend)((id)cocoaClass, sel_registerName("removeElementsFromCache:"), @[placeholder]);
    if (!QAccessible::accessibleInterface(tableId)) { qCritical() << "Regression: synthesized row deleted its parent table interface"; return 1; }
    [placeholder release];
    if (!QAccessible::accessibleInterface(tableId)) { qCritical() << "Regression: synthesized row deallocation deleted its parent"; return 1; }
    id table = ((id (*)(id, SEL, QAccessible::Id))objc_msgSend)((id)cocoaClass, sel_registerName("elementWithId:"), tableId);
    [table retain];
    auto *cellInterface = QAccessible::accessibleInterface(tableId)->tableInterface()->cellAt(0, 0);
    const auto cellId = QAccessible::uniqueId(cellInterface);
    id cell = ((id (*)(id, SEL, QAccessible::Id))objc_msgSend)((id)cocoaClass, sel_registerName("elementWithId:"), cellId);
    ((void (*)(id, SEL, id))objc_msgSend)((id)cocoaClass, sel_registerName("removeElementsFromCache:"), @[cell]);
    if (!QAccessible::accessibleInterface(cellId)) {
        qCritical() << "Regression: Cocoa deleted a cell still owned by the Widgets table"; return 1;
    }
    for (int cycle = 0; cycle < 20; ++cycle) {
        @autoreleasepool {
            const int columns = cycle % 2 + 2; tree.setColumnCount(columns);
            tree.setCurrentItem(tree.topLevelItem(cycle % 3)); tree.topLevelItem(cycle % 3)->setSelected(true);
            app.processEvents();
            NSArray *rows = [table accessibilityRows];
            for (id row in rows) (void)[row accessibilityChildren];
            NSArray *selected = [table accessibilitySelectedChildren];
            if (!QAccessible::accessibleInterface(tableId) || [selected count] != NSUInteger(columns)) {
                qCritical() << "Native selected-child query lost the table or cells in cycle" << cycle; return 1;
            }
            tree.clearSelection();
        }
    }
    // File Manager refresh/navigation clears and repopulates the fixed-column
    // tree. Exercise that real application route after native cells exist.
    tree.setColumnCount(9);
    for (int cycle = 0; cycle < 20; ++cycle) {
        @autoreleasepool {
            tree.clear();
            for (int row = 0; row < 3; ++row)
                new QTreeWidgetItem(&tree, {QString::number(cycle), QString::number(row), QStringLiteral("日本語 file"), "", "", "", "", "", "TXT"});
            tree.doItemsLayout(); app.processEvents();
            tree.setCurrentItem(tree.topLevelItem(cycle % 3));
            tree.topLevelItem(cycle % 3)->setSelected(true);
            app.processEvents();
            for (id row in [table accessibilityRows]) (void)[row accessibilityChildren];
            NSArray *selected = [table accessibilitySelectedChildren];
            if (!QAccessible::accessibleInterface(tableId) || [selected count] != 9u) {
                qCritical() << "Native selected-child query failed after list refresh" << cycle
                            << "selected cells" << [selected count] << "table columns" << tree.columnCount()
                            << "Qt selected indexes" << tree.selectionModel()->selectedIndexes().size()
                            << "stable parent" << bool(QAccessible::accessibleInterface(tableId)); return 1;
            }
        }
    }
    [table release]; qInfo() << "Cocoa ownership guards, 20 column-change and 20 list-refresh cycles passed"; return 0;
}
