# Project tests use the local mini_test framework.
# No external test framework needs to be downloaded.

set(ATM_TEST_MAIN
    "${CMAKE_CURRENT_SOURCE_DIR}/support/mini_test_main.cpp"
)

set(ATM_ENVIRONMENT_TEST_SOURCE
    "${CMAKE_CURRENT_SOURCE_DIR}/domain/environment_test.cpp"
)

# --- domain -----------------------------------------------------------------

file(GLOB_RECURSE DOMAIN_TEST_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/domain/*.cpp"
)

# The environment check has its own executable.
list(REMOVE_ITEM DOMAIN_TEST_SOURCES
    "${ATM_ENVIRONMENT_TEST_SOURCE}"
)

if(DOMAIN_TEST_SOURCES)
    add_executable(atm_domain_tests
        ${DOMAIN_TEST_SOURCES}
        ${ATM_TEST_MAIN}
    )

    target_compile_features(atm_domain_tests PRIVATE cxx_std_20)

    target_include_directories(atm_domain_tests PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}"
    )

    target_link_libraries(atm_domain_tests PRIVATE atm_domain)

    add_test(
        NAME atm_domain_tests
        COMMAND atm_domain_tests
    )
endif()

# --- runtime environment ----------------------------------------------------

add_executable(atm_environment_tests
    "${ATM_ENVIRONMENT_TEST_SOURCE}"
    ${ATM_TEST_MAIN}
)

target_compile_features(atm_environment_tests PRIVATE cxx_std_20)

target_include_directories(atm_environment_tests PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
)

target_link_libraries(atm_environment_tests PRIVATE atm_domain)

add_test(
    NAME atm_environment_tests
    COMMAND atm_environment_tests
)

if(NOT WIN32)
    # Requires installed time-zone data with the Europe/Kyiv zone.
    # Windows uses the configured runtime/system time zone.
    set_tests_properties(atm_environment_tests PROPERTIES
        ENVIRONMENT "TZ=Europe/Kyiv"
    )
endif()

# --- standalone domain header compilation -----------------------------------

file(GLOB DOMAIN_HEADERS
    RELATIVE "${PROJECT_SOURCE_DIR}/src"
    CONFIGURE_DEPENDS
    "${PROJECT_SOURCE_DIR}/src/domain/*.hpp"
)

set(HEADER_CHECK_SOURCES "")

foreach(header IN LISTS DOMAIN_HEADERS)
    string(MAKE_C_IDENTIFIER "${header}" header_id)

    set(check_source
        "${CMAKE_CURRENT_BINARY_DIR}/header_check_${header_id}.cpp"
    )

    file(CONFIGURE
        OUTPUT "${check_source}"
        CONTENT "#include \"${header}\"\n"
        @ONLY
    )

    list(APPEND HEADER_CHECK_SOURCES "${check_source}")
endforeach()

if(HEADER_CHECK_SOURCES)
    add_library(atm_domain_header_check OBJECT
        ${HEADER_CHECK_SOURCES}
    )

    target_compile_features(atm_domain_header_check PRIVATE cxx_std_20)

    target_link_libraries(atm_domain_header_check PRIVATE atm_domain)
endif()

# --- application ------------------------------------------------------------

file(GLOB_RECURSE APPLICATION_TEST_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/application/*.cpp"
)

if(APPLICATION_TEST_SOURCES)
    add_executable(atm_application_tests
        ${APPLICATION_TEST_SOURCES}
        ${ATM_TEST_MAIN}
    )

    target_compile_features(atm_application_tests PRIVATE cxx_std_20)

    target_include_directories(atm_application_tests PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}"
    )

    target_link_libraries(atm_application_tests PRIVATE
        atm_application
        atm_domain
    )

    add_test(
        NAME atm_application_tests
        COMMAND atm_application_tests
    )

    set_tests_properties(atm_application_tests PROPERTIES
        TIMEOUT 60
    )
endif()

# --- infrastructure ---------------------------------------------------------

file(GLOB_RECURSE INFRASTRUCTURE_TEST_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/infrastructure/*.cpp"
)

if(INFRASTRUCTURE_TEST_SOURCES)
    add_executable(atm_infrastructure_tests
        ${INFRASTRUCTURE_TEST_SOURCES}
        ${ATM_TEST_MAIN}
    )

    target_compile_features(atm_infrastructure_tests PRIVATE cxx_std_20)

    target_include_directories(atm_infrastructure_tests PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}"
    )

    target_link_libraries(atm_infrastructure_tests PRIVATE
        atm_infrastructure
        atm_domain
        sqlite3
    )

    add_test(
        NAME atm_infrastructure_tests
        COMMAND atm_infrastructure_tests
    )
endif()

