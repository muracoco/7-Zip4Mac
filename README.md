# 7-Zip4Mac

[日本語](README.ja.md) · [User guide](docs/features.md#english) · [Changelog](CHANGELOG.md)

7-Zip4Mac is a Mac archive manager for people familiar with 7-Zip on Windows.
Create 7z and ZIP archives, browse their contents, and extract only the files you need.
The menus, toolbar, and compression settings follow the Windows 7-Zip File Manager.

This is an unofficial, open-source project, independent of the official 7-Zip project.
Compression and extraction use the official 7-Zip engine.

## Features

- Create and extract 7z and ZIP archives; extract RAR and other formats supported by 7-Zip.
- Browse archives, extract selected files, and test archives for corruption.
- Create and open password-protected archives, including encrypted filenames in 7z.
- Choose compression settings and split large archives into volumes.
- Copy, move, and rename files; use two panels and favorites.
- Open a compression and extraction menu from Finder.

**RAR extraction is supported; RAR compression is not.**
See the [user guide](docs/features.md#english) for other formats and operations.

## Supported Macs

The app targets macOS 15 and later. It has been tested on an Apple Silicon Mac running macOS 26.6.2.
macOS 15 and Intel Macs have not yet been tested on hardware.

## Installation

Build the app from source using the steps below. Prebuilt app downloads are not yet available.
You need Apple Command Line Tools, Git, and Python 3. Xcode.app is not required.

Run these commands in Terminal:

```bash
git clone https://github.com/muracoco/7-Zip4Mac.git
cd 7-Zip4Mac
./scripts/bootstrap.sh
./scripts/build.sh
./scripts/install.sh
```

The first build downloads the required tools, 7-Zip, and Qt.
The installer places 7-Zip Mac.app in your Applications folder.
You do not need a separate 7-Zip or Qt installation.

If Command Line Tools are missing, run `xcode-select --install` first.
For custom locations or build troubleshooting, see the [build guide](docs/building.md#english).

To update the app, close it, update your source checkout, and run the three scripts again.

## Basic use

### Extract from Finder

1. Right-click an archive.
2. Choose **Open With → 7-Zip Mac**.
3. In the action menu, choose **Extract Here** to extract into the same folder, or **Extract files…** to choose a destination.

To browse the contents instead, choose **Open in 7-Zip File Manager** at the bottom of the menu.

### Use the File Manager

Launch 7-Zip Mac from your Applications folder.

- Compress: Select files or folders, click **Add**, and choose a destination and format such as 7z or ZIP.
- Extract: Select an archive, click **Extract**, and choose a destination.
- Browse: Double-click an archive in the file list.
- Check for corruption: Select an archive and click **Test**.

See the [user guide](docs/features.md#english) for detailed instructions.

## Differences from Windows

A direct 7-Zip submenu in Finder's right-click menu is not yet available.
Use **Open With** to display the action menu instead.
Window controls and the Trash use macOS behavior.

Some explanations and error messages remain in English. Windows self-extracting archive creation and Explorer-specific features are unavailable.
Current builds do not have Developer ID signing or Apple notarization.
See the [Windows comparison](docs/windows-parity.md) for detailed differences and missing features.

## Report a problem

Open a [GitHub issue](https://github.com/muracoco/7-Zip4Mac/issues) with your macOS and app versions, steps to reproduce, and the error message.
Do not attach passwords or files containing personal information.

## For contributors

The current app version is 0.2.5, using 7-Zip 26.03 and Qt 6.11.3.
The app is written in C++ with Qt Widgets and built with CMake, Ninja, and Apple Clang.
To run the optional developer tests, use these commands from the repository root:

```bash
PORT_BUILD_TESTS=ON ./scripts/build.sh
./scripts/test.sh
```

[Build, run, and test instructions](docs/building.md#english) · [Development workflow](docs/development-workflow.md) ·
[Updating the 7-Zip sources](docs/upstream-updates.md)

## License

The port is licensed under [LGPL-3.0-or-later](LICENSE).
7-Zip and Qt retain their respective licenses. The 7-Zip RAR extraction code also carries the unRAR restriction.

[7-Zip license](licenses/License.txt) · [Qt licenses](licenses/Qt) ·
[Bundled components and copyright notices](licenses/NOTICE.md)
