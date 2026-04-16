/*--------------------------------------------------------------------
 *    The MB-system:  mesh_generator.cpp  4/15/2026
 *
 *    Generate triangle mesh from point cloud using greedy approach
 *    with spatial grid acceleration for fast nearest-neighbor search
 *--------------------------------------------------------------------*/

#include "mesh_generator.h"
#include <cmath>
#include <algorithm>
#include <cstdio>
#include <set>
#include <unordered_map>

/**
 * @brief 3D spatial grid cell locator for fast neighborhood queries
 */
struct SpatialGrid {
  float cell_size;
  std::unordered_map<uint64_t, std::vector<uint32_t>> cells;  // cell_hash -> point indices
  
  SpatialGrid(float cs) : cell_size(cs) {}
  
  /**
   * Hash a 3D grid cell to a single 64-bit key
   */
  static uint64_t cell_hash(int x, int y, int z) {
    // Pack x, y, z (each up to 21 bits) into 64 bits
    return ((uint64_t)(x & 0x1FFFFF) << 42) | 
           ((uint64_t)(y & 0x1FFFFF) << 21) | 
           ((uint64_t)(z & 0x1FFFFF));
  }
  
  /**
   * Insert point into grid
   */
  void insert(uint32_t idx, float x, float y, float z) {
    int cx = (int)std::floor(x / cell_size);
    int cy = (int)std::floor(y / cell_size);
    int cz = (int)std::floor(z / cell_size);
    uint64_t h = cell_hash(cx, cy, cz);
    cells[h].push_back(idx);
  }
  
  /**
   * Find all points within distance r from (x, y, z)
   */
  std::vector<uint32_t> find_in_radius(float x, float y, float z, float r) const {
    std::vector<uint32_t> result;
    
    int cx = (int)std::floor(x / cell_size);
    int cy = (int)std::floor(y / cell_size);
    int cz = (int)std::floor(z / cell_size);
    
    int r_cells = (int)std::ceil(r / cell_size) + 1;
    
    for (int dx = -r_cells; dx <= r_cells; dx++) {
      for (int dy = -r_cells; dy <= r_cells; dy++) {
        for (int dz = -r_cells; dz <= r_cells; dz++) {
          uint64_t h = cell_hash(cx + dx, cy + dy, cz + dz);
          auto it = cells.find(h);
          if (it != cells.end()) {
            for (uint32_t idx : it->second) {
              result.push_back(idx);
            }
          }
        }
      }
    }
    
    return result;
  }
};

/**
 * @brief Compute distance between two 3D points
 */
static inline double distance(double x0, double y0, double z0,
                              double x1, double y1, double z1) {
  double dx = x1 - x0;
  double dy = y1 - y0;
  double dz = z1 - z0;
  return std::sqrt(dx*dx + dy*dy + dz*dz);
}

/**
 * @brief Find K nearest neighbors using spatial grid
 */
static std::vector<uint32_t> find_nearest_neighbors_grid(
    const std::vector<double>& xyz,  // x, y, z coords interleaved
    const SpatialGrid& grid,
    uint32_t point_idx,
    int k) {
  
  double x0 = xyz[point_idx * 3 + 0];
  double y0 = xyz[point_idx * 3 + 1];
  double z0 = xyz[point_idx * 3 + 2];
  
  // Start with a reasonable search radius and expand if needed
  float search_radius = grid.cell_size * 5.0f;
  std::vector<std::pair<double, uint32_t>> candidates;
  
  while (candidates.size() < (size_t)k) {
    candidates.clear();
    
    auto nearby = grid.find_in_radius(x0, y0, z0, search_radius);
    
    for (uint32_t i : nearby) {
      if (i == point_idx) continue;  // Skip self
      
      double x1 = xyz[i * 3 + 0];
      double y1 = xyz[i * 3 + 1];
      double z1 = xyz[i * 3 + 2];
      double d = distance(x0, y0, z0, x1, y1, z1);
      
      candidates.push_back({d, i});
    }
    
    if (candidates.empty()) {
      search_radius *= 2.0f;  // Expand search if no candidates found
    } else {
      break;
    }
  }
  
  // Sort and keep first K
  std::sort(candidates.begin(), candidates.end());
  
  std::vector<uint32_t> result;
  result.reserve(k);
  for (int i = 0; i < k && i < (int)candidates.size(); i++) {
    result.push_back(candidates[i].second);
  }
  
  return result;
}

/**
 * @brief Compute normal vector for a triangle given three vertex positions
 */
static void compute_triangle_normal(
    float x0, float y0, float z0,
    float x1, float y1, float z1,
    float x2, float y2, float z2,
    float& nx, float& ny, float& nz) {
  
  // Compute two edge vectors
  float ex1 = x1 - x0, ey1 = y1 - y0, ez1 = z1 - z0;
  float ex2 = x2 - x0, ey2 = y2 - y0, ez2 = z2 - z0;
  
  // Cross product
  nx = ey1 * ez2 - ez1 * ey2;
  ny = ez1 * ex2 - ex1 * ez2;
  nz = ex1 * ey2 - ey1 * ex2;
  
  // Normalize
  float len = std::sqrt(nx*nx + ny*ny + nz*nz);
  if (len > 1e-10f) {
    nx /= len;
    ny /= len;
    nz /= len;
  }
}

/**
 * @brief Generate mesh using greedy nearest-neighbor approach
 */
Mesh generate_greedy_mesh(const std::vector<double>& point_cloud_xyz,
                          int k, int verbose) {
  
  Mesh mesh;
  const size_t num_points = point_cloud_xyz.size() / 3;
  
  if (num_points < 3) {
    fprintf(stderr, "Error: Need at least 3 points for mesh generation\n");
    return mesh;
  }
  
  if (verbose > 0) {
    fprintf(stderr, "\n=== Phase 2: Mesh Generation ===\n");
    fprintf(stderr, "Input points: %zu\n", num_points);
    fprintf(stderr, "K (neighbors per point): %d\n", k);
  }
  
  // Step 0: Build spatial grid for fast neighbor lookup
  if (verbose > 0) {
    fprintf(stderr, "Building spatial grid...\n");
  }
  
  // Estimate grid cell size based on point cloud extent
  float min_x = point_cloud_xyz[0], max_x = point_cloud_xyz[0];
  float min_y = point_cloud_xyz[1], max_y = point_cloud_xyz[1];
  float min_z = point_cloud_xyz[2], max_z = point_cloud_xyz[2];
  
  for (size_t i = 0; i < num_points; i++) {
    float x = point_cloud_xyz[i * 3 + 0];
    float y = point_cloud_xyz[i * 3 + 1];
    float z = point_cloud_xyz[i * 3 + 2];
    min_x = std::min(min_x, x); max_x = std::max(max_x, x);
    min_y = std::min(min_y, y); max_y = std::max(max_y, y);
    min_z = std::min(min_z, z); max_z = std::max(max_z, z);
  }
  
  // Cell size: aim for ~1000-5000 points per cell on average
  float range_x = max_x - min_x;
  float range_y = max_y - min_y;
  float range_z = max_z - min_z;
  float max_range = std::max({range_x, range_y, range_z});
  float cell_size = max_range / std::cbrt(num_points / 2000.0f);  // Adaptive cell size
  
  SpatialGrid grid(cell_size);
  for (uint32_t i = 0; i < num_points; i++) {
    float x = point_cloud_xyz[i * 3 + 0];
    float y = point_cloud_xyz[i * 3 + 1];
    float z = point_cloud_xyz[i * 3 + 2];
    grid.insert(i, x, y, z);
  }
  
  if (verbose > 0) {
    fprintf(stderr, "Grid cell size: %.3f meters\n", cell_size);
  }
  
  // Step 1: Create vertex buffer from point cloud
  mesh.vertices.reserve(num_points);
  for (size_t i = 0; i < num_points; i++) {
    float x = (float)point_cloud_xyz[i * 3 + 0];
    float y = (float)point_cloud_xyz[i * 3 + 1];
    float z = (float)point_cloud_xyz[i * 3 + 2];
    mesh.vertices.push_back(Vertex(x, y, z));
  }
  
  if (verbose > 0) {
    fprintf(stderr, "Vertices created: %zu\n", mesh.vertices.size());
    fprintf(stderr, "Creating triangles from nearest neighbors...\n");
  }
  
  // Step 2: For each point, create triangles with its K nearest neighbors
  std::set<std::tuple<uint32_t, uint32_t, uint32_t>> unique_triangles;
  
  for (uint32_t i = 0; i < num_points; i++) {
    if (verbose > 1 && i % 100000 == 0) {
      fprintf(stderr, "  Processed %u/%zu points...\r", i, num_points);
      fflush(stderr);
    }
    
    std::vector<uint32_t> neighbors = find_nearest_neighbors_grid(point_cloud_xyz, grid, i, k);
    
    // Create triangles from consecutive neighbors (fan triangulation)
    for (size_t j = 0; j + 1 < neighbors.size(); j++) {
      uint32_t v0 = i;
      uint32_t v1 = neighbors[j];
      uint32_t v2 = neighbors[j + 1];
      
      // Ensure consistent ordering (avoid duplicate triangles)
      std::vector<uint32_t> tri_verts = {v0, v1, v2};
      std::sort(tri_verts.begin(), tri_verts.end());
      
      auto tri_key = std::make_tuple(tri_verts[0], tri_verts[1], tri_verts[2]);
      if (unique_triangles.find(tri_key) == unique_triangles.end()) {
        unique_triangles.insert(tri_key);
        mesh.triangles.push_back(Triangle(v0, v1, v2));
      }
    }
  }
  
  if (verbose > 0) {
    fprintf(stderr, "  Processed %zu/%zu points (complete)\n", num_points, num_points);
  }
  
  // Step 3: Compute vertex normals from adjacent triangles
  if (verbose > 0) {
    fprintf(stderr, "Computing vertex normals...\n");
  }
  
  std::vector<std::vector<size_t>> vertex_triangles(num_points);
  for (size_t ti = 0; ti < mesh.triangles.size(); ti++) {
    const Triangle& t = mesh.triangles[ti];
    vertex_triangles[t.v0].push_back(ti);
    vertex_triangles[t.v1].push_back(ti);
    vertex_triangles[t.v2].push_back(ti);
  }
  
  for (uint32_t i = 0; i < num_points; i++) {
    float nx = 0.0f, ny = 0.0f, nz = 0.0f;
    
    for (size_t ti : vertex_triangles[i]) {
      const Triangle& tri = mesh.triangles[ti];
      
      float x0 = mesh.vertices[tri.v0].x;
      float y0 = mesh.vertices[tri.v0].y;
      float z0 = mesh.vertices[tri.v0].z;
      float x1 = mesh.vertices[tri.v1].x;
      float y1 = mesh.vertices[tri.v1].y;
      float z1 = mesh.vertices[tri.v1].z;
      float x2 = mesh.vertices[tri.v2].x;
      float y2 = mesh.vertices[tri.v2].y;
      float z2 = mesh.vertices[tri.v2].z;
      
      float tnx, tny, tnz;
      compute_triangle_normal(x0, y0, z0, x1, y1, z1, x2, y2, z2,
                              tnx, tny, tnz);
      nx += tnx;
      ny += tny;
      nz += tnz;
    }
    
    // Normalize summed normal
    float len = std::sqrt(nx*nx + ny*ny + nz*nz);
    if (len > 1e-10f) {
      mesh.vertices[i].nx = nx / len;
      mesh.vertices[i].ny = ny / len;
      mesh.vertices[i].nz = nz / len;
    } else {
      mesh.vertices[i].nx = 0.0f;
      mesh.vertices[i].ny = 0.0f;
      mesh.vertices[i].nz = 1.0f;
    }
  }
  
  if (verbose > 0) {
    fprintf(stderr, "Mesh generation complete:\n");
    fprintf(stderr, "  Vertices: %zu\n", mesh.vertices.size());
    fprintf(stderr, "  Triangles: %zu\n", mesh.triangles.size());
  }
  
  return mesh;
}
