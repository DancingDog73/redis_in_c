cmake_minimum_required(VERSION 3.15)
project(REDIS_CLONE VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(project_options INTERFACE)

target_compile_options(project_options INTERFACE
    -Wall 
    -Wextra 
    -Wpedantic 
    -fsanitize=address
    -fsanitize=undefined
)

target_link_options(project_options INTERFACE
    -fsanitize=address
    -fsanitize=undefined
)

set(CLIENT_SOURCES
    src/client/client.cpp
)

set(SERVER_CORE_SOURCES
    src/server/avl.cpp
    src/server/hashtable.cpp
    src/server/heap.cpp
    src/server/thread_pool.cpp
    src/server/zset.cpp
    src/server/server_core.cpp
    
)

set(SERVER_SOURCES
    src/server/main.cpp
)

add_library(server_core STATIC ${SERVER_CORE_SOURCES})
add_executable(client ${CLIENT_SOURCES})
add_executable(server ${SERVER_SOURCES})

target_link_libraries(client PRIVATE project_options)
target_include_directories(server_core PUBLIC src/server)
target_link_libraries(server_core PUBLIC project_options)

target_link_libraries(server PRIVATE server_core)


