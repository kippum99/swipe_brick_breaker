#include "sdl_wrapper.h"
#include "scene.h"
#include "forces.h"

#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <time.h>
#include <string.h>

#define WIDTH 100.0     // width of game screen
#define HEIGHT 100.0

#define WHITE ((RGBColor) {1, 1, 1})
#define BACKGROUND_COLOR ((RGBColor) {49.0 / 255, 56.0 / 255, 72.0 / 255})
#define BALL_COLOR ((RGBColor) {138.0 / 255, 223.0 / 255, 220.0 / 255})

// Full health brick color
#define BRICK_COLOR ((RGBColor) {115.0 / 255, 83.0 / 255, 114.0 / 255})

// Collectible ball color
#define C_BALL_COLOR ((RGBColor) {252.0 / 255, 81.0 / 255, 133.0 / 255})

#define LIFE_COLOR ((RGBColor) {92.0 / 255, 214.0 / 255, 92.0 / 255})
#define BOMB_COLOR ((RGBColor) {1.0, 0.3, 0.3})
#define WALL_COLOR ((RGBColor) {0, 0, 0})
#define TRAJ_COLOR ((RGBColor) {0.6, 0.9, 0.9})
#define WALL_WIDTH 3 * WIDTH
#define TRAJ_WIDTH 0.5
#define TRAJ_HEIGHT HEIGHT * 2
#define TRAJ_INCREMENT M_PI / 200 // Change in angle with arrow keys
#define TRAJ_LIMIT M_PI / 20      // Limit angle trajectory can aim

#define N_ROWS 3    // # rows of bricks
#define N_COLS 6    // # cols of bricks
#define BRICK_SPACING 1
#define BRICK_TOTAL_WIDTH (WIDTH / N_COLS)  // Includes margin
#define BRICK_TOTAL_HEIGHT 10
#define BRICK_WIDTH ((WIDTH - ((N_COLS + 1) * BRICK_SPACING)) / N_COLS)
#define BRICK_HEIGHT (BRICK_TOTAL_HEIGHT - BRICK_SPACING)
#define ARROW_HEIGHT 100
#define ARROW_WIDTH 20
#define LIFE_SIZE 6.0               // Width and height of collectibles
#define BOMB_SIZE 4.0

#define SCORE_X 130.0               // Position x, y for rendering score text
#define SCORE_Y 10.0
#define GAMEOVER_X 350.0            // " for rendering game over score text
#define GAMEOVER_Y 200.0
#define GAMEOVER_XNUM GAMEOVER_X + 210.0  // for actual number of score

#define POINTS_IN_CIRCLE 20         // For drawing circles as polygons
#define RADIUS 2.0                  // Radius of ball

#define MASS 5.0                    // Mass of the ball
#define VELOCITY (Vector) {0, 100}  // Velocity of ball (perpendicular to floor)
#define VELOCITY_RIGHT (Vector) {100, 0}   // Velocity of ball being collected
#define PLAYER_SPEED 100.0
#define ELASTICITY 1.0

#define INTERVAL 0.05               // Time interval at which balls are shot

#define LIFE_PROB 0.08              // Probability of generating a life powerup
#define BOMB_PROB 0.04              // Probability of generating a bomb powerup

// game_state.ball_loc when no balls have been collected
#define NULL_BALL_LOC (Vector) {0, 0}


typedef enum {
    BALL,
    TRAJ,
    BRICK,
    C_BALL,
    C_LIFE,     // Collectible life
    C_BOMB,     // Collectible bomb
    LIFE,       // Already collected life (icon on the side)
    DEBRIS,
    WALL,
    MISC,       // All others
} BodyType;

typedef enum {
    READY,
    BOUNCING,
    WAITING
} BallStatus;

typedef struct brick_info {
    size_t level;       // Maximum health of brick
    size_t health;      // Current health of brick
} BrickInfo;

typedef struct body_info {
    BodyType type;
    void *aux;
    FreeFunc aux_free;
} BodyInfo;

typedef struct game_state {
    bool player_enabled;
    int lives;
    List *shoot_balls;
    Vector ball_loc;    // Where balls are collected after bouncing
} GameState;


// Global game state
GameState game_state;

// Function forward declarations
Body *generate_ball();
void add_ball_forces(Scene *s, Body *ball);
void use_life(Scene *s);
void create_collect_collision(Scene *s, Body *c_ball, Body *ball);


/* Frees the BodyInfo struct */
void free_body_info(BodyInfo *info) {
    if (info->aux_free) {
        info->aux_free(info->aux);
    }
    free(info);
}

/* Generates a random number between 0 and 1 */
double rand_num() {
    return (double)rand() / RAND_MAX;
}

/* Generates a random integer between begin (inclusive) and end (exclusive). */
int rand_int(int begin, int end) {
    int num = begin + floor(rand_num() * (end - begin));
    assert(num >= begin && num < end);
    return num;
}

/* Randomly choose which columns to put new bricks in the new row and returns
 * an array of boolean values with size N_COLS indicating which columns will
 * have bricks.
 */
void choose_cols(bool *cols) {
    int count = 0;
    for (size_t i = 0; i < N_COLS; i++) {
        if (rand_num() < 0.3) {
            cols[i] = 1;
            count += 1;
        }
        else {
            cols[i] = 0;
        }
    }
    if (count == 0) {
        cols[0] = 1;
    }
}

/** Constructs a rectangle with the given dimensions centered at (0, 0) */
List *rect_init(double width, double height) {
    Vector half_width  = {.x = width / 2, .y = 0.0},
           half_height = {.x = 0.0, .y = height / 2};
    List *rect = list_init(4, free);
    Vector *v = malloc(sizeof(*v));
    *v = vec_add(half_width, half_height);
    list_add(rect, v);
    v = malloc(sizeof(*v));
    *v = vec_subtract(half_height, half_width);
    list_add(rect, v);
    v = malloc(sizeof(*v));
    *v = vec_negate(*(Vector *) list_get(rect, 0));
    list_add(rect, v);
    v = malloc(sizeof(*v));
    *v = vec_subtract(half_width, half_height);
    list_add(rect, v);
    return rect;
}

/** Constructs a circle with the given dimensions centered at (0, 0) */
List *circle_init(double radius) {
    List *shape = list_init(POINTS_IN_CIRCLE, free);
    for (int i = 0; i < POINTS_IN_CIRCLE; i++) {
        double angle = 2.0 * M_PI * i / POINTS_IN_CIRCLE;
        Vector p = {radius * cos(angle), radius * sin(angle)};
        list_add(shape, vp_init(p));
    }
    return shape;
}

/* Returns the body's body type. */
BodyType get_body_type(Body *body) {
    BodyInfo *info = body_get_info(body);
    return info->type;
}

/* Returns the current status of the ball. */
BallStatus get_ball_status(Body *ball) {
    BodyInfo *info = body_get_info(ball);
    BallStatus *status = info->aux;
    return *status;
}

/* Changes ball status in ball's BodyInfo. */
void set_ball_status(Body *ball, BallStatus status) {
    BodyInfo *info = body_get_info(ball);
    assert(info->type == BALL);
    BallStatus *curr_status = info->aux;
    *curr_status = status;
}

/* Return ball.
 * This function relies on the initialization of the scene adding ball as the
 * third body to its list of bodies.
 */
Body *get_ball(Scene *s) {
    return scene_get_body(s, 2);
}

/* Return trajectory.
 * This function relies on the initialization of the scene adding trajectory as
 * the second body to its list of bodies.
 */
Body *get_trajectory(Scene *s) {
    return scene_get_body(s, 1);
}

/* Animates destruction of a brick. */
void animate_destruction(Scene *s, Body *brick) {
    double h = BRICK_HEIGHT;
    double w = BRICK_WIDTH;

    List *l = list_init(13, free);
    List *r = list_init(16, free);

    // 13 pts for the left half
    list_add(l, vp_init((Vector) {-8.0/16.0 * w, -5.0/10.0 * h}));
    list_add(l, vp_init((Vector) {-10.0/16.0 * w, 2.0/10.0 * h}));
    list_add(l, vp_init((Vector) {-7.0/16.0 * w, 4.0/10.0 * h}));
    list_add(l, vp_init((Vector) {-3.0/16.0 * w, 5.0/10.0 * h}));
    list_add(l, vp_init((Vector) {0, 6.0/10.0 * h }));
    list_add(l, vp_init((Vector) {1.0/16.0 * w, 3.0/10.0 * h}));
    list_add(l, vp_init((Vector) {2.0/16.0 * w, 2.0/10.0 * h}));
    list_add(l, vp_init((Vector) {1.0/16.0 * w, 1.0/10.0 * h}));
    list_add(l, vp_init((Vector) {2.0/16.0 * w, 0}));
    list_add(l, vp_init((Vector) {1.0/16.0 * w, -1.0/10.0 * h}));
    list_add(l, vp_init((Vector) {2.0/16.0 * w, -2.0/10.0 * h}));
    list_add(l, vp_init((Vector) {-1.0/16.0 * w, -3.0/10.0 * h}));
    list_add(l, vp_init((Vector) {-5.0/16.0 * w, -4.0/10.0 * h}));

    // 16 pts for the right half
    list_add(r, vp_init((Vector) {3.0/16.0 * w, 6.0/10.0 * h}));
    list_add(r, vp_init((Vector) {5.0/16.0 * w, 6.0/10.0 * h}));
    list_add(r, vp_init((Vector) {6.0/16.0 * w, 5.0/10.0 * h}));
    list_add(r, vp_init((Vector) {8.0/16.0 * w, 5.0/10.0 * h}));
    list_add(r, vp_init((Vector) {9.0/16.0 * w, 4.0/10.0 * h}));
    list_add(r, vp_init((Vector) {8.0/16.0 * w, 3.0/10.0 * h}));
    list_add(r, vp_init((Vector) {8.0/16.0 * w, 1.0/10.0 * h}));
    list_add(r, vp_init((Vector) {7.0/16.0 * w, 0 }));
    list_add(r, vp_init((Vector) {7.0/16.0 * w, -2.0/10.0 * h}));
    list_add(r, vp_init((Vector) {6.0/16.0 * w, -3.0/10.0 * h}));
    list_add(r, vp_init((Vector) {4.0/16.0 * w, -2.0/10.0 * h}));
    list_add(r, vp_init((Vector) {5.0/16.0 * w, -1.0/10.0 * h}));
    list_add(r, vp_init((Vector) {4.0/16.0 * w, 0}));
    list_add(r, vp_init((Vector) {5.0/16.0 * w, 1.0/10.0 * h}));
    list_add(r, vp_init((Vector) {3.0/16.0 * w, -3.0/10.0 * h}));
    list_add(r, vp_init((Vector) {4.0/16.0 * w, -4.0/10.0 * h}));

    BodyInfo *infol = malloc(sizeof(BodyInfo));
    BodyInfo *infor = malloc(sizeof(BodyInfo));
    *infol = (BodyInfo){DEBRIS, NULL, NULL};
    *infor = (BodyInfo){DEBRIS, NULL, NULL};
    Body *left = body_init_with_info(l, MASS, WHITE, infol,
        (FreeFunc) free_body_info);
    Body *right = body_init_with_info(r, MASS, WHITE, infor,
        (FreeFunc) free_body_info);
    body_set_velocity(left, vec_negate(VELOCITY));
    body_set_velocity(right, vec_negate(VELOCITY));

    Vector brick_loc = body_get_centroid(brick);
    body_set_centroid(left, brick_loc);
    body_set_centroid(right, brick_loc);

    scene_add_body(s, left);
    scene_add_body(s, right);
}

/* Remove all the debris that finished falling. */
void clear_debris(Scene *s) {
    for (size_t i = 0; i < scene_bodies(s); i++) {
        Body *b = scene_get_body(s, i);
        if (get_body_type(b) == DEBRIS && body_get_centroid(b).y < 0) {
            body_remove(b);
        }
    }
}

/* Damages brick that collides with ball, and destroys the brick if it has 0
 * health left. Ball is unaffected. Auxiliary value aux should hold a pointer
 * to scene.
 */
void damage_collision_handler(Body *brick, Body *ball, Vector axis, void *aux) {
    Scene *s = aux;

    BodyInfo *body_info = body_get_info(brick);
    BrickInfo *info = body_info->aux;
    info->health--;

    if (info->health <= 0) {
        animate_destruction(s, brick);
        body_remove(brick);
        return;
    }

    // Reflect changes in brick color
    double brightness = (double)info->health / (double)info->level;
    double r = 1 - (1 - BRICK_COLOR.r) * brightness;
    double g = 1 - (1 - BRICK_COLOR.g) * brightness;
    double b = 1 - (1 - BRICK_COLOR.b) * brightness;
    RGBColor color = {r, g, b};
    body_set_color(brick, color);
}

/* Move life to an extra life on the side of the scene */
void life_collision_handler(Body *life, Body *ball, Vector axis,
                                    void *aux) {
    if (get_body_type(life) == C_LIFE) {
        BodyInfo *info = body_get_info(life);
        *info = (BodyInfo){LIFE, NULL, NULL};

        body_set_velocity(life, (Vector) {0.0, 0.0});
        double x = WIDTH * 1.05;
        double y = HEIGHT - (BRICK_TOTAL_HEIGHT) * game_state.lives;
        body_set_centroid(life, (Vector) {x, y - BRICK_TOTAL_HEIGHT / 2});
        game_state.lives++;
    }
}

/* Bomb powerup destroys all bricks */
void bomb_collision_handler(Body *bomb, Body *ball, Vector axis,
                                    void *aux) {
    Scene *s = aux;
    for (size_t i = 0; i < scene_bodies(s); i++) {
        Body *b = scene_get_body(s, i);
        if (get_body_type(b) == BRICK) {
            animate_destruction(s, b);
            body_remove(b);
        }
    }
    body_remove(bomb);
}

void collect_collision_handler(Body *c_ball, Body *ball, Vector axis, void *aux) {
    if (get_ball_status(c_ball) == WAITING && get_ball_status(ball) == READY) {
        body_set_centroid(c_ball, body_get_centroid(ball));
        body_set_velocity(c_ball, (Vector) {0.0, 0.0});
        set_ball_status(c_ball, READY);
    }
}

/* Convert collectible ball to ball and drop to the floor. Auxiliary value
 * aux should contain a pointer to the scene.
 */
void collectible_collision_handler(Body *c_ball, Body *ball, Vector axis,
                                    void *aux) {
    Scene *s = aux;

    // Convert collectible ball to ball
    Body *new_ball = generate_ball();
    body_set_centroid(new_ball, body_get_centroid(c_ball));
    body_remove(c_ball);
    set_ball_status(new_ball, BOUNCING);
    scene_add_body(s, new_ball);
    add_ball_forces(s, new_ball);

    // Drop to the floor
    body_set_color(new_ball, BALL_COLOR);
    body_set_velocity(new_ball, vec_negate(VELOCITY));
}

/* Creates a damage collision force between brick and ball. */
void create_damage_collision(Scene *s, Body *brick, Body *ball) {
    create_collision(s, brick, ball, damage_collision_handler, s, NULL);
}

/* Creates a collectible (ball) collision force between a collectible ball and
 * a ball.*/
void create_collectible_collision(Scene *s, Body *c_ball, Body *ball) {
    create_collision(s, c_ball, ball, collectible_collision_handler, s, NULL);
}

/* Creates a life collision force between a life collectible and a ball. */
void create_life_collision(Scene *s, Body *life, Body *ball) {
    create_collision(s, life, ball, life_collision_handler, s, NULL);
}

/* Creates a bomb collision force between a bomb collectible and a ball. */
void create_bomb_collision(Scene *s, Body *bomb, Body *ball) {
    create_collision(s, bomb, ball, bomb_collision_handler, s, NULL);
}


/* Creates damage and physics collision forces between ball and all bricks,
 * collectible forces between ball and all collectible items (not ball),
 * and physics collision forces between ball and walls.
 */
void add_ball_forces(Scene *s, Body *ball) {
    for (size_t i = 0; i < scene_bodies(s); i++) {
        Body *b = scene_get_body(s, i);

        switch (get_body_type(b)) {
            case BRICK:
                create_physics_collision(s, ELASTICITY, ball, b);
                create_damage_collision(s, b, ball);
                break;
            case C_BALL:
                if (b != ball) {
                    create_collectible_collision(s, b, ball);
                }
                break;
            case C_LIFE:
                create_life_collision(s, b, ball);
                break;
            case C_BOMB:
                create_bomb_collision(s, b, ball);
                break;
            case WALL:
                create_physics_collision(s, ELASTICITY, ball, b);
                break;
            default:
                break;
        }
    }
}

/* Creates collision forces between a brick and all balls. */
void add_brick_forces(Scene *s, Body *brick) {
    for (size_t i = 0; i < scene_bodies(s); i++) {
        Body *b = scene_get_body(s, i);
        if (get_body_type(b)== BALL) {
            create_physics_collision(s, ELASTICITY, brick, b);
            create_damage_collision(s, brick, b);
        }
    }
}

/* Creates collectible_collision forces between a collectible ball and all
 * balls.
 */
void add_collectible_forces(Scene *s, Body *c_ball) {
    for (size_t i = 0; i < scene_bodies(s); i++) {
        Body *b = scene_get_body(s, i);
        if (get_body_type(b) == BALL) {
            create_collectible_collision(s, c_ball, b);
        }
    }
}

/* Creates life forces between a life and all balls. */
void add_life_forces(Scene *s, Body *life) {
    for (size_t i = 0; i < scene_bodies(s); i++) {
        Body *b = scene_get_body(s, i);
        if (get_body_type(b) == BALL) {
            create_life_collision(s, life, b);
        }
    }
}

/* Creates bomb forces between a life and all balls. */
void add_bomb_forces(Scene *s, Body *bomb) {
    for (size_t i = 0; i < scene_bodies(s); i++) {
        Body *b = scene_get_body(s, i);
        if (get_body_type(b) == BALL) {
            create_bomb_collision(s, bomb, b);
        }
    }
}

/* Generates a ball. */
Body *generate_ball() {
    List *circle = circle_init(RADIUS);

    BallStatus *status = malloc(sizeof(BallStatus));
    assert(status);
    *status = READY;
    BodyInfo *info = malloc(sizeof(BodyInfo));
    assert(info);
    *info = (BodyInfo) {BALL, status, free};

    Body *ball = body_init_with_info(circle, MASS, BALL_COLOR, info,
                    (FreeFunc) free_body_info);
    return ball;
}

/* Generates a collectible life. */
Body *generate_life() {
    // Generate a plus shape
    List *plus = list_init(12, free);
    list_add(plus, vp_init((Vector) {LIFE_SIZE * 2 / 3, 0}));
    list_add(plus, vp_init((Vector) {LIFE_SIZE * 2 / 3, LIFE_SIZE / 3}));
    list_add(plus, vp_init((Vector) {LIFE_SIZE, LIFE_SIZE / 3}));
    list_add(plus, vp_init((Vector) {LIFE_SIZE, LIFE_SIZE * 2 / 3}));
    list_add(plus, vp_init((Vector) {LIFE_SIZE * 2 / 3, LIFE_SIZE * 2 / 3}));
    list_add(plus, vp_init((Vector) {LIFE_SIZE * 2 / 3, LIFE_SIZE}));
    list_add(plus, vp_init((Vector) {LIFE_SIZE / 3, LIFE_SIZE}));
    list_add(plus, vp_init((Vector) {LIFE_SIZE / 3, LIFE_SIZE * 2 / 3}));
    list_add(plus, vp_init((Vector) {0, LIFE_SIZE * 2 / 3}));
    list_add(plus, vp_init((Vector) {0, LIFE_SIZE / 3}));
    list_add(plus, vp_init((Vector) {LIFE_SIZE / 3, LIFE_SIZE / 3}));
    list_add(plus, vp_init((Vector) {LIFE_SIZE / 3, 0}));

    BodyInfo *info = malloc(sizeof(BodyInfo));
    assert(info);
    *info = (BodyInfo){C_LIFE, NULL, NULL};

    Body *ball = body_init_with_info(plus, MASS, LIFE_COLOR, info,
                    (FreeFunc) free_body_info);
    return ball;
}

/**
 * Helper function that generates one of the points of a 5-pointed star.
 *
 * @param radius radius of star
 * @param i the i-th vertex (convex AND concave) of the star
 * @return the point as a Vector*
 */
Vector *get_pt (double radius, int i) {
    Vector *v = vp_init((Vector){
        radius * cos((2.0 * M_PI * i + M_PI / 2) / (10.0)),
        radius * sin((2.0 * M_PI * i + M_PI / 2) / (10.0))});
    return v;
}

/* Generates a 5-pointed star.
 */
 List *generate_star() {
      List *star = list_init(10, free);
      Vector *v;
      for (int i = 0; i < 10; i++) {
         if (i % 2 == 0) {
             v = get_pt(BOMB_SIZE, i);
         }
         else {
             v = get_pt(BOMB_SIZE / 2.0, i);
         }
         list_add(star, v);
     }
     return star;
 }

/* Generates a collectible bomb. */
Body *generate_bomb() {
    // Generate a star
    List *star = generate_star();

    BodyInfo *info = malloc(sizeof(BodyInfo));
    assert(info);
    *info = (BodyInfo) {C_BOMB, NULL, NULL};

    Body *ball = body_init_with_info(star, MASS, BOMB_COLOR, info,
                    (FreeFunc) free_body_info);
    return ball;
}

/* Generates a collectible ball. */
Body *generate_collectible_ball() {
    List *circle = circle_init(RADIUS);

    BodyInfo *info = malloc(sizeof(BodyInfo));
    assert(info);
    *info = (BodyInfo){C_BALL, NULL, NULL};

    Body *ball = body_init_with_info(circle, MASS, C_BALL_COLOR, info,
                    (FreeFunc) free_body_info);
    return ball;
}

/* Generates a brick with given level at given column at the top of the screen.
 */
Body *generate_brick(size_t level, size_t col) {
    assert(level > 0);
    assert(col >= 0 && col < N_COLS);

    List *shape = rect_init(BRICK_WIDTH, BRICK_HEIGHT);

    BrickInfo *brick_info = malloc(sizeof(BrickInfo));
    assert(brick_info);
    BodyInfo *info = malloc(sizeof(BodyInfo));
    assert(info);
    *brick_info = (BrickInfo) {level, level};
    *info = (BodyInfo){BRICK, brick_info, free};

    Body *brick = body_init_with_info(shape, INFINITY, BRICK_COLOR, info,
                    (FreeFunc) free_body_info);

    // Place at the given column at top of screen
    double x = (BRICK_TOTAL_WIDTH / 2) + col * BRICK_TOTAL_WIDTH;
    double y = HEIGHT - BRICK_TOTAL_HEIGHT / 2;
    body_set_centroid(brick, (Vector){x, y});

    return brick;
}

/* Generates a body that shows ball's trajectory starting from the position
 * of the balls. */
Body *init_trajectory(Vector ball_pos) {
    List *shape = rect_init(TRAJ_WIDTH, TRAJ_HEIGHT);
    BodyInfo *info = malloc(sizeof(BodyInfo));
    assert(info);
    *info = (BodyInfo){TRAJ, NULL, NULL};
    Body *traj = body_init_with_info(shape, INFINITY, TRAJ_COLOR, info,
                    (FreeFunc) free_body_info);
    body_set_centroid(traj, vec_add(ball_pos, (Vector) {0, TRAJ_HEIGHT / 2}));

    return traj;
}

/* Add background */
Body *generate_background() {
    List *shape = rect_init(WIDTH, HEIGHT);
    BodyInfo *info = malloc(sizeof(BodyInfo));
    assert(info);
    *info = (BodyInfo){MISC, NULL, NULL};
    Body *background = body_init_with_info(shape, INFINITY, BACKGROUND_COLOR,
                    info, (FreeFunc) free_body_info);
    body_set_centroid(background, (Vector) {WIDTH / 2.0, HEIGHT / 2.0});

    return background;
}

/* Adds a wall of given dimension at centered at position given by centroid to
 * the scene.
 */
void add_wall(Scene *scene, double width, double height, Vector centroid) {
    List *shape = rect_init(width, height);
    polygon_translate(shape, centroid);
    BodyInfo *info = malloc(sizeof(BodyInfo));
    assert(info);
    *info = (BodyInfo) {WALL, NULL, NULL};
    Body *body = body_init_with_info(shape, INFINITY, WALL_COLOR, info,
                    (FreeFunc) free_body_info);
    scene_add_body(scene, body);
}

/* Initializes scene for Swipe Break Breaker game with ball and bricks, and
 * returns the scene.
 */
Scene *generate_scene() {
    Scene *s = scene_init();

    Body *background = generate_background();
    scene_add_body(s, background);

    // Generate a trajectory where ball is
    Body *traj = init_trajectory((Vector){WIDTH / 2.0 , RADIUS});
    scene_add_body(s, traj);

    // Generate a ball at bottom center of screen
    Body *ball = generate_ball();
    body_set_centroid(ball, (Vector){WIDTH / 2.0 , RADIUS});
    scene_add_body(s, ball);

    // Generate walls
    add_wall(s, WIDTH, WALL_WIDTH,
        (Vector) {WIDTH / 2.0, HEIGHT + WALL_WIDTH / 2.0});
    add_wall(s, WALL_WIDTH, HEIGHT,
        (Vector) {-WALL_WIDTH / 2.0, HEIGHT / 2.0});
    add_wall(s, WALL_WIDTH, HEIGHT,
        (Vector) {WIDTH + WALL_WIDTH / 2.0, HEIGHT / 2.0});

    // Generate a brick to start with
    Body *b = generate_brick(1, rand_int(0, N_COLS));
    scene_add_body(s, b);

    add_ball_forces(s, ball);

    return s;
}

void find_shooting_balls(Scene *s) {
    for (size_t i = 0; i < scene_bodies(s); i++) {
        Body *b = scene_get_body(s, i);
        BodyInfo *info = body_get_info(b);
        if (info->type == BALL) {
            list_add(game_state.shoot_balls, b);
        }
    }
}

/* Shoot a ball stored in the game state's shoot_balls list. */
void shoot_ball(Scene *s) {
    Body *ball = list_remove(game_state.shoot_balls,
                            list_size(game_state.shoot_balls) - 1);
    Vector v = vec_rotate(VELOCITY, body_get_orientation(get_trajectory(s)));
    body_set_velocity(ball, v);
    set_ball_status(ball, BOUNCING);
}

/*
 * Key handler that controls ball's trajectory based on arrow keys and shoots
 * with space key.
 */
void on_key(char key, KeyEventType type, double held_time, void *aux) {
    Scene *s = aux;

    // Don't do anything if player is disabled
    if (!game_state.player_enabled) {
        return;
    }

    Body *traj = get_trajectory(s);
    Vector ball_loc = body_get_centroid(get_ball(s));
    double orientation = body_get_orientation(traj);

    switch (key) {
        case LEFT_ARROW:
            if (orientation + TRAJ_LIMIT < M_PI / 2) {
                body_set_rotation(traj, orientation + TRAJ_INCREMENT, ball_loc);
            }
            break;
        case RIGHT_ARROW:
            if (orientation - TRAJ_LIMIT > -M_PI / 2) {
                body_set_rotation(traj, orientation - TRAJ_INCREMENT, ball_loc);
            }
            break;
        case ' ':;
            // Shoot balls and disable player / trajectory.
            find_shooting_balls(s);
            game_state.player_enabled = false;
            body_set_color(traj, BACKGROUND_COLOR);
            break;
        default:
            break;
    }
}

/* Checks if the current round is over (all balls are waiting). */
bool round_over(Scene *s) {
    for (size_t i = 0; i < scene_bodies(s); i++) {
        Body *b = scene_get_body(s, i);
        if (get_body_type(b) == BALL) {
            if (get_ball_status(b) == WAITING) {
                assert(body_is_stationary(b));
            }
            else {
                return false;
            }
        }
    }
    return true;
}

/* Stops a bouncing ball and mark its status as 'WAITING'.
 */
void wait_ball(Body *ball) {
    body_set_velocity(ball, VEC_ZERO);
    set_ball_status(ball, WAITING);
}

/* Checks if balls pass any of the four boundaries of the screen. This is
 * necessary in case wall collision fails to bounce off the ball (with high
 * speed) and to collect balls when it reaches the floor.
 */
void check_boundary(Scene *s) {
    for (size_t i = 0; i < scene_bodies(s); i++) {
        Body *b = scene_get_body(s, i);
        if (get_body_type(b) == BALL) {
            Vector pos = body_get_centroid(b);

            // Check side walls
            if (pos.x < 0) {
                pos.x = 0;
            }
            else if (pos.x > WIDTH) {
                pos.x = WIDTH;
            }

            // Check top wall
            if (pos.y > HEIGHT) {
                pos.y = HEIGHT;
            }
            // Handle floor collision
            else if (pos.y < RADIUS) {
                assert(get_ball_status(b) == BOUNCING);
                pos.y = RADIUS;

                // Needed for edge case when ball collides a wall simultaneously
                body_reset_impulse(b);

                // Collect the ball at ball_loc
                if (vec_equal(game_state.ball_loc, NULL_BALL_LOC)) {
                    // Set ball_loc and wait the ball
                    game_state.ball_loc = pos;
                    wait_ball(b);
                }
                else {
                    // Ball is left of ball_loc
                    if (pos.x < game_state.ball_loc.x) {
                        body_set_velocity(b, VELOCITY_RIGHT);
                    }
                    // Ball is right of ball_loc
                    else if (pos.x > game_state.ball_loc.x) {
                        body_set_velocity(b, vec_negate(VELOCITY_RIGHT));
                    }
                    // Ball is at ball_loc
                    else {
                        wait_ball(b);
                    }
                }
            }
            body_set_centroid(b, pos);
        }
    }
}

/* Checks if any balls being collected reached ball_loc and stops them at
 * ball_loc after marking its status as 'WAITING'.
 */
void finish_collection(Scene *s) {
    for (size_t i = 0; i < scene_bodies(s); i++) {
        Body *b = scene_get_body(s, i);
        if (get_body_type(b) == BALL) {
            Vector v = body_get_velocity(b);
            Vector pos = body_get_centroid(b);
            // Check if ball passed ball_loc
            if ((vec_equal(v, VELOCITY_RIGHT) && pos.x >= game_state.ball_loc.x)
                || (vec_equal(v, vec_negate(VELOCITY_RIGHT))
                    && pos.x <= game_state.ball_loc.x)) {
                body_set_centroid(b, game_state.ball_loc);
                wait_ball(b);
            }
        }
    }
}

/* Prepare for next round.
 * 1) Adds a row of bricks with the given level to the scene.
 *    The row would consist of one to five bricks, decided randomly.
 * 2) Add a new ball to collect at a random place
 * 3) Reset ball_loc and mark all balls as ready.
 * 4) Activate trajectory and enable player.
 *
 * Returns false if game is over because the newly added row touches the ground.
 */
bool next_round(Scene *s, size_t level) {
    bool life_used = false;

    // Shift all existing bricks and collectibles 1 row down
    for (size_t i = 0; i < scene_bodies(s); i++) {
        Body *b = scene_get_body(s, i);
        BodyType type = get_body_type(b);
        if (type == BRICK || type == C_BALL || type == C_LIFE || type == C_BOMB)
        {
            body_set_centroid(b, vec_add(body_get_centroid(b),
                (Vector) {0, -1.0 * BRICK_TOTAL_HEIGHT}));
            if (get_body_type(b) == BRICK &&
                body_get_centroid(b).y < BRICK_TOTAL_HEIGHT) {
                // Use life or game over
                if (game_state.lives > 0) {
                    life_used = true;
                    body_remove(b);
                }
                else {
                    return false;   // Game over
                }
            }
        }
    }

    if (life_used) {
        use_life(s);
    }

    // Add a new row of bricks and populate forces
    bool *cols = malloc(sizeof(bool) * N_COLS);
    choose_cols(cols);
    int powerup_col = rand_int(0, N_COLS);
    assert(0 <= powerup_col && powerup_col < N_COLS);
    cols[powerup_col] = 0;

    for (size_t j = 0; j < N_COLS; j++) {
        if (cols[j]) {
            Body *brick = generate_brick(level, j);
            add_brick_forces(s, brick);
            scene_add_body(s, brick);
        }
    }

    double powerup_probability = rand_num();
    double x = (BRICK_TOTAL_WIDTH / 2) + powerup_col * BRICK_TOTAL_WIDTH;
    double y = HEIGHT - BRICK_TOTAL_HEIGHT / 2;

    if (powerup_probability < LIFE_PROB) {
        // Add one life among new row of bricks
        Body *life = generate_life();
        add_life_forces(s, life);
        body_set_centroid(life, (Vector){x, y});
        scene_add_body(s, life);
    }
    else if (powerup_probability < LIFE_PROB + BOMB_PROB) {
        // Add one bomb among new row of bricks
        Body *bomb = generate_bomb();
        add_bomb_forces(s, bomb);
        body_set_centroid(bomb, (Vector){x, y});
        scene_add_body(s, bomb);
    }
    else {
        // Add one collectible ball among new row of bricks
        Body *c_ball = generate_collectible_ball();
        add_collectible_forces(s, c_ball);
        body_set_centroid(c_ball, (Vector){x, y});
        scene_add_body(s, c_ball);
    }
    free(cols);

    // Reset ball_loc
    game_state.ball_loc = NULL_BALL_LOC;

    // Mark all balls as ready
    for (size_t i = 0; i < scene_bodies(s); i++) {
        Body *b = scene_get_body(s, i);
        if (get_body_type(b) == BALL) {
            assert(get_ball_status(b) == WAITING);
            set_ball_status(b, READY);
        }
    }

    // Activate trajectory where balls are
    body_set_color(get_trajectory(s), TRAJ_COLOR);
    game_state.player_enabled = true;

    return true;
}

/* Places where balls are collected and resets the angle. */
void reset_trajectory(Scene *s) {
    Body *traj = get_trajectory(s);
    Vector ball_loc = body_get_centroid(get_ball(s)); // get first ball
    Vector v = vec_add(ball_loc, (Vector){0, TRAJ_HEIGHT / 2});
    body_set_centroid(traj, v);
    body_set_rotation(traj, 0, ball_loc);
}

/* Use the collected life. Only call this function if there are lives left. */
void use_life(Scene *s) {
    for (int i = scene_bodies(s) - 1; i >= 0; i--) {
        Body *b = scene_get_body(s, i);
        if (get_body_type(b) == LIFE) {
            body_remove(b);
            break;
        }
    }
    game_state.lives--;
}

/* Game over screen */
void game_over(int level) {
    Scene *gameover = scene_init();
    List *shape = rect_init(WALL_WIDTH, WALL_WIDTH);
    BodyInfo *info = malloc(sizeof(BodyInfo));
    assert(info);
    *info = (BodyInfo) {MISC, NULL, NULL};
    Body *background = body_init_with_info(shape, INFINITY, BACKGROUND_COLOR,
                    info, (FreeFunc) free_body_info);
    scene_add_body(gameover, background);

    SDL_Texture *text1, *text2;
    SDL_Rect rect1, rect2;

    List *text = list_init(2, (FreeFunc) SDL_DestroyTexture);
    List *rect = list_init(2, NULL);

    char snum[4];
    sprintf(snum, "%d", level);

    get_text_and_rect(GAMEOVER_X, GAMEOVER_Y, "Game over! Score:",
        &text1, &rect1);
    get_text_and_rect(GAMEOVER_XNUM, GAMEOVER_Y, snum, &text2, &rect2);

    list_add(text, text1);
    list_add(text, text2);
    list_add(rect, &rect1);
    list_add(rect, &rect2);

    while(!sdl_is_done()) {
        sdl_render_scene(gameover, text, rect);
    }

    list_free(text);
    list_free(rect);
    scene_free(gameover);
}

int main(int argc, char const *argv[]) {
    srand(time(NULL));

    Vector min = {0, 0};
    Vector max = {WIDTH, HEIGHT};
    sdl_init(min, max);

    Scene *s = generate_scene();

    // Initialize global game state
    game_state.player_enabled = true;
    game_state.lives = 0;
    game_state.shoot_balls = list_init(1, NULL);
    game_state.ball_loc = NULL_BALL_LOC;

    size_t level = 1;
    double total_time = 0;

    // Show score with sdl_ttf
    SDL_Rect *rect = malloc(sizeof(SDL_Rect));
    List *texts = list_init(1, (FreeFunc) SDL_DestroyTexture);
    List *rects = list_init(1, free);
    list_add(rects, rect);

    sdl_on_key(on_key, s);
    while (!sdl_is_done()) {
        // Game has 3 states:
        // 1) If player's turn, wait for input
        // 2) bounce
        // 3) Add row / check game over

        double dt = time_since_last_tick();
        total_time += dt;

        // Shoot ball if there are ready balls
        if (total_time > INTERVAL && list_size(game_state.shoot_balls) > 0) {
            shoot_ball(s);
            total_time = 0;
        }

        check_boundary(s);
        scene_tick(s, dt);
        finish_collection(s);
        clear_debris(s);

        if (round_over(s)) {
            level++;
            if (!next_round(s, level)) {
                printf("Game over! Score is %zu\n", level);
                game_over(level);
                break;
            }
            reset_trajectory(s);
        }

        // Text rendering (score)
        SDL_Texture *text;
        char snum[12];
        sprintf(snum, "Score: %zu", level);
        get_text_and_rect(SCORE_X, SCORE_Y, snum, &text, rect);
        list_add(texts, text);

        sdl_render_scene(s, texts, rects);

        list_remove(texts, 0);
    }

    list_free(texts);
    list_free(rects);
    scene_free(s);
    list_free(game_state.shoot_balls);

    return 0;
}
