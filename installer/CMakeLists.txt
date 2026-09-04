cmake_minimum_required(VERSION 3.28)

project(patm_installer
    VERSION 0.0.0
    DESCRIPTION "PATM Installer Wizard"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Widgets)

add_executable(patm_installer
    main.cpp
    installerwizard.cpp
    resources.qrc
)

target_link_libraries(patm_installer PRIVATE
    Qt6::Core
    Qt6::Widgets
)

target_include_directories(patm_installer PRIVATE
    ${CMAKE_BINARY_DIR}/generated
)

install(TARGETS patm_installer RUNTIME DESTINATION bin)
