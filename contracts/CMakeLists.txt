cmake_minimum_required(VERSION 3.5)

project(platfrom_contracts)

set(EOSIO_WASM_OLD_BEHAVIOR "Off")
find_package(eosio.cdt)

add_subdirectory(platform)
add_subdirectory(casino)