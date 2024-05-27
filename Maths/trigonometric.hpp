//
// Created by Lucas on 07.12.2023.
//

#ifndef OPENGL_TRIGONOMETRIC_HPP
#define OPENGL_TRIGONOMETRIC_HPP

#include <cmath>
#include <limits>

namespace c_math {
    // radians
    template<typename genType>
    genType radians(genType degrees)
    {
        static_assert(std::numeric_limits<genType>::is_iec559, "'radians' only accept floating-point input");

        return degrees * static_cast<genType>(0.01745329251994329576923690768489);
    }
}

#endif //OPENGL_TRIGONOMETRIC_HPP
