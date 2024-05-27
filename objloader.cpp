#define _CRT_SECURE_NO_WARNINGS

#include <sstream>
#include "objloader.h"

// Function to process face vertices and add triangle indices to out_indices
void processFaceVertices(const std::vector<unsigned int>& faceVertexIndices, std::vector<unsigned int>& out_indices, int &out_facesNb) {
    if (faceVertexIndices.size() >= 3) {
        // Split the face into triangles and add triangle indices to out_indices
        for (size_t i = 1; i < faceVertexIndices.size() - 1; ++i) {
            out_facesNb++;
            out_indices.push_back(faceVertexIndices[0] - 1);
            out_indices.push_back(faceVertexIndices[i] - 1);
            out_indices.push_back(faceVertexIndices[i + 1] - 1);
        }
    }
}

// Function to handle face vertices in the format (f v1 v2 v3 ...)
void handleOnlyVertices(const char* line, std::vector<unsigned int>& out_indices, int &out_facesNb) {
    std::vector<unsigned int> faceVertexIndices;

    char* token = strtok(const_cast<char*>(line), " "); // Split the line by space

    while (token != nullptr) {
        if (token[0] == 'f') {
            token = strtok(nullptr, " "); // Move to next token
            continue;
        }
        unsigned int vertexIndex;
        if (sscanf(token, "%u", &vertexIndex) == 1) {
            faceVertexIndices.push_back(vertexIndex);
        }
        token = strtok(nullptr, " "); // Move to next token
    }

    // Process the face vertices and add triangle indices to out_indices
    processFaceVertices(faceVertexIndices, out_indices, out_facesNb);
}

// Function to handle face vertices in the extended format (f v/vt/vn)
void handleExtendedVertices(const char* line, std::vector<unsigned int>& out_indices, int &out_facesNb) {
    std::vector<unsigned int> faceVertexIndices;
    std::vector<unsigned int> faceUVIndices;
    std::vector<unsigned int> faceNormalIndices;

    char* token = strtok(const_cast<char*>(line), " "); // Split the line by space

    while (token != nullptr) {
        if (token[0] == 'f') {
            token = strtok(nullptr, " "); // Move to next token
            continue;
        }
        unsigned int vertexIndex, uvIndex, normalIndex;
        if (sscanf(token, "%u/%u/%u", &vertexIndex, &uvIndex, &normalIndex) == 3) {
            faceVertexIndices.push_back(vertexIndex);
            faceUVIndices.push_back(uvIndex);
            faceNormalIndices.push_back(normalIndex);
        }
        token = strtok(nullptr, " "); // Move to next token
    }

    // Process the face vertices and add triangle indices to out_indices
    processFaceVertices(faceVertexIndices, out_indices, out_facesNb);
}

bool objloader::loadOBJ(
	const char* path,
	std::vector<c_math::vec3>   &out_vertices,
    std::vector<unsigned int>   &out_indices,
    int                         &out_facesNb,
    int                         &out_verticesNb
)
{
	printf("Loading OBJ file %s...\n", path);

	std::vector< unsigned int > vertexIndices, uvIndices, normalIndices;
	std::vector< c_math::vec3 > temp_vertices;
	std::vector< c_math::vec2 > temp_uvs;
	std::vector< c_math::vec3 > temp_normals;

	FILE* file = fopen(path, "r");
	if (file == nullptr) {
		printf("Impossible to open the file %s !\n", path);
		return false;
	}

	while (1) {

		char lineHeader[128];
		// read the first word of the line
		int res = fscanf(file, "%s", lineHeader);
		if (res == EOF)
			break; // EOF = End Of File. Quit the loop.

		// else : parse lineHeader

		if (strcmp(lineHeader, "v") == 0) {
            out_verticesNb++;
			c_math::vec3 vertex;
			fscanf(file, "%f %f %f\n", &vertex.x, &vertex.y, &vertex.z);
			temp_vertices.push_back(vertex);
		} else if (strcmp(lineHeader, "vt") == 0) {
			c_math::vec2 uv;
			fscanf(file, "%f %f\n", &uv.x, &uv.y);
//			uv.y = -uv.y; // Invert V coordinate if using DDS texture, which are inverted.
			temp_uvs.push_back(uv);
		} else if (strcmp(lineHeader, "vn") == 0) {
			c_math::vec3 normal;
			fscanf(file, "%f %f %f\n", &normal.x, &normal.y, &normal.z);
			temp_normals.push_back(normal);
		} else if (strcmp(lineHeader, "f") == 0) {
            // Read face indices until end of line
            char line[128];
            while (fgets(line, sizeof(line), file) != nullptr) {
                if (strchr(line, '/')) {
                    // Extended format (f v/vt/vn)
                    handleExtendedVertices(line, out_indices, out_facesNb);
                    break;
                }
                else {
                    // Current format (f v1 v2 v3 ...)
                    handleOnlyVertices(line, out_indices, out_facesNb);
                    break;
                }
            }
        } else {
			// Probably a comment, eat up the rest of the line
			char stupidBuffer[1000];
			fgets(stupidBuffer, 1000, file);
		}

	}
    out_vertices = temp_vertices;
	fclose(file);
	return true;
}
