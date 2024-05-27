//
// Created by Lucas on 07.12.2023.
//

#ifndef MATRIX_HPP
#define MATRIX_HPP

#include <cassert>

#include "vector.hpp"

namespace c_math {
    template<int C, int R, typename T> struct mat;

    template<typename T>
    struct mat<4, 4, T> {
        typedef vec<4, T> col_type;
        typedef vec<4, T> row_type;
        typedef mat<4, 4, T> type;
        typedef mat<4, 4, T> transpose_type;
        typedef T value_type;

    private:
        col_type value[4];

    public:
        typedef int length_type;
        static length_type length(){return 4;}

        col_type & operator[](length_type i);
        col_type const& operator[](length_type i) const;

        mat();
        explicit mat(T const& s);

    };

    typedef mat<4, 4, float>    mat4;


    // Binary operators

    template<typename T>
    mat<4, 4, T> operator*(mat<4, 4, T> const& m, T const& s);

    template<typename T>
    mat<4, 4, T> operator*(T const& s, mat<4, 4, T> const& m);

    template<typename T>
    typename mat<4, 4, T>::col_type operator*(mat<4, 4, T> const& m, typename mat<4, 4, T>::row_type const& v);

    template<typename T>
    typename mat<4, 4, T>::row_type operator*(typename mat<4, 4, T>::col_type const& v, mat<4, 4, T> const& m);

    template<typename T>
    mat<2, 4, T> operator*(mat<4, 4, T> const& m1, mat<2, 4, T> const& m2);

    template<typename T>
    mat<3, 4, T> operator*(mat<4, 4, T> const& m1, mat<3, 4, T> const& m2);

    template<typename T>
    mat<4, 4, T> operator*(mat<4, 4, T> const& m1, mat<4, 4, T> const& m2);


    // Constructors
    template<typename T>
    mat<4, 4, T>::mat()
    {
		this->value[0] = col_type(1, 0, 0, 0);
		this->value[1] = col_type(0, 1, 0, 0);
		this->value[2] = col_type(0, 0, 1, 0);
		this->value[3] = col_type(0, 0, 0, 1);
	}

    template<typename T>
    mat<4, 4, T>::mat(T const& s)
    {
		this->value[0] = col_type(s, 0, 0, 0);
		this->value[1] = col_type(0, s, 0, 0);
		this->value[2] = col_type(0, 0, s, 0);
		this->value[3] = col_type(0, 0, 0, s);
	}

    // -- Accesses --

    template<typename T>
    typename mat<4, 4, T>::col_type & mat<4, 4, T>::operator[](typename mat<4, 4, T>::length_type i)
    {
        assert(i < this->length());
        return this->value[i];
    }

    template<typename T>
    typename mat<4, 4, T>::col_type const& mat<4, 4, T>::operator[](typename mat<4, 4, T>::length_type i) const
    {
        assert(i < this->length());
        return this->value[i];
    }

    // Binary arithmetic operators

    template<typename T>
    mat<4, 4, T> operator*(mat<4, 4, T> const& m, T const  & s)
    {
        return mat<4, 4, T>(
                m[0] * s,
                m[1] * s,
                m[2] * s,
                m[3] * s);
    }

    template<typename T>
    mat<4, 4, T> operator*(T const& s, mat<4, 4, T> const& m)
    {
        return mat<4, 4, T>(
                m[0] * s,
                m[1] * s,
                m[2] * s,
                m[3] * s);
    }

    template<typename T>
    typename mat<4, 4, T>::col_type operator*
            (
                    mat<4, 4, T> const& m,
                    typename mat<4, 4, T>::row_type const& v
            )
    {
        typename mat<4, 4, T>::col_type const Mov0(v[0]);
        typename mat<4, 4, T>::col_type const Mov1(v[1]);
        typename mat<4, 4, T>::col_type const Mul0 = m[0] * Mov0;
        typename mat<4, 4, T>::col_type const Mul1 = m[1] * Mov1;
        typename mat<4, 4, T>::col_type const Add0 = Mul0 + Mul1;
        typename mat<4, 4, T>::col_type const Mov2(v[2]);
        typename mat<4, 4, T>::col_type const Mov3(v[3]);
        typename mat<4, 4, T>::col_type const Mul2 = m[2] * Mov2;
        typename mat<4, 4, T>::col_type const Mul3 = m[3] * Mov3;
        typename mat<4, 4, T>::col_type const Add1 = Mul2 + Mul3;
        typename mat<4, 4, T>::col_type const Add2 = Add0 + Add1;
        return Add2;
    }
}



#endif //MATRIX_HPP
