#include "scene.h"
#include <stdlib.h>
#include <stdio.h>


#define BODIES 25       // # bodies to initialize scene with


typedef struct scene {
    List *bodies;
    List *forces;
} Scene;

typedef struct force {
    ForceCreator forcer;
    void *aux;
    List *bodies;
    FreeFunc freer;     // Frees aux
} Force;


void force_free(Force *f) {
    f->freer(f->aux);
    list_free(f->bodies);
    free(f);
}

Scene *scene_init() {
    Scene *s = malloc(sizeof(Scene));
    List *bodies = list_init(BODIES, (FreeFunc)body_free);
    List *forces = list_init(BODIES, (FreeFunc)force_free);
    s->bodies = bodies;
    s->forces = forces;
    return s;
}

void scene_free(Scene *scene) {
    list_free(scene->bodies);
    list_free(scene->forces);
    free(scene);
}

size_t scene_bodies(Scene *scene) {
    return list_size(scene->bodies);
}

Body *scene_get_body(Scene *scene, size_t index) {
    return ((Body *)list_get(scene->bodies, index));
}

void scene_add_body(Scene *scene, Body *body) {
    list_add(scene->bodies, body);
}

void scene_remove_body(Scene *scene, size_t index) {
    Body *b = list_remove(scene->bodies, index);
    body_free(b);
}

void scene_add_force_creator(Scene *scene, ForceCreator forcer, void *aux,
                                                            FreeFunc freer) {
    scene_add_bodies_force_creator(scene, forcer, aux, NULL, freer);
}

void scene_add_bodies_force_creator(Scene *scene, ForceCreator forcer,
                                    void *aux, List *bodies, FreeFunc freer) {
    Force *f = malloc(sizeof(Force));
    *f = (Force){forcer, aux, bodies, freer};
    list_add(scene->forces, f);
}

void scene_tick(Scene *scene, double dt) {
    // Apply all forces
    for (size_t i = 0; i < list_size(scene->forces); i++) {
        Force *f = (Force *)list_get(scene->forces, i);

        // Only apply force if all related bodies exist
        bool bodies_exist = true;
        for (size_t j = 0; j < list_size(f->bodies); j++) {
            Body *b = (Body *)list_get(f->bodies, j);
            if (body_is_removed(b)) {
                bodies_exist = false;
                break;
            }
        }
        if (bodies_exist) {
            f->forcer(f->bodies, f->aux);
        }
    }

    // Remove force creators acting on bodies marked for removal
    size_t i = 0;
    while (i < list_size(scene->forces)) {
        Force *f = list_get(scene->forces, i);
        bool force_removed = false;
        for (size_t j = 0; j < list_size(f->bodies); j++) {
            Body *b = (Body *)list_get(f->bodies, j);
            if (body_is_removed(b)) {
                force_removed = true;
                break;
            }
        }
        if (force_removed) {
            list_remove(scene->forces, i);
            force_free(f);
        }
        else {
            i++;
        }
    }

    // Remove bodies marked for removal and tick each body
    i = 0;
    while (i < scene_bodies(scene)) {
        Body *b = scene_get_body(scene, i);
        if (body_is_removed(b)) {
            scene_remove_body(scene, i);
        }
        else {
            body_tick(b, dt);
            i++;
        }
    }
}
