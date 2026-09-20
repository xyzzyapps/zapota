/**
 * @file demo_box2d.c
 * @brief Box2D 3.0 Physics Engine Demonstration.
 */

#include <stdio.h>
#include "box2d/box2d.h"

int main(void) {
    printf("====================================================\n");
    printf("Box2D 3.0 Rigid Body Physics Demonstration\n");
    printf("====================================================\n");

    b2WorldDef worldDef = b2DefaultWorldDef();
    worldDef.gravity = (b2Vec2){ 0.0f, -9.8f };

    b2WorldId worldId = b2CreateWorld(&worldDef);
    if (!b2World_IsValid(worldId)) {
        fprintf(stderr, "Failed to create Box2D world\n");
        return 1;
    }
    printf("[1] Box2D world initialized with gravity (0, -9.8)\n");

    // Create ground body
    b2BodyDef groundDef = b2DefaultBodyDef();
    groundDef.position = (b2Vec2){ 0.0f, 0.0f };
    b2BodyId groundId = b2CreateBody(worldId, &groundDef);

    b2Polygon groundBox = b2MakeBox(50.0f, 10.0f);
    b2ShapeDef groundShapeDef = b2DefaultShapeDef();
    b2CreatePolygonShape(groundId, &groundShapeDef, &groundBox);

    // Create dynamic falling body
    b2BodyDef bodyDef = b2DefaultBodyDef();
    bodyDef.type = b2_dynamicBody;
    bodyDef.position = (b2Vec2){ 0.0f, 20.0f };
    b2BodyId bodyId = b2CreateBody(worldId, &bodyDef);

    b2Polygon dynamicBox = b2MakeBox(1.0f, 1.0f);
    b2ShapeDef shapeDef = b2DefaultShapeDef();
    shapeDef.density = 1.0f;
    shapeDef.material.friction = 0.3f;
    b2CreatePolygonShape(bodyId, &shapeDef, &dynamicBox);

    printf("[2] Created ground body and dynamic falling box at (0.0, 20.0)\n");
    printf("[3] Stepping simulation (10 frames at 1/60s):\n");

    float timeStep = 1.0f / 60.0f;
    int subStepCount = 4;

    for (int i = 0; i < 10; ++i) {
        b2World_Step(worldId, timeStep, subStepCount);
        b2Vec2 position = b2Body_GetPosition(bodyId);
        b2Rot rotation = b2Body_GetRotation(bodyId);
        printf("  Step %2d -> Box Pos: (%.3f, %.3f), Angle: %.3f rad\n",
               i + 1, position.x, position.y, b2Rot_GetAngle(rotation));
    }

    b2DestroyWorld(worldId);
    printf("\nBox2D world destroyed cleanly.\n");
    return 0;
}
