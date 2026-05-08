#include "../api/pillars_api.h"

void feedback_loop(PillarsAPIContext ctx, float dt) {
    pillars_api_update(ctx, dt);
}
