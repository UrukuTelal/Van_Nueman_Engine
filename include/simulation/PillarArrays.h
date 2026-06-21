#pragma once

#include "../../vn/ScaledInt.h"
#include "../Entity.h"
#include <vector>
#include <cstdint>
#include <algorithm>

namespace vn { namespace simulation {

class PillarArrays {
public:
    void reserve(size_t capacity) {
        for (auto& arr : arrays_) arr.reserve(capacity);
    }

    void resize(size_t count) {
        for (auto& arr : arrays_) arr.resize(count);
    }

    size_t size() const {
        return arrays_[0].size();
    }

    void set_pillars(size_t idx, const PillarStateVector& psv) {
        for (int p = 0; p < NumPillars; ++p) {
            arrays_[p][idx] = psv[p];
        }
    }

    void set_pillars(size_t idx, const PillarStateVector& psv, size_t /*num_pillars*/) {
        set_pillars(idx, psv);
    }

    fp20_t& pillar(PillarIndex pillar_idx, size_t entity_idx) {
        return arrays_[static_cast<int>(pillar_idx)][entity_idx];
    }

    const fp20_t& pillar(PillarIndex pillar_idx, size_t entity_idx) const {
        return arrays_[static_cast<int>(pillar_idx)][entity_idx];
    }

    std::vector<fp20_t>& pillar_array(PillarIndex pillar_idx) {
        return arrays_[static_cast<int>(pillar_idx)];
    }

    const std::vector<fp20_t>& pillar_array(PillarIndex pillar_idx) const {
        return arrays_[static_cast<int>(pillar_idx)];
    }

    void swap_remove(size_t idx, size_t last_idx) {
        for (int p = 0; p < NumPillars; ++p) {
            std::swap(arrays_[p][idx], arrays_[p][last_idx]);
        }
    }

private:
    std::vector<fp20_t> arrays_[NumPillars];
};

}} // namespace vn::simulation
