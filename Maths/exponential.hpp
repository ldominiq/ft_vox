//
// Created by Lucas on 07.12.2023.
//

#ifndef OPENGL_EXPONENTIAL_HPP
#define OPENGL_EXPONENTIAL_HPP

#include "vector.hpp"
#include <cmath>
#include <limits>

namespace c_math {
namespace detail {
    template<int L, typename T>
    struct compute_inversesqrt
    {
        static vec<L, T> call(vec<L, T> const& x)
        {
            return static_cast<T>(1) / sqrt(x);
        }
    };

    template<int L>
    struct compute_inversesqrt<L, float>
    {
        static vec<L, float> call(vec<L, float> const& x)
        {
            vec<L, float> tmp(x);
            vec<L, float> xhalf(tmp * 0.5f);
            vec<L, unsigned int>* p = reinterpret_cast<vec<L, unsigned int>*>(const_cast<vec<L, float>*>(&x));
            vec<L, unsigned int> i = vec<L, unsigned int>(0x5f375a86) - (*p >> vec<L, unsigned int>(1));
            vec<L, float>* ptmp = reinterpret_cast<vec<L, float>*>(&i);
            tmp = *ptmp;
            tmp = tmp * (1.5f - xhalf * tmp * tmp);
            return tmp;
        }
    };
}
    template<typename genType>
    genType inversesqrt(genType x)
    {
        return static_cast<genType>(1) / sqrt(x);
    }

    template<int L, typename T>
    vec<L, T> inversesqrt(vec<L, T> const& x)
{
    static_assert(std::numeric_limits<T>::is_iec559, "'inversesqrt' only accept floating-point inputs");
    return detail::compute_inversesqrt<L, T>::call(x);
}
}

#endif //OPENGL_EXPONENTIAL_HPP
