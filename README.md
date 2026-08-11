# TagIt — Intelligent Music Library Manager

Codename **Atlas** · Version 1.0

A native, cross-platform desktop application that intelligently organises,
enriches, and manages local music libraries.  Built with **C++20**, **Qt 6**,
TagLib, SQLite and spdlog.

---

## Build & Run

### Step 1 — Install the only system dependency: Qt 6

Qt is a 500 MB framework that cannot be bundled automatically.
Everything else (TagLib, SQLite, spdlog, GoogleTest) is downloaded and built
from source by CMake the first time you configure.

```bash
sudo apt-get install -y qt6-base-dev cmake ninja-build git build-essential
```

That is the only `apt-get` command you will ever need.

---

### Step 2 — Build

```bash
cmake -S . -B build -G Ninja && cmake --build build --parallel
```

The first run downloads and compiles all dependencies (~2–3 minutes on a
typical machine).  Subsequent builds only recompile changed files and finish
in seconds.

---

### Step 3 — Run

```bash
./build/tagit
```

---

## Quick reference

| Task | Command |
|---|---|
| Configure + build | `cmake -S . -B build -G Ninja && cmake --build build --parallel` |
| Rebuild after changes | `cmake --build build --parallel` |
| Run | `./build/tagit` |
| Run tests | `ctest --test-dir build --output-on-failure` |
| Debug build | `cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug && cmake --build build --parallel` |
| AddressSanitizer | add `-DTAGIT_ENABLE_ASAN=ON` to the cmake configure step |
| Skip tests | add `-DTAGIT_BUILD_TESTS=OFF` to the cmake configure step |

---

## Using the application

1. Launch `./build/tagit`.
2. Use **File → Open Music Folder** (or `Ctrl+O`) to select a directory.
   TagIt scans it recursively for audio files in the background and populates
   the song table.
3. Alternatively, **double-click any folder** in the Library Browser on the
   left to scan it directly.
4. Click any row in the song table to inspect its metadata in the **Metadata**
   panel on the right.
5. Use the **search bar** at the top of the song list (or press `Ctrl+F`) to
   filter by title, artist, album, genre or filename in real time.
6. Open **Tools → Settings** (`Ctrl+,`) to configure library, metadata,
   backup and appearance options.

---

## Design principles

- **Local-first** — filename intelligence extracts artist/title before
  anything touches the network.
- **Non-destructive** — existing tags are never overwritten; only missing
  fields are filled.
- **Backup before write** — every tag write creates a `.bak` restore point
  that can be reverted via `BackupManager`.
- **Online lookup is off by default** — can be enabled once a metadata
  provider is wired in (Phase 5).

---

## Dependency versions (auto-fetched)

| Library | Version | Source |
|---|---|---|
| TagLib | 2.0.2 | git tag `v2.0.2` |
| SQLite | 3.46.1 | official amalgamation zip |
| spdlog | 1.14.1 | git tag `v1.14.1` |
| GoogleTest | 1.15.2 | git tag `v1.15.2` |

---

## Project layout

```
tagit/
├── src/
│   ├── main.cpp
│   ├── Application.cpp / .h       — bootstrap & lifecycle
│   ├── core/                      — business logic (no Qt Widgets)
│   │   ├── ApplicationCore        — composition root
│   │   ├── LibraryManager         — recursive scanning (QThread worker)
│   │   ├── MetadataEngine         — 4-step metadata decision engine
│   │   ├── FilenameIntelligence   — rule-based filename parser
│   │   ├── BackupManager          — safe restore-point management
│   │   ├── DuplicateEngine        — metadata + hash duplicate detection
│   │   ├── FileOrganizer          — pattern-based bulk file organisation
│   │   ├── SearchEngine           — ranked full-text search
│   │   ├── ArtworkManager         — artwork cache
│   │   ├── SettingsManager        — QSettings wrapper
│   │   └── Logger                 — spdlog + Qt fallback
│   ├── model/                     — plain data types (Song, AudioMetadata)
│   ├── platform/                  — OS abstractions (Filesystem, DB, TagLib, Network)
│   │   └── linux/                 — Linux XDG paths, xdg-open
│   └── ui/                        — Qt Widgets views
│       ├── MainWindow             — docked layout, toolbar, search bar
│       ├── SongTableModel         — QAbstractTableModel for the song list
│       ├── MetadataInspector      — editable tag form
│       ├── LibraryBrowser         — QFileSystemModel folder tree
│       ├── ActivityLogView        — scrollable log panel
│       └── SettingsDialog         — grouped settings form
├── tests/                         — GoogleTest unit tests (17 tests, 0 failures)
├── cmake/                         — FindTagLib, FindChromaprint, CompilerWarnings
└── CMakeLists.txt                 — single-file build system, all deps via FetchContent
```
