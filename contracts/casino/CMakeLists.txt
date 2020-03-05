configure_file(${CMAKE_CURRENT_SOURCE_DIR}/../platform/include/platform/version.hpp.in
               ${CMAKE_CURRENT_BINARY_DIR}/../platform/include/platform/version.hpp
)

add_contract(casino casino ${CMAKE_CURRENT_SOURCE_DIR}/src/casino.cpp)

target_include_directories(casino
   PUBLIC
   ${CMAKE_CURRENT_SOURCE_DIR}/include
   ${CMAKE_CURRENT_BINARY_DIR}/include
   ${CMAKE_CURRENT_SOURCE_DIR}/../platform/include
   ${CMAKE_CURRENT_BINARY_DIR}/../platform/include
)
