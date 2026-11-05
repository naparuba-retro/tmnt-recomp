/* Flags of the fixes, see fixes.h. */
#include "fixes.h"

static int enabled[FIX_COUNT];

int fix_enabled(FixId id) {
    return enabled[id];
}

void fix_set(FixId id, int on) {
    enabled[id] = on != 0;
}

void fix_set_all(int on) {
    for (int i = 0; i < FIX_COUNT; i++) {
        enabled[i] = on != 0;
    }
}

int fix_any_enabled(void) {
    for (int i = 0; i < FIX_COUNT; i++) {
        if (enabled[i]) {
            return 1;
        }
    }
    return 0;
}

void fix_toggle_all(void) {
    fix_set_all(!fix_any_enabled());
}
