#ifndef VECTOR
#define VECTOR
#include <cstddef>

namespace abyss
{
    namespace math
    {
        /**
         * @brief Anyss Engine 3D Vector class
         *
         */
        class Vector
        {
            float v[3];

        public:
            /**
             * @brief Construct a new Vector object
             *
             */
            Vector();
            /**
             * @brief Destroy the Vector object
             *
             */
            ~Vector();

            /**
             * @brief Adds two vectors
             * 
             * @return abyss::math::Vector 
             */
            abyss::math::Vector operator+(const abyss::math::Vector &) const;
            /**
             * @brief Subtracts two vectors
             * 
             * @return abyss::math::Vector 
             */
            abyss::math::Vector operator-(const abyss::math::Vector &) const;
            /**
             * @brief Multiplies a vector by a scalar
             * 
             * @return abyss::math::Vector 
             */
            abyss::math::Vector operator*(float) const;
            /**
             * @brief Divides a vector by a scalar
             * 
             * @return abyss::math::Vector 
             */
            abyss::math::Vector operator/(float) const;
            /**
             * @brief Adds and assigns a vector
             * 
             * @return abyss::math::Vector 
             */
            abyss::math::Vector& operator+=(const abyss::math::Vector &);
            /**
             * @brief Subtracts and assigns a vector
             * 
             * @return abyss::math::Vector 
             */
            abyss::math::Vector& operator-=(const abyss::math::Vector &);
            /**
             * @brief Multiplies and assigns by a scalar
             * 
             * @return abyss::math::Vector 
             */
            abyss::math::Vector& operator*=(float);
            /**
             * @brief Divides and assigns by a scalar
             * 
             * @return abyss::math::Vector 
             */
            abyss::math::Vector& operator/=(float);

            /**
             * @brief Checks if two vectors are equal
             * 
             * @return bool 
             */
            bool operator==(const abyss::math::Vector &) const;
            /**
             * @brief Checks if two vectors are not equal
             * 
             * @return bool 
             */
            bool operator!=(const abyss::math::Vector &) const;

            /**
             * @brief Accesses the element at the given index
             * 
             * @param index 
             * @return float 
             */
            float operator[](std::size_t index) const;
            
            /**
             * @brief Accesses the element at the given index
             * 
             * @param index 
             * @return float& 
             */
            float& operator[](std::size_t index);
            /**
             * @brief Computes the dot product of two vectors
             * 
             * @return float 
             */
            float Dot(const abyss::math::Vector &) const;
            /**
             * @brief Computes the cross product of two vectors
             * 
             * @return abyss::math::Vector 
             */
            abyss::math::Vector Cross(const abyss::math::Vector &) const;
            /**
             * @brief Computes the length (magnitude) of the vector
             * 
             * @return float 
             */
            float Length() const;
            /**
             * @brief Normalizes the vector
             * 
             * @return abyss::math::Vector 
             */
            abyss::math::Vector Normalize() const;

            /**
             * @brief Linearly interpolates between this vector and a target vector
             * 
             * @return abyss::math::Vector 
             */
            abyss::math::Vector Lerp(const abyss::math::Vector &target, float t) const;

        };
    }
} // namespace abyss

#endif /* VECTOR */
