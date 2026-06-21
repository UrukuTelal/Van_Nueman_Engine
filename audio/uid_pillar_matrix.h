#ifndef UID_PILLAR_MATRIX_H

#define UID_PILLAR_MATRIX_H

#include <cstdint>
#include "../include/Entity.h"

struct UIDPillarMatrix {
    uint32_t uid;
    float pillars[NumPillars];
};

void update_uid_pillar_matrix(UIDPillarMatrix* m, uint32_t uid, const float pillars[NumPillars]);

#endif // UID_PILLAR_MATRIX_H
