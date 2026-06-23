cmake_minimum_required(VERSION 3.10)

# Single source of truth for the version: the VERSION file at the repo root.
file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" SPLIST_VERSION)
string(STRIP "${SPLIST_VERSION}" SPLIST_VERSION)

project(splist VERSION ${SPLIST_VERSION} LANGUAGES C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# MSVC does not understand the GCC/Clang warning flags.
if(MSVC)
    add_compile_options(/W3)
else()
    add_compile_options(-Wall -Wextra)
endif()

# All backends are guarded by per-platform #ifdefs, so it is safe to compile
# every source on every platform; only the matching one emits code.
add_executable(splist
    src/main.c
    src/splist.c
    src/platform_linux.c
    src/platform_macos.c
    src/platform_windows.c
    src/platform_stub.c
)

target_include_directories(splist PRIVATE include)

# Expose the version string to the program (used by `splist --ver`).
target_compile_definitions(splist PRIVATE SPLIST_VERSION="${SPLIST_VERSION}")

# macOS enumeration is built on IOKit / CoreFoundation.
if(APPLE)
    target_link_libraries(splist PRIVATE
        "-framework IOKit"
        "-framework CoreFoundation"
    )
endif()

# Windows enumeration uses SetupAPI.
if(WIN32)
    target_link_libraries(splist PRIVATE setupapi)
endif()

install(TARGETS splist RUNTIME DESTINATION bin)
