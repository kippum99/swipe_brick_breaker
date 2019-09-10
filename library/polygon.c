#include "polygon.h"
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

double polygon_area(List *polygon) {
    double area = 0;
    size_t size = list_size(polygon);
    for(size_t i = 0; i < size; i++) {
        Vector v1 = v_cast(list_get(polygon, i));
        /* If index is out of bounds, wrap around to access index zero */
        Vector v2 = v_cast(list_get(polygon, (i + 1) % size));
        area += vec_cross(v1, v2);
    }
    area = fabs(area) * (1.0 / 2.0);
    return area;
}

Vector polygon_centroid(List *polygon) {
    Vector centroid = {0.0, 0.0};
    size_t size = list_size(polygon);
    for(size_t i = 0; i < size; i++) {
        Vector v1 = v_cast(list_get(polygon, i));
        Vector v2 = v_cast(list_get(polygon, (i + 1) % size));
        Vector v3 = {v1.x + v2.x, v1.y + v2.y};
        double cross_product = vec_cross(v1, v2);
        v3 = vec_multiply(cross_product, v3);
        centroid = vec_add(centroid, v3);
    }
    centroid = vec_divide(6 * polygon_area(polygon), centroid);
    return centroid;
}

void polygon_translate(List *polygon, Vector translation) {
    for(size_t i = 0; i < list_size(polygon); i++) {
        Vector *v = malloc(sizeof(Vector));
        *v = vec_add(v_cast(list_get(polygon, i)), translation);
        list_set(polygon, i, v);
    }
}

void polygon_rotate(List *polygon, double angle, Vector point) {
    size_t n = list_size(polygon);
    polygon_translate(polygon, vec_negate(point));
    for (size_t i = 0; i < n; i++) {
        Vector *v = malloc(sizeof(Vector));
        *v = vec_rotate(v_cast(list_get(polygon, i)), angle);
        list_set(polygon, i, v);
    }
    polygon_translate(polygon, point);
}
