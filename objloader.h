#ifndef OBJLOADER_H
#define OBJLOADER_H

#include <vector>
#include <string>
#include <cstring>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

class objloader
{
public:
	static bool loadOBJ(
		const char* path, 
		std::vector<glm::vec3>   &out_vertices,
        std::vector<unsigned int>   &out_indices,
        int                         &out_facesNb,
        int                         &out_verticesNb
    );
};

#endif
