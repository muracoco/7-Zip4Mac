#include <QLocale>
#include "test-activation.h"
// Copyright (C) 2026 7-Zip Mac Port contributors
// SPDX-License-Identifier: LGPL-3.0-or-later
#include "MainWindow.h"
#include "PortStyle.h"
#include <QMenuBar>
#include <QMenu>
#include <QMessageBox>
#include "Help.h"
#include "OpenWith.h"
#include <QFileOpenEvent>
#include <QTemporaryDir>
#include <QTimer>
#include "input-driver.h"
#include <QElapsedTimer>
#include <QPlainTextEdit>
#include <QFile>
#include <QFileInfo>
#include <QTabWidget>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QtTest>
#import <AppKit/AppKit.h>

// Deliver events through AppKit inside this test process, so Cocoa's native
// down/up handling is exercised instead of QTest's Qt event injection.
static void nativeClick(QWidget *widget, QPoint point, Qt::MouseButton button = Qt::LeftButton) {
    NSView *view = reinterpret_cast<NSView *>(widget->window()->winId());
    NSWindow *window = view.window;
    const QPoint local = widget->mapTo(widget->window(), point);
    const NSPoint location = [view convertPoint:NSMakePoint(local.x(), local.y()) toView:nil];
    const auto down = button == Qt::RightButton ? NSEventTypeRightMouseDown : NSEventTypeLeftMouseDown;
    const auto up = button == Qt::RightButton ? NSEventTypeRightMouseUp : NSEventTypeLeftMouseUp;
    for (NSEventType type : {down, up}) {
        NSEvent *event = [NSEvent mouseEventWithType:type location:location modifierFlags:0
            timestamp:NSProcessInfo.processInfo.systemUptime windowNumber:window.windowNumber
            context:nil eventNumber:0 clickCount:1 pressure:1];
        [NSApp sendEvent:event];
        QApplication::processEvents();
    }
}

static void nativeAltKey(QWidget *widget, unsigned short keyCode, NSString *letter) {
    NSWindow *window = reinterpret_cast<NSView *>(widget->window()->winId()).window;
    for (NSEventType type : {NSEventTypeKeyDown, NSEventTypeKeyUp}) {
        NSEvent *event = [NSEvent keyEventWithType:type location:NSZeroPoint
            modifierFlags:NSEventModifierFlagOption timestamp:NSProcessInfo.processInfo.systemUptime
            windowNumber:window.windowNumber context:nil characters:letter
            charactersIgnoringModifiers:letter isARepeat:NO keyCode:keyCode];
        [NSApp sendEvent:event]; QApplication::processEvents();
    }
}

int main(int argc, char **argv) {
    QLocale::setDefault(QLocale(QLocale::English));
    if (argc < 2) return 1;
    if (!qEnvironmentVariable("PORT_TEST_PLUGIN_ROOT").isEmpty()) QCoreApplication::setLibraryPaths({qEnvironmentVariable("PORT_TEST_PLUGIN_ROOT")});
    QCoreApplication::setAttribute(Qt::AA_MacDontSwapCtrlAndMeta);
    QCoreApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
    QApplication app(argc, argv); applyPortAppearance(app);
    app.setOrganizationName("SevenZipMacPortTests"); app.setApplicationName("Cocoa menu regression");
    QTemporaryDir temp; if (!temp.isValid()) return 1;
    QSettings::setDefaultFormat(QSettings::IniFormat); QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, temp.filePath("preferences"));
    QFile fixture(temp.filePath("menu file.txt")); if (!fixture.open(QIODevice::WriteOnly) || fixture.write("menu test") != 9) return 1; fixture.close();
    MainWindow window(QString::fromLocal8Bit(argv[1])); window.openPath(temp.path());
    window.show(); window.raise(); window.activateWindow();
    if (!activateTestWindow(&window)) { qCritical() << "Test window did not activate"; return 1; }
    auto bar = window.menuBar();
    for (auto top : bar->actions()) {
        auto menu = top->menu();
        nativeClick(bar, bar->actionGeometry(top).center());
        QTest::qWait(100);
        NSWindow *nativeMenu = reinterpret_cast<NSView *>(menu->winId()).window;
        if (!menu->isVisible() || QApplication::activePopupWidget() != menu || !nativeMenu.visible) {
            qCritical() << "Native mouse failed to open" << top->text() << "Qt visible" << menu->isVisible() << "native visible" << bool(nativeMenu.visible) << "bar global" << QRect(bar->mapToGlobal(QPoint()), bar->size()) << "menu rect" << menu->geometry(); return 1;
        }
        QTest::keyClick(menu, Qt::Key_Escape); QApplication::processEvents();
    }
    if (bar->actions().size() != 6) { qCritical() << "Expected six original menus"; return 1; }
    const unsigned short menuKeyCodes[] = {3, 14, 9, 0, 17, 4}; // F/E/V/A/T/H.
    NSString *menuLetters[] = {@"f", @"e", @"v", @"a", @"t", @"h"};
    for (int index = 0; index < bar->actions().size(); ++index) {
        auto menu = bar->actions()[index]->menu();
        window.findChild<FileList *>("fileList")->setFocus();
        nativeAltKey(&window, menuKeyCodes[index], menuLetters[index]); QTest::qWait(50);
        if (!menu->isVisible() || QApplication::activePopupWidget() != menu) {
            qCritical() << "Native Alt access key failed" << bar->actions()[index]->text(); return 1;
        }
        QTest::keyClick(menu, Qt::Key_Escape); QApplication::processEvents();
    }
    qInfo() << "AppKit Alt access keys opened all six original top-level menus";
    auto help = bar->actions().last()->menu();
    nativeClick(bar, bar->actionGeometry(bar->actions().last()).center());
    auto about = window.findChild<QAction *>("aboutAction");
    bool sawAbout = false; QTimer dismiss; dismiss.setInterval(20);
    QObject::connect(&dismiss, &QTimer::timeout, [&] {
        if (auto dialog = window.findChild<AboutDialog *>()) {
            sawAbout = true; dialog->accept();
        }
    }); dismiss.start();
    nativeClick(help, help->actionGeometry(about).center());
    QTest::qWait(100); dismiss.stop();
    if (!sawAbout) { qCritical() << "Native mouse failed to invoke About from Help"; return 1; }
    window.activateWindow(); if (!activateTestWindow(&window)) return 1;
    auto file = bar->actions().first()->menu(); auto rename = window.findChild<QAction *>("renameAction");
    if (rename->isEnabled()) { qCritical() << "Rename should require a selection"; return 1; }
    auto tree = window.findChild<FileList *>("fileList");
    for (int n = 0; n < tree->topLevelItemCount(); ++n) if (tree->topLevelItem(n)->text(0) == "menu file.txt") tree->setCurrentItem(tree->topLevelItem(n));
    if (!rename->isEnabled()) { qCritical() << "Rename did not enable after selecting a file"; return 1; }
    nativeClick(bar, bar->actionGeometry(bar->actions().first()).center());
    QTimer::singleShot(100, [&] { if (auto dialog = testInput(&window)) { dialog->setTextValue("renamed.txt"); dialog->accept(); } });
    nativeClick(file, file->actionGeometry(rename).center());
    QElapsedTimer renamedWait; renamedWait.start(); while (window.operationBusy() && renamedWait.elapsed() < 10000) QTest::qWait(10);
    if (!QFileInfo::exists(temp.filePath("renamed.txt")) || QFileInfo::exists(temp.filePath("menu file.txt"))) { qCritical() << "Native menu Rename failed"; return 1; }
    for (int n = 0; n < tree->topLevelItemCount(); ++n) if (tree->topLevelItem(n)->text(0) == "renamed.txt") tree->setCurrentItem(tree->topLevelItem(n));
    nativeClick(bar, bar->actionGeometry(bar->actions().first()).center());
    nativeClick(file, file->actionGeometry(window.findChild<QAction *>("infoAction")).center());
    QDialog *properties = nullptr;
    for (auto dialog : window.findChildren<QDialog *>()) if (dialog->windowTitle() == "Properties" && dialog->isVisible()) properties = dialog;
    if (!properties || !properties->findChild<QTreeWidget *>("propertyList") || properties->findChild<QTreeWidget *>("propertyList")->findItems("renamed.txt", Qt::MatchContains, 1).isEmpty()) { qCritical() << "Native menu Properties failed"; return 1; }
    properties->close();
    window.activateWindow(); if (!activateTestWindow(&window)) return 1;
    bool sawContext = false;
    QTimer dismissContext; dismissContext.setInterval(20);
    QObject::connect(&dismissContext, &QTimer::timeout, [&] {
        if (auto menu = window.findChild<QMenu *>("fileContextMenu"); menu && menu->isVisible()) {
            sawContext = true;
            menu->close();
        }
    });
    dismissContext.start();
    nativeClick(tree->viewport(), tree->visualItemRect(tree->currentItem()).center(), Qt::RightButton);
    QTest::qWait(100); dismissContext.stop();
    if (!sawContext) { qCritical() << "AppKit right mouse failed to open the File Manager context menu"; return 1; }
    window.activateWindow(); if (!activateTestWindow(&window)) return 1;
    auto tools = bar->actions()[4]->menu(); auto options = window.findChild<QAction *>("optionsAction");
    if (!options || !options->isEnabled()) { qCritical() << "Options is unavailable"; return 1; }
    bool optionsApplied = false;
    QTimer::singleShot(100, [&] {
        if (auto dialog = window.findChild<OptionsDialog *>()) {
            auto tabs = dialog->findChild<QTabWidget *>(); if (tabs->count() != 6) { dialog->reject(); return; }
            tabs->setCurrentIndex(4); dialog->findChild<QCheckBox *>("showDots")->setChecked(true);
            QTest::mouseClick(dialog->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Apply), Qt::LeftButton);
            optionsApplied = FileManagerSettings::load().showDots;
            dialog->reject();
        }
    });
    nativeClick(bar, bar->actionGeometry(bar->actions()[4]).center()); nativeClick(tools, tools->actionGeometry(options).center());
    if (!optionsApplied) { qCritical() << "AppKit menu failed to open and apply Options"; return 1; }
    window.activateWindow(); if (!activateTestWindow(&window)) return 1;
    OpenWithController launcher(&window, false);
    QFileOpenEvent fileOpen(temp.filePath("renamed.txt"));
    QApplication::sendEvent(qApp, &fileOpen);
    QTest::qWait(300);
    auto operationMenu = window.findChild<OpenWithMenu *>("openWithMenu");
    if (!operationMenu || !operationMenu->isVisible() || QApplication::activeModalWidget() != operationMenu) {
        qCritical() << "FileOpen did not present the operation menu with File Manager already visible"; return 1;
    }
    NSWindow *nativeOperationMenu = reinterpret_cast<NSView *>(operationMenu->winId()).window;
    if (!nativeOperationMenu.visible || NSApp.keyWindow != nativeOperationMenu) {
        qCritical() << "Open With menu is not the visible native key window"; return 1;
    }
    operationMenu->reject(); QTest::qWait(100);
    if (QApplication::activeModalWidget()) { qCritical() << "Open With Cancel did not restore the File Manager"; return 1; }
    qInfo() << "AppKit mouse opened six menus and the file context menu, and invoked About, Rename, Properties and Options; Options Apply and visible-manager FileOpen passed"; return 0;
}
