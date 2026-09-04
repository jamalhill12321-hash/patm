add_executable(test_strbuf test_strbuf.c)
target_link_libraries(test_strbuf PRIVATE patm_core)
add_test(NAME strbuf COMMAND test_strbuf)

add_executable(test_db_quoting test_db_quoting.c)
target_link_libraries(test_db_quoting PRIVATE patm_core)
add_test(NAME db_quoting COMMAND test_db_quoting)

add_executable(test_config test_config.c)
target_link_libraries(test_config PRIVATE patm_core)
add_test(NAME config COMMAND test_config)

add_executable(test_theme_resources test_theme_resources.cpp)
target_link_libraries(test_theme_resources PRIVATE patm_core)
add_test(NAME theme_resources COMMAND test_theme_resources)
set_tests_properties(theme_resources PROPERTIES
    ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0")

add_executable(test_pg_integration test_pg_integration.c)
target_link_libraries(test_pg_integration PRIVATE patm_core)
add_test(NAME pg_integration COMMAND test_pg_integration)

find_program(XVFB_RUN xvfb-run)
if(XVFB_RUN)
    add_executable(test_ui_smoke test_ui_smoke.cpp)
    target_link_libraries(test_ui_smoke PRIVATE patm_core)
    add_test(NAME ui_smoke
             COMMAND ${XVFB_RUN} -a $<TARGET_FILE:test_ui_smoke>)
    set_tests_properties(ui_smoke PROPERTIES
        ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0")
else()
    message(STATUS "xvfb-run not found: UI smoke test not registered")
endif()
