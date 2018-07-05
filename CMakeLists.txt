cmake_minimum_required(VERSION 2.8)

project(nx_log)

SET(CMAKE_C_FLAGS_DEBUG "$ENV{CFLAGS} -O0 -Wall -g -ggdb")
ADD_DEFINITIONS(-DDEBUG)

ADD_SUBDIRECTORY(test)

set(SRCS
    nx_log.c
    nx_log.h
    nx_array.c
    nx_array.h
    )

add_library(${PROJECT_NAME}
    STATIC
    ${SRCS}
    )
target_link_libraries(${PROJECT_NAME} pthread)


