#include <vector>
#include <string>
#include <iostream>
#include "../include/Entity.h"
#include "../api/skelly_api.h"

// Single player tutorial (linear narrative)
// From FULL_ARCHITECTURE.md: single_player/tutorial - Linear narrative, ephemeral completion toggle

enum TutorialStep {
    TUT_STEP_START = 0,
    TUT_STEP_SURVEY,
    TUT_STEP_CRISPR,
    TUT_STEP_CREATURE,
    TUT_STEP_SIMULATION,
    TUT_STEP_CLAIM,
    TUT_STEP_SCRIPTING,
    TUT_STEP_RELAY,
    TUT_STEP_COMPLETE,
    TUT_STEP_COUNT
};

const char* TUTORIAL_TEXT[TUT_STEP_COUNT] = {
    "Welcome to Van_Nueman. You are a probe sent to find new homes.",
    "Survey the star system. Use drones to scan planets and collect resources.",
    "Discover life? Capture specimens and add them to your CRISPR vault.",
    "Use the CRISPR vault to create custom creatures.",
    "Test your creatures in the simulation chamber before deployment.",
    "Claim a star system using Influence and Presence pillars.",
    "Learn to use the emergency console for Python scripting.",
    "Build a relay station to help other players.",
    "Tutorial complete! Your journey begins."
};

struct TutorialState {
    uint32_t player_id;
    TutorialStep current_step;
    bool completed[TUT_STEP_COUNT];
    bool ephemeral;  // Does not persist to multiplayer
};

class TutorialSystem {
public:
    TutorialSystem() : cluster_scanned_(false), specimen_count_(0), creature_designed_(false),
        simulation_ran_(false), system_claimed_(false), scripting_used_(false), relay_built_(false) {}

    void start_tutorial(uint32_t player_id) {
        state.player_id = player_id;
        state.current_step = TUT_STEP_START;
        state.ephemeral = true;
        for (int i = 0; i < TUT_STEP_COUNT; i++) state.completed[i] = false;
        std::cout << TUTORIAL_TEXT[TUT_STEP_START] << std::endl;
    }

    void advance_step() {
        if (state.current_step >= TUT_STEP_COMPLETE) return;

        state.completed[state.current_step] = true;
        state.current_step = (TutorialStep)(state.current_step + 1);

        if (state.current_step < TUT_STEP_COUNT) {
            std::cout << TUTORIAL_TEXT[state.current_step] << std::endl;
        }
    }

    void complete_tutorial() {
        state.current_step = TUT_STEP_COMPLETE;
        state.completed[TUT_STEP_COMPLETE] = true;
        std::cout << TUTORIAL_TEXT[TUT_STEP_COMPLETE] << std::endl;
        // Do NOT persist to multiplayer (ephemeral)
    }

    bool check_step_complete(TutorialStep step) {
        switch (step) {
            case TUT_STEP_SURVEY:
                return survey_complete();
            case TUT_STEP_CRISPR:
                return crispr_complete();
            case TUT_STEP_CREATURE:
                return creature_complete();
            case TUT_STEP_SIMULATION:
                return simulation_complete();
            case TUT_STEP_CLAIM:
                return claim_complete();
            case TUT_STEP_SCRIPTING:
                return scripting_complete();
            case TUT_STEP_RELAY:
                return relay_complete();
            default:
                return state.completed[step];
        }
    }

    void update() {
        if (state.current_step >= TUT_STEP_COMPLETE) return;

        if (check_step_complete(state.current_step)) {
            advance_step();
        }
    }

    TutorialState get_state() { return state; }

private:
    // Member variables to track actual progress
    bool cluster_scanned_;
    int specimen_count_;
    bool creature_designed_;
    bool simulation_ran_;
    bool system_claimed_;
    bool scripting_used_;
    bool relay_built_;

    // Check if star cluster has been scanned (survey step)
    bool survey_complete() {
        // Access StarCluster singleton or global - check if any stars surveyed
        // For now, check if we have scanned any planets with habitable=true
        return cluster_scanned_;
    }

    // Check if any specimens added to CRISPR vault
    bool crispr_complete() {
        return specimen_count_ > 0;
    }

    // Check if custom creature designed
    bool creature_complete() {
        return creature_designed_;
    }

    // Verify simulation run
    bool simulation_complete() {
        return simulation_ran_;
    }

    // Check if star system claimed
    bool claim_complete() {
        return system_claimed_;
    }

    // Check if Python scripting was used
    bool scripting_complete() {
        return scripting_used_;
    }

    // Check if relay station built
    bool relay_complete() {
        return relay_built_;
    }

    TutorialState state;
};
