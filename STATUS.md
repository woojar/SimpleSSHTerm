# Status

## Summary
Cross-platform Qt5 SSH terminal emulator with tabs, embedded libssh, encrypted local profiles, SSH config import, key-based auth, and a global theme UI. Fallback terminal mode uses a single editable QPlainTextEdit for input/output with selection copy and middle-click paste.

## Current blockers / known issues
- libvterm on macOS (installed from source in /usr/local) crashes inside `vterm_set_size` during resize. Workaround: `vterm_set_size` is disabled on macOS; PTY size is updated but the terminal view will not reflow on window resize.
- MacPorts libvterm lacks `pkg-config` file; CMake now uses `find_path/find_library` fallback.

## Key features implemented
- Tabs with close/new, connect per tab
- Connect in New Tab button
- Embedded SSH via libssh (PTY + shell)
- Profile manager with encrypted storage (libsodium)
- SSH config import (~/.ssh/config)
- Key-based auth with passphrase prompt
- Reconnect last sessions on startup (QSettings)
- Theme dialog (fg/bg/font) applied to all tabs
- Fallback terminal UX: selection copy, middle-click paste, scrollback, direct input

## Build
```bash
cmake -S . -B build
cmake --build build
./build/sshterminal
```

## libvterm detection
CMake fallback detection added (MacPorts doesn’t ship libvterm.pc). If libvterm is in /usr/local, CMake should pick it up via fallback.

Check:
```bash
cmake -LA -N build | rg LIBVTERM
```

## Latest workaround for macOS crash
In `src/TerminalWidget.cpp`, `vterm_set_size` is skipped on macOS to avoid crashes from libvterm resize. PTY resize still happens via `terminalResized`.

## Files touched recently
- `CMakeLists.txt`
- `include/TerminalWidget.h`
- `src/TerminalWidget.cpp`
- `include/ProfileManagerDialog.h`
- `src/ProfileManagerDialog.cpp`
- `include/ProfileStore.h`
- `src/ProfileStore.cpp`
- `include/TerminalTab.h`
- `src/TerminalTab.cpp`
- `include/MainWindow.h`
- `src/MainWindow.cpp`
- `include/ThemeDialog.h`
- `src/ThemeDialog.cpp`
- `README.md`

## Next suggested steps
- Resolve libvterm resize crash on macOS (investigate libvterm build flags or recreate vterm on resize).
- Add per-profile theme or SSH config advanced fields (IdentityFile, ProxyJump).
