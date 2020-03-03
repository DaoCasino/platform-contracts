cmake_minimum_required(VERSION 3.5)

project(platform_contracts)

include(ExternalProject)
find_package(eosio.cdt)

set(EOSIO_CDT_VERSION_MIN "1.6.3")
set(EOSIO_CDT_VERSION_SOFT_MAX "1.6.3")

execute_process(COMMAND git describe --tags --always --dirty
    OUTPUT_VARIABLE GIT_TAG_RAW
    ERROR_QUIET
)
string(STRIP ${GIT_TAG_RAW} VERSION_FULL)


message(STATUS "Building pratform contracts ${VERSION_FULL}")

ExternalProject_Add(
    contracts_project
    SOURCE_DIR ${CMAKE_SOURCE_DIR}/contracts
    BINARY_DIR ${CMAKE_BINARY_DIR}/contracts
    CMAKE_ARGS -DCMAKE_TOOLCHAIN_FILE=${EOSIO_CDT_ROOT}/lib/cmake/eosio.cdt/EosioWasmToolchain.cmake -DCONTRACT_VERSION=${VERSION_FULL}
    UPDATE_COMMAND ""
    PATCH_COMMAND ""
    TEST_COMMAND ""
    INSTALL_COMMAND ""
    BUILD_ALWAYS 1
)
