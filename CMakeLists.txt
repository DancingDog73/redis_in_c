cmake_minimum_required(VERSION 3.15)
project(REDIS_CLONE VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CLIENT_SOURCES
    src/14/client/client.cpp
)

set(SERVER_SOURCES
    src/14/server/avl.cpp
    src/14/server/hashtable.cpp
    src/14/server/heap.cpp
    src/14/server/server.cpp
    src/14/server/thread_pool.cpp
    src/14/server/zset.cpp
    
)

add_executable(client ${CLIENT_SOURCES})
add_executable(server ${SERVER_SOURCES})


