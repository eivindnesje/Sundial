#pragma once

#include <glm/glm.hpp>
#include <glm/mat4x4.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <stack>
#include <vector>
#include <cstdio>
#include <stdbool.h>
#include <cstdlib> 
#include <ctime> 
#include <chrono>
#include <fstream>

enum SceneNodeType {
    GEOMETRY, POINT_LIGHT, SPOT_LIGHT, SKYBOX
};


struct SceneNode {
    SceneNode() {
        position = glm::vec3(0, 0, 0);
        rotation = glm::vec3(0, 0, 0);
        scale = glm::vec3(1, 1, 1);
        referencePoint = glm::vec3(0, 0, 0);
        vertexArrayObjectID = -1;
        VAOIndexCount = 0;
        nodeType = GEOMETRY;
        textureID = 0;
        hasTexture = false;
    }

	// A list of all children that belong to this node.
	// For instance, in case of the scene graph of a human body shown in the assignment text, the "Upper Torso" node would contain the "Left Arm", "Right Arm", "Head" and "Lower Torso" nodes in its list of children.
	std::vector<SceneNode*> children;
	
	// The node's position and rotation relative to its parent
	glm::vec3 position;
	glm::vec3 rotation;
	glm::vec3 scale;

	// Color of the light
	glm::vec3 lightColor;

	// A transformation matrix representing the transformation of the node's location relative to its parent. This matrix is updated every frame.
	glm::mat4 modelMatrix;
	glm::mat4 MVP;

	// The location of the node's reference point
	glm::vec3 referencePoint;

	// The ID of the VAO containing the "appearance" of this SceneNode.
	int vertexArrayObjectID;
	unsigned int VAOIndexCount;

    unsigned int textureID;
    bool hasTexture;

	// Node type is used to determine how to handle the contents of a node
	SceneNodeType nodeType;
};

SceneNode* createSceneNode();
void addChild(SceneNode* parent, SceneNode* child);
void printNode(SceneNode* node);
int totalChildren(SceneNode* parent);

// For more details, see SceneGraph.cpp.