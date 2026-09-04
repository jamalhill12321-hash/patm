# PATM

A desktop app for people who are tired of writing throwaway scripts to move data between databases. Connect, query, export, transfer — all in one place.

**GPL-3.0-or-later** · **v0.0.0-dev** · Linux first (Windows/macOS later)

## Demo

https://youtu.be/5DlpbQOPMj8

## What it does

- **Connect to PostgreSQL, MySQL, or MariaDB** — passwords stay in your system keyring, never in config files. Supports TLS and SSH tunnels.
- **Browse tables** — see what's in your database, double-click to preview.
- **SQL query windows** — write queries, highlight and execute just the selection (or all of it). Save/load `.sql` files. Results show up in a grid.
- **SQL terminal** — psql-style REPL with history (Up/Down arrows).
- **Run tools** — built-in CSV export and table transfer. Write your own Python tools; PATM sandboxes them through a restricted API so they can query data but never touch credentials.

## Building

```sh
# Fedora
sudo dnf install gcc make cmake qt6-qtbase-devel qt6-qtsvg-devel \
    libpq-devel mariadb-connector-c-devel python3-devel libsecret-devel

cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
./build/src/patm
```

## Testing

Run `ctest --test-dir build --output-on-failure`. The `pg_integration` test skips itself when no PostgreSQL server is running — that's normal.

## How tools work

PATM bundles a couple of Python scripts (`export_csv.py`, `transfer_table.py`). They run inside an embedded Python interpreter and can only talk to databases through the `_patm` module — no raw connections, no credentials.

Edit any tool inside the app and it saves your copy to `~/.config/patm/tools/`. Your copy shadows the shipped one so updates don't clobber your changes.

## Config lives here

```
~/.config/patm/connections.conf   — saved connections (no passwords)
~/.config/patm/ui.conf            — theme and icon settings
~/.config/patm/session.conf       — open tabs and last connection
~/.config/patm/reconnect.dat      — auto-reconnect data
~/.config/patm/tools/             — your edited tool scripts
```

Passwords are in your desktop keyring (GNOME Keyring, KDE Wallet, etc.) via libsecret.

## Themes

Settings → Appearance. Ships with Fusion Light/Dark, Breeze Light/Dark, Classic Light/Dark, and Windows 9x. System theme is also available.

## License

GPL-3.0-or-later. See [LICENSE](LICENSE).
