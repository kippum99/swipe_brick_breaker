#include "collision.h"
#include <math.h>
#include <stdlib.h>
#include <stdio.h>

double min(double x, double y) {
    return (x < y) ? x : y;
}

double max(double x, double y) {
    return (x > y) ? x : y;
}

/**
 * Finds the interval that a shape takes up when projected onto a vector and
 * returns that interval as a vector (min, max).
 *
 * @param v, unit vector to be projected onto
 * @param shape, shape to be projected
 * @return a vector containing the (min, max) of the interval shape takes up
 */
Vector find_polygon_projection(Vector v, List *shape) {
    double minimum, maximum;
    for (int i = 0; i < list_size(shape); i++) {
        double projection = vec_dot(v_cast(list_get(shape, i)), v);
        if (i == 0) {
            minimum = projection;
            maximum = projection;
        }
        else {
            if (projection < minimum) {
                minimum = projection;
            }
            if (projection > maximum) {
                maximum = projection;
            }
        }
    }
    return (Vector) {minimum, maximum};
}

/**
 * Helper function that computes a unit vector perpendicular to the input
 *
 * @param a vector
 * @return a unit vector perpendicular to the input
 */
Vector find_perpendicular_vector(Vector v) {
    Vector p = (Vector) {v.y, -1.0 * v.x};
    p = vec_divide(vec_norm(p), p);
    return p;
}

/**
 * Returns the total area of overlap between two intervals given they overlap
 *
 * @param v1 a (min, max) interval as a vector
 * @param v2 a (min, max) interval as a vector
 * @return the amount of overlap as a double
 */
double overlap_interval(Vector v1, Vector v2) {
    double biggest_min = max(v1.x, v2.x);
    double smallest_max = min(v1.y, v2.y);
    return smallest_max - biggest_min;
}

/**
 * Checks whether two intervals overlap
 *
 * @param v, vector to be projected onto
 * @param shape1, list of vectors
 * @param shape2, list of vectors
 * @return amount of overlap, or 0.0 if no overlap, as a double
 */
double check_overlap(Vector v, List *shape1, List *shape2) {
    Vector v1 = find_polygon_projection(v, shape1);
    Vector v2 = find_polygon_projection(v, shape2);
    if ((v2.y > v1.y && v2.x > v1.y) || (v2.x < v1.x && v2.y < v1.x)) {
        return 0.0;
    }
    return overlap_interval(v1, v2);
}

/**
 * Checks whether for each perpendicular edge in shape1, the projections of
 * shape1 and shape2 overlap with that perpendicular edge
 *
 * @param shape1, list of vectors
 * @param shape2, list of vectors
 * @return the amount of interval overlap as a double
 */
CollisionInfo check_collisions(List *shape1, List *shape2, double *overlap) {
    int size = list_size(shape1);
    CollisionInfo check = (CollisionInfo) {false, VEC_ZERO};

    for (int i = 0; i < size; i++) {
        Vector v1 = v_cast(list_get(shape1, i));
        Vector v2 = v_cast(list_get(shape1, (i + 1) % size));
        Vector diff = vec_subtract(v2, v1);

        Vector p = find_perpendicular_vector(diff);
        double interval = check_overlap(p, shape1, shape2);

        if (interval == 0.0) {
            return (CollisionInfo) {false, VEC_ZERO};
        }
        else if (interval < *overlap) {
            *overlap = interval;
            check.collided = true;
            check.axis = p;
        }
    }
    return check;
}

/**
 * Checks if two polygons collide using Separating Axis (checks whether there
 * exists a separating axis between two polygons).
 *
 * @param shape1, list of vectors
 * @param shape2, list of vectors
 * @return CollisionInfo including whether the shapes are colliding and the axis
 *      they're colliding on.
 */
CollisionInfo find_collision(List *shape1, List *shape2) {
    double *overlap1 = (double *)malloc(sizeof(double));
    double *overlap2 = (double *)malloc(sizeof(double));
    *overlap1 = INFINITY;
    *overlap2 = INFINITY;
    CollisionInfo check1 = check_collisions(shape1, shape2, overlap1);
    CollisionInfo check2 = check_collisions(shape2, shape1, overlap2);
    CollisionInfo check = (CollisionInfo) {false, VEC_ZERO};
    if (check1.collided && check2.collided && *overlap1 <= *overlap2) {
        check =  check1;
    }
    else if (check1.collided && check2.collided && *overlap1 > *overlap2) {
        check =  check2;
    }
    free(overlap1);
    free(overlap2);
    return check;
}
