# 3D mesh reconstruction

- mbgrid2gltf pipeline: swath data -> grid(heightmap) -> mesh -> .glb
- plan: remove the step from .xyz to grid
- Why? Removing this step will allow the mesh to represent caves and overhangs which a height map cannot.

## Install:

- dependency: Open3D
- https://github.com/isl-org/Open3D

1. Clone Open3D repository

**cd ~/documents/CSUMB_Spring2026/capstone_sandbox
git clone --recursive https://github.com/isl-org/Open3D.git
cd Open3D**

2. Create a clean build directory

**mkdir build && cd build**

3. Configure CMake with correct options

**cmake .. \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX=/mnt/c/Users/noahm/open3d_install \
  -DBUILD_BORINGSSL=ON \
  -DUSE_SYSTEM_OPENSSL=OFF**

4. Build Open3D

**make -j$(nproc)**

**-j$(nproc) uses all CPU cores for faster compilation.**

5. Install Open3D

**make install**

# Research Update

Original plan use CGAL math geometry library

### Point 3D reconstruction
- Uses Delaunay triangulation
- problem: caused flat terrane to foil and fold
- Pipeline is made to create enclosed meshes

Example of Problem:
<img width="954" height="726" alt="image" src="https://github.com/user-attachments/assets/4a52cee2-221d-40e3-8be5-1a47d776fa82" />

### TIN 2.5D reconstruction
- Uses Delaunay triangulation
- Problem: is not fully 3D. Does not allow overhangs or caves to be expressed in the mesh.
- Better than a heightmap but is not fully 3D.
- Fixes Point reconstruction folding

Example of Problem:
<img width="729" height="878" alt="image" src="https://github.com/user-attachments/assets/c725f25c-0e71-401a-a36e-9ba9e815da4e" />


## Open3D BPA Algorithm

- BPA
- https://www.open3d.org/docs/release/tutorial/geometry/surface_reconstruction.html

Why BPA?
- Allows for full 3D reconstruction
- Doesnt fold/foil/enclose mesh
- Small issue commonly known with BPA = holes in mesh

Example ouput:
<img width="1125" height="826" alt="image" src="https://github.com/user-attachments/assets/4a89f009-dd79-4e5e-82b0-06f984320f03" />


