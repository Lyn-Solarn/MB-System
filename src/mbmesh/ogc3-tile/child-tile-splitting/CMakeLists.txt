cmake_minimum_required(VERSION 3.10)
project(split_mesh)

set(Open3D_DIR "/mnt/c/Users/noahm/open3d_install/lib/cmake/Open3D")

find_package(Open3D REQUIRED)

add_executable(split_mesh split_mesh.cpp)

target_link_libraries(split_mesh Open3D::Open3D)
