cmake_minimum_required(VERSION 3.17)
project(Assignments C)

set(CMAKE_C_STANDARD 11)

add_executable(Assignments
        udp/._udp_client.c
        udp/._udp_server.c
        udp/udp_client.c
        udp/udp_server.c)
