/* Types $A7 / $A8: goes back and forth on a platform and fires. (functions shared with the other enemy files) */
#ifndef ENEMIES_PLATFORM_SHOOTER_H
#define ENEMIES_PLATFORM_SHOOTER_H
#include <stdint.h>
#include <stdio.h>

typedef struct Game Game;

void ai_platform_shooter(Game *g, int x, uint8_t first);
void platform_shooter_state(Game *g, int x);

#endif
