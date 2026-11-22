cmake_minimum_required(VERSION 3.20)
project(ATM_std_move_cash C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(MSVC)
    add_compile_options(/W4 /permissive- /utf-8)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

include(FetchContent)

FetchContent_Declare(
    picosha2
    GIT_REPOSITORY https://github.com/okdshin/PicoSHA2.git
    GIT_TAG 161cb3fc4170fa7a3eca9e582cebd27cc4d1fe29
)
FetchContent_MakeAvailable(picosha2)

enable_testing()

add_subdirectory(src)
add_subdirectory(tests)