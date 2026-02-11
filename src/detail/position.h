#pragma once

namespace pos {

struct Vec2D {
    Vec2D() = default;
    Vec2D(double x, double y)
        : x(x)
        , y(y) {
    }

    Vec2D& operator*=(double scale) {
        x *= scale;
        y *= scale;
        return *this;
    }

    auto operator<=>(const Vec2D&) const = default;

    double x = 0;
    double y = 0;
};

inline Vec2D operator*(Vec2D lhs, double rhs) {
    return lhs *= rhs;
}

inline Vec2D operator*(double lhs, Vec2D rhs) {
    return rhs *= lhs;
}

struct Coordinate {
    Coordinate() = default;
    Coordinate(double x, double y)
        : x(x)
        , y(y) {
    }

    Coordinate& operator+=(const Vec2D& rhs) {
        x += rhs.x;
        y += rhs.y;
        return *this;
    }

    auto operator<=>(const Coordinate&) const = default;

    double x = 0;
    double y = 0;
};

inline Coordinate operator+(Coordinate lhs, const Vec2D& rhs) {
    return lhs += rhs;
}

inline Coordinate operator+(const Vec2D& lhs, Coordinate rhs) {
    return rhs += lhs;
}

struct Velocity {
    double vx;
    double vy;
};

enum class Direction {
    NORTH,
    SOUTH,
    WEST,
    EAST
};

struct Position {
    Coordinate coordinates_;
    Velocity velocity_;
    Direction direction_;
};

} // namespace pos