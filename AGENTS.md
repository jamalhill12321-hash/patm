# AGENTS.md

## Project

PATM (Pipeline Automation Tool Manager) — GPL-3.0-or-later desktop app for
database automation (ETL, transfers, exports). Qt 6 UI, embedded CPython
for user-editable tools, libpq + libmariadb drivers.
Linux-first; Windows/macOS later.

## Build & Test (Fedora, verified)

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
./build/src/patm
```

Required packages: `gcc make cmake qt6-qtbase-devel qt6-qtsvg-devel
libpq-devel mariadb-connector-c-devel python3-devel libsecret-devel`.

## Architecture

- DB access goes through the vtable in `src/db/db.h`. Add a new engine
  by adding one driver file + one line in `db_factory.c`. Three engines:
  PostgreSQL, MySQL, MariaDB (MySQL and MariaDB share libmariadb but
  are presented as separate options with separate config).
- Identifiers pass `quote_ident`, literals pass `quote_literal` before
  entering SQL text. Always.
- TLS: per-connection `ssl_mode` = require (default) / prefer / disable.
- Passwords live only in the system keyring (`src/secure/`, libsecret).
  Never in config, logs, or argv.
- Python tools run inside an embedded interpreter and touch databases
  only through the injected `_patm` module. Tools live next to the binary
  and in `~/.config/patm/tools/`.

## UI structure

- **Tab-based notebook**: main window contains a `QTabWidget`. Query windows,
  SQL terminals, and tool runners are all tab pages. Tabs are closeable.
- Result grids use `QTableWidget` inside `QScrollArea`.

## Gotchas

- GResources in a STATIC library get dropped by the linker. Fix:
  `main.c` calls `patm_get_resource()` + `g_resources_register()`.
- `gtk_drop_down_new(model)` takes FULL ownership — never unref the model.
- `GtkDialog` is deprecated in GTK 4.22 — use modal `GtkWindow`.
- Debug builds link ASan/UBSan; install `libasan libubsan`.
- Every source file starts with the GPL header block.
- Tests are plain executables under `tests/` registered with CTest.
  The ui_smoke test needs xvfb-run.

## Feature wiring

- Tools get run parameters via `_patm.config` (JSON parsed in
  `patm_pipeline_run_tool`).
- User tool edits save to `~/.config/patm/tools/` and shadow shipped copies.
- SSH tunnels spawn system `ssh -N -L` — need key/agent auth, no passwords.
- Session persistence: `~/.config/patm/session.conf` (open tabs, last connection).
- Reconnect: `~/.config/patm/reconnect.dat` (encrypted conn ID).
