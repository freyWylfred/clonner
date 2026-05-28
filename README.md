# clonner

A lightweight Windows folder watcher that automatically copies newly added files with the extensions you specify from a source folder to a destination folder.

`clonner` is a single, dependency-free `clonner.exe` (Win32 C++). Drop it anywhere, run it, point it at a folder, and any new matching files that appear in that folder will be copied to your destination - including files uploaded across a network share.

---

## Table of contents

- [Features](#features)
- [Requirements](#requirements)
- [Quick start](#quick-start)
- [Build from source](#build-from-source)
- [How it works](#how-it-works)
- [Configuration file (config.ini)](#configuration-file-configini)
- [Log messages](#log-messages)
- [FAQ / troubleshooting](#faq--troubleshooting)
- [Limitations](#limitations)
- [Releases](#releases)
- [License](#license)

---

## Features

- **Recursive watching** of a source folder, including all sub-folders. Sub-folder structure is preserved under the destination.
- **Local and UNC paths** are both supported on each side, e.g. `C:\WatchSource` or `\\server\share\folder`.
- **Multiple extensions** in one go, e.g. `*.pdf,*.xlsx` or `pdf;xlsx` (comma, semicolon, pipe, or whitespace separated; the leading `*` and `.` are optional). Matching is case-insensitive.
- **Upload-safe copy detection.** A file is copied only after two consecutive scans report the same size and last-write time, so partially uploaded files are never copied prematurely.
- **Automatic retry on failure.** If a copy fails (sharing violation, transient network error, etc.) the file is retried on the next scan instead of being silently skipped forever.
- **Randomized polling interval.** Every scan picks a random wait between 1 s and the **Max interval** you configure.
- **Built-in folder pickers** via the Windows shell dialog. UNC paths can also be typed directly.
- **Clear log panel** with `[HH:MM:SS]` timestamps for every event (detection, retry, copy success with size and destination path, file disappearance, etc.).
- **Persistent settings** in a `config.ini` next to the executable, written as UTF-16 LE so non-ASCII paths are preserved.
- **Safe recursion** - symlinks and junctions are skipped, depth capped at 64, access-denied sub-folders logged individually.
- **Hardened against exceptions** at the thread, scan, copy, and log layers, so a single failure cannot crash the app.

---

## Requirements

- Windows 10 / 11 (x64)
- To **run** the released binary: no extra dependencies (the Visual C++ runtime shipped with modern Windows is sufficient)
- To **build**: Visual Studio 2022 with the *Desktop development with C++* workload

---

## Quick start

1. Download `clonner.exe` from the [Releases page](https://github.com/freyWylfred/clonner/releases) and put it in any folder.
2. Double-click to launch.
3. Fill in:
   - **Watch folder** - the folder to monitor (local or UNC).
   - **Destination folder** - where matching files will be copied (local or UNC).
   - **Extensions** - one or more, e.g. `*.txt`, `*.pdf,*.xlsx`, `pdf;xlsx`, `log doc`.
   - **Max interval (sec)** - upper bound of the polling interval; the actual wait is randomized in `[1 s, max]`.
4. Click **Start**. Settings are written to `config.ini` and the log panel begins showing events.
5. Drop a matching file into the watch folder. Within roughly `2 x Max interval` seconds you should see a `Copied: ...` line.
6. Click **Stop** to end monitoring. The next launch will load the same settings.

---

## Build from source

Open `clonner.slnx` in Visual Studio and build **Release / x64**, or use MSBuild from a Developer PowerShell:

```powershell
& "C:\Program Files\Microsoft Visual Studio\18\Professional\MSBuild\Current\Bin\MSBuild.exe" `
    clonner\clonner.vcxproj /p:Configuration=Release /p:Platform=x64
```

The output is `clonner\x64\Release\clonner.exe`. There are no NuGet or vcpkg dependencies - only `Shlwapi.lib` and `Shell32.lib` (linked via `#pragma comment` in `clonner.cpp`).

---

## How it works

`clonner` uses a **polling watcher** (not `ReadDirectoryChangesW`) so it works the same way on local drives, mapped network drives, and SMB / UNC shares - environments where directory-change notifications are unreliable or unavailable.

1. On **Start**, the watcher thread enumerates the watch folder recursively and treats every matching file as already known. Existing files are not re-copied.
2. The thread sleeps for a random number of milliseconds in `[1 s, Max interval]`, while remaining responsive to the **Stop** event.
3. On each wake-up it re-enumerates the tree and remembers the size and last-write time of each file.
4. A file is treated as **stable** (i.e. fully written) only when two consecutive scans report the same size and last-write time. Only stable files are passed to `CopyFileW`.
5. `CopyFileW` retries up to 5 times with 500 ms back-off on transient errors. A success is recorded in an "already copied" set; a failure is not, so it is retried on the next scan.
6. If a file disappears from the watch folder, the corresponding entry is removed from the "already copied" set - a re-upload with the same name will be copied again.

Net effect:

- **No partial files are ever copied** - large or slow uploads are reported as `Detected (waiting for upload to finish)` first and copied only once they stop changing.
- **No file is silently lost** - failed copies are retried until they succeed or the source vanishes.

---

## Configuration file (config.ini)

When you press **Start** for the first time, `clonner` writes a UTF-16 LE `config.ini` next to `clonner.exe`:

```ini
[Settings]
WatchFolder=C:\WatchSource
DestinationFolder=\\server\share\incoming
Extensions=*.pdf,*.xlsx
MaxIntervalSec=2
```

| Key                 | Meaning                                                                                                              |
| ------------------- | -------------------------------------------------------------------------------------------------------------------- |
| `WatchFolder`       | Folder to monitor. Local path or UNC path.                                                                           |
| `DestinationFolder` | Folder where new files are copied. Local path or UNC path.                                                           |
| `Extensions`        | One or more extensions, separated by comma, semicolon, pipe, or whitespace. The `*.` prefix is optional.             |
| `MaxIntervalSec`    | Upper bound of the polling interval in seconds (1-3600). The actual wait is random in `[1, MaxIntervalSec]`.         |

You can edit the file by hand while `clonner` is not running; the values are loaded on next launch.

The file is created as UTF-16 LE with a BOM so non-ASCII paths (Japanese, accented characters, etc.) are preserved.

---

## Log messages

Every line is prefixed with `[HH:MM:SS]`.

| Message                                                                              | Meaning                                                                            |
| ------------------------------------------------------------------------------------ | ---------------------------------------------------------------------------------- |
| `Watching started (recursive, random interval N-M s): SRC -> DST (*.ext1, *.ext2)`   | Successfully started monitoring with the displayed parameters.                     |
| `Detected (waiting for upload to finish): <file>`                                    | First sighting of a new file - `clonner` will confirm stability on the next scan.  |
| `Copied: <file> (size) -> <full destination path>`                                   | File copied successfully.                                                          |
| `Copy retry K/5 (err=N): <file>`                                                     | Transient error (commonly sharing violation). A retry is in progress.              |
| `Copy failed (will retry next scan): <file>`                                         | Five retries in this scan failed. The next scan will try again.                    |
| `Skipped (source disappeared): <file>`                                               | Source file vanished before the copy could complete.                               |
| `Removed from watch folder: <file>`                                                  | A previously copied file was removed from the watch folder.                        |
| `Cannot enumerate (err=N): <dir>`                                                    | A sub-folder could not be listed (commonly access denied).                         |
| `Recursion depth limit reached, skipping: <dir>`                                     | 64-level recursion cap hit.                                                        |
| `Scan failed (will retry next interval).`                                            | A scan-wide exception was caught.                                                  |
| `Watching stopped.`                                                                  | Monitoring stopped via **Stop** or window close.                                   |

---

## FAQ / troubleshooting

**Nothing happens when I drop a file into the watch folder.**
- Make sure the extension matches one of the entries in *Extensions* (case-insensitive).
- Wait at least `2 x Max interval` seconds - stability is required across two scans.
- Check the log for `Detected (waiting for upload to finish)` to confirm detection.

**The destination is on a UNC share but the Browse dialog will not let me pick it.**
Type or paste the UNC path directly into the *Destination folder* field; the Browse dialog is optional.

**Existing files are not copied when I press Start.**
Intentional. Only files that appear *after* Start are copied. Move them out and back in if you want them copied.

**Will the destination be cleaned up when I delete a file in the watch folder?**
No. `clonner` only copies; it never deletes at the destination.

**A file with the same name already exists at the destination.**
It will be overwritten.

**`Copy failed (will retry next scan)` keeps appearing.**
The destination is probably locked, full, or not writable. Check permissions, free space, and that the file is not held open by another process.

**Does it work with OneDrive / Google Drive / Dropbox folders?**
Yes. From clonner's point of view those are normal folders.

**Where are settings stored?**
In `config.ini` next to `clonner.exe`, created on first **Start**.

---

## Limitations

- Polling design means a copy is detected within roughly `2 x Max interval` seconds, not instantaneously. Set Max interval to `1` for the fastest response.
- Symbolic links and junctions inside the watch tree are intentionally **not** followed.
- Recursion depth is capped at 64 levels.
- File renames are seen as deletion of the old name plus addition of the new one.
- `clonner` only **copies** - it does not propagate deletions, moves, or content changes back to the destination.

---

## Releases

Pre-built binaries are available on the [Releases page](https://github.com/freyWylfred/clonner/releases). The latest release contains the `clonner.exe` you can run directly.

---

## License

This project is provided as-is without warranty. See the repository for details.