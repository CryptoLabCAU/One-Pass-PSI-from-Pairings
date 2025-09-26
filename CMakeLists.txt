cmake_minimum_required(VERSION 3.12)
project(PSI-RELIC)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -march=native -mtune=native -ggdb")

find_package(OpenSSL REQUIRED)
include_directories(${OPENSSL_INCLUDE_DIR})

# --- RELIC paths ---
set(RELIC_INSTALL_DIR "${PROJECT_SOURCE_DIR}/out/install/linux")
set(RELIC_LIBRARY     "${RELIC_INSTALL_DIR}/lib/librelic_s.a")
set(RELIC_HEADER      "${RELIC_INSTALL_DIR}/include/relic/relic.h")

if (EXISTS ${RELIC_LIBRARY} AND EXISTS ${RELIC_HEADER})
    message(STATUS "Found RELIC in ${RELIC_INSTALL_DIR}")
else()
    message(STATUS "RELIC not found, building with build_relic.mk ...")
    execute_process(
        COMMAND make -f ${PROJECT_SOURCE_DIR}/thirdparty/build_relic.mk
        WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}/thirdparty
        RESULT_VARIABLE RELIC_BUILD_RESULT
    )
    if (NOT RELIC_BUILD_RESULT EQUAL 0)
        message(FATAL_ERROR "Failed to build RELIC using build_relic.mk")
    endif()
endif()

set(RELIC_INCLUDE_DIR "${RELIC_INSTALL_DIR}/include/relic")

set(GMP_LIBRARY "-lgmp")

# --- Executables ---
add_executable(Semi_Honest_receiver src/Semi_Honest_receiver.cpp src/utils.cpp)
add_executable(Semi_Honest_sender   src/Semi_Honest_sender.cpp   src/utils.cpp)
add_executable(Malicious_receiver   src/Malicious_receiver.cpp   src/utils.cpp)
add_executable(Malicious_sender     src/Malicious_sender.cpp     src/utils.cpp)

# --- Linking ---
foreach(target Semi_Honest_receiver Semi_Honest_sender Malicious_receiver Malicious_sender)
    target_include_directories(${target} PRIVATE ${RELIC_INCLUDE_DIR})
    target_link_libraries(${target} PUBLIC ${OPENSSL_LIBRARIES} ${RELIC_LIBRARY} ${GMP_LIBRARY} pthread)
endforeach()

# --- Output dir ---
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)

include_directories(include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR})
