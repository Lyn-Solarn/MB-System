/*--------------------------------------------------------------------
 *    The MB-system:  glb_writer.cpp  4/15/2026
 *
 *    Write 3D mesh to GLB (binary glTF) format
 *--------------------------------------------------------------------*/

#include "glb_writer.h"

// tinygltf configuration
#define TINYGLTF_IMPLEMENTATION
#define TINYGLTF_NO_STB_IMAGE
#define TINYGLTF_NO_STB_IMAGE_WRITE
#include "../mbgrd2gltf/tinygltf/tiny_gltf.h"

#include <cstring>
#include <cstdio>
#include <algorithm>
#include <vector>

/**
 * @brief Append bytes to binary buffer with proper alignment
 */
static void append_bytes(std::vector<unsigned char>& bin, const void* data, size_t size) {
  if (size == 0) return;
  size_t old = bin.size();
  bin.resize(old + size);
  std::memcpy(bin.data() + old, data, size);
}

/**
 * @brief Write mesh to GLB file using tinygltf
 */
int write_glb_file(const Mesh& mesh, const std::string& filename, int verbose) {
  
  if (verbose > 0) {
    fprintf(stderr, "\n=== Phase 3: GLB Export ===\n");
    fprintf(stderr, "Writing mesh to: %s\n", filename.c_str());
  }
  
  // Validate input
  if (mesh.vertices.empty() || mesh.triangles.empty()) {
    fprintf(stderr, "Error: Empty mesh (no vertices or triangles)\n");
    return -1;
  }
  
  // Create glTF model
  tinygltf::Model model;
  tinygltf::Scene scene;
  tinygltf::Node node;
  tinygltf::Mesh gltf_mesh;
  tinygltf::Primitive primitive;
  
  // ---- Build vertex position buffer ----
  std::vector<float> positions;
  positions.reserve(mesh.vertices.size() * 3);
  
  float pos_min[3] = {mesh.vertices[0].x, mesh.vertices[0].y, mesh.vertices[0].z};
  float pos_max[3] = {mesh.vertices[0].x, mesh.vertices[0].y, mesh.vertices[0].z};
  
  for (const auto& v : mesh.vertices) {
    positions.push_back(v.x);
    positions.push_back(v.y);
    positions.push_back(v.z);
    
    pos_min[0] = std::min(pos_min[0], v.x);
    pos_min[1] = std::min(pos_min[1], v.y);
    pos_min[2] = std::min(pos_min[2], v.z);
    pos_max[0] = std::max(pos_max[0], v.x);
    pos_max[1] = std::max(pos_max[1], v.y);
    pos_max[2] = std::max(pos_max[2], v.z);
  }
  
  // ---- Build normal buffer ----
  std::vector<float> normals;
  normals.reserve(mesh.vertices.size() * 3);
  for (const auto& v : mesh.vertices) {
    normals.push_back(v.nx);
    normals.push_back(v.ny);
    normals.push_back(v.nz);
  }
  
  // ---- Build index buffer ----
  std::vector<uint32_t> indices;
  indices.reserve(mesh.triangles.size() * 3);
  for (const auto& t : mesh.triangles) {
    indices.push_back(t.v0);
    indices.push_back(t.v1);
    indices.push_back(t.v2);
  }
  
  // ---- Pack into binary buffers ----
  std::vector<unsigned char> buffer_data;
  
  // Position data
  size_t pos_offset = buffer_data.size();
  append_bytes(buffer_data, positions.data(), positions.size() * sizeof(float));
  
  // Normal data
  size_t norm_offset = buffer_data.size();
  append_bytes(buffer_data, normals.data(), normals.size() * sizeof(float));
  
  // Index data  
  size_t idx_offset = buffer_data.size();
  append_bytes(buffer_data, indices.data(), indices.size() * sizeof(uint32_t));
  
  // ---- Create glTF buffer and buffer views ----
  tinygltf::Buffer buffer;
  buffer.data = buffer_data;
  model.buffers.push_back(buffer);
  
  // Position buffer view
  tinygltf::BufferView pos_bv;
  pos_bv.buffer = 0;
  pos_bv.byteOffset = pos_offset;
  pos_bv.byteLength = positions.size() * sizeof(float);
  pos_bv.byteStride = sizeof(float) * 3;
  pos_bv.target = TINYGLTF_TARGET_ARRAY_BUFFER;
  int pos_bv_idx = (int)model.bufferViews.size();
  model.bufferViews.push_back(pos_bv);
  
  // Normal buffer view
  tinygltf::BufferView norm_bv;
  norm_bv.buffer = 0;
  norm_bv.byteOffset = norm_offset;
  norm_bv.byteLength = normals.size() * sizeof(float);
  norm_bv.byteStride = sizeof(float) * 3;
  norm_bv.target = TINYGLTF_TARGET_ARRAY_BUFFER;
  int norm_bv_idx = (int)model.bufferViews.size();
  model.bufferViews.push_back(norm_bv);
  
  // Index buffer view
  tinygltf::BufferView idx_bv;
  idx_bv.buffer = 0;
  idx_bv.byteOffset = idx_offset;
  idx_bv.byteLength = indices.size() * sizeof(uint32_t);
  idx_bv.target = TINYGLTF_TARGET_ELEMENT_ARRAY_BUFFER;
  int idx_bv_idx = (int)model.bufferViews.size();
  model.bufferViews.push_back(idx_bv);
  
  // ---- Create accessors ----
  
  // Position accessor
  tinygltf::Accessor pos_acc;
  pos_acc.bufferView = pos_bv_idx;
  pos_acc.byteOffset = 0;
  pos_acc.componentType = TINYGLTF_COMPONENT_TYPE_FLOAT;
  pos_acc.count = mesh.vertices.size();
  pos_acc.type = TINYGLTF_TYPE_VEC3;
  pos_acc.minValues = {pos_min[0], pos_min[1], pos_min[2]};
  pos_acc.maxValues = {pos_max[0], pos_max[1], pos_max[2]};
  int pos_acc_idx = (int)model.accessors.size();
  model.accessors.push_back(pos_acc);
  
  // Normal accessor
  tinygltf::Accessor norm_acc;
  norm_acc.bufferView = norm_bv_idx;
  norm_acc.byteOffset = 0;
  norm_acc.componentType = TINYGLTF_COMPONENT_TYPE_FLOAT;
  norm_acc.count = mesh.vertices.size();
  norm_acc.type = TINYGLTF_TYPE_VEC3;
  int norm_acc_idx = (int)model.accessors.size();
  model.accessors.push_back(norm_acc);
  
  // Index accessor
  tinygltf::Accessor idx_acc;
  idx_acc.bufferView = idx_bv_idx;
  idx_acc.byteOffset = 0;
  idx_acc.componentType = TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT;
  idx_acc.count = indices.size();
  idx_acc.type = TINYGLTF_TYPE_SCALAR;
  int idx_acc_idx = (int)model.accessors.size();
  model.accessors.push_back(idx_acc);
  
  // ---- Create primitive ----
  primitive.attributes["POSITION"] = pos_acc_idx;
  primitive.attributes["NORMAL"] = norm_acc_idx;
  primitive.indices = idx_acc_idx;
  primitive.mode = TINYGLTF_MODE_TRIANGLES;
  gltf_mesh.primitives.push_back(primitive);
  
  // ---- Add mesh to model ----
  gltf_mesh.name = "PointCloudMesh";
  model.meshes.push_back(gltf_mesh);
  
  // ---- Create node ----
  node.mesh = 0;
  node.name = "PointCloudNode";
  model.nodes.push_back(node);
  
  // ---- Create scene ----
  scene.nodes.push_back(0);
  model.scenes.push_back(scene);
  model.defaultScene = 0;
  
  // ---- Write GLB file ----
  tinygltf::TinyGLTF writer;
  std::string err, warn;
  
  bool success = writer.WriteGltfSceneToFile(&model, filename, true, true);
  
  if (!success) {
    fprintf(stderr, "Error writing GLB file: %s\n", err.c_str());
    return -1;
  }
  
  if (verbose > 0) {
    fprintf(stderr, "GLB file written successfully: %s\n", filename.c_str());
    fprintf(stderr, "Vertices: %zu\n", mesh.vertices.size());
    fprintf(stderr, "Triangles: %zu\n", mesh.triangles.size());
  }
  
  return 0;
}
