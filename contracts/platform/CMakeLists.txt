configure_file(${CMAKE_CURRENT_SOURCE_DIR}/include/platform/version.hpp.in
               ${CMAKE_CURRENT_BINARY_DIR}/include/platform/version.hpp
)

add_contract(platform platform ${CMAKE_CURRENT_SOURCE_DIR}/src/platform.cpp)

target_include_directories(platform
   PUBLIC
   ${CMAKE_CURRENT_SOURCE_DIR}/include
   ${CMAKE_CURRENT_BINARY_DIR}/include
)
