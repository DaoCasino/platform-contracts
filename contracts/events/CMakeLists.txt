configure_file(${CMAKE_CURRENT_SOURCE_DIR}/include/events/version.hpp.in
               ${CMAKE_CURRENT_BINARY_DIR}/include/events/version.hpp
)

add_contract(events events ${CMAKE_CURRENT_SOURCE_DIR}/src/events.cpp)

target_include_directories(events
   PUBLIC
   ${CMAKE_CURRENT_SOURCE_DIR}/include
   ${CMAKE_CURRENT_BINARY_DIR}/include
   ${CMAKE_CURRENT_SOURCE_DIR}/../platform/include
   ${CMAKE_CURRENT_BINARY_DIR}/../platform/include
)
