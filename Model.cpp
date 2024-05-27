//
// Created by Lucas on 19.12.2023.
//

#include "Model.hpp"

#include <utility>

#define MAX_FLOAT 3.40282e+38
#define MIN_FLOAT 1.17549e-38

glm::vec3 minVertex(MAX_FLOAT, MAX_FLOAT, MAX_FLOAT);
glm::vec3 maxVertex(MIN_FLOAT, MIN_FLOAT, MIN_FLOAT);

Model::Model(const char *path) {
    loadModel(path);
}

void Model::Draw(Shader &shader) {
    for (unsigned int i = 0; i < meshes.size(); i++)
        meshes[i].Draw(shader);
}

void Model::loadModel(const char *path) {
    std::vector< glm::vec3 > vertices;
    std::vector< glm::vec3 > normals;
    std::vector< glm::vec2 > uvs;

    std::vector<Vertex> m_vertices;
    std::vector<unsigned int> m_indices;
    std::vector<Texture> textures;
    int facesNb = 0;
    int verticesNb = 0;

    bool res = objloader::loadOBJ(path, vertices, m_indices, facesNb, verticesNb);

    if (!res)
        return;

    // Calculate the min and max vertex for the object
    for (const glm::vec3& vertex : vertices) {
        minVertex.x = std::min(minVertex.x, vertex.x);
        minVertex.y = std::min(minVertex.y, vertex.y);
        minVertex.z = std::min(minVertex.z, vertex.z);

        maxVertex.x = std::max(maxVertex.x, vertex.x);
        maxVertex.y = std::max(maxVertex.y, vertex.y);
        maxVertex.z = std::max(maxVertex.z, vertex.z);
    }

    // Calculate the center of the object
    glm::vec3 objectCenter;
    objectCenter.x = (minVertex.x + maxVertex.x) / 2.0f;
    objectCenter.y = (minVertex.y + maxVertex.y) / 2.0f;
    objectCenter.z = (minVertex.z + maxVertex.z) / 2.0f;

    glm::vec3 objectSize = maxVertex - minVertex;

    for (unsigned long i = 0; i < vertices.size(); i++) {
        Vertex vertex;
        
        glm::vec3 vector;

        vector.x = vertices[i].x;
        vector.y = vertices[i].y;
        vector.z = vertices[i].z;
        vertex.Position = vector;

        if (!normals.empty()) {
            vector.x = normals[i].x;
            vector.y = normals[i].y;
            vector.z = normals[i].z;
            vertex.Normal = vector;
        } else {
            vertex.Normal = glm::vec3(0.0f, 0.0f, 0.0f);
        }

        if (!uvs.empty()) {
            glm::vec2 vec;
            vec.x = uvs[i].x;
            vec.y = uvs[i].y;
            vertex.TexCoords = vec;
        } else {
            // If the model doesn't have texture coordinates, we generate them
            vertex.TexCoords.x = (vertex.Position.x - minVertex.x) / objectSize.x;
            vertex.TexCoords.y = (vertex.Position.y - minVertex.y) / objectSize.y;
        }

        // Move the object to the center
        vertex.Position -= objectCenter;

        m_vertices.push_back(vertex);
    }

    std::cout << "Loading " << verticesNb << " vertices from " << path << std::endl;
    std::cout << "Loaded " << facesNb << " faces from " << path << std::endl;

    Texture texture;
    texture.id = TextureFromFile();

    if (texture.id == -1)
        return;

    texture.type = "";
    textures.push_back(texture);

    Mesh mesh(m_vertices, m_indices, textures);
    meshes.push_back(mesh);

    loaded = true;
}

unsigned int Model::TextureFromFile()
{
    const char *filename = "Textures/nyan.bmp";
    unsigned int textureID;
    glGenTextures(1, &textureID);

    int width, height, nrChannels;
    stbi_set_flip_vertically_on_load(true);
    unsigned char *data = stbi_load(filename, &width, &height, &nrChannels, 0);

    std::cout << "Loading texture from file: " << filename << std::endl;

    if (data)
    {
        std::cout << "Texture loaded (" << width << "x" << height << ")" << std::endl;
        // /!\ .BMP stores the image in BGR format rather than RGB /!\ //
        GLenum format = GL_RGB;

        glBindTexture(GL_TEXTURE_2D, textureID);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, format, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        stbi_image_free(data);
        return textureID;
    }
    else
    {
        std::cout << "Texture failed to load at path: " << filename << std::endl;
        stbi_image_free(data);
        return -1;
    }
}
