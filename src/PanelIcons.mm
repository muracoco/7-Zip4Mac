// SPDX-License-Identifier: LGPL-3.0-or-later
#include "PanelIcons.h"
#include <QFileInfo>
#import <AppKit/AppKit.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>

QImage nativePanelIcon(const PanelIconRequest &request, QSize size) {
    // Qt's own QFileInfoGatherer also calls the Cocoa file-icon provider from
    // a worker. Copy pixels here so no NSImage/native icon engine reaches a UI.
    @autoreleasepool {
        NSImage *icon = nil;
        if (!request.archive) icon = [[NSWorkspace sharedWorkspace] iconForFile:request.path.toNSString()];
        else {
            UTType *type = request.directory ? UTTypeFolder : [UTType typeWithFilenameExtension:QFileInfo(request.path).suffix().toNSString()];
            icon = [[NSWorkspace sharedWorkspace] iconForContentType:type ? type : UTTypeData];
        }
        if (!icon) return {};
        NSRect rect = NSMakeRect(0, 0, size.width(), size.height());
        CGImageRef pixels = [icon CGImageForProposedRect:&rect context:nil hints:nil]; if (!pixels) return {};
        QImage image(size, QImage::Format_RGBA8888_Premultiplied); image.fill(Qt::transparent);
        CGColorSpaceRef color = CGColorSpaceCreateDeviceRGB();
        CGContextRef context = CGBitmapContextCreate(image.bits(), size.width(), size.height(), 8, image.bytesPerLine(), color, kCGImageAlphaPremultipliedLast | kCGBitmapByteOrder32Big);
        CGColorSpaceRelease(color); if (!context) return {};
        CGContextDrawImage(context, CGRectMake(0, 0, size.width(), size.height()), pixels); CGContextRelease(context); return image;
    }
}
