cmake_minimum_required(VERSION 3.10)
project(Webserver_My)

set(CMAKE_CXX_STANDARD 17)

file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/public DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

add_compile_options(-fsanitize=address)
add_link_options(-fsanitize=address)

include_directories(
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/sql
    ${CMAKE_SOURCE_DIR}/include/http
    ${CMAKE_SOURCE_DIR}/include/threadpool
    ${CMAKE_SOURCE_DIR}/include/log
    ${CMAKE_SOURCE_DIR}/include/epollmanager
    ${CMAKE_SOURCE_DIR}/include/connection
    ${CMAKE_SOURCE_DIR}/include/server
    ${CMAKE_SOURCE_DIR}/include/timer
    ${CMAKE_SOURCE_DIR}/include/filehandle
)

add_executable(server
    src/main.cpp
    # src/test_unit.cpp
)