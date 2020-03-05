cmake_minimum_required(VERSION 3.5)

find_package(eosio)

message(STATUS "Downloading daobet.contracts version: ${DAOBET_CONTRACTS_VERSION}")
set(DAOBET_CONTRACTS_URL "https://github.com/DaoCasino/DAOBET.contracts/releases/download/${DAOBET_CONTRACTS_VERSION}/contracts-${DAOBET_CONTRACTS_VERSION}.tar.gz")
set(DAOBET_CONTRACTS_TAR_NAME "daobet.contracts-${DAOBET_CONTRACTS_VERSION}.tar.gz")
set(DAOBET_CONTRACTS_TAR_PATH "${CMAKE_BINARY_DIR}/${DAOBET_CONTRACTS_TAR_NAME}")
set(DAOBET_CONTRACTS_PATH "${CMAKE_BINARY_DIR}/daobet.contracts")

file(DOWNLOAD ${DAOBET_CONTRACTS_URL} ${DAOBET_CONTRACTS_TAR_PATH}
    SHOW_PROGRESS
)
file(MAKE_DIRECTORY ${DAOBET_CONTRACTS_PATH})
execute_process(COMMAND tar -xf ${DAOBET_CONTRACTS_TAR_PATH} --strip=1 -C ${DAOBET_CONTRACTS_PATH}
                WORKING_DIRECTORY ${CMAKE_BINARY_DIR})

enable_testing()

configure_file(${CMAKE_SOURCE_DIR}/contracts.hpp.in ${CMAKE_BINARY_DIR}/contracts.hpp)

add_eosio_test(unit_test
    main.cpp
    platform_test.cpp
    casino_test.cpp
)

target_include_directories(unit_test PUBLIC "${CMAKE_BINARY_DIR}")
