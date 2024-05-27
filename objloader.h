#ifndef OBJLOADER_H
#define OBJLOADER_H

#include <vector>
#include <string>
#include <cstring>
#include "Maths/c_math_inc.hpp"

class objloader
{
public:
	static bool loadOBJ(
		const char* path, 
		std::vector<c_math::vec3>   &out_vertices,
        std::vector<unsigned int>   &out_indices,
        int                         &out_facesNb,
        int                         &out_verticesNb
    );
};

#endif
