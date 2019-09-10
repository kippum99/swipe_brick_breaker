#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include "forces.h"

#define THRESHOLD 5  // Threshold within which bodies don't experience gravity


/* Auxiliary struct holding a CollisionHandler, whether the collision happened
 * in the previous scene tick, and aux value to be passed into the
 * CollisionHandler.
 */
typedef struct collision_aux {
    CollisionHandler handler;
    bool prev_collided;
    void *aux;
    FreeFunc aux_freer;
} CollisionAux;

/* Frees CollisionAux. */
void collision_aux_free(CollisionAux *collision_aux) {
    FreeFunc aux_freer = collision_aux->aux_freer;
    if (aux_freer) {
        aux_freer(collision_aux->aux);
    }
    free(collision_aux);
}

/* Destroys two bodies that collide.
 */
void destructive_collision_handler(Body *body1, Body *body2, Vector axis,
                                                                void *aux) {
    body_remove(body1);
    body_remove(body2);
}

/* Adds impulse to two bodies that collide.
 */
void physics_collision_handler(Body *body1, Body *body2, Vector axis, void *aux)
{
    double elasticity = *(double *)aux;
    double m1 = body_get_mass(body1);
    double m2 = body_get_mass(body2);
    double u1 = vec_dot(body_get_velocity(body1), axis);
    double u2 = vec_dot(body_get_velocity(body2), axis);

    double reduced_mass;
    if (m1 == INFINITY) {
        reduced_mass = m2;
    }
    else if (m2 == INFINITY) {
        reduced_mass = m1;
    }
    else {
        reduced_mass = m1 * m2 / (m1 + m2);
    }

    double impulse_scalar = reduced_mass * (1 + elasticity) * (u2 - u1);
    Vector impulse = vec_multiply(impulse_scalar, axis);
    body_add_impulse(body1, impulse);
    body_add_impulse(body2, vec_negate(impulse));
}

/* Takes a list of two bodies and an auxiliary value holding a gravitational
 * constant, and adds gravitational force between the two bodies.
 */
void gravity_forcer(List *bodies, void *aux) {
    double G = *(double *)aux;
    Body *b1 = list_get(bodies, 0);
    Body *b2 = list_get(bodies, 1);
    Vector r = vec_subtract(body_get_centroid(b2), body_get_centroid(b1));

    // Don't apply force if bodies are too close (< THRESHOLD)
    if (vec_norm(r) > THRESHOLD) {
        double scalar = G * body_get_mass(b1) * body_get_mass(b2)
                        / pow(vec_norm(r), 3.0);
        Vector force = vec_multiply(scalar, r);
        body_add_force(b1, force);
        body_add_force(b2, vec_negate(force));
    }
}

/* Takes a list of two bodies and an auxiliary value holding a spring
 * constant, and adds spring force between the two bodies.
 */
void spring_forcer(List *bodies, void *aux) {
    double k = *((double *) aux);
    Body *b1 = list_get(bodies, 0);
    Body *b2 = list_get(bodies, 1);
    Vector dist = vec_subtract(body_get_centroid(b2), body_get_centroid(b1));
    Vector force = vec_multiply(k, dist);
    body_add_force(b1, force);
    body_add_force(b2, vec_negate(force));
}

/* Takes a list holding a body and an auxiliary value holding a drag
 * constant, and adds drag force to body.
 */
void drag_forcer(List *bodies, void *aux) {
    double gamma = *((double *) aux);
    Body *b = list_get(bodies, 0);
    Vector force = vec_multiply(gamma, vec_negate(body_get_velocity(b)));
    body_add_force(b, force);
}

/* Takes a list of two bodies and an auxiliary value holding a CollisionHandler
 * and calls the given CollisionHandler on the bodies.
 */
void collision_forcer(List *bodies, void *aux) {
    Body *b1 = list_get(bodies, 0);
    Body *b2 = list_get(bodies, 1);
    List *shape1 = body_get_shape(b1);
    List *shape2 = body_get_shape(b2);
    CollisionInfo info = find_collision(shape1, shape2);
    list_free(shape1);
    list_free(shape2);
    CollisionAux *collision_aux = (CollisionAux *)aux;
    if (info.collided && !collision_aux->prev_collided) {
        collision_aux->handler(b1, b2, info.axis, collision_aux->aux);
    }
    collision_aux->prev_collided = info.collided;
}

void create_newtonian_gravity(Scene *scene, double G, Body *body1, Body *body2) {
    double *G_ptr = malloc(sizeof(double));
    *G_ptr = G;
    List *bodies = list_init(2, NULL);
    list_add(bodies, body1);
    list_add(bodies, body2);
    scene_add_bodies_force_creator(scene, gravity_forcer, G_ptr, bodies, free);
}

void create_spring(Scene *scene, double k, Body *body1, Body *body2) {
    double *k_ptr = malloc(sizeof(double));
    *k_ptr = k;
    List *bodies = list_init(2, NULL);
    list_add(bodies, body1);
    list_add(bodies, body2);
    scene_add_bodies_force_creator(scene, spring_forcer, k_ptr, bodies, free);
}

void create_drag(Scene *scene, double gamma, Body *body) {
    double *gamma_ptr = malloc(sizeof(double));
    *gamma_ptr = gamma;
    List *bodies = list_init(1, NULL);
    list_add(bodies, body);
    scene_add_bodies_force_creator(scene, drag_forcer, gamma_ptr, bodies, free);
}

void create_collision(Scene *scene, Body *body1, Body *body2,
                        CollisionHandler handler, void *aux, FreeFunc freer) {
    List *bodies = list_init(2, NULL);
    list_add(bodies, body1);
    list_add(bodies, body2);
    CollisionAux *collision_aux = malloc(sizeof(CollisionAux));
    *collision_aux = (CollisionAux){handler, false, aux, freer};
    scene_add_bodies_force_creator(scene, collision_forcer, collision_aux,
        bodies, (FreeFunc)collision_aux_free);
}

void create_destructive_collision(Scene *scene, Body *body1, Body *body2) {
    create_collision(scene, body1, body2, destructive_collision_handler,
        NULL, NULL);
}

void create_physics_collision(Scene *scene, double elasticity, Body *body1,
                                Body *body2) {
    double *elasticity_ptr = malloc(sizeof(double));
    *elasticity_ptr = elasticity;
    create_collision(scene, body1, body2, physics_collision_handler,
        elasticity_ptr, free);
}
