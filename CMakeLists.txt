cmake_minimum_required(VERSION 3.20)
project(FlightSimulator CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Discover system packages
find_package(glfw3 3.3 REQUIRED)
find_package(glm REQUIRED)
find_package(OpenGL REQUIRED)

include_directories(include)

# Include source files + GLAD's C implementation
file(GLOB SOURCES "src/*.cpp" "src/*.c")

add_executable(FlightSimulator ${SOURCES})

target_link_libraries(FlightSimulator
    PRIVATE
    glfw
    glm::glm
    OpenGL::GL
    ${CMAKE_DL_LIBS}
)