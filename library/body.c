#include <stdlib.h>
#include <assert.h>
#include "body.h"

typedef struct body {
    List *shape;
    Vector centroid;
    double mass;
    RGBColor color;
    double orientation;
    Vector velocity;
    Vector force;
    Vector impulse;
    void *info;
    FreeFunc info_freer;
    bool removed;
} Body;

Body *body_init(List *shape, double mass, RGBColor color) {
    return body_init_with_info(shape, mass, color, NULL, NULL);
}

Body *body_init_with_info(List *shape, double mass, RGBColor color, void *info,
                            FreeFunc info_freer)
{
    assert(mass > 0);
    Body *b = malloc(sizeof(Body));
    assert(b != NULL);

    b->shape = shape;
    b->centroid = polygon_centroid(shape);
    b->mass = mass;
    b->color = color;
    b->orientation = 0.0;
    b->velocity = (Vector) {0.0, 0.0};
    b->force = (Vector) {0.0, 0.0};
    b->impulse = (Vector) {0.0, 0.0};
    b->info = info;
    b->info_freer = info_freer;
    b->removed = false;

    return b;
}

void body_free(Body *body) {
    list_free(body->shape);
    body->info_freer(body->info);
    free(body);
}

List *body_get_shape(Body *body) {
    List *pts = list_init(list_size(body->shape), free);
    for (int i = 0; i < list_size(body->shape); i++) {
        Vector *v = vp_init(v_cast(list_get(body->shape, i)));
        list_add(pts, v);
    }
    return pts;
}

Vector body_get_centroid(Body *body) {
    return body->centroid;
}

Vector body_get_velocity(Body *body) {
    return body->velocity;
}

double body_get_mass(Body *body) {
    return body->mass;
}

RGBColor body_get_color(Body *body) {
    return body->color;
}

double body_get_orientation(Body *body) {
    return body->orientation;
}

void *body_get_info(Body *body) {
    return body->info;
}

void body_set_color(Body *body, RGBColor color) {
    body->color = color;
}

void body_set_centroid(Body *body, Vector x) {
    Vector delta = vec_subtract(x, body->centroid);
    polygon_translate(body->shape, delta);
    body->centroid = x;
}

void body_set_velocity(Body *body, Vector v) {
    body->velocity = v;
}

void body_set_rotation(Body *body, double angle, Vector point) {
    double delta = angle - body->orientation;
    body->orientation = angle;
    polygon_rotate(body->shape, delta, point);
}

void body_add_force(Body *body, Vector force) {
    body->force = vec_add(body->force, force);
}

void body_add_impulse(Body *body, Vector impulse) {
    body->impulse = vec_add(body->impulse, impulse);
}

void body_reset_impulse(Body *body) {
    body->impulse = VEC_ZERO;
}

void body_tick(Body *body, double dt) {
    Vector v_old = body->velocity; // Store old value to use when translating

    // Apply force
    Vector accel = vec_divide(body->mass, body->force);
    body->velocity = vec_add(v_old, vec_multiply(dt, accel));

    // Apply impulse
    Vector v_delta = vec_divide(body->mass, body->impulse);
    body->velocity = vec_add(body->velocity, v_delta);

    // Translate body at avg of velocities before and after tick
    Vector v_avg = vec_divide(2, vec_add(v_old, body->velocity));
    Vector displacement = vec_multiply(dt, v_avg);
    body_set_centroid(body, vec_add(body->centroid, displacement));

    // Reset force / impulse
    body->force = body->impulse = VEC_ZERO;
}

void body_remove(Body *body) {
    body->removed = true;
}

bool body_is_removed(Body *body) {
    return body->removed;
}

bool body_is_stationary(Body *body) {
    return (body->velocity.x == 0 && body->velocity.y == 0);
}
