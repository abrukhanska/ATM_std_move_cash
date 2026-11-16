
set(SRC_DIR "${PROJECT_SOURCE_DIR}/src")

file(GLOB_RECURSE SOURCE_FILES
    "${SRC_DIR}/*.hpp"
    "${SRC_DIR}/*.cpp"
)

foreach(file IN LISTS SOURCE_FILES)
    file(RELATIVE_PATH relative "${SRC_DIR}" "${file}")
    file(READ "${file}" content)

    if(relative MATCHES "^domain/")
        if(content MATCHES "#[ \t]*include[ \t]*[<\"](application|presentation|infrastructure|sqlite3)")
            message(FATAL_ERROR "ST2 violation in ${relative}")
        endif()
    elseif(relative MATCHES "^application/")
        if(content MATCHES "#[ \t]*include[ \t]*[<\"](presentation|infrastructure|sqlite3)")
            message(FATAL_ERROR "ST2 violation in ${relative}")
        endif()
    elseif(relative MATCHES "^presentation/")
        if(content MATCHES "#[ \t]*include[ \t]*[<\"](infrastructure|sqlite3)")
            message(FATAL_ERROR "ST2 violation in ${relative}")
        endif()
    elseif(relative MATCHES "^infrastructure/")
        if(content MATCHES "#[ \t]*include[ \t]*[<\"](application|presentation)")
            message(FATAL_ERROR "ST2 violation in ${relative}")
        endif()
    endif()

    if(NOT relative MATCHES "^infrastructure/sql/")
        if(content MATCHES "#[ \t]*include[ \t]*[<\"]sqlite3\\.h")
            message(FATAL_ERROR "ST2 SQL violation in ${relative}")
        endif()
    endif()
endforeach()

message(STATUS "ST2 dependency check passed")
