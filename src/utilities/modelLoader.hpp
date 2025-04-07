#pragma once
#include "mesh.h"
#include <string>

// Loads an OBJ file and converts it into a Mesh.
// If the MTL file is found and contains a diffuse texture, the filename is returned via diffuseTexName.
Mesh loadOBJModel(const std::string &filename, const std::string &baseDir, std::string &diffuseTexName);
