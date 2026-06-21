#pragma once

#include "../agents/cognition.h"
#include "../include/Entity.h"
#include "../audio/wht_scaled.h"
#include <cmath>
#include <cstdint>
#include <cstdlib>

enum class MeditationState : uint8_t {
    Normal = 0,
    Focused = 1,
    Deep = 2,
    Transcendental = 3,
    AstralProjected = 4
};

inline const char* meditation_state_name(MeditationState s) {
    switch (s) {
        case MeditationState::Normal: return "Normal";
        case MeditationState::Focused: return "Focused";
        case MeditationState::Deep: return "Deep";
        case MeditationState::Transcendental: return "Transcendental";
        case MeditationState::AstralProjected: return "AstralProjected";
        default: return "Unknown";
    }
}

struct CosmologicalInsight {
    uint32_t target_depth;
    float pillar_impression[NumPillars];
    float clarity;
    bool glimpsed_entity;
    PillarVector entity_psv_snapshot;

    CosmologicalInsight() : target_depth(0), clarity(0), glimpsed_entity(false) {
        for (int i = 0; i < NumPillars; i++) pillar_impression[i] = 0.5f;
        entity_psv_snapshot.fill(vn::fp20_t(0.5f));
    }
};

class AstralProjector {
public:
    AstralProjector() : state_(MeditationState::Normal), meditation_depth_(0),
                        projection_timer_(0), astral_active_(false) {}

    bool begin_meditation(PillarStateVector& pillars) {
        float awareness = pillars[Awareness];
        float willpower = pillars[Willpower];

        if (awareness < 0.6f || willpower < 0.5f) return false;

        state_ = MeditationState::Focused;
        meditation_depth_ = 0.1f;
        return true;
    }

    void meditation_tick(PillarStateVector& pillars, float dt) {
        float awareness = pillars[Awareness];
        float willpower = pillars[Willpower];
        float presence = pillars[Presence];

        meditation_depth_ += dt * 0.02f * awareness;
        if (meditation_depth_ > 1.0f) meditation_depth_ = 1.0f;

        pillars[Awareness] = vn::fp20_t(awareness + dt * 0.01f * willpower);
        if (pillars[Awareness] > 1.0f)
            pillars[Awareness] = vn::fp20_t(1.0f);

        float energy_cost = dt * 0.02f * meditation_depth_;
        float energy = pillars[Warmth] - energy_cost;
        if (energy < 0.0f) energy = 0.0f;
        pillars[Warmth] = vn::fp20_t(energy);

        if (meditation_depth_ > 0.3f && state_ == MeditationState::Focused) {
            state_ = MeditationState::Deep;
        }
        if (meditation_depth_ > 0.6f && state_ == MeditationState::Deep) {
            state_ = MeditationState::Transcendental;
        }
        if (meditation_depth_ > 0.85f && state_ == MeditationState::Transcendental &&
            awareness > 0.8f && presence > 0.6f) {
            state_ = MeditationState::AstralProjected;
        }
    }

    bool astral_project(PillarStateVector& body_pillars,
                         PillarStateVector& astral_body,
                         uint32_t target_depth) {
        if (state_ != MeditationState::AstralProjected) return false;

        float awareness = body_pillars[Awareness];
        float willpower = body_pillars[Willpower];
        uint32_t max_depth = static_cast<uint32_t>((awareness + willpower) * 8.0f);
        if (target_depth > max_depth) target_depth = max_depth;

        for (int p = 0; p < NumPillars; p++)
            astral_body.pillars[p] = body_pillars.pillars[p];

        float shift = static_cast<float>(target_depth) / 8.0f;
        for (int p = 0; p < NumPillars; p++) {
            float val = astral_body.pillars[p];
            val = val * (1.0f - shift * 0.5f) + 0.5f * shift * 0.5f;
            if (val > 1.0f) val = 1.0f;
            astral_body.pillars[p] = vn::fp20_t(val);
        }

        astral_body[Depth] = vn::fp20_t(static_cast<float>(target_depth) / 15.0f);
        astral_body[Awareness] = vn::fp20_t(awareness);
        astral_body[Willpower] = vn::fp20_t(willpower);

        body_pillars[Awareness] = vn::fp20_t(awareness * 0.1f);

        astral_active_ = true;
        projection_timer_ = 0;
        return true;
    }

    CosmologicalInsight astral_tick(PillarStateVector& astral_body,
                                     uint32_t current_depth, float dt) {
        CosmologicalInsight insight;
        insight.target_depth = current_depth;

        projection_timer_ += dt;

        float awareness = astral_body[Awareness];
        float willpower = astral_body[Willpower];
        float depth = astral_body[Depth];

        insight.clarity = awareness * (0.5f + willpower * 0.3f) * (1.0f - depth * 0.2f);
        if (insight.clarity > 1.0f) insight.clarity = 1.0f;

        for (int p = 0; p < NumPillars; p++) {
            float base = astral_body[p];
            float depth_bias = static_cast<float>(current_depth) / 15.0f;
            float realm_val = base * (1.0f - depth_bias * 0.3f)
                            + 0.5f * depth_bias * 0.3f
                            + (static_cast<float>(std::rand() % 200 - 100) / 1000.0f) * (1.0f - insight.clarity);
            if (realm_val < 0.0f) realm_val = 0.0f;
            if (realm_val > 1.0f) realm_val = 1.0f;
            insight.pillar_impression[p] = realm_val;
        }

        if (awareness > 0.7f && (std::rand() % 100) < static_cast<int>(awareness * 30.0f)) {
            insight.glimpsed_entity = true;
            for (int p = 0; p < NumPillars; p++) {
                float glimpse = static_cast<float>(std::rand() % 256) / 255.0f;
                insight.entity_psv_snapshot[p] = vn::fp20_t(glimpse * insight.clarity * 0.5f + 0.25f);
            }
        }

        float energy_drain = dt * 0.05f * (1.0f + static_cast<float>(current_depth) * 0.1f);
        for (int p = 0; p < NumPillars; p++) {
            float v = astral_body[p] - energy_drain * 0.01f;
            if (v < 0.0f) v = 0.0f;
            astral_body[p] = vn::fp20_t(v);
        }

        return insight;
    }

    bool return_from_astral(PillarStateVector& body_pillars,
                             PillarStateVector& astral_body) {
        if (!astral_active_) return false;

        float projected_awareness = astral_body[Awareness];
        float current_body_awareness = body_pillars[Awareness];

        float reintegrated = current_body_awareness + projected_awareness * 0.8f;
        if (reintegrated > 1.0f) reintegrated = 1.0f;
        body_pillars[Awareness] = vn::fp20_t(reintegrated);

        body_pillars[Memory] = vn::fp20_t(
            (body_pillars[Memory] + astral_body[Memory] * 0.3f)
        );
        if (body_pillars[Memory] > 1.0f)
            body_pillars[Memory] = vn::fp20_t(1.0f);

        state_ = MeditationState::Normal;
        meditation_depth_ = 0;
        astral_active_ = false;
        projection_timer_ = 0;
        return true;
    }

    float realm_access_required(uint32_t depth) const {
        return 0.5f + static_cast<float>(depth) * 0.033f;
    }

    MeditationState state() const { return state_; }
    float meditation_depth() const { return meditation_depth_; }
    bool astral_active() const { return astral_active_; }
    float projection_timer() const { return projection_timer_; }

private:
    MeditationState state_;
    float meditation_depth_;
    float projection_timer_;
    bool astral_active_;
};
