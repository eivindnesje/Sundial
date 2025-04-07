#include "modelLoader.hpp"
#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"
#include <iostream>
#include <glm/glm.hpp>
#include <glm/gtc/epsilon.hpp>

void computeNormalsForMesh(Mesh &mesh) {
    // Initialize normals to zero.
    mesh.normals.resize(mesh.vertices.size(), glm::vec3(0.0f));

    // Iterate over each triangle.
    for (size_t i = 0; i < mesh.indices.size(); i += 3) {
        unsigned int i0 = mesh.indices[i];
        unsigned int i1 = mesh.indices[i + 1];
        unsigned int i2 = mesh.indices[i + 2];

        const glm::vec3 &v0 = mesh.vertices[i0];
        const glm::vec3 &v1 = mesh.vertices[i1];
        const glm::vec3 &v2 = mesh.vertices[i2];

        // Compute the face normal.
        glm::vec3 edge1 = v1 - v0;
        glm::vec3 edge2 = v2 - v0;
        glm::vec3 faceNormal = glm::normalize(glm::cross(edge1, edge2));

        // Add the face normal to each vertex normal.
        mesh.normals[i0] += faceNormal;
        mesh.normals[i1] += faceNormal;
        mesh.normals[i2] += faceNormal;
    }
    // Normalize all vertex normals.
    for (auto &n : mesh.normals) {
        n = glm::normalize(n);
    }
}

Mesh loadOBJModel(const std::string &filename, const std::string &baseDir, std::string &diffuseTexName) {
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn, err;

    // Pass the base directory so that the MTL file is found.
    bool ret = tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, filename.c_str(), baseDir.c_str(), true);
    if (!warn.empty()) {
        std::cout << "Warning: " << warn << std::endl;
    }
    if (!err.empty()) {
        std::cerr << "Error: " << err << std::endl;
    }
    if (!ret) {
        std::cerr << "Failed to load/parse OBJ file: " << filename << std::endl;
        return Mesh();
    }

    Mesh mesh;
    // Iterate over shapes and build the mesh.
    for (size_t s = 0; s < shapes.size(); s++) {
        for (size_t i = 0; i < shapes[s].mesh.indices.size(); i++) {
            tinyobj::index_t index = shapes[s].mesh.indices[i];
            // Vertex
            glm::vec3 vertex;
            vertex.x = attrib.vertices[3 * index.vertex_index + 0];
            vertex.y = attrib.vertices[3 * index.vertex_index + 1];
            vertex.z = attrib.vertices[3 * index.vertex_index + 2];
            mesh.vertices.push_back(vertex);
            // Normal (if available)
            if (!attrib.normals.empty() && index.normal_index >= 0) {
                glm::vec3 normal;
                normal.x = attrib.normals[3 * index.normal_index + 0];
                normal.y = attrib.normals[3 * index.normal_index + 1];
                normal.z = attrib.normals[3 * index.normal_index + 2];
                mesh.normals.push_back(normal);
            }
            // Texture Coordinates (if available)
            if (!attrib.texcoords.empty() && index.texcoord_index >= 0) {
                glm::vec2 texcoord;
                texcoord.x = attrib.texcoords[2 * index.texcoord_index + 0];
                // Flip the V coordinate to match OpenGL's expected origin.
                texcoord.y = 1.0f - attrib.texcoords[2 * index.texcoord_index + 1];
                mesh.textureCoordinates.push_back(texcoord);
            }
            // Since we're reordering vertices, simply add the new index.
            mesh.indices.push_back(static_cast<unsigned int>(mesh.indices.size()));
        }
    }
    
    if (!materials.empty()) {
    const tinyobj::material_t &mat = materials[0];
    if (!mat.diffuse_texname.empty()) {
        diffuseTexName = mat.diffuse_texname;
        std::cout << "Found diffuse texture in MTL: " << diffuseTexName << std::endl;
    } else {
        std::cout << "No diffuse texture specified in the MTL file." << std::endl;
    }
    if (mesh.normals.empty()) {
        std::cout << "No normals found, computing normals..." << std::endl;
        computeNormalsForMesh(mesh);
    }
}

    
    return mesh;
}
