#pragma once

#include <cmath>
#include <algorithm>

/**
 * Simple 3D vector implementation.
 * https://ultralig.ht/api/cpp/1_3_0/structultralight_1_1vec3.html
 * Used for positions, directions, and other 3D vector math.
 */
struct Vec3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    
    // Constructors
    Vec3() = default;
    Vec3(float x, float y, float z) : x(x), y(y), z(z) {}
    explicit Vec3(float value) : x(value), y(value), z(value) {}
    
    // Vector operations
    Vec3 operator+(const Vec3& other) const {
        return Vec3(x + other.x, y + other.y, z + other.z);
    }
    
    Vec3 operator-(const Vec3& other) const {
        return Vec3(x - other.x, y - other.y, z - other.z);
    }
    
    Vec3 operator*(float scalar) const {
        return Vec3(x * scalar, y * scalar, z * scalar);
    }
    
    Vec3 operator/(float scalar) const {
        return Vec3(x / scalar, y / scalar, z / scalar);
    }
    
    Vec3& operator+=(const Vec3& other) {
        x += other.x;
        y += other.y;
        z += other.z;
        return *this;
    }
    
    Vec3& operator-=(const Vec3& other) {
        x -= other.x;
        y -= other.y;
        z -= other.z;
        return *this;
    }
    
    Vec3& operator*=(float scalar) {
        x *= scalar;
        y *= scalar;
        z *= scalar;
        return *this;
    }
    
    Vec3& operator/=(float scalar) {
        x /= scalar;
        y /= scalar;
        z /= scalar;
        return *this;
    }
    
    // Dot product
    float dot(const Vec3& other) const {
        return x * other.x + y * other.y + z * other.z;
    }
    
    // Cross product
    Vec3 cross(const Vec3& other) const {
        return Vec3(
            y * other.z - z * other.y,
            z * other.x - x * other.z,
            x * other.y - y * other.x
        );
    }
    
    // Length/magnitude
    float length() const {
        return std::sqrt(x * x + y * y + z * z);
    }
    
    float length_squared() const {
        return x * x + y * y + z * z;
    }
    
    // Normalize (in-place)
    Vec3& normalize() {
        float len = length();
        if (len > 0.0f) {
            x /= len;
            y /= len;
            z /= len;
        }
        return *this;
    }
    
    // Normalized copy
    Vec3 normalized() const {
        Vec3 copy = *this;
        copy.normalize();
        return copy;
    }
    
    // Distance to another point
    float distance_to(const Vec3& other) const {
        return (*this - other).length();
    }
    
    float distance_squared_to(const Vec3& other) const {
        return (*this - other).length_squared();
    }
    
    // Comparison
    bool operator==(const Vec3& other) const {
        return x == other.x && y == other.y && z == other.z;
    }
    
    bool operator!=(const Vec3& other) const {
        return !(*this == other);
    }
};

// Scalar multiplication (left operand)
inline Vec3 operator*(float scalar, const Vec3& vec) {
    return vec * scalar;
}
