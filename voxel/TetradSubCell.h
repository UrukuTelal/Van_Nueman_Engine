#pragma once

#include "../vn/ScaledInt.h"
#include "../include/Entity.h"
#include <cstdint>

struct TetradSubCell {
    vn::fp20_t structural_integrity;
    vn::fp20_t mass;
    vn::fp20_t strain_accumulator;
    vn::fp20_t material_composition[NumPillars];
    vn::fp20_t volume;
    uint32_t face_index;
    bool fractured;

    TetradSubCell()
        : structural_integrity(vn::fp20_t(1.0f)), mass(vn::fp20_t(1.0f)),
          strain_accumulator(vn::fp20_t(0)), volume(vn::fp20_t(0)),
          face_index(0), fractured(false)
    {
        for (int i = 0; i < NumPillars; i++)
            material_composition[i] = vn::fp20_t(0.5f);
    }
};
