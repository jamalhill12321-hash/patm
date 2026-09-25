<table width="100%">
  <tr>
    <td width="50%" align="center">
      <img src="https://github.com/user-attachments/assets/cb8b66bc-3555-4bdd-9486-5cdf49a7f08d" alt="Database Interface" style="max-width:100%;">
      <br><sub><b>Database Manager View</b></sub>
    </td>
    <td width="50%" align="center">
      <img src="https://github.com/user-attachments/assets/151d827d-6b37-4bf1-8a31-e9986709df4c" alt="Automation Interface" style="max-width:100%;">
      <br><sub><b>Automation Tools View</b></sub>
    </td>
  </tr>
</table>

# PATM

A desktop app for people who are tired of writing throwaway scripts to move data between databases. Connect, query, tools, all in one place.

**GPL-3.0-or-later** · **v0.0.0-experimental patch 12** · currently available for Linux only, might make it available to Windows later.

## Quick start

Download the prebuilt binary or AppImage from [Releases](https://github.com/jamalhill12321-hash/patm/releases):

```sh
# Binary
chmod +x patm
./patm

# AppImage
chmod +x PATM-x86_64.AppImage
./PATM-x86_64.AppImage
```

## Presentation

https://youtu.be/5DlpbQOPMj8

## What it does

- **Connect to PostgreSQL, MySQL, MariaDB, or SQLite** : passwords stay in your system keyring, never in config files. Supports TLS and SSH tunnels.
- **Browse tables** : see what's in your database, double-click to preview.
- **SQL query windows** : write queries, highlight and execute just the selection (or all of it). Save/load `.sql` files. Results show up in a grid.
- **SQL terminal** : Get access to SQL with commands.
- **DB Statistics** : view table row counts and database overview at a glance.
- **Run tools** : built-in CSV export and table transfer. Write your own Python tools; PATM sandboxes them through a restricted API so they can query data but never touch credentials.
- **Built-in installer** : one binary handles installation, setup, and the app itself.


```

## Building

```sh
# Fedora
sudo dnf install gcc make xmake qt6-qtbase-devel qt6-qtsvg-devel \
    libpq-devel mariadb-connector-c-devel python3-devel libsecret-devel sqlite-devel

# Ubuntu/Debian
sudo apt update && sudo apt install -y \
    gcc make xmake \
    qt6-base-dev qt6-svg-dev \
    libpq-dev libmariadb-dev python3-dev libsecret-1-dev libsqlite3-dev

# Arch (Pacman)
sudo pacman -Syu --needed \
    gcc make xmake \
    qt6-base qt6-svg \
    postgresql-libs mariadb-libs python libsecret sqlite
```

### Building from source

```sh
xmake config
xmake build patm
./build/linux/x86_64/release/patm
```

### Building the AppImage

```sh
xmake config
xmake build appimage
# Output: PATM-x86_64.AppImage
```

On first run, PATM shows an installer wizard that sets up SQL engines, desktop shortcuts, and config. After that it opens straight into the app.

### Command line flags

```
./patm              — launch app (shows installer if not set up)
./patm --app        — skip installer, go straight to app
./patm --uninstall  — run the uninstaller
```

## Testing

```sh
xmake build test_strbuf
xmake build test_db_quoting
xmake build test_config
xmake build test_theme_resources
xmake build test_ui_smoke
ctest --test-dir build --output-on-failure
```

The `pg_integration` test skips itself when no PostgreSQL server is running, that's okay

## How tools work

PATM bundles a couple of Python scripts (`export_csv.py`, `transfer_table.py`). They run inside an embedded Python interpreter and can only talk to databases through the `_patm` module.

Edit any tool inside the app and it saves your copy to `~/.config/patm/tools/`. Your copy shadows the shipped one so updates don't clobber your changes.

## Config lives here

```
~/.config/patm/connections.conf   - saved connections (no passwords)
~/.config/patm/ui.conf            - theme and icon settings
~/.config/patm/session.conf       - open tabs and last connection
~/.config/patm/reconnect.dat      - auto-reconnect data
~/.config/patm/tools/             - your edited tool scripts
```

Passwords are in your desktop keyring (GNOME Keyring, KDE Wallet, etc.) via libsecret.

## Themes

Settings → Appearance. Ships with Fusion Light/Dark, Breeze Light/Dark, Classic Light/Dark, and Windows 9x. System theme is also available.

## License

GPL-3.0-or-later. See [LICENSE](LICENSE).
