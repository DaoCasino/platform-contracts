cmake_minimum_required(VERSION 3.5)

project(platform_contracts)

include(ExternalProject)
find_package(eosio.cdt)

set(EOSIO_CDT_VERSION_MIN "1.6.3")
set(EOSIO_CDT_VERSION_SOFT_MAX "1.6.3")
set(DAOBET_CONTRACTS_VERSION "v1.0.7") # daobet system contracts version from git tag

execute_process(COMMAND git describe --tags --always --dirty
    OUTPUT_VARIABLE GIT_TAG_RAW
    ERROR_QUIET
)
string(STRIP "${GIT_TAG_RAW}" VERSION_FULL)


if(CMAKE_BUILD_TYPE STREQUAL "Debug")
   set(TEST_BUILD_TYPE "Debug")
   set(CMAKE_BUILD_TYPE "Release")
else()
   set(TEST_BUILD_TYPE ${CMAKE_BUILD_TYPE})
endif()


message(STATUS "Building pratform contracts ${VERSION_FULL}")

ExternalProject_Add(
    contracts_project
    SOURCE_DIR ${CMAKE_SOURCE_DIR}/contracts
    BINARY_DIR ${CMAKE_BINARY_DIR}/contracts
    CMAKE_ARGS
        -DCMAKE_TOOLCHAIN_FILE=${EOSIO_CDT_ROOT}/lib/cmake/eosio.cdt/EosioWasmToolchain.cmake
    PATCH_COMMAND ""
    TEST_COMMAND ""
    INSTALL_COMMAND ""
    BUILD_ALWAYS 1
)


if (APPLE)
    set(OPENSSL_ROOT "/usr/local/opt/openssl")
elseif (UNIX)
    set(OPENSSL_ROOT "/usr/include/openssl")
endif()

set(SECP256K1_ROOT "/usr/local")

if (APPLE)
    set(OPENSSL_ROOT "/usr/local/opt/openssl")
elseif (UNIX)
    set(OPENSSL_ROOT "/usr/include/openssl")
endif()
set(SECP256K1_ROOT "/usr/local")

string(REPLACE ";" "|" TEST_FRAMEWORK_PATH "${CMAKE_FRAMEWORK_PATH}")
string(REPLACE ";" "|" TEST_MODULE_PATH "${CMAKE_MODULE_PATH}")

ExternalProject_Add(
    contracts_unit_tests
    LIST_SEPARATOR | # Use the alternate list separator
    CMAKE_ARGS
        -DBOOST_ROOT=${BOOST_ROOT}
        -DBoost_NO_SYSTEM_PATHS=${Boost_NO_SYSTEM_PATHS}
        -DCMAKE_BUILD_TYPE=${TEST_BUILD_TYPE}
        -DCMAKE_FRAMEWORK_PATH=${TEST_FRAMEWORK_PATH}
        -DCMAKE_MODULE_PATH=${TEST_MODULE_PATH}
        -DCMAKE_PREFIX_PATH=${TEST_PREFIX_PATH} #XXX: remove???
        -DEOSIO_ROOT=${EOSIO_ROOT}
        -DLLVM_DIR=${LLVM_DIR}
        -Deosio_DIR=${CMAKE_MODULE_PATH}
        -DDAOBET_CONTRACTS_VERSION=${DAOBET_CONTRACTS_VERSION}
    SOURCE_DIR ${CMAKE_SOURCE_DIR}/tests
    BINARY_DIR ${CMAKE_BINARY_DIR}/tests
    BUILD_ALWAYS 1
    TEST_COMMAND   ""
    INSTALL_COMMAND ""
)
add_dependencies(contracts_unit_tests contracts_project)

add_custom_target(create_tar COMMAND
    mkdir -p assets &&
    tar -C ${CMAKE_BINARY_DIR} -cvz --exclude='include' --exclude='*.cmake' --exclude='Makefile' --exclude='CMake*' -f "assets/contracts-${VERSION_FULL}.tar.gz" "contracts"
)

add_dependencies(create_tar contracts_unit_tests)
