cmake_minimum_required(VERSION 3.5)
project(yahcl VERSION 1.0.1 DESCRIPTION "Yet another HTTP Client Libarry")

set(CMAKE_CXX_STANDARD 14)

include(GNUInstallDirs)

include_directories("${CMAKE_SOURCE_DIR}/src")

file(GLOB sources src/*.cpp src/*.hpp)
add_library(yahcl SHARED ${sources})

set_target_properties(yahcl PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION 1
    PUBLIC_HEADER include/yahcl.hpp)
configure_file(yahcl.pc.in yahcl.pc @ONLY)
target_include_directories(yahcl PRIVATE ./src)
install(TARGETS yahcl
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(FILES ${CMAKE_BINARY_DIR}/yahcl.pc
    DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/pkgconfig)
