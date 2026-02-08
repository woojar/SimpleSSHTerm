# SimpleSSHTerm

Cross-platform Qt terminal emulator with embedded SSH and local profiles.

## Build

Dependencies:
- Qt 5.15 (Widgets)
- libssh (optional but required for SSH features)
- libsodium (optional but required for encrypted profiles)
- libvterm (optional but required for ANSI/VT terminal emulation)
- pkg-config

Build:

```bash
cmake -S . -B build
cmake --build build
```

Run:

```bash
./build/SimpleSSHTerm
```

## Notes

## Features

- Tabbed SSH sessions with embedded `libssh`
- Connect-in-new-tab behavior per profile
- Encrypted profile storage (optional) with passphrase
- SSH config import (`~/.ssh/config`)
- Key-based auth with passphrase prompt (never stored)
- Reconnect last sessions on startup
- Theme editor (foreground, background, font)
- Base16 theme import
- Copy/paste with mouse selection
- ANSI/VT terminal via `libvterm` when available

## Notes

- If `libvterm` is available at build time, the terminal uses ANSI/VT emulation. Otherwise it falls back to a basic text view.
- Profiles can be stored unencrypted by default; enable protection in the Profiles dialog.
