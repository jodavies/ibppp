cmake_minimum_required(VERSION 3.16)

project(ibppp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(ZLIB REQUIRED)
find_package(Boost REQUIRED COMPONENTS iostreams program_options)
find_package(PkgConfig REQUIRED)
pkg_check_modules(FLINT REQUIRED IMPORTED_TARGET flint)

add_executable(ibppp
    src/ibppp.cpp
    src/fire_reader.cpp
    src/table_writer.cpp
)

target_include_directories(ibppp PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_options(ibppp PRIVATE
    -Wall
    -Wextra
    -pedantic
    -O3
    -g
)

target_link_libraries(ibppp PRIVATE
    ZLIB::ZLIB
    Boost::iostreams
    Boost::program_options
    PkgConfig::FLINT
    # Ubuntu 24.04's flint.pc apparently omits -lflint ?
    flint
)

set_target_properties(ibppp PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin"
)
