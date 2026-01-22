#ifndef MATRIX
#define MATRIX
#include <abyss/math/Vector.hpp>
#include <cstdint>

namespace abyss
{
    namespace math
    {
        /**
         * @brief Anyss Engine 3x4 Column-Major Matrix class
         *
         */
        class Matrix
        {
            /**
             * @brief Matrix elements in column-major order
             *
             *
             * [ m0 m1 m2    0]
             *
             * [ m4 m5 m6    0]
             *
             * [ m8 m9 m10   0]
             *
             * [ m12 m13 m14 1]
             */
            float m[16];

        public:
            /// @brief Returns zero matrix
            Matrix();
            /// @brief Destroys the matrix
            ~Matrix();

            /**
             * @brief Operator =
             *
             * @return abyss::math::Matrix
             */
            abyss::math::Matrix operator=(const abyss::math::Matrix &) const;
            /**
             * @brief Multiply two matrices
             *
             * @return abyss::math::Matrix
             */
            abyss::math::Matrix operator*(const abyss::math::Matrix &) const;
            /**
             * @brief Add two matrices
             *
             * @return abyss::math::Matrix
             */
            abyss::math::Matrix operator+(const abyss::math::Matrix &) const;
            /**
             * @brief Subtract two matrices
             *
             * @return abyss::math::Matrix
             */
            abyss::math::Matrix operator-(const abyss::math::Matrix &) const;
            /**
             * @brief Multiply matrix by a scalar
             *
             * @return abyss::math::Matrix
             */
            abyss::math::Matrix operator*(float) const;
            /**
             * @brief Divide matrix by a scalar
             *
             * @return abyss::math::Matrix
             */
            abyss::math::Matrix operator/(float) const;
            /**
             * @brief Multiply and assign matrix
             *
             * @return abyss::math::Matrix
             */
            abyss::math::Matrix &operator*=(const abyss::math::Matrix &);
            /**
             * @brief Add and assign matrix
             *
             * @return abyss::math::Matrix
             */
            abyss::math::Matrix &operator+=(const abyss::math::Matrix &);
            /**
             * @brief Subtract and assign matrix
             *
             * @return abyss::math::Matrix
             */
            abyss::math::Matrix &operator-=(const abyss::math::Matrix &);
            /**
             * @brief Multiply and assign by a scalar
             *
             * @return abyss::math::Matrix
             */
            abyss::math::Matrix &operator*=(float);
            /**
             * @brief Divide and assign by a scalar
             *
             * @return abyss::math::Matrix
             */
            abyss::math::Matrix &operator/=(float);
            /**
             * @brief Checks if two matrices are equal
             *
             * @return bool
             */
            bool operator==(const abyss::math::Matrix &) const;

            /**
             * @brief Checks if two matrices are not equal
             *
             * @return bool
             */
            bool operator!=(const abyss::math::Matrix &) const;

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
            float &operator[](std::size_t index);
            /**
             * @brief Returns identity matrix
             *
             * @return Matrix
             */
            static Matrix Identity();
            /**
             * @brief Returns the right vector of the matrix
             *
             * @return abyss::math::Vector
             */
            abyss::math::Vector Right() const;
            /**
             * @brief Returns the up vector of the matrix
             *
             * @return abyss::math::Vector
             */
            abyss::math::Vector Up() const;
            /**
             * @brief Returns the direction vector of the matrix
             *
             * @return abyss::math::Vector
             */
            abyss::math::Vector Dir() const;
            /**
             * @brief Returns the position vector of the matrix
             *
             * @return abyss::math::Vector
             */
            abyss::math::Vector Position() const;

            /**
             * @brief Transforms a vector by this matrix
             *
             * @return abyss::math::Vector
             */
            abyss::math::Vector Transform(abyss::math::Vector) const;
            /**
             * @brief Rotates a vector by this matrix
             *
             * @return abyss::math::Vector
             */
            abyss::math::Vector Rotate(abyss::math::Vector) const;
            /**
             * @brief Compute the inverse transform of a vector by this matrix
             *
             * @return abyss::math::Vector
             */
            abyss::math::Vector InverseTransform(abyss::math::Vector) const;
            /**
             * @brief Compute the inverse rotation of a vector by this matrix
             *
             * @return abyss::math::Vector
             */
            abyss::math::Vector InverseRotate(abyss::math::Vector) const;

            /**
             * @brief Returns a new matrix scaled by the given factors on each axis
             *
             * @param sx Factor to scale on the X axis
             * @param sy Factor to scale on the Y axis
             * @param sz Factor to scale on the Z axis
             * @return abyss::math::Matrix
             */
            abyss::math::Matrix SetScaling(float sx, float sy, float sz) const;
            /**
             * @brief Returns a new matrix translated by the given amounts on each axis
             *
             * @param tx Translation on the X axis
             * @param ty Translation on the Y axis
             * @param tz Translation on the Z axis
             * @return abyss::math::Matrix
             */
            abyss::math::Matrix SetTranslation(float tx, float ty, float tz) const;
            /**
             * @brief Returns a new matrix rotated by the given pitch, yaw, and roll (in radians)
             *
             * @param pitch Rotation around the X axis
             * @param yaw Rotation around the Y axis
             * @param roll Rotation around the Z axis
             * @return abyss::math::Matrix
             */
            abyss::math::Matrix SetRotation(float pitch, float yaw, float roll) const;

            /**
             * @brief Compute the inverse of this matrix
             *
             * @return abyss::math::Matrix
             */
            abyss::math::Matrix Inverse() const;
            /**
             * @brief Creates a view matrix for a camera looking from eye to target with the given up vector
             *
             * @param eye The position of the camera
             * @param target The point the camera is looking at
             * @param up The up direction for the camera
             * @return abyss::math::Matrix
             */
            abyss::math::Matrix LookAt(const abyss::math::Vector &eye, const abyss::math::Vector &target, const abyss::math::Vector &up) const;
            /**
             * @brief Returns the matrix in OpenGL compatible format
             *
             * @return abyss::math::Matrix
             */
            abyss::math::Matrix OpenGL() const;
        };
    }
} // namespace abyss

#endif /* MATRIX */
