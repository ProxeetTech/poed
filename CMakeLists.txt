cmake_minimum_required(VERSION 3.29)
project(poed)

set(CMAKE_CXX_STANDARD 11)

find_library(UCI_LIBRARY NAMES uci PATHS ${CMAKE_SYSROOT}/usr/lib)

add_executable(poed src/main.cpp
        src/utils.cpp
        src/logs.cpp
        src/uci_config.cpp
        src/poe_controller.cpp
        src/poe_simulator.cpp
        src/main_utils.cpp)

target_link_libraries(poed ${UCI_LIBRARY})
include_directories(poed libs/clipp/include libs/json/include inc)

install(TARGETS poed RUNTIME DESTINATION bin)