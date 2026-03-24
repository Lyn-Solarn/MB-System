#include <open3d/Open3D.h>
#include <iostream>
#include <vector>

using namespace open3d;

int main(int argc, char** argv)
{
    if (argc != 2)
    {
        std::cout << "Usage: split_mesh mesh.obj\n";
        return 1;
    }

    // Load mesh
    auto mesh = std::make_shared<geometry::TriangleMesh>();
    if (!io::ReadTriangleMesh(argv[1], *mesh))
    {
        std::cerr << "Failed to read mesh\n";
        return 1;
    }

    mesh->ComputeVertexNormals();

    const auto& vertices = mesh->vertices_;
    const auto& triangles = mesh->triangles_;

    // Compute bounding box (XY only)
    double minx = 1e9, maxx = -1e9;
    double miny = 1e9, maxy = -1e9;

    for (const auto& v : vertices)
    {
        minx = std::min(minx, v(0));
        maxx = std::max(maxx, v(0));
        miny = std::min(miny, v(1));
        maxy = std::max(maxy, v(1));
    }

    double midx = (minx + maxx) / 2.0;
    double midy = (miny + maxy) / 2.0;

    // Create 4 tile meshes
    std::vector<std::shared_ptr<geometry::TriangleMesh>> tiles(4);
    for (int i = 0; i < 4; i++)
    {
        tiles[i] = std::make_shared<geometry::TriangleMesh>();
        tiles[i]->vertices_ = vertices; // reuse all vertices (simple version)
    }

    // Assign triangles to tiles
    for (const auto& tri : triangles)
    {
        Eigen::Vector3d v0 = vertices[tri(0)];
        Eigen::Vector3d v1 = vertices[tri(1)];
        Eigen::Vector3d v2 = vertices[tri(2)];

        double cx = (v0(0) + v1(0) + v2(0)) / 3.0;
        double cy = (v0(1) + v1(1) + v2(1)) / 3.0;

        int tile;

        if (cx < midx && cy < midy) tile = 0;
        else if (cx >= midx && cy < midy) tile = 1;
        else if (cx < midx && cy >= midy) tile = 2;
        else tile = 3;

        tiles[tile]->triangles_.push_back(tri);
    }

    // Save tiles
    for (int i = 0; i < 4; i++)
    {
        std::string filename = "tile_" + std::to_string(i) + ".obj";
        io::WriteTriangleMesh(filename, *tiles[i]);
    }

    std::cout << "Tiles written\n";
}