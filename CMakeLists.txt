cmake_minimum_required(VERSION 3.10)
project(smallchat C)

set(CMAKE_C_STANDARD 99)

add_executable(server smallchat-server.c
        chatlib.c
        chatlib.h)
add_executable(cli smallchat-client.c)
