# Imported targets for the dependencies installed by build.sh. Paths are fixed
# to ONEPASS_DEPS_PREFIX so a system-wide RELIC/OpenSSL can never be picked up.

set(_relic_lib    "${ONEPASS_DEPS_PREFIX}/lib/librelic_s.a")
set(_crypto_lib   "${ONEPASS_DEPS_PREFIX}/lib/libcrypto.a")
set(_include_dir  "${ONEPASS_DEPS_PREFIX}/include")

foreach(_f ${_relic_lib} ${_crypto_lib} ${_include_dir}/relic/relic.h ${_include_dir}/openssl/evp.h)
    if(NOT EXISTS ${_f})
        message(FATAL_ERROR "Missing ${_f}\nRun ./build.sh deps first.")
    endif()
endforeach()

find_package(Threads REQUIRED)

find_library(GMP_LIBRARY NAMES gmp REQUIRED)

add_library(relic::relic STATIC IMPORTED GLOBAL)
set_target_properties(relic::relic PROPERTIES
    IMPORTED_LOCATION ${_relic_lib}
    INTERFACE_INCLUDE_DIRECTORIES ${_include_dir}
    INTERFACE_LINK_LIBRARIES "${GMP_LIBRARY};Threads::Threads")

add_library(OpenSSL::Crypto STATIC IMPORTED GLOBAL)
set_target_properties(OpenSSL::Crypto PROPERTIES
    IMPORTED_LOCATION ${_crypto_lib}
    INTERFACE_INCLUDE_DIRECTORIES ${_include_dir}
    INTERFACE_LINK_LIBRARIES "Threads::Threads;${CMAKE_DL_LIBS}")
