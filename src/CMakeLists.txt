include(FetchContent)

FetchContent_Declare(
    sqlite3_src
    URL "https://www.sqlite.org/2026/sqlite-amalgamation-3540100.zip"
    URL_HASH SHA256=0a9c8e34352dad46006aa82836d5383bff9dc6bc9ce5a60553c05aa30e6ad1f9
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)

FetchContent_GetProperties(sqlite3_src)
if(NOT sqlite3_src_POPULATED)
    FetchContent_MakeAvailable(sqlite3_src)
    add_library(sqlite3 STATIC "${sqlite3_src_SOURCE_DIR}/sqlite3.c")
    target_include_directories(sqlite3 PUBLIC "${sqlite3_src_SOURCE_DIR}")
endif()

function(atm_layer target dir)
    file(GLOB_RECURSE layer_sources CONFIGURE_DEPENDS "${dir}/*.cpp")

    if(NOT layer_sources)
        set(stub "${CMAKE_CURRENT_BINARY_DIR}/${target}_stub.cpp")
        file(WRITE "${stub}" "// stub: no sources yet\n")
        set(layer_sources "${stub}")
    endif()

    add_library(${target} STATIC ${layer_sources})
    target_include_directories(${target} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
endfunction()

# 1. domain
atm_layer(atm_domain domain)

# 2. application depends on domain
atm_layer(atm_application application)
target_link_libraries(atm_application PUBLIC atm_domain)

# 3. infrastructure depends on domain
atm_layer(atm_infrastructure infrastructure)
target_link_libraries(atm_infrastructure PUBLIC atm_domain PRIVATE sqlite3)

# 4. presentation depends on application and domain
atm_layer(atm_presentation presentation)
target_link_libraries(atm_presentation PUBLIC atm_application atm_domain)

# 5. Main ATM executable
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/main.cpp")
    set(ATM_MAIN_SOURCE main.cpp)
else()
    set(ATM_MAIN_SOURCE "${CMAKE_CURRENT_BINARY_DIR}/main_stub.cpp")
    file(WRITE "${ATM_MAIN_SOURCE}" "int main() { return 0; }\n")
endif()

add_executable(atm_main ${ATM_MAIN_SOURCE})

target_link_libraries(atm_main PRIVATE
    atm_presentation
    atm_infrastructure
    atm_application
    atm_domain
)