//
// Created by Lucas on 07.12.2023.
//

#ifndef MATRIX_TRANSFORM_HPP
#define MATRIX_TRANSFORM_HPP

#include "matrix.hpp"
#include "geometric.hpp"

namespace c_math {
    template<typename T>
    mat<4, 4, T> translate(
            mat<4, 4, T> const& m, vec<3, T> const& v);

    template<typename T>
    mat<4, 4, T> translate(mat<4, 4, T> const& m, vec<3, T> const& v)
    {
        mat<4, 4, T> Result(m);
        Result[3] = m[0] * v[0] + m[1] * v[1] + m[2] * v[2] + m[3];
        return Result;
    }

    template<typename T>
    mat<4, 4, T> rotate(mat<4, 4, T> const& m, T angle, vec<3, T> const& v)
    {
        T const a = angle;
        T const c = cos(a);
        T const s = sin(a);

        vec<3, T> axis(normalize(v));
        vec<3, T> temp((T(1) - c) * axis);

        mat<4, 4, T> Rotate;
        Rotate[0][0] = c + temp[0] * axis[0];
        Rotate[0][1] = temp[0] * axis[1] + s * axis[2];
        Rotate[0][2] = temp[0] * axis[2] - s * axis[1];

        Rotate[1][0] = temp[1] * axis[0] - s * axis[2];
        Rotate[1][1] = c + temp[1] * axis[1];
        Rotate[1][2] = temp[1] * axis[2] + s * axis[0];

        Rotate[2][0] = temp[2] * axis[0] + s * axis[1];
        Rotate[2][1] = temp[2] * axis[1] - s * axis[0];
        Rotate[2][2] = c + temp[2] * axis[2];

        mat<4, 4, T> Result;
        Result[0] = m[0] * Rotate[0][0] + m[1] * Rotate[0][1] + m[2] * Rotate[0][2];
        Result[1] = m[0] * Rotate[1][0] + m[1] * Rotate[1][1] + m[2] * Rotate[1][2];
        Result[2] = m[0] * Rotate[2][0] + m[1] * Rotate[2][1] + m[2] * Rotate[2][2];
        Result[3] = m[3];
        return Result;
    }

    template<typename T>
    mat<4, 4, T> scale(mat<4, 4, T> const& m, vec<3, T> const& v)
    {
        mat<4, 4, T> Result;
        Result[0] = m[0] * v[0];
        Result[1] = m[1] * v[1];
        Result[2] = m[2] * v[2];
        Result[3] = m[3];
        return Result;
    }

    template<typename T>
    mat<4, 4, T> lookAt(vec<3, T> const& eye, vec<3, T> const& center, vec<3, T> const& up)
    {
        vec<3, T> const f(normalize(center - eye));
        vec<3, T> const s(normalize(cross(f, up)));
        vec<3, T> const u(cross(s, f));

        mat<4, 4, T> Result(1);
        Result[0][0] = s.x;
        Result[1][0] = s.y;
        Result[2][0] = s.z;
        Result[0][1] = u.x;
        Result[1][1] = u.y;
        Result[2][1] = u.z;
        Result[0][2] =-f.x;
        Result[1][2] =-f.y;
        Result[2][2] =-f.z;
        Result[3][0] =-dot(s, eye);
        Result[3][1] =-dot(u, eye);
        Result[3][2] = dot(f, eye);
        return Result;
    }
}

#endif //MATRIX_TRANSFORM_HPP
