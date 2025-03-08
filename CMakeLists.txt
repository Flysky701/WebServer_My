cmake_minimum_required(VERSION 3.10)
project(Webserver_My)

set(CMAKE_CXX_STANDARD 17)

include_directories(
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/http
    ${CMAKE_SOURCE_DIR}/include/threadpool
    ${CMAKE_SOURCE_DIR}/include/log
)

add_executable(main 
    src/main.cpp
    include/http/httprequest.cpp
)