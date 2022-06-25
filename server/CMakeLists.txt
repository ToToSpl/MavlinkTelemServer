cmake_minimum_required(VERSION 3.10.2)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

project(takeoff_and_land)

if(NOT MSVC)
    add_definitions("-Wall -Wextra")
else()
    add_definitions("-WX -W2")
endif()

find_package(MAVSDK REQUIRED)

add_executable(server
    src/main.cpp    
)

target_link_libraries(server
   LINK_PRIVATE pthread
   #   LINK_PRIVATE MAVSDK::mavsdk_telemetry 
   #   LINK_PRIVATE MAVSDK::mavsdk_action
   #   LINK_PRIVATE MAVSDL::mavsdk_offboard
   LINK_PRIVATE MAVSDK::mavsdk 
)

