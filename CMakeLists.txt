cmake_minimum_required(VERSION 3.5)

project(platform_contracts)

include(ExternalProject)
find_package(eosio.cdt)


execute_process(COMMAND git describe --tags
    OUTPUT_VARIABLE GIT_TAG_RAW
    ERROR_QUIET
)

if (GIT_TAG_RAW)
    set(VERSION_FULL "${GIT_TAG_RAW}")
else()
    execute_process(COMMAND git describe --always
        OUTPUT_VARIABLE VERSION_FULL
        ERROR_QUIET
    )
endif()


message(STATUS "Building pratform contracts v${VERSION_FULL}")

ExternalProject_Add(
   contracts_project
   SOURCE_DIR ${CMAKE_SOURCE_DIR}/contracts
   BINARY_DIR ${CMAKE_BINARY_DIR}/contracts
   CMAKE_ARGS -DCMAKE_TOOLCHAIN_FILE=${EOSIO_CDT_ROOT}/lib/cmake/eosio.cdt/EosioWasmToolchain.cmake
   UPDATE_COMMAND ""
   PATCH_COMMAND ""
   TEST_COMMAND ""
   INSTALL_COMMAND ""
   BUILD_ALWAYS 1
)
