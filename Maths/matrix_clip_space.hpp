//
// Created by Lucas on 12.12.2023.
//

#ifndef OPENGL_MATRIX_CLIP_SPACE_HPP
#define OPENGL_MATRIX_CLIP_SPACE_HPP

#include <cmath>
#include <limits>

#include "matrix.hpp"

namespace c_math {

    template<typename T>
    mat<4, 4, T> perspective(T fovy, T aspect, T zNear, T zFar)
    {
        assert(abs(aspect - std::numeric_limits<T>::epsilon()) > static_cast<T>(0));

        T const tanHalfFovy = tan(fovy / static_cast<T>(2));

        mat<4, 4, T> Result(static_cast<T>(0));
        Result[0][0] = static_cast<T>(1) / (aspect * tanHalfFovy);
        Result[1][1] = static_cast<T>(1) / (tanHalfFovy);
        Result[2][2] = - (zFar + zNear) / (zFar - zNear);
        Result[2][3] = - static_cast<T>(1);
        Result[3][2] = - (static_cast<T>(2) * zFar * zNear) / (zFar - zNear);
        return Result;
    }
}

#endif //OPENGL_MATRIX_CLIP_SPACE_HPP
