//
// Created by Lucas on 07.12.2023.
//

#ifndef OPENGL_GEOMETRIC_HPP
#define OPENGL_GEOMETRIC_HPP

#include <cmath>
#include <limits>
#include "vector.hpp"
#include "exponential.hpp"

namespace c_math {
namespace detail {

    template<typename V, typename T>
    struct compute_dot{};

    template<typename T>
    struct compute_dot<vec<1, T>, T>
    {
        static T call(vec<1, T> const& a, vec<1, T> const& b)
        {
            return a.x * b.x;
        }
    };

    template<typename T>
    struct compute_dot<vec<2, T>, T>
    {
        static T call(vec<2, T> const& a, vec<2, T> const& b)
        {
            vec<2, T> tmp(a * b);
            return tmp.x + tmp.y;
        }
    };

    template<typename T>
    struct compute_dot<vec<3, T>, T>
    {
        static T call(vec<3, T> const& a, vec<3, T> const& b)
        {
            vec<3, T> tmp(a * b);
            return tmp.x + tmp.y + tmp.z;
        }
    };

    template<typename T>
    struct compute_dot<vec<4, T>, T>
    {
        static T call(vec<4, T> const& a, vec<4, T> const& b)
        {
            vec<4, T> tmp(a * b);
            return (tmp.x + tmp.y) + (tmp.z + tmp.w);
        }
    };

    template<typename T>
    struct compute_cross
    {
        static vec<3, T> call(vec<3, T> const& x, vec<3, T> const& y)
        {
            static_assert(std::numeric_limits<T>::is_iec559, "'cross' accepts only floating-point inputs");

            return vec<3, T>(
                    x.y * y.z - y.y * x.z,
                    x.z * y.x - y.z * x.x,
                    x.x * y.y - y.x * x.y);
        }
    };
}

    template<typename T>
    T dot(T x, T y)
    {
        static_assert(std::numeric_limits<T>::is_iec559, "'dot' accepts only floating-point inputs");
        return x * y;
    }

    template<int L, typename T>
    T dot(vec<L, T> const& x, vec<L, T> const& y)
    {
        static_assert(std::numeric_limits<T>::is_iec559, "'dot' accepts only floating-point inputs");
        return detail::compute_dot<vec<L, T>, T>::call(x, y);
    }

    template<int L, typename T>
    struct compute_normalize
    {
        static vec<L, T> call(vec<L, T> const& v)
        {
            static_assert(std::numeric_limits<T>::is_iec559, "'normalize' accepts only floating-point inputs");

            return v * inversesqrt(dot(v, v));
        }
    };

    template<int L, typename T>
    vec<L, T> normalize(vec<L, T> const& x)
    {
        static_assert(std::numeric_limits<T>::is_iec559, "'normalize' accepts only floating-point inputs");

        return compute_normalize<L, T>::call(x);
    }

    // cross
    template<typename T>
    vec<3, T> cross(vec<3, T> const& x, vec<3, T> const& y)
    {
        return detail::compute_cross<T>::call(x, y);
    }
}

#endif //OPENGL_GEOMETRIC_HPP
