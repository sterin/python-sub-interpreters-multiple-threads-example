cmake_minimum_required(VERSION 3.12)

project(py1)

find_package(Python COMPONENTS Development)
find_package(Threads REQUIRED)

add_executable(py1 main.cpp)
target_compile_features(py1 PUBLIC cxx_std_11)
target_link_libraries(py1 PUBLIC Python::Python Threads::Threads)

add_custom_target(
    run
    DEPENDS py1
    COMMAND py1
)
