/* Types $82 / $88 / $89: spider platform along its anchor. (functions shared with the other enemy files) */
#ifndef ENEMIES_SPIDER_PLATFORM_H
#define ENEMIES_SPIDER_PLATFORM_H
#include <stdint.h>
#include <stdio.h>

typedef struct Game Game;

void ai_hopper(Game *g, int x);
void ai_spider_anchor(Game *g, int x);
void ai_spider_platform(Game *g, int x);

#endif
