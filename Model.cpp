//
// Created by Lucas on 19.12.2023.
//

#include "Model.hpp"

#include <utility>

#define MAX_FLOAT 3.40282e+38
#define MIN_FLOAT 1.17549e-38

c_math::vec3 minVertex(MAX_FLOAT, MAX_FLOAT, MAX_FLOAT);
c_math::vec3 maxVertex(MIN_FLOAT, MIN_FLOAT, MIN_FLOAT);

Model::Model(const char *path) {
    loadModel(path);
}

void Model::Draw(Shader &shader) {
    for (unsigned int i = 0; i < meshes.size(); i++)
        meshes[i].Draw(shader);
}

void Model::loadModel(const char *path) {
    std::vector< c_math::vec3 > vertices;
    std::vector< c_math::vec3 > normals;
    std::vector< c_math::vec2 > uvs;

    std::vector<Vertex> m_vertices;
    std::vector<unsigned int> m_indices;
    std::vector<Texture> textures;
    int facesNb = 0;
    int verticesNb = 0;

    bool res = objloader::loadOBJ(path, vertices, m_indices, facesNb, verticesNb);

    if (!res)
        return;

    // Calculate the min and max vertex for the object
    for (const c_math::vec3& vertex : vertices) {
        minVertex.x = std::min(minVertex.x, vertex.x);
        minVertex.y = std::min(minVertex.y, vertex.y);
        minVertex.z = std::min(minVertex.z, vertex.z);

        maxVertex.x = std::max(maxVertex.x, vertex.x);
        maxVertex.y = std::max(maxVertex.y, vertex.y);
        maxVertex.z = std::max(maxVertex.z, vertex.z);
    }

    // Calculate the center of the object
    c_math::vec3 objectCenter;
    objectCenter.x = (minVertex.x + maxVertex.x) / 2.0f;
    objectCenter.y = (minVertex.y + maxVertex.y) / 2.0f;
    objectCenter.z = (minVertex.z + maxVertex.z) / 2.0f;

    c_math::vec3 objectSize = maxVertex - minVertex;

    for (unsigned long i = 0; i < vertices.size(); i++) {
        Vertex vertex;
        
        c_math::vec3 vector;

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
            vertex.Normal = c_math::vec3(0.0f, 0.0f, 0.0f);
        }

        if (!uvs.empty()) {
            c_math::vec2 vec;
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

std::vector<Texture> Model::loadMaterialTextures(std::string typeName)
{
    std::vector<Texture> textures;

    Texture texture;
    texture.id = TextureFromFile();
    texture.type = std::move(typeName);
    textures.push_back(texture);

    return textures;
}

unsigned int Model::TextureFromFile()
{
    // TODO: pass args instead
    const char *filename = "Textures/nyan.bmp";
    unsigned int textureID;
    glGenTextures(1, &textureID);

    int width, height;

    std::cout << "Loading texture from file: " << filename << std::endl;
    std::vector<uint8_t> data = LoadBMP(filename, &width, &height);

    if (data.data())
    {
        std::cout << "Texture loaded (" << width << "x" << height << ")" << std::endl;
        // /!\ .BMP stores the image in BGR format rather than RGB /!\ //
        GLenum format = GL_BGR;

        glBindTexture(GL_TEXTURE_2D, textureID);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, format, GL_UNSIGNED_BYTE, data.data());
        glGenerateMipmap(GL_TEXTURE_2D);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        return textureID;
    }
    else
    {
        std::cout << "Texture failed to load at path: " << filename << std::endl;
        return -1;
    }
}

std::vector<uint8_t> Model::LoadBMP(const char *imagepath, int *width, int *height) {
    BMPFILEHEADER   bmpHeader; // Header
    BMPINFOHEADER   bmpInfo; // Info

    std::vector<uint8_t> data;

    std::ifstream f{imagepath, std::ios_base::binary };
    if (!f.is_open()) {
        std::cout << "Unable to open the input image file." << std::endl;
        return data;
    }
    else {

        f.read((char*)&bmpHeader, sizeof(bmpHeader));
        if(bmpHeader.file_type != 0x4D42) {
            std::cout << "Error! Unrecognized file format." << std::endl;
            return data;
        }
        f.read((char*)&bmpInfo, sizeof(bmpInfo));

        if(bmpInfo.bit_count != 24) {
            std::cout << "Only 24-bit images are supported. (" << bmpInfo.bit_count << ")" << std::endl;
            return data;
        }

        // Jump to the pixel data location
        f.seekg(bmpHeader.offset_data, f.beg);

        // Adjust the header fields for output.
        // Some editors will put extra info in the image file, we only save the headers and the data.
        bmpInfo.size = sizeof(BMPINFOHEADER);
        bmpHeader.offset_data = sizeof(BMPFILEHEADER) + sizeof(BMPINFOHEADER);

        bmpHeader.file_size = bmpHeader.offset_data;

        if (bmpInfo.height < 0) {
            std::cout << "The program can treat only BMP images with the origin in the bottom left corner!" << std::endl;
            return data;
        }

        data.resize(bmpInfo.width * bmpInfo.height * bmpInfo.bit_count / 8);

        // Here we check if we need to take into account row padding
        if (bmpInfo.width % 4 == 0) {
            f.read((char*)data.data(), data.size());
            bmpHeader.file_size += static_cast<uint32_t>(data.size());
        }
        else {
            row_stride = bmpInfo.width * bmpInfo.bit_count / 8;
            uint32_t new_stride = make_stride_aligned(3);
            std::vector<uint8_t> padding_row(new_stride - row_stride);

            bmpHeader.file_size += static_cast<uint32_t>(data.size()) + bmpInfo.height * static_cast<uint32_t>(padding_row.size());
        }
        *width = bmpInfo.width;
        *height = bmpInfo.height;
    }
    return data;
}

uint32_t Model::make_stride_aligned(int align_stride) const {
    int new_stride = row_stride;
    while (new_stride % align_stride != 0) {
        new_stride++;
    }
    return new_stride;
}
