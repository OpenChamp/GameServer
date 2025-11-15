#pragma once

#include <cmath>
#include <algorithm>

/**
 * Simple 2D vector implementation.
 * Used for Navigation and other 2D vector math.
 * https://ultralig.ht/api/cpp/1_3_0/structultralight_1_1vec2.html
 */
struct Vec2 {
    float x = 0.0f;
    float y = 0.0f;
    
    // Constructors
    Vec2() = default;
    Vec2(float x, float y) : x(x), y(y) {}
    explicit Vec2(float value) : x(value), y(value) {}
    
    // Vector operations
    Vec2 operator+(const Vec2& other) const {
        return Vec2(x + other.x, y + other.y);
    }
    
    Vec2 operator-(const Vec2& other) const {
        return Vec2(x - other.x, y - other.y);
    }
    
    Vec2 operator*(float scalar) const {
        return Vec2(x * scalar, y * scalar);
    }
    
    Vec2 operator/(float scalar) const {
        return Vec2(x / scalar, y / scalar);
    }
    
    Vec2& operator+=(const Vec2& other) {
        x += other.x;
        y += other.y;
        return *this;
    }
    
    Vec2& operator-=(const Vec2& other) {
        x -= other.x;
        y -= other.y;
        return *this;
    }
    
    Vec2& operator*=(float scalar) {
        x *= scalar;
        y *= scalar;
        return *this;
    }
    
    Vec2& operator/=(float scalar) {
        x /= scalar;
        y /= scalar;
        return *this;
    }
    
    // Dot product
    float dot(const Vec2& other) const {
        return x * other.x + y * other.y;
    }
    
    // Length/magnitude
    float length() const {
        return std::sqrt(x * x + y * y);
    }
    
    float length_squared() const {
        return x * x + y * y;
    }
    
    // Normalize (in-place)
    Vec2& normalize() {
        float len = length();
        if (len > 0.0f) {
            x /= len;
            y /= len;
        }
        return *this;
    }
    
    // Normalized copy
    Vec2 normalized() const {
        Vec2 copy = *this;
        copy.normalize();
        return copy;
    }
    
    // Distance to another point
    float distance_to(const Vec2& other) const {
        return (*this - other).length();
    }
    
    float distance_squared_to(const Vec2& other) const {
        return (*this - other).length_squared();
    }
    
    // Comparison
    bool operator==(const Vec2& other) const {
        return x == other.x && y == other.y;
    }
    
    bool operator!=(const Vec2& other) const {
        return !(*this == other);
    }
};

// Scalar multiplication (left operand)
inline Vec2 operator*(float scalar, const Vec2& vec) {
    return vec * scalar;
}

/**
 * Simple 3D vector implementation.
 * https://ultralig.ht/api/cpp/1_3_0/structultralight_1_1vec3.html
 * Used for TSCN 3D conversions
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
