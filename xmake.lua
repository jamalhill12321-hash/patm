-- Copyright (c) Jamal Hill
-- SPDX-License-Identifier: GPL-3.0-or-later

set_project("PATM")
set_version("0.0.0")

add_rules("mode.debug", "mode.release")

if is_mode("debug") then
    set_policy("build.sanitizer.address", true)
    set_policy("build.sanitizer.ubsan", true)
end

add_cflags("-Wall", "-Wextra", "-Werror", "-fno-strict-aliasing", "-D_DEFAULT_SOURCE")
add_cxxflags("-Wall", "-Wextra", "-Werror", "-fno-strict-aliasing", "-D_DEFAULT_SOURCE")
set_languages("c17", "cxx17")

set_configvar("PROJECT_VERSION", "0.0.0")
set_configvar("VERSION_STAGE", "experimental patch 12")
set_configdir("$(builddir)/generated")
add_configfiles("src/core/version.h.in")

-- Qt6 flags
local qt_cflags = {
    "-I/usr/include/qt6/QtWidgets", "-I/usr/include/qt6", "-DQT_WIDGETS_LIB",
    "-I/usr/include/qt6/QtGui", "-DQT_GUI_LIB",
    "-I/usr/include/qt6/QtSql", "-DQT_SQL_LIB",
    "-I/usr/include/qt6/QtCore", "-DQT_CORE_LIB",
    "-I/usr/lib64/qt6/mkspecs/linux-g++"
}

local qt_ldflags = {
    "-lQt6Widgets", "-lQt6Gui", "-lQt6Sql", "-lQt6Core"
}

local native_cflags = {
    "-I/usr/include/mysql/", "-I/usr/include/libsecret-1",
    "-I/usr/include/gio-unix-2.0", "-I/usr/include/glib-2.0",
    "-I/usr/lib64/glib-2.0/include", "-DWITH_GZFILEOP",
    "-I/usr/include/libmount", "-I/usr/include/blkid",
    "-I/usr/include/sysprof-6", "-pthread", "-I/usr/include/p11-kit-1",
    "-I/usr/include/python3.14"
}

local native_ldflags = {
    "-lpq", "-lmariadb", "-lsqlite3",
    "-lsecret-1", "-lgio-2.0", "-lgobject-2.0", "-lglib-2.0",
    "-lpython3.14", "-lpthread"
}

target("patm_core")
    set_kind("static")
    add_files(
        "src/core/error.c",
        "src/core/log.c",
        "src/core/strbuf.c",
        "src/core/config.c",
        "src/core/settings.c",
        "src/db/db.c",
        "src/db/db_factory.c",
        "src/db/db_pg.c",
        "src/db/db_mysql.c",
        "src/db/db_sqlite.c",
        "src/db/db_internal.c",
        "src/secure/secure.c",
        "src/pipeline/pipeline.c",
        "src/net/ssh_tunnel.c",
        "src/net/update_check.c",
        "src/ui/mainwindow.cpp",
        "src/ui/querywindow.cpp",
        "src/ui/sqlterminal.cpp",
        "src/ui/toolrunner.cpp",
        "src/ui/resultgrid.cpp",
        "src/ui/tooleditor.cpp",
        "src/ui/settingsdialog.cpp",
        "src/ui/connectionpropertiesdialog.cpp",
        "src/ui/thememanager.cpp",
        "installer/installerwizard.cpp",
        "$(builddir)/generated/qrc_resources.cpp",
        "$(builddir)/moc/moc_mainwindow.cpp",
        "$(builddir)/moc/moc_querywindow.cpp",
        "$(builddir)/moc/moc_sqlterminal.cpp",
        "$(builddir)/moc/moc_toolrunner.cpp",
        "$(builddir)/moc/moc_resultgrid.cpp",
        "$(builddir)/moc/moc_tooleditor.cpp",
        "$(builddir)/moc/moc_settingsdialog.cpp",
        "$(builddir)/moc/moc_connectionpropertiesdialog.cpp",
        "$(builddir)/moc/moc_installerwizard.cpp"
    )
    add_includedirs("src", "installer", "$(builddir)/generated", {public = true})
    add_cflags(qt_cflags, native_cflags)
    add_cxxflags(qt_cflags, native_cflags)
    add_ldflags(qt_ldflags, native_ldflags)

target("patm")
    set_kind("binary")
    add_files("src/main.cpp")
    add_deps("patm_core")
    add_cflags(qt_cflags, native_cflags)
    add_cxxflags(qt_cflags, native_cflags)
    add_ldflags(qt_ldflags, native_ldflags)

target("test_strbuf")
    set_kind("binary")
    add_files("tests/test_strbuf.c")
    add_deps("patm_core")
    add_cflags(native_cflags)
    add_cxxflags(native_cflags)
    add_ldflags(native_ldflags)
    set_targetdir("$(builddir)/tests")

target("test_db_quoting")
    set_kind("binary")
    add_files("tests/test_db_quoting.c")
    add_deps("patm_core")
    add_cflags(native_cflags)
    add_cxxflags(native_cflags)
    add_ldflags(native_ldflags)
    set_targetdir("$(builddir)/tests")

target("test_config")
    set_kind("binary")
    add_files("tests/test_config.c")
    add_deps("patm_core")
    add_cflags(native_cflags)
    add_cxxflags(native_cflags)
    add_ldflags(native_ldflags)
    set_targetdir("$(builddir)/tests")

target("test_theme_resources")
    set_kind("binary")
    add_files("tests/test_theme_resources.cpp")
    add_deps("patm_core")
    add_cflags(qt_cflags, native_cflags)
    add_cxxflags(qt_cflags, native_cflags)
    add_ldflags(qt_ldflags, native_ldflags)
    set_targetdir("$(builddir)/tests")

target("test_ui_smoke")
    set_kind("binary")
    add_files("tests/test_ui_smoke.cpp")
    add_deps("patm_core")
    add_cflags(qt_cflags, native_cflags)
    add_cxxflags(qt_cflags, native_cflags)
    add_ldflags(qt_ldflags, native_ldflags)
    set_targetdir("$(builddir)/tests")

-- AppImage: runs after `xmake build appimage`
-- Usage: xmake build appimage
-- Steps: 1) builds patm, 2) packages into AppImage
target("appimage")
    set_kind("phony")
    after_build(function (target)
        import("lib.detect.find_program")

        local project_dir = os.projectdir()
        local bindir = path.join(project_dir, "build", "linux", "x86_64", "release")
        local patm_binary = path.join(bindir, "patm")
        local appdir = path.join(project_dir, "AppDir")
        local moc = find_program("/usr/lib64/qt6/libexec/moc")
        local rcc = find_program("/usr/lib64/qt6/libexec/rcc")
        local appimagetool = find_program("/tmp/appimagetool") or find_program("appimagetool")

        -- Run moc
        print("Running MOC...")
        local headers = {
            "src/ui/querywindow.h", "src/ui/sqlterminal.h", "src/ui/toolrunner.h",
            "src/ui/resultgrid.h", "src/ui/tooleditor.h", "src/ui/settingsdialog.h",
            "src/ui/connectionpropertiesdialog.h", "src/ui/mainwindow.h",
            "installer/installerwizard.h"
        }
        local mocdir = path.join(project_dir, "build", "moc")
        os.mkdir(mocdir)
        for _, h in ipairs(headers) do
            local name = path.basename(h)
            os.execv(moc, {path.join(project_dir, h), "-o", path.join(mocdir, "moc_" .. name .. ".cpp")})
        end

        -- Run rcc
        print("Running RCC...")
        os.execv(rcc, {"--name", "resources", path.join(project_dir, "src/assets", "resources.qrc"),
            "-o", path.join(project_dir, "build", "generated", "qrc_resources.cpp")})

        -- Verify binary exists
        if not os.isfile(patm_binary) then
            raise("patm binary not found at " .. patm_binary .. ". Run `xmake build patm` first.")
        end

        -- Create AppDir
        print("Creating AppDir...")
        os.exec("rm -rf " .. appdir)
        os.mkdir(path.join(appdir, "usr", "bin"))
        os.mkdir(path.join(appdir, "usr", "lib"))
        os.mkdir(path.join(appdir, "usr", "share", "applications"))
        os.mkdir(path.join(appdir, "usr", "share", "icons", "hicolor", "256x256", "apps"))

        os.execv("cp", {"-L", patm_binary, path.join(appdir, "usr", "bin", "patm")})
        os.execv("strip", {"-s", path.join(appdir, "usr", "bin", "patm")})

        -- Bundle libraries
        print("Bundling shared libraries...")
        local ldd_output = os.iorunv("ldd", {patm_binary})
        for line in ldd_output:gmatch("[^\r\n]+") do
            local lib = line:match("%s(/[^%s]+)")
            if lib and os.isfile(lib) then
                os.execv("cp", {"-L", lib, path.join(appdir, "usr", "lib")})
            end
        end

        -- Patch RPATH
        print("Patching RPATH...")
        os.execv("patchelf", {"--set-rpath", "$ORIGIN/../lib", path.join(appdir, "usr", "bin", "patm")})
        local libdir = path.join(appdir, "usr", "lib")
        for _, f in ipairs(os.files(path.join(libdir, "lib*.so*"))) do
            if os.isfile(f) and not os.islink(f) then
                os.execv("patchelf", {"--set-rpath", "$ORIGIN", f})
            end
        end

        -- Desktop file
        io.writefile(path.join(appdir, "usr", "share", "applications", "patm.desktop"), [[
[Desktop Entry]
Type=Application
Name=PATM
Comment=Pipeline Automation Tool Manager
Exec=patm
Icon=patm-icon
Categories=Development;Database;Utility;
Terminal=false
StartupNotify=true
]])

        -- Icon and symlinks
        os.execv("cp", {path.join(project_dir, "src/assets", "patm-icon.svg"),
            path.join(appdir, "usr", "share", "icons", "hicolor", "256x256", "apps", "patm-icon.svg")})
        os.execv("ln", {"-sf", "usr/share/applications/patm.desktop", path.join(appdir, "patm.desktop")})
        os.execv("ln", {"-sf", "usr/share/icons/hicolor/256x256/apps/patm-icon.svg", path.join(appdir, "patm-icon.svg")})

        -- AppRun
        io.writefile(path.join(appdir, "AppRun"), [[#!/bin/sh
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
export LD_LIBRARY_PATH="$SCRIPT_DIR/usr/lib:$LD_LIBRARY_PATH"
export QT_PLUGIN_PATH="$SCRIPT_DIR/usr/lib/qt6/plugins"
exec "$SCRIPT_DIR/usr/bin/patm" "$@"
]])
        os.execv("chmod", {"+x", path.join(appdir, "AppRun")})

        -- Build AppImage
        print("Building AppImage...")
        local appimage = path.join(project_dir, "PATM-x86_64.AppImage")
        os.exec("rm -f " .. appimage)
        os.execv("env", {"ARCH=x86_64", appimagetool, appdir, appimage})

        print("Done: " .. appimage)
    end)
