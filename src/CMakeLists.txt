
add_executable(main main.cpp)

target_link_libraries(main PRIVATE X11)

add_dependencies(main Assets3D)