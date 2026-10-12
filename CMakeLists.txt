cmake_minimum_required(VERSION 3.20)
project(ATM_std_move_cash CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Підключаємо вкладені директорії
add_subdirectory(src)
add_subdirectory(tests)
