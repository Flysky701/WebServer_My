cmake_minimum_required(VERSION 3.10)
project(Webserver_My)

set(CMAKE_CXX_STANDARD 17)

include_directories(
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/http
    ${CMAKE_SOURCE_DIR}/include/threadpool
    ${CMAKE_SOURCE_DIR}/include/log
    ${CMAKE_SOURCE_DIR}/include/epollmanager
    ${CMAKE_SOURCE_DIR}/include/connection
    ${CMAKE_SOURCE_DIR}/include/server
    ${CMAKE_SOURCE_DIR}/include/timer
)

add_executable(main 
    src/main.cpp
    src/httprequest.cpp
    
)