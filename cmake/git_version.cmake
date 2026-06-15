# git_version.cmake — runs at BUILD time to capture branch + commit
# Generates ${OUT}/git_version.h with #define GIT_BRANCH / GIT_COMMIT

execute_process(
    COMMAND git rev-parse --abbrev-ref HEAD
    WORKING_DIRECTORY "${SRC}"
    OUTPUT_VARIABLE GIT_BRANCH
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)
execute_process(
    COMMAND git rev-parse --short HEAD
    WORKING_DIRECTORY "${SRC}"
    OUTPUT_VARIABLE GIT_COMMIT
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)

if(NOT GIT_BRANCH)
    set(GIT_BRANCH "unknown")
endif()
if(NOT GIT_COMMIT)
    set(GIT_COMMIT "unknown")
endif()

file(WRITE "${OUT}/git_version.h.tmp"
    "#pragma once\n"
    "#define GIT_BRANCH \"${GIT_BRANCH}\"\n"
    "#define GIT_COMMIT \"${GIT_COMMIT}\"\n"
)
execute_process(COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${OUT}/git_version.h.tmp" "${OUT}/git_version.h")
file(REMOVE "${OUT}/git_version.h.tmp")
