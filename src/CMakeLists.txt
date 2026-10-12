# 1. domain (не залежить ні від чого)
file(GLOB_RECURSE DOMAIN_SOURCES "domain/*.cpp")
add_library(atm_domain STATIC ${DOMAIN_SOURCES})
target_include_directories(atm_domain PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# 2. application (залежить лише від domain)
file(GLOB_RECURSE APP_SOURCES "application/*.cpp")
add_library(atm_application STATIC ${APP_SOURCES})
target_include_directories(atm_application PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(atm_application PUBLIC atm_domain)

# 3. infrastructure (залежить лише від domain)
file(GLOB_RECURSE INFRA_SOURCES "infrastructure/*.cpp" "infrastructure/sql/*.cpp")
add_library(atm_infrastructure STATIC ${INFRA_SOURCES})
target_include_directories(atm_infrastructure PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(atm_infrastructure PUBLIC atm_domain)

# 4. presentation (залежить від application та domain)
file(GLOB_RECURSE PRES_SOURCES "presentation/*.cpp")
add_library(atm_presentation STATIC ${PRES_SOURCES})
target_include_directories(atm_presentation PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(atm_presentation PUBLIC atm_application atm_domain)

# 5. Головний виконуваний файл банкомата
add_executable(atm_main main.cpp)
target_link_libraries(atm_main PRIVATE 
    atm_presentation 
    atm_infrastructure 
    atm_application 
    atm_domain
)
