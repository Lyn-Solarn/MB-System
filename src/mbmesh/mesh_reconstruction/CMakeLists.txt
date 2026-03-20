cmake_minimum_required(VERSION 3.10)
project(BallPivotMesh)

set(CMAKE_CXX_STANDARD 17)

set(Open3D_DIR "/mnt/c/Users/noahm/open3d_install/lib/cmake/Open3D")

find_package(Open3D REQUIRED)

add_executable(mesh_ballpivot main.cpp)
target_link_libraries(mesh_ballpivot PRIVATE Open3D::Open3D)
