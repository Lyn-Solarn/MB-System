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

    if(argc < 2){
        std::cout << "Usage: ./off_to_glb mesh.off\n";
        return 1;
    }

    std::string inputFile = argv[1];

    std::ifstream file(inputFile);
    if(!file.is_open()){
        std::cerr << "Failed to open OFF file\n";
        return 1;
    }

    std::string header;
    file >> header;

    if(header != "OFF"){
        std::cerr << "Not a valid OFF file\n";
        return 1;
    }

    int numVertices, numFaces, numEdges;
    file >> numVertices >> numFaces >> numEdges;

    std::vector<float> vertices;
    vertices.reserve(numVertices * 3);

    for(int i=0;i<numVertices;i++){
        float x,y,z;
        file >> x >> y >> z;

        vertices.push_back(x);
        vertices.push_back(y);
        vertices.push_back(z);
    }

    std::vector<unsigned int> indices;

    for(int i=0;i<numFaces;i++){
        int vertsInFace;
        file >> vertsInFace;

        if(vertsInFace != 3){
            std::cerr << "Non-triangle face detected\n";
            return 1;
        }

        unsigned int a,b,c;
        file >> a >> b >> c;

        indices.push_back(a);
        indices.push_back(b);
        indices.push_back(c);
    }

    file.close();

    tinygltf::Model model;

    // ---------- Vertex Buffer ----------
    tinygltf::Buffer vertexBuffer;
    vertexBuffer.data.resize(vertices.size()*sizeof(float));
    memcpy(vertexBuffer.data.data(),vertices.data(),vertexBuffer.data.size());

    int vertexBufferIndex = model.buffers.size();
    model.buffers.push_back(vertexBuffer);

    tinygltf::BufferView vertexView;
    vertexView.buffer = vertexBufferIndex;
    vertexView.byteOffset = 0;
    vertexView.byteLength = vertexBuffer.data.size();
    vertexView.target = TINYGLTF_TARGET_ARRAY_BUFFER;

    int vertexViewIndex = model.bufferViews.size();
    model.bufferViews.push_back(vertexView);

    tinygltf::Accessor vertexAccessor;
    vertexAccessor.bufferView = vertexViewIndex;
    vertexAccessor.componentType = TINYGLTF_COMPONENT_TYPE_FLOAT;
    vertexAccessor.count = numVertices;
    vertexAccessor.type = TINYGLTF_TYPE_VEC3;

    int vertexAccessorIndex = model.accessors.size();
    model.accessors.push_back(vertexAccessor);

    // ---------- Index Buffer ----------
    tinygltf::Buffer indexBuffer;
    indexBuffer.data.resize(indices.size()*sizeof(unsigned int));
    memcpy(indexBuffer.data.data(),indices.data(),indexBuffer.data.size());

    int indexBufferIndex = model.buffers.size();
    model.buffers.push_back(indexBuffer);

    tinygltf::BufferView indexView;
    indexView.buffer = indexBufferIndex;
    indexView.byteOffset = 0;
    indexView.byteLength = indexBuffer.data.size();
    indexView.target = TINYGLTF_TARGET_ELEMENT_ARRAY_BUFFER;

    int indexViewIndex = model.bufferViews.size();
    model.bufferViews.push_back(indexView);

    tinygltf::Accessor indexAccessor;
    indexAccessor.bufferView = indexViewIndex;
    indexAccessor.componentType = TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT;
    indexAccessor.count = indices.size();
    indexAccessor.type = TINYGLTF_TYPE_SCALAR;

    int indexAccessorIndex = model.accessors.size();
    model.accessors.push_back(indexAccessor);

    // ---------- Material ----------
    tinygltf::Material material;
    material.name = "LightGreenMaterial";
    material.pbrMetallicRoughness.baseColorFactor = {0.6, 1.0, 0.6, 1.0}; // RGBA
    material.doubleSided = true;

    int materialIndex = model.materials.size();
    model.materials.push_back(material);

    // ---------- Primitive ----------
    tinygltf::Primitive primitive;
    primitive.attributes["POSITION"] = vertexAccessorIndex;
    primitive.indices = indexAccessorIndex;
    primitive.mode = TINYGLTF_MODE_TRIANGLES;
    primitive.material = materialIndex;

    tinygltf::Mesh mesh;
    mesh.primitives.push_back(primitive);

    int meshIndex = model.meshes.size();
    model.meshes.push_back(mesh);

    // ---------- Scene ----------
    tinygltf::Node node;
    node.mesh = meshIndex;

    int nodeIndex = model.nodes.size();
    model.nodes.push_back(node);

    tinygltf::Scene scene;
    scene.nodes.push_back(nodeIndex);

    model.scenes.push_back(scene);
    model.defaultScene = 0;

    // ---------- Output ----------
    std::string outputFile = inputFile.substr(0,inputFile.find_last_of(".")) + ".glb";

    tinygltf::TinyGLTF gltf;
    if(!gltf.WriteGltfSceneToFile(&model,outputFile,true,true,true,true)){
        std::cerr << "Failed to write glb\n";
        return 1;
    }

    std::cout << "Created " << outputFile << "\n";
    return 0;
}
