//
// Created by Lucas on 07.12.2023.
// Custom Math Library inspired by GLM
//

#ifndef VECTOR_HPP
#define VECTOR_HPP

#include <cassert>

namespace c_math {

    template<int L, typename T> struct vec;

    // vec2
    template<typename T>
    struct vec<2, T>
    {
        typedef T value_type;
        typedef vec<2, T> type;

        T x, y;

        // -- Implicit basic constructors --

        vec();
        vec(vec<2, T> const& v);

        // Explicit basic constructors
        explicit vec(T scalar);
        vec(T x, T y);

        /// Return the count of components of the vector
        typedef int length_type;
        static length_type length(){return 2;}

        T & operator[](length_type i);
        T const& operator[](length_type i) const;

        // Unary arithmetic operators

		vec<2, T>& operator=(vec<2, T> const& v);
        template<typename U>
        vec<2, T>& operator+=(U scalar);
        template<typename U>
        vec<2, T>& operator+=(vec<1, U> const& v);
        template<typename U>
        vec<2, T>& operator+=(vec<2, U> const& v);
        template<typename U>
        vec<2, T>& operator*=(U scalar);
        template<typename U>
        vec<2, T>& operator*=(vec<1, U> const& v);
        template<typename U>
        vec<2, T>& operator*=(vec<2, U> const& v);
    };

    // vec3
    template<typename T>
    struct vec<3, T> {
        typedef T value_type;
        typedef vec<3, T> type;

        vec();

        T x, y, z;

        vec(T x, T y, T z);

        /// Return the count of components of the vector
        typedef int length_type;
        static length_type length(){return 3;}

        T & operator[](length_type i);
        T const& operator[](length_type i) const;

        template<typename U>
        vec<3, T> & operator+=(vec<3, U> const& v);
        template<typename U>
        vec<3, T> & operator-=(vec<3, U> const& v);
    };


    // vec4
    template<typename T>
    struct vec<4, T> {
        typedef T value_type;
        typedef vec<4, T> type;

        T x, y, z, w;

        // -- Implicit basic constructors --

        vec();
        vec(vec<4, T> const& v);

        // Explicit basic constructors
        explicit vec(T scalar);
        vec(T x, T y, T z, T w);

        /// Return the count of components of the vector
        typedef int length_type;
        static length_type length(){return 4;}

        T & operator[](length_type i);
        T const& operator[](length_type i) const;


        // Unary arithmetic operators

		vec<4, T>& operator=(vec<4, T> const& v);
        template<typename U>
        vec<4, T>& operator+=(U scalar);
        template<typename U>
        vec<4, T>& operator+=(vec<1, U> const& v);
        template<typename U>
        vec<4, T>& operator+=(vec<4, U> const& v);
        template<typename U>
        vec<4, T>& operator*=(U scalar);
        template<typename U>
        vec<4, T>& operator*=(vec<1, U> const& v);
        template<typename U>
        vec<4, T>& operator*=(vec<4, U> const& v);
    };


    // float vectors definition
    typedef vec<2, float>   vec2;
    typedef vec<3, float>   vec3;
    typedef vec<4, float>   vec4;


    // vec2 -- implicit basic constructors --
    template<typename T>
    vec<2, T>::vec() : x(0), y(0) {}

    template<typename T>
    vec<2, T>::vec(vec<2, T> const& v) : x(v.x), y(v.y) {}

    // Explicit basic constructors
    template<typename T>
    vec<2, T>::vec(T scalar) : x(scalar), y(scalar) {}

    template<typename T>
    vec<2, T>::vec(T _x, T _y) : x(_x), y(_y) {}

    // Unary arithmetic operators
	template<typename T>
	vec<2, T>& vec<2, T>::operator=(vec<2, T> const& v)
	{
		if (this != &v) {
            x = v.x;
            y = v.y;
        }

		return *this;
	}

    template<typename T>
    template<typename U>
    vec<2, T> & vec<2, T>::operator+=(U scalar)
    {
        this->x += static_cast<T>(scalar);
        this->y += static_cast<T>(scalar);
        return *this;
    }

    template<typename T>
    template<typename U>
    vec<2, T> & vec<2, T>::operator+=(vec<1, U> const& v)
    {
        this->x += static_cast<T>(v.x);
        this->y += static_cast<T>(v.x);
        return *this;
    }

    template<typename T>
    template<typename U>
    vec<2, T> & vec<2, T>::operator+=(vec<2, U> const& v)
    {
        this->x += static_cast<T>(v.x);
        this->y += static_cast<T>(v.y);
        return *this;
    }

    template<typename T>
    template<typename U>
    vec<2, T> & vec<2, T>::operator*=(U scalar)
    {
        this->x *= static_cast<T>(scalar);
        this->y *= static_cast<T>(scalar);
        return *this;
    }

    template<typename T>
    template<typename U>
    vec<2, T> & vec<2, T>::operator*=(vec<1, U> const& v)
    {
        this->x *= static_cast<T>(v.x);
        this->y *= static_cast<T>(v.x);
        return *this;
    }

    template<typename T>
    template<typename U>
    vec<2, T> & vec<2, T>::operator*=(vec<2, U> const& v)
    {
        this->x *= static_cast<T>(v.x);
        this->y *= static_cast<T>(v.y);
        return *this;
    }


    // vec3 Binary Operators
    template<typename T>
    vec<3, T> operator*(vec<3, T> const& v, T scalar);

    template<typename T>
    vec<3, T> operator*(vec<3, T> const& v1, vec<1, T> const& v2);

    template<typename T>
    vec<3, T> operator*(T scalar, vec<3, T> const& v);

    template<typename T>
    vec<3, T> operator*(vec<1, T> const& v1, vec<3, T> const& v2);

    template<typename T>
    vec<3, T> operator*(vec<3, T> const& v1, vec<3, T> const& v2);



    //  vec4 -- Unary operators --

    template<typename T>
    vec<4, T> operator+(vec<4, T> const& v);

    template<typename T>
    vec<4, T> operator-(vec<4, T> const& v);



    // Binary operators

    template<typename T>
    vec<4, T> operator+(vec<4, T> const& v, T const & scalar);

    template<typename T>
    vec<4, T> operator+(vec<4, T> const& v1, vec<1, T> const& v2);

    template<typename T>
    vec<4, T> operator+(T scalar, vec<4, T> const& v);

    template<typename T>
    vec<4, T> operator+(vec<1, T> const& v1, vec<4, T> const& v2);

    template<typename T>
    vec<4, T> operator+(vec<4, T> const& v1, vec<4, T> const& v2);


    template<typename T>
    vec<4, T> operator*(vec<4, T> const& v, T const & scalar);

    template<typename T>
    vec<4, T> operator*(vec<4, T> const& v1, vec<1, T> const& v2);

    template<typename T>
    vec<4, T> operator*(T scalar, vec<4, T> const& v);

    template<typename T>
    vec<4, T> operator*(vec<1, T> const& v1, vec<4, T> const& v2);

    template<typename T>
    vec<4, T> operator*(vec<4, T> const& v1, vec<4, T> const& v2);



    // vec3 -- Component accesses --

    template<typename T>
    T & vec<3, T>::operator[](typename vec<3, T>::length_type i)
    {
        assert(i >= 0 && i < this->length());
        switch(i)
        {
            default:
            case 0:
                return x;
            case 1:
                return y;
            case 2:
                return z;
        }
    }

    template<typename T>
    T const& vec<3, T>::operator[](typename vec<3, T>::length_type i) const
    {
        assert(i >= 0 && i < this->length());
        switch(i)
        {
            default:
            case 0:
                return x;
            case 1:
                return y;
            case 2:
                return z;
        }
    }


    // Constructors vec3
    template<typename T>
    vec<3, T>::vec() : x(0), y(0), z(0) {}

    template<typename T>
    vec<3, T>::vec(T _x, T _y, T _z)
            : x(_x), y(_y), z(_z) {}


    // Constructors vec4
    template<typename T>
    vec<4, T>::vec()
    : x(0), y(0), z(0), w(0) {}

    template<typename T>
    vec<4, T>::vec(T _x, T _y, T _z, T _w)
            : x(_x), y(_y), z(_z), w(_w) {}

    template<typename T>
    vec<4, T>::vec(vec<4, T> const& v)
            : x(v.x), y(v.y), z(v.z), w(v.w)
    {}

    template<typename T>
    vec<4, T>::vec(T scalar)
            : x(scalar), y(scalar), z(scalar), w(scalar)
    {}

    // Unary constant operators

    template<typename T>
    vec<3, T> operator+(vec<4, T> const& v)
    {
        return v;
    }

    template<typename T>
    vec<4, T> operator-(vec<4, T> const& v)
    {
        return vec<4, T>(0) -= v;
    }


    // Unary arithmetic operators
    template<typename T>
    vec<3, T> operator*(vec<3, T> const& v, T scalar)
    {
        return vec<3, T>(
                v.x * scalar,
                v.y * scalar,
                v.z * scalar);
    }

    template<typename T>
    vec<3, T> operator*(vec<3, T> const& v, vec<1, T> const& scalar)
    {
        return vec<3, T>(
                v.x * scalar.x,
                v.y * scalar.x,
                v.z * scalar.x);
    }

    template<typename T>
    vec<3, T> operator*(T scalar, vec<3, T> const& v)
    {
        return vec<3, T>(
                scalar * v.x,
                scalar * v.y,
                scalar * v.z);
    }

    template<typename T>
    vec<3, T> operator*(vec<1, T> const& scalar, vec<3, T> const& v)
    {
        return vec<3, T>(
                scalar.x * v.x,
                scalar.x * v.y,
                scalar.x * v.z);
    }

    template<typename T>
    vec<3, T> operator*(vec<3, T> const& v1, vec<3, T> const& v2)
    {
        return vec<3, T>(
                v1.x * v2.x,
                v1.y * v2.y,
                v1.z * v2.z);
    }

    template<typename T>
    template<typename U>
    vec<3, T> & vec<3, T>::operator+=(vec<3, U> const& v)
    {
        this->x += static_cast<T>(v.x);
        this->y += static_cast<T>(v.y);
        this->z += static_cast<T>(v.z);
        return *this;
    }

    template<typename T>
    template<typename U>
    vec<3, T> & vec<3, T>::operator-=(vec<3, U> const& v)
    {
        this->x -= static_cast<T>(v.x);
        this->y -= static_cast<T>(v.y);
        this->z -= static_cast<T>(v.z);
        return *this;
    }

    // Binary arithmetic operators

    template<typename T>
    vec<3, T> operator+(vec<3, T> const& v, T scalar)
    {
        return vec<3, T>(
                v.x + scalar,
                v.y + scalar,
                v.z + scalar);
    }

    template<typename T>
    vec<3, T> operator+(vec<3, T> const& v, vec<1, T> const& scalar)
    {
        return vec<3, T>(
                v.x + scalar.x,
                v.y + scalar.x,
                v.z + scalar.x);
    }

    template<typename T>
    vec<3, T> operator+(T scalar, vec<3, T> const& v)
    {
        return vec<3, T>(
                scalar + v.x,
                scalar + v.y,
                scalar + v.z);
    }

    template<typename T>
    vec<3, T> operator+(vec<1, T> const& scalar, vec<3, T> const& v)
    {
        return vec<3, T>(
                scalar.x + v.x,
                scalar.x + v.y,
                scalar.x + v.z);
    }

    template<typename T>
    vec<3, T> operator+(vec<3, T> const& v1, vec<3, T> const& v2)
    {
        return vec<3, T>(
                v1.x + v2.x,
                v1.y + v2.y,
                v1.z + v2.z);
    }

    template<typename T>
    vec<3, T> operator-(vec<3, T> const& v1, vec<3, T> const& v2)
    {
        return vec<3, T>(
                v1.x - v2.x,
                v1.y - v2.y,
                v1.z - v2.z);
    }

    // vec4
    // Details
    template<typename T>
    struct compute_vec4_add
    {
        static vec<4, T> call(vec<4, T> const& a, vec<4, T> const& b)
        {
            return vec<4, T>(a.x + b.x, a.y + b.y, a.z + b.z, a.w + b.w);
        }
    };

    template<typename T>
    struct compute_vec4_mul
    {
        static vec<4, T> call(vec<4, T> const& a, vec<4, T> const& b)
        {
            return vec<4, T>(a.x * b.x, a.y * b.y, a.z * b.z, a.w * b.w);
        }
    };

    // Component accesses
    template<typename T>
    T& vec<4, T>::operator[](typename vec<4, T>::length_type i)
    {
        assert(i >= 0 && i < this->length());
        switch(i)
        {
            default:
            case 0:
                return x;
            case 1:
                return y;
            case 2:
                return z;
            case 3:
                return w;
        }
    }

    template<typename T>
    T const& vec<4, T>::operator[](typename vec<4, T>::length_type i) const
    {
        assert(i >= 0 && i < this->length());
        switch(i)
        {
            default:
            case 0:
                return x;
            case 1:
                return y;
            case 2:
                return z;
            case 3:
                return w;
        }
    }

    // -- Unary constant operators --

    template<typename T>
    vec<4, T> operator+(vec<4, T> const& v)
    {
        return v;
    }


    // Unary arithmetic operators
	template<typename T>
	vec<4, T>& vec<4, T>::operator=(vec<4, T> const& v)
	{
		if (this != &v) {
            x = v.x;
            y = v.y;
            z = v.z;
            w = v.w;
        }

		return *this;
	}

    template<typename T>
    template<typename U>
    vec<4, T> & vec<4, T>::operator+=(U scalar)
    {
        return (*this = compute_vec4_add<T>::call(*this, vec<4, T>(scalar)));
    }

    template<typename T>
    template<typename U>
    vec<4, T> & vec<4, T>::operator+=(vec<1, U> const& v)
    {
        return (*this = compute_vec4_add<T>::call(*this, vec<4, T>(v.x)));
    }

    template<typename T>
    template<typename U>
    vec<4, T> & vec<4, T>::operator+=(vec<4, U> const& v)
    {
        return (*this = compute_vec4_add<T>::call(*this, vec<4, T>(v)));
    }


    template<typename T>
    template<typename U>
    vec<4, T> & vec<4, T>::operator*=(U scalar)
    {
        return (*this = compute_vec4_mul<T>::call(*this, vec<4, T>(scalar)));
    }

    template<typename T>
    template<typename U>
    vec<4, T> & vec<4, T>::operator*=(vec<1, U> const& v)
    {
        return (*this = compute_vec4_mul<T>::call(*this, vec<4, T>(v.x)));
    }

    template<typename T>
    template<typename U>
    vec<4, T> & vec<4, T>::operator*=(vec<4, U> const& v)
    {
        return (*this = compute_vec4_mul<T>::call(*this, vec<4, T>(v)));
    }

    // -- Binary arithmetic operators --

    template<typename T>
    vec<4, T> operator+(vec<4, T> const& v, T const & scalar)
    {
        return vec<4, T>(v) += scalar;
    }

    template<typename T>
    vec<4, T> operator+(vec<4, T> const& v1, vec<1, T> const& v2)
    {
        return vec<4, T>(v1) += v2;
    }

    template<typename T>
    vec<4, T> operator+(T scalar, vec<4, T> const& v)
    {
        return vec<4, T>(v) += scalar;
    }

    template<typename T>
    vec<4, T> operator+(vec<1, T> const& v1, vec<4, T> const& v2)
    {
        return vec<4, T>(v2) += v1;
    }

    template<typename T>
    vec<4, T> operator+(vec<4, T> const& v1, vec<4, T> const& v2)
    {
        return vec<4, T>(v1) += v2;
    }


    template<typename T>
    vec<4, T> operator*(vec<4, T> const& v, T const & scalar)
    {
        return vec<4, T>(v) *= scalar;
    }

    template<typename T>
    vec<4, T> operator*(vec<4, T> const& v1, vec<1, T> const& v2)
    {
        return vec<4, T>(v1) *= v2;
    }

    template<typename T>
    vec<4, T> operator*(T scalar, vec<4, T> const& v)
    {
        return vec<4, T>(v) *= scalar;
    }

    template<typename T>
    vec<4, T> operator*(vec<1, T> const& v1, vec<4, T> const& v2)
    {
        return vec<4, T>(v2) *= v1;
    }

    template<typename T>
    vec<4, T> operator*(vec<4, T> const& v1, vec<4, T> const& v2)
    {
        return vec<4, T>(v1) *= v2;
    }

}


#endif //VECTOR_HPP
