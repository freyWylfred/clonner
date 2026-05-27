# clonner

A lightweight Windows folder watcher that automatically copies newly added files with a specified extension from a source folder to a destination folder.

## Features

- Watches a source folder (recursively, including sub-folders) for new files
- Works with both local paths (e.g. `C:\WatchSource`) and UNC paths to shared folders (e.g. `\\server\share\folder`)
- Copies only files whose extension matches the one you specify
- Preserves the relative sub-folder structure under the destination
- Configurable from the GUI:
  - Watch folder (local or UNC)
  - Destination folder (local or UNC)
  - Target extension(s) — one or more, e.g. `*.txt` or `*.pdf,*.xlsx` (comma / semicolon / space separated; the leading `*` and `.` are optional)
  - Maximum polling interval (seconds, 1–3600) — the actual wait between scans is randomized in the range `[1 s, max]`
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
4. Set **Extensions** — one or more extensions to watch for. Examples: `*.txt`, `*.pdf,*.xlsx`, `pdf;xlsx`, `log doc`. The leading `*` and `.` are optional.
5. Set **Max interval (sec)** — the upper bound of the polling interval. Each scan waits a random number of milliseconds between 1 s and this value.
6. Click **Start**. The log panel will show copies as they happen.
7. Click **Stop** to end monitoring.

Notes:

- Files that already exist in the watch folder when monitoring starts are treated as "known" and are **not** copied. Only files added afterwards are copied.
- If a file with the same name already exists at the destination it will be overwritten.
- UNC paths such as `\\server\share\folder` are supported for both watch and destination. The current user must have read access to the source and write access to the destination.

## Releases

Pre-built binaries are available on the [Releases page](https://github.com/freyWylfred/clonner/releases).

## License

This project is provided as-is without warranty. See repository for details.
