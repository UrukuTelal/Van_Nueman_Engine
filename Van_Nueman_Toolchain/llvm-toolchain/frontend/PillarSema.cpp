//===-- PillarSema.cpp - Semantic Analysis for Pillar Types -----------===//
//
// Semantic analysis for Van_Nueman pillar types and attributes.
// Validates [[pillar_vector]] and [[fractal]] attributes against the
// 16-Pillar Identity Dimensions (PID) defined in pillars.yaml.
// Enforces mathematical preconditions before SPIR-V lowering.
//
//===----------------------------------------------------------------------===//

#include <iostream>
#include <string>
#include <vector>
#include <array>
#include <algorithm>
#include <cmath>

#include "vn/PillarEnumUGC.h"
#include "vn/PillarConstraintsUGC.h"

namespace vn {

// Validate pillar index against PID
static bool validate_pillar_index(int idx, const std::string& context) {
    if (idx < 0 || idx >= 16) {
        std::cerr << "vncc: error: invalid pillar index " << idx
                  << " in " << context << ". Must be 0-15.\n";
        return false;
    }
    return true;
}

// Visitor to validate pillar-related AST nodes against PID
class PillarASTVisitor {
public:
    PillarASTVisitor() {}

    // Validate struct declarations with [[pillar_vector]] attribute
    bool VisitRecordDecl(const std::string& name,
                         const std::vector<std::string>& fields) {
        if (fields.size() != 16) {
            std::cerr << "vncc: error: [[pillar_vector]] struct '" << name
                      << "' must have exactly 16 fields (one per PID dimension), got "
                      << fields.size() << "\n";
            return false;
        }

        // Validate field names against PID
        for (size_t i = 0; i < fields.size() && i < 16; i++) {
            std::string fname = fields[i];
            // Convert to lowercase for comparison
            std::transform(fname.begin(), fname.end(), fname.begin(), ::tolower);
            if (fname != PID_CONSTRAINTS[i].name) {
                std::cerr << "vncc: warning: [[pillar_vector]] field " << i
                          << " expected '" << PID_CONSTRAINTS[i].name << "', got '" << fname << "'\n";
                // Non-fatal: allow flexible naming but warn
            }
        }
        return true;
    }

    // Validate [[pillar_rotation(operator, target)]] attribute
    bool VisitPillarRotationAttr(int operator_pillar, int target_pillar,
                                 const std::string& source_loc) {
        if (!validate_pillar_index(operator_pillar, source_loc + " operator"))
            return false;
        if (!validate_pillar_index(target_pillar, source_loc + " target"))
            return false;
        if (!is_valid_pillar_target(operator_pillar, target_pillar)) {
            std::cerr << "vncc: error: pillar '" << PID_CONSTRAINTS[operator_pillar].name
                      << "' cannot rotate pillar '" << PID_CONSTRAINTS[target_pillar].name
                      << "' at " << source_loc
                      << ". Allowed targets: ";
            for (int i = 0; i < 16; i++) {
                int t = PID_CONSTRAINTS[operator_pillar].allowed_targets[i];
                if (t == NO_TARGET) break;
                std::cerr << PID_CONSTRAINTS[t].name << " ";
            }
            std::cerr << "\n";
            return false;
        }
        return true;
    }

    // Validate [[fractal]] attributes on functions
    bool VisitFunctionDecl(const std::string& name, int numParams,
                           const std::vector<int>& pillar_params) {
        if (numParams == 0) {
            std::cerr << "vncc: warning: [[fractal]] function '" << name
                      << "' has no parameters; fractal scaling requires at least "
                      << "one parameter for implicit scale context\n";
        }
        // Check that fractal functions only use pillar-validated parameters
        for (size_t i = 0; i < pillar_params.size(); i++) {
            if (!validate_pillar_index(pillar_params[i],
                                       name + " param " + std::to_string(i))) {
                return false;
            }
        }
        return true;
    }

    // Validate [[pillar_constraint(min, max)]] on values
    bool VisitPillarValue(float val, int pillar_idx, const std::string& context) {
        if (!validate_pillar_index(pillar_idx, context)) return false;
        const auto& c = PID_CONSTRAINTS[pillar_idx];
        if (val < c.min_val || val > c.max_val) {
            std::cerr << "vncc: error: pillar '" << c.name << "' value " << val
                      << " out of range [" << c.min_val << ", " << c.max_val
                      << "] at " << context << "\n";
            return false;
        }
        return true;
    }

    // Validate a complete 16-element pillar vector against Bloch sphere constraints
    bool ValidatePillarVector(const float values[16], const std::string& context) {
        bool ok = true;
        for (int i = 0; i < 16; i++) {
            if (!VisitPillarValue(values[i], i, context)) ok = false;
        }
        // Check Harm + Distortion mutual exclusion (both cannot be > 0.8)
        if (values[Harm] > 0.8f && values[Distortion] > 0.8f) {
            std::cerr << "vncc: error: both Harm (" << values[Harm]
                      << ") and Distortion (" << values[Distortion]
                      << ") exceed 0.8 at " << context
                      << ". These pillars are mutually exclusive at high intensity.\n";
            ok = false;
        }
        return ok;
    }
};

// ── Free-standing semantic analysis functions ──

bool validate_pillar_rotation(int operator_pillar, int target_pillar,
                              const std::string& source_loc) {
    PillarASTVisitor visitor;
    return visitor.VisitPillarRotationAttr(operator_pillar, target_pillar, source_loc);
}

bool validate_pillar_vector(const float values[16], const std::string& context) {
    PillarASTVisitor visitor;
    return visitor.ValidatePillarVector(values, context);
}

} // namespace vn
