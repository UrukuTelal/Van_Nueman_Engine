#pragma once

#include <cstdint>
#include <array>

namespace Van_Nueman {

enum class OrganismCategory : uint8_t {
    Mineral,
    Plant,
    Animal,
    Sentient,
    Phantom,
    Hybrid
};

enum class ReproductionType : uint8_t {
    Asexual,
    Sexual,
    Budding,
    Fragmentation,
    Metamorphic
};

enum class OrganismBase : uint8_t {
    Carbon,
    Silicon,
    Hybrid
};

struct MetabolismProfile {
    float base_energy_consumption;
    float peak_energy_consumption;
    float energy_efficiency;
    float matter_decay_rate;
    float matter_growth_rate;
    float photosynthesis_rate;
    float predation_rate;
    float symbiosis_rate;

    __declspec(dllexport) static constexpr MetabolismProfile Default() {
        return MetabolismProfile{1.0f, 2.0f, 0.5f, 0.1f, 0.05f, 0.0f, 0.0f, 0.0f};
    }

    __declspec(dllexport) static constexpr MetabolismProfile Photosynthetic() {
        return MetabolismProfile{0.5f, 1.0f, 0.7f, 0.05f, 0.15f, 1.0f, 0.0f, 0.0f};
    }

    __declspec(dllexport) static constexpr MetabolismProfile Predatory() {
        return MetabolismProfile{2.0f, 5.0f, 0.4f, 0.2f, 0.1f, 0.0f, 1.0f, 0.0f};
    }

    __declspec(dllexport) static constexpr MetabolismProfile Phantom() {
        return MetabolismProfile{0.1f, 0.3f, 0.9f, 0.01f, 0.02f, 0.0f, 0.0f, 0.5f};
    }
};

struct OrganismTypeDefinition {
    uint32_t type_id;
    OrganismCategory category;
    ReproductionType reproduction;
    MetabolismProfile metabolism;
    float base_reproduction_probability;
    float energy_threshold;
    float matter_threshold;
    float shadow_resistance;
    float dream_attraction;
    std::array<float, 8> pillar_weights;
    uint32_t valid_pillar_mask;
};

class OrganismTypeRegistry {
public:
    static constexpr size_t MAX_ORGANISM_TYPES = 64;

    __declspec(dllexport) static OrganismTypeDefinition get_type(uint32_t type_id) {
        static const std::array<OrganismTypeDefinition, 8> types = {{
            {0,  OrganismCategory::Mineral,    ReproductionType::Fragmentation,  MetabolismProfile{0.1f, 0.2f, 0.9f, 0.0f,  0.0f,  0.0f,  0.0f,  0.0f},  0.01f, 10.0f, 50.0f, 1.0f, 0.0f,  {1.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f}, 0x01},
            {1,  OrganismCategory::Plant,      ReproductionType::Budding,        MetabolismProfile::Photosynthetic(),                                              0.05f, 20.0f, 100.0f, 0.7f, 0.1f,  {0.0f,1.0f,0.5f,0.0f,0.0f,0.0f,0.0f,0.0f}, 0x02},
            {2,  OrganismCategory::Animal,     ReproductionType::Sexual,        MetabolismProfile::Predatory(),                                                 0.10f, 50.0f, 200.0f, 0.4f, 0.2f,  {0.0f,0.0f,1.0f,0.5f,0.0f,0.0f,0.0f,0.0f}, 0x04},
            {3,  OrganismCategory::Sentient,   ReproductionType::Metamorphic,  MetabolismProfile{3.0f, 8.0f, 0.3f, 0.3f,  0.2f,  0.0f,  0.3f,  0.2f}, 0.20f, 100.0f, 500.0f, 0.2f, 0.5f, {0.0f,0.0f,0.0f,1.0f,1.0f,0.5f,0.0f,0.0f}, 0x08},
            {4,  OrganismCategory::Phantom,   ReproductionType::Asexual,       MetabolismProfile::Phantom(),                                                  0.15f, 5.0f,  20.0f,  0.0f, 1.0f,  {0.0f,0.0f,0.0f,0.0f,0.0f,1.0f,0.5f,0.5f}, 0x10},
            {5,  OrganismCategory::Hybrid,     ReproductionType::Budding,      MetabolismProfile{1.5f, 4.0f, 0.5f, 0.15f, 0.1f,  0.3f,  0.2f,  0.3f}, 0.08f, 40.0f, 300.0f, 0.5f, 0.4f, {0.0f,0.0f,0.5f,0.5f,0.0f,0.0f,0.5f,0.0f}, 0x0C},
            {6,  OrganismCategory::Plant,      ReproductionType::Fragmentation, MetabolismProfile{0.3f, 0.8f, 0.8f, 0.02f, 0.2f,  0.8f,  0.0f,  0.1f}, 0.03f, 15.0f, 80.0f,  0.8f, 0.15f, {0.0f,1.0f,0.0f,0.0f,0.0f,0.0f,0.5f,0.0f}, 0x12},
            {7,  OrganismCategory::Animal,     ReproductionType::Asexual,      MetabolismProfile{2.5f, 6.0f, 0.35f, 0.25f, 0.15f, 0.0f,  0.5f,  0.1f}, 0.12f, 60.0f, 250.0f, 0.3f, 0.25f, {0.0f,0.0f,1.0f,0.0f,0.0f,0.0f,0.0f,0.5f}, 0x14}
        }};

        if (type_id < types.size()) {
            return types[type_id];
        }
        return types[0];
    }

    __declspec(dllexport) static float calculate_reproduction_probability(
        uint32_t type_id,
        const float pillar_states[8],
        float energy,
        float matter,
        float shadow_influence,
        float dream_influence) 
    {
        auto def = get_type(type_id);
        float prob = def.base_reproduction_probability;
        
        float pillar_score = 0.0f;
        for (int i = 0; i < 8; ++i) {
            if (def.valid_pillar_mask & (1 << i)) {
                pillar_score += pillar_states[i] * def.pillar_weights[i];
            }
        }
        prob *= (0.5f + 0.5f * pillar_score);
        
        if (energy >= def.energy_threshold && matter >= def.matter_threshold) {
            prob *= 2.0f;
        }
        
        prob *= (1.0f + def.dream_attraction * dream_influence);
        prob *= (1.0f - def.shadow_resistance * shadow_influence);
        
        return prob;
    }
};

struct PillarState {
    float alignment;
    float resonance;
    float stability;
    float entropy;
    float coherence;
    float flux;
    float manifestation;
    float dissolution;
};

}
