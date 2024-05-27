//
// Created by Lucas on 19.12.2023.
//

#include "Mesh.hpp"

Mesh::Mesh(std::vector<Vertex> &vertices, std::vector<unsigned int> &indices, std::vector<Texture> &textures) {
    this->vertices = std::move(vertices);
    this->indices = std::move(indices);
    this->textures = std::move(textures);

    setupMesh();
}

void Mesh::setupMesh() {
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);

    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);

    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), &vertices[0], GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int),
                 &indices[0], GL_STATIC_DRAW);

    // vertex positions
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void *) nullptr);

    // vertex texture coords
    glEnableVertexAttribArray(1);

    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void *) offsetof(Vertex, TexCoords));

    // vertex normals
    /*
     * The offset is now defined using the offsetof macro that, in this case,
     * sets the byte offset of the normal vector equal to the byte offset of the normal attribute
     * in the struct which is 3 floats and thus 12 bytes.
     */
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void *) offsetof(Vertex, Normal));

    glBindVertexArray(0);
}

void Mesh::Draw(Shader &shader) {

    glActiveTexture(GL_TEXTURE0);

    shader.setInt("material", 0);
    glBindTexture(GL_TEXTURE_2D, textures[0].id);

    // draw mesh
    glBindVertexArray(VAO);
    glDrawElements(GL_TRIANGLES, indices.size(), GL_UNSIGNED_INT, nullptr);
    glBindVertexArray(0);

    // always good practice to set everything back to defaults once configured.
    glActiveTexture(GL_TEXTURE0);
}