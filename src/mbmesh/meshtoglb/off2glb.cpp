#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION

#include "tiny_gltf.h"
#include <vector>
#include <fstream>
#include <sstream>
#include <string>
#include <iostream>
#include <cstring>

int main(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " input.off output.glb\n";
        return 1;
    }

    std::string inputFile = argv[1];
    std::string outputFile = argv[2];

    std::ifstream file(inputFile);
    if (!file.is_open()) {
        std::cerr << "Failed to open " << inputFile << "\n";
        return 1;
    }

    std::string line;
    std::getline(file, line);
    if (line != "OFF") {
        std::cerr << "Not a valid OFF file\n";
        return 1;
    }

    // Read counts
    std::getline(file, line);
    int numVertices = 0, numFaces = 0, numEdges = 0;
    std::istringstream iss(line);
    iss >> numVertices >> numFaces >> numEdges;

    // Read vertices
    std::vector<float> positions;
    for (int i = 0; i < numVertices; i++) {
        std::getline(file, line);
        if (line.empty()) { i--; continue; }
        std::istringstream viss(line);
        float x, y, z;
        viss >> x >> y >> z;
        positions.push_back(x);
        positions.push_back(y);
        positions.push_back(z);
    }

    // Read faces and convert to triangle indices
    std::vector<unsigned short> indices;
    for (int i = 0; i < numFaces; i++) {
        std::getline(file, line);
        if (line.empty()) { i--; continue; }
        std::istringstream fiss(line);
        int n, a, b, c, d;
        fiss >> n;
        if (n == 3) { // triangle
            fiss >> a >> b >> c;
            indices.push_back(a);
            indices.push_back(b);
            indices.push_back(c);
        } else if (n == 4) { // quad -> split into 2 triangles
            fiss >> a >> b >> c >> d;
            indices.push_back(a); indices.push_back(b); indices.push_back(c);
            indices.push_back(a); indices.push_back(c); indices.push_back(d);
        } else {
            std::cerr << "Face with " << n << " vertices not supported\n";
            return 1;
        }
    }

    file.close();

    tinygltf::Model model;
    tinygltf::Scene scene;
    tinygltf::Node node;
    tinygltf::Mesh mesh;
    tinygltf::Primitive primitive;
    tinygltf::Buffer buffer;
    tinygltf::BufferView bufferView;
    tinygltf::Accessor accessor;

    // -----------------------
    // Vertex buffer
    // -----------------------
    tinygltf::Buffer vertexBuffer;
    vertexBuffer.data.resize(positions.size() * sizeof(float));
    memcpy(vertexBuffer.data.data(), positions.data(), vertexBuffer.data.size());
    int vertexBufferIndex = model.buffers.size();
    model.buffers.push_back(vertexBuffer);

    tinygltf::BufferView vertexBufferView;
    vertexBufferView.buffer = vertexBufferIndex;
    vertexBufferView.byteOffset = 0;
    vertexBufferView.byteLength = vertexBuffer.data.size();
    vertexBufferView.target = TINYGLTF_TARGET_ARRAY_BUFFER;
    int vertexBufferViewIndex = model.bufferViews.size();
    model.bufferViews.push_back(vertexBufferView);

    // Accessor for positions
    float minX = positions[0], maxX = positions[0];
    float minY = positions[1], maxY = positions[1];
    float minZ = positions[2], maxZ = positions[2];

    for (size_t i = 0; i < positions.size(); i += 3) {
        float x = positions[i];
        float y = positions[i+1];
        float z = positions[i+2];
        if (x < minX) minX = x; if (x > maxX) maxX = x;
        if (y < minY) minY = y; if (y > maxY) maxY = y;
        if (z < minZ) minZ = z; if (z > maxZ) maxZ = z;
    }

    tinygltf::Accessor vertexAccessor;
    vertexAccessor.bufferView = vertexBufferViewIndex;
    vertexAccessor.byteOffset = 0;
    vertexAccessor.componentType = TINYGLTF_COMPONENT_TYPE_FLOAT;
    vertexAccessor.count = positions.size()/3;
    vertexAccessor.type = TINYGLTF_TYPE_VEC3;
    vertexAccessor.minValues = {minX, minY, minZ};
    vertexAccessor.maxValues = {maxX, maxY, maxZ};
    int vertexAccessorIndex = model.accessors.size();
    model.accessors.push_back(vertexAccessor);

    // -----------------------
    // Index buffer
    // -----------------------
    tinygltf::Buffer indexBuffer;
    indexBuffer.data.resize(indices.size() * sizeof(unsigned short));
    memcpy(indexBuffer.data.data(), indices.data(), indexBuffer.data.size());
    int indexBufferIndex = model.buffers.size();
    model.buffers.push_back(indexBuffer);

    tinygltf::BufferView indexBufferView;
    indexBufferView.buffer = indexBufferIndex;
    indexBufferView.byteOffset = 0;
    indexBufferView.byteLength = indexBuffer.data.size();
    indexBufferView.target = TINYGLTF_TARGET_ELEMENT_ARRAY_BUFFER;
    int indexBufferViewIndex = model.bufferViews.size();
    model.bufferViews.push_back(indexBufferView);

    tinygltf::Accessor indexAccessor;
    indexAccessor.bufferView = indexBufferViewIndex;
    indexAccessor.byteOffset = 0;
    indexAccessor.componentType = TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT;
    indexAccessor.count = indices.size();
    indexAccessor.type = TINYGLTF_TYPE_SCALAR;
    int indexAccessorIndex = model.accessors.size();
    model.accessors.push_back(indexAccessor);

    // -----------------------
    // Material (bright red)
    // -----------------------
    tinygltf::Material material;
    material.name = "RedMaterial";
    material.pbrMetallicRoughness.baseColorFactor = {1.0,0.0,0.0,1.0};
    material.pbrMetallicRoughness.metallicFactor = 0.0;
    material.pbrMetallicRoughness.roughnessFactor = 1.0;
    int materialIndex = model.materials.size();
    model.materials.push_back(material);

    // -----------------------
    // Primitive
    // -----------------------
    primitive.attributes["POSITION"] = vertexAccessorIndex;
    primitive.indices = indexAccessorIndex;
    primitive.mode = TINYGLTF_MODE_TRIANGLES;
    primitive.material = materialIndex;

    mesh.primitives.push_back(primitive);
    int meshIndex = model.meshes.size();
    model.meshes.push_back(mesh);

    node.mesh = meshIndex;
    int nodeIndex = model.nodes.size();
    model.nodes.push_back(node);
    scene.nodes.push_back(nodeIndex);
    model.scenes.push_back(scene);
    model.defaultScene = 0;

    // -----------------------
    // Write GLB
    // -----------------------
    tinygltf::TinyGLTF gltf;
    if (!gltf.WriteGltfSceneToFile(&model, outputFile, true, true, true, true)) {
        std::cerr << "Failed to write " << outputFile << "\n";
        return 1;
    }

    std::cout << outputFile << " created from " << inputFile << " as solid mesh with bright red material.\n";
    return 0;
}
