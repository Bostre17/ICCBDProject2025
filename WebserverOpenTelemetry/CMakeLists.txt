cmake_minimum_required(VERSION 3.10)
project(WebServerCounter)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Directory per le dipendenze
include_directories(${PROJECT_SOURCE_DIR}/include)

# Target principale
add_executable(webserver WebServer.cpp)

# Su Linux potrebbe essere necessario pthread
target_link_libraries(webserver pthread)