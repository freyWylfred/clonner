# clonner

A lightweight Windows folder watcher that automatically copies newly added files with a specified extension from a source folder to a destination folder.

## Features

- Watches a source folder (recursively, including sub-folders) for new files
- Copies only files whose extension matches the one you specify
- Preserves the relative sub-folder structure under the destination
- Configurable from the GUI:
  - Watch folder
  - Destination folder
  - Target extension (e.g. `.txt`, `.log`)
  - Polling interval (seconds, 1–3600)
- Simple log panel showing copy results
- Browse buttons for picking folders via the standard Windows shell dialog
- Robust copying with retry (useful when files are still being written)
- Clean start/stop control with a background worker thread

## Requirements

- Windows 10 / 11 (x64)
- Visual Studio 2022 with the "Desktop development with C++" workload (to build)

## Build

Open `clonner.slnx` in Visual Studio and build the **Release / x64** configuration, or from a Developer PowerShell:

```powershell
& "C:\Program Files\Microsoft Visual Studio\18\Professional\MSBuild\Current\Bin\MSBuild.exe" `
    clonner\clonner.vcxproj /p:Configuration=Release /p:Platform=x64
```

The resulting binary is `clonner\x64\Release\clonner.exe`.

## Usage

1. Launch `clonner.exe`.
2. Set **Watch folder** — the folder to monitor.
3. Set **Destination folder** — where matching files will be copied.
4. Set **Extension** — for example `.txt` (the leading dot is optional).
5. Set **Interval (sec)** — how often the watch folder is scanned.
6. Click **Start**. The log panel will show copies as they happen.
7. Click **Stop** to end monitoring.

Notes:

- Files that already exist in the watch folder when monitoring starts are treated as "known" and are **not** copied. Only files added afterwards are copied.
- If a file with the same name already exists at the destination it will be overwritten.

## Releases

Pre-built binaries are available on the [Releases page](https://github.com/freyWylfred/clonner/releases).

## License

This project is provided as-is without warranty. See repository for details.
