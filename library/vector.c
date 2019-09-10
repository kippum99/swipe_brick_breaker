#include "vector.h"
#include <math.h>
#include <stdlib.h>

const Vector VEC_ZERO = {0.0, 0.0};

Vector v_cast(void *p) {
    return *((Vector *)p);
}

Vector *vp_init(Vector v) {
    Vector *vp = malloc(sizeof(Vector));
    *vp = v;
    return vp;
}

Vector vec_add(Vector v1, Vector v2) {
    Vector v = {v1.x + v2.x, v1.y + v2.y};
    return v;
}

Vector vec_subtract(Vector v1, Vector v2) {
    Vector v = {v1.x - v2.x, v1.y - v2.y};
    return v;
}

Vector vec_negate(Vector v) {
    Vector x = {-v.x, -v.y};
    return x;
}

Vector vec_multiply(double scalar, Vector v) {
    Vector x = {scalar * v.x, scalar * v.y};
    return x;
}

Vector vec_divide(double scalar, Vector v) {
    Vector x = {v.x / scalar, v.y / scalar};
    return x;
}

double vec_dot(Vector v1, Vector v2) {
    return (v1.x * v2.x) + (v1.y * v2.y);
}

double vec_cross(Vector v1, Vector v2) {
    return (v1.x * v2.y) - (v2.x * v1.y);
}

double vec_norm(Vector v) {
    double norm = sqrt(pow(v.x, 2.0) + pow(v.y, 2.0));
    return norm;
}

Vector vec_rotate(Vector v, double angle) {
    double x = v.x * cos(angle) - v.y * sin(angle);
    double y = v.x * sin(angle) + v.y * cos(angle);
    Vector z = {x, y};
    return z;
}

bool vec_equal(Vector v1, Vector v2) {
    return (v1.x == v2.x && v1.y == v2.y);
}
