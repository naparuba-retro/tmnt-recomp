/* Flags of the fixes, see fixes.h. */
#include "fixes.h"

static int enabled[FIX_COUNT];

int fix_enabled(FixId id) {
    return enabled[id];
}

void fix_set(FixId id, int on) {
    enabled[id] = on != 0;
}

int fix_is_water(FixId id) {
    return id == FIX_WATER_DRIFT || id == FIX_WATER_SPINNER;
}

void fix_set_water(int on) {
    for (int i = 0; i < FIX_COUNT; i++) {
        if (fix_is_water((FixId)i)) {
            enabled[i] = on != 0;
        }
    }
}

int fix_water_enabled(void) {
    for (int i = 0; i < FIX_COUNT; i++) {
        if (fix_is_water((FixId)i) && enabled[i]) {
            return 1;
        }
    }
    return 0;
}

void fix_toggle_water(void) {
    fix_set_water(!fix_water_enabled());
}

void fix_set_all(int on) {
    for (int i = 0; i < FIX_COUNT; i++) {
        enabled[i] = on != 0;
    }
}

int fix_any_enabled(void) {
    for (int i = 0; i < FIX_COUNT; i++) {
        if (enabled[i] && !fix_is_water((FixId)i)) {
            return 1;
        }
    }
    return 0;
}

void fix_toggle_all(void) {
    int on = !fix_any_enabled();
    for (int i = 0; i < FIX_COUNT; i++) {
        if (!fix_is_water((FixId)i)) {
            enabled[i] = on;
        }
    }
}
