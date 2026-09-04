set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

add_library(patm_core STATIC
    core/error.c
    core/log.c
    core/strbuf.c
    core/config.c
    core/settings.c
    db/db.c
    db/db_factory.c
    db/db_pg.c
    db/db_mysql.c
    db/db_internal.c
    secure/secure.c
    pipeline/pipeline.c
    net/ssh_tunnel.c
    net/update_check.c
    ui/mainwindow.cpp
    ui/querywindow.cpp
    ui/sqlterminal.cpp
    ui/toolrunner.cpp
    ui/resultgrid.cpp
    ui/tooleditor.cpp
    ui/settingsdialog.cpp
    ui/connectionpropertiesdialog.cpp
    ui/thememanager.cpp
    ${CMAKE_SOURCE_DIR}/src/assets/resources.qrc
)

target_include_directories(patm_core PUBLIC
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_BINARY_DIR}/generated
)

target_link_libraries(patm_core PUBLIC
    Qt6::Core
    Qt6::Widgets
    Qt6::Sql
    PkgConfig::LIBPQ
    PkgConfig::MARIADB
    PkgConfig::LIBSECRET
    Python3::Python
    pthread
)

add_executable(patm main.cpp)
target_link_libraries(patm PRIVATE patm_core)
