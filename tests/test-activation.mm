// SPDX-License-Identifier: LGPL-3.0-or-later
#include "test-activation.h"
#include <QApplication>
#include <QTest>
#import <AppKit/AppKit.h>

bool activateTestWindow(QWidget *widget) {
    // A command-line Qt test is not launched by Launch Services. Explicitly
    // activate this test process before checking real Cocoa focus.
    [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
    [NSApp activateIgnoringOtherApps:YES];
    widget->raise(); widget->activateWindow();
    NSWindow *window = reinterpret_cast<NSView *>(widget->winId()).window;
    [window makeKeyAndOrderFront:nil];
    const bool active = QTest::qWaitForWindowActive(widget);
    if (!active) qWarning() << "Cocoa focus unavailable" << bool(NSApp.active) << bool(window.keyWindow) << bool(window.visible);
    return active;
}
