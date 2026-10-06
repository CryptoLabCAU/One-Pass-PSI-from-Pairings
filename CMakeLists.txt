cmake_minimum_required(VERSION 3.21)

project(OnePassPSI LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

option(ONEPASS_SANITIZE "Build with AddressSanitizer and UBSan" OFF)
set(ONEPASS_DEPS_PREFIX "${PROJECT_SOURCE_DIR}/out/install" CACHE PATH "Where build.sh installed the dependencies")

include(cmake/Dependencies.cmake)

add_executable(frontend
    src/Crypto.cpp
    src/Network.cpp
    src/OnePassPSI.cpp
    src/main.cpp
    src/test/Test.cpp
    src/test/OnePassPSITests.cpp
)

target_include_directories(frontend PRIVATE src)
target_link_libraries(frontend PRIVATE relic::relic OpenSSL::Crypto)
target_compile_options(frontend PRIVATE -O3 -march=native -mtune=native -ggdb -Wall -Wextra)

if(ONEPASS_SANITIZE)
    target_compile_options(frontend PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(frontend PRIVATE -fsanitize=address,undefined)
endif()
