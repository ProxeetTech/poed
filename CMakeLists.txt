cmake_minimum_required(VERSION 3.29)
project(poed)

set(CMAKE_CXX_STANDARD 11)

find_library(UCI_LIBRARY NAMES uci PATHS ${CMAKE_SYSROOT}/usr/lib)

add_executable(poed main.cpp
        main.cpp
        utils.cpp
        utils.h
        logs.cpp
        logs.h
        uci_config.cpp
        uci_config.h
        poe_controller.cpp
        poe_controller.h
        poe_simulator.cpp
        poe_simulator.h)

target_link_libraries(poed ${UCI_LIBRARY})
include_directories(poed clipp/include json/include)
