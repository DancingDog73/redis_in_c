cmake_minimum_required(VERSION 3.15)
project(REDIS_CLONE VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(PROJECT_SOURCES
    src/14/avl.cpp
    src/14/hashtable.cpp
    src/14/heap.cpp
    src/14/server.cpp
    src/14/thread_pool.cpp
    src/14/zset.cpp
    
)

add_executable(redis_clone ${PROJECT_SOURCES})


