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

# 1. domain (не залежить ні від чого)
atm_layer(atm_domain domain)

# 2. application (залежить лише від domain)
atm_layer(atm_application application)
target_link_libraries(atm_application PUBLIC atm_domain)

# 3. infrastructure (залежить лише від domain)
atm_layer(atm_infrastructure infrastructure)
target_link_libraries(atm_infrastructure PUBLIC atm_domain)

# 4. presentation (залежить від application та domain)
atm_layer(atm_presentation presentation)
target_link_libraries(atm_presentation PUBLIC atm_application atm_domain)

# 5. Головний виконуваний файл банкомата
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