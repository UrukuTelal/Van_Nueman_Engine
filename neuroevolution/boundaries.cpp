#include <vector>
#include <string>
#include <iostream>
#include "../include/Entity.h"
#include "neuroevolution.h"

// Operator-set constraints for neuroevolution
// From FULL_ARCHITECTURE.md: neuroevolution/boundaries - Operator-set constraints

#ifndef FROM_SCALED
#define FROM_SCALED(x) (x)
#define TO_SCALED(x) (x)
#endif

struct Boundary {
    int pillar;
    float min_val;
    float max_val;
    std::string description;
};

class OperatorBoundaries {
public:
    OperatorBoundaries(uint32_t server_id) : server_id(server_id) {}

    void set_constraint(int pillar, float min_val, float max_val, const std::string& desc = "") {
        for (auto& b : constraints) {
            if (b.pillar == pillar) {
                b.min_val = min_val;
                b.max_val = max_val;
                b.description = desc;
                return;
            }
        }
        constraints.push_back({pillar, min_val, max_val, desc});
    }

    bool validate(float value, int pillar) {
        for (const auto& b : constraints) {
            if (b.pillar == pillar) {
                return value >= b.min_val && value <= b.max_val;
            }
        }
        return true;  // No constraint = always valid
    }

    void constrain_pillar_vector(PillarStateVector& psv) {
        for (int i = 0; i < NUM_PILLARS; i++) {
            float val = FROM_SCALED(psv.pillars[i]);
            for (const auto& b : constraints) {
                if (b.pillar == i) {
                    val = std::max(b.min_val, std::min(b.max_val, val));
                    psv.pillars[i] = TO_SCALED(val);
                    break;
                }
            }
        }
    }

    void print_boundaries() {
        std::cout << "Operator Boundaries for server " << server_id << ":\n";
        for (const auto& b : constraints) {
            std::cout << "  Pillar " << b.pillar << ": [" << b.min_val << ", " << b.max_val << "]";
            if (!b.description.empty()) std::cout << " (" << b.description << ")";
            std::cout << "\n";
        }
    }

private:
    uint32_t server_id;
    std::vector<Boundary> constraints;
};
