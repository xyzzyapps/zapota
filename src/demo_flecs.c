/**
 * @file demo_flecs.c
 * @brief Flecs Entity Component System (ECS) Demonstration.
 */

#include <stdio.h>
#include "flecs.h"

typedef struct {
    float x;
    float y;
} Position;

typedef struct {
    float dx;
    float dy;
} Velocity;

void MoveSystem(ecs_iter_t *it) {
    Position *p = ecs_field(it, Position, 0);
    const Velocity *v = ecs_field(it, Velocity, 1);

    for (int i = 0; i < it->count; i++) {
        p[i].x += v[i].dx;
        p[i].y += v[i].dy;
        printf("  Entity moved -> Pos: (%.1f, %.1f)\n", p[i].x, p[i].y);
    }
}

int main(void) {
    printf("====================================================\n");
    printf("Flecs Entity Component System (ECS) Demonstration\n");
    printf("====================================================\n");

    ecs_world_t *world = ecs_init();

    ECS_COMPONENT(world, Position);
    ECS_COMPONENT(world, Velocity);

    ECS_SYSTEM(world, MoveSystem, EcsOnUpdate, Position, [in] Velocity);

    // Create Entities
    ecs_entity_t e1 = ecs_new(world);
    ecs_set(world, e1, Position, { 10.0f, 20.0f });
    ecs_set(world, e1, Velocity, { 1.5f, 2.5f });

    ecs_entity_t e2 = ecs_new(world);
    ecs_set(world, e2, Position, { 100.0f, 200.0f });
    ecs_set(world, e2, Velocity, { -5.0f, 10.0f });

    printf("[1] Initialized Flecs world with 2 entities\n");
    printf("[2] Stepping ECS simulation frame 1:\n");
    ecs_progress(world, 0.016f);

    printf("[3] Stepping ECS simulation frame 2:\n");
    ecs_progress(world, 0.016f);

    ecs_fini(world);
    printf("\nFlecs ECS world destroyed cleanly.\n");
    return 0;
}
