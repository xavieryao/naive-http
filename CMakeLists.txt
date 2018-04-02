cmake_minimum_required(VERSION 3.5)
project(naive_http C)

set(CMAKE_C_STANDARD 11)

add_executable(naive_http main.c error_handler.c error_handler.h rio.c rio.h socket_util.c socket_util.h misc.h http.c http.h transaction.c transaction.h)
add_executable(copy_txt test/copy_txt.c error_handler.c error_handler.h rio.c rio.h)
