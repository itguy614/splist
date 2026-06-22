cmake_minimum_required(VERSION 3.10)
project(splist C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

add_compile_options(-Wall -Wextra)

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
