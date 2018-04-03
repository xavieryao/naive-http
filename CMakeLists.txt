cmake_minimum_required(VERSION 3.5)
project(naive_http C)

set(CMAKE_C_STANDARD 11)

set(CMAKE_C_FLAGS "-Wall -g")

add_executable(naive_http main.c error_handler.c error_handler.h socket_util.c socket_util.h misc.h http.c http.h transaction.c transaction.h)
