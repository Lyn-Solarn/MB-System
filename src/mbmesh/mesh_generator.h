/*--------------------------------------------------------------------
 *    The MB-system:  mesh_generator.h  4/15/2026
 *
 *    Copyright (c) 2026 by
 *    David W. Caress (caress@mbari.org)
 *      Monterey Bay Aquarium Research Institute
 *      Moss Landing, California, USA
 *
 *    See README.md file for copying and redistribution conditions.
 *--------------------------------------------------------------------*/
/**
 * @file mesh_generator.h
 * @brief Generate triangle mesh from point cloud using greedy nearest-neighbor approach
 */

#ifndef MESH_GENERATOR_H
#define MESH_GENERATOR_H

#include <vector>
#include <cstdint>

/**
 * @brief Triangle defined by three vertex indices
 */
struct Triangle {
  uint32_t v0, v1, v2;
  
  Triangle(uint32_t a, uint32_t b, uint32_t c) : v0(a), v1(b), v2(c) {}
};

/**
 * @brief Vertex with position and normal
 */
struct Vertex {
  float x, y, z;          // Position
  float nx, ny, nz;       // Normal
  
  Vertex() : x(0), y(0), z(0), nx(0), ny(0), nz(0) {}
  Vertex(float px, float py, float pz)
    : x(px), y(py), z(pz), nx(0), ny(0), nz(0) {}
};

/**
 * @brief Mesh structure with vertices and indices
 */
struct Mesh {
  std::vector<Vertex> vertices;
  std::vector<Triangle> triangles;
  
  size_t vertex_count() const { return vertices.size(); }
  size_t triangle_count() const { return triangles.size(); }
};

/**
 * @brief Generate mesh from point cloud using greedy nearest-neighbor approach
 * 
 * For each point, find K nearest neighbors and create triangles.
 * This is O(n*k) for small K, avoiding expensive Delaunay in favor of speed.
 * 
 * @param points Input point cloud (x, y, z, ...)
 * @param k Number of nearest neighbors to connect per point (default 6)
 * @param verbose Print progress messages
 * @return Mesh with vertices and triangle faces
 */
Mesh generate_greedy_mesh(const std::vector<double>& point_cloud_xyz,
                          int k = 6, int verbose = 0);

#endif // MESH_GENERATOR_H
