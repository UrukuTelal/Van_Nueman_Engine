#include "fugure.h"
#include <algorithm>
#include <numeric>
#include <cstring>

FugureSandbox::FugureSandbox() {
    contained_psv_.fill(0.0f);
    contained_wht_.fill(0.0f);
    contained_hopf_.fill(0.0f);
}

bool FugureSandbox::ingest(const PillarStateVector& input_psv,
                            const std::array<float, FUGURE_WHT_DIM>& input_wht,
                            const std::array<float, FUGURE_HOPF_FIBER_DIM>& input_hopf) {
    contained_psv_ = input_psv;
    contained_wht_ = input_wht;
    contained_hopf_ = input_hopf;

    original_resistance_ = input_psv[Resistance];
    original_willpower_ = input_psv[Willpower];

    spike_resistance();
    hold_willpower_static();
    isolated_ = true;
    return true;
}

void FugureSandbox::spike_resistance(float target) {
    contained_psv_[Resistance] = target;
}

void FugureSandbox::hold_willpower_static() {
    contained_psv_[Willpower] = original_willpower_;
}

FugureDistortionMetrics FugureAccelerator::extrapolate(FugureSandbox& sandbox,
                                                        float acceleration_mult,
                                                        int max_steps) {
    FugureDistortionMetrics metrics;
    PillarStateVector psv = sandbox.get_contained_psv();
    float flux = psv[Flux];

    for (int step = 0; step < max_steps; ++step) {
        float rotation = flux * acceleration_mult * 0.01f;
        float harm = psv[Harm];
        float resistance = psv[Resistance];
        float depth = psv[Depth];
        float relation = psv[Relation];

        for (int p = 0; p < NumPillars; ++p) {
            if (p == Flux) {
                psv[p] = bloch_rotate(psv[p], rotation);
            } else if (p == Harm) {
                float harm_increase = (1.0f - resistance) * 0.1f;
                psv[p] = bloch_rotate(psv[p], harm_increase);
            } else if (p == Depth) {
                float depth_increase = std::fabs(flux) * 0.05f;
                psv[p] = bloch_rotate(psv[p], depth_increase);
            } else if (p == Relation) {
                float relation_decay = std::fabs(flux) * 0.02f;
                psv[p] = bloch_rotate(psv[p], -relation_decay);
            } else if (p == Resistance) {
                float resistance_decay = harm * 0.03f;
                psv[p] = bloch_rotate(psv[p], -resistance_decay);
            }
        }

        metrics.flux_rotation_rate = rotation;
        metrics.torsion_magnitude = compute_torsion(psv);
        metrics.relation_decay = 1.0f - psv[Relation];
        metrics.depth_boundary = psv[Depth];
        metrics.steps_to_saturation = step + 1;

        if (check_saturation(psv, metrics)) {
            metrics.saturated = true;
            break;
        }
    }

    return metrics;
}

float FugureAccelerator::compute_torsion(const PillarStateVector& psv) const {
    float harm_flux = psv[Harm] * psv[Flux];
    float depth_distortion = psv[Depth] * psv[Distortion];
    return std::fabs(harm_flux - depth_distortion);
}

bool FugureAccelerator::check_saturation(const PillarStateVector& psv,
                                          const FugureDistortionMetrics& metrics) const {
    if (psv[Harm] >= FUGURE_SATURATION_THRESHOLD) return true;
    if (psv[Relation] <= 1.0f - FUGURE_SATURATION_THRESHOLD) return true;
    if (psv[Integrity] <= 1.0f - FUGURE_SATURATION_THRESHOLD) return true;
    return false;
}

FugureLieTopology FugureTracer::trace(const FugureSandbox& sandbox,
                                       const FugureDistortionMetrics& metrics) {
    FugureLieTopology topology;
    const PillarStateVector& base = sandbox.get_contained_psv();

    topology.torsion_trace.reserve(metrics.steps_to_saturation);
    topology.pillar_snapshots.reserve(metrics.steps_to_saturation);
    topology.flux_trajectory.reserve(metrics.steps_to_saturation);
    topology.relation_boundary.reserve(metrics.steps_to_saturation);

    PillarStateVector evolving = base;
    float flux = base[Flux];

    for (int step = 0; step < metrics.steps_to_saturation; ++step) {
        float rotation = flux * FUGURE_FLUX_ACCELERATION_MULT * 0.01f;

        for (int p = 0; p < NumPillars; ++p) {
            if (p == Flux) {
                evolving[p] = bloch_rotate(evolving[p], rotation);
            } else if (p == Harm) {
                evolving[p] = bloch_rotate(evolving[p], (1.0f - evolving[Resistance]) * 0.1f);
            } else if (p == Depth) {
                evolving[p] = bloch_rotate(evolving[p], std::fabs(flux) * 0.05f);
            } else if (p == Relation) {
                evolving[p] = bloch_rotate(evolving[p], -std::fabs(flux) * 0.02f);
            } else if (p == Resistance) {
                evolving[p] = bloch_rotate(evolving[p], -evolving[Harm] * 0.03f);
            }
        }

        std::array<float, NumPillars> snap;
        for (int p = 0; p < NumPillars; ++p) snap[p] = evolving[p];
        topology.pillar_snapshots.push_back(snap);

        std::array<float, NumPillars> base_arr;
        for (int p = 0; p < NumPillars; ++p) base_arr[p] = base[p];
        topology.torsion_trace.push_back(compute_torsion_delta(snap, base_arr));
        topology.flux_trajectory.push_back(evolving[Flux]);
        topology.relation_boundary.push_back(evolving[Relation]);
    }

    map_negative_space(sandbox, topology);
    return topology;
}

void FugureTracer::map_negative_space(const FugureSandbox& sandbox,
                                       FugureLieTopology& topology) const {
    float volume = 0.0f;
    for (size_t i = 1; i < topology.pillar_snapshots.size(); ++i) {
        float delta = compute_torsion_delta(topology.pillar_snapshots[i],
                                            topology.pillar_snapshots[i - 1]);
        volume += delta;
    }
    topology.negative_space_volume = volume / static_cast<float>(NumPillars);
}

float FugureTracer::compute_torsion_delta(const std::array<float, NumPillars>& a,
                                           const std::array<float, NumPillars>& b) const {
    float sum = 0.0f;
    for (int p = 0; p < NumPillars; ++p) {
        float diff = a[p] - b[p];
        sum += diff * diff;
    }
    return std::sqrt(sum / static_cast<float>(NumPillars));
}

FugureExtractionResult FugureExtractor::extract(const FugureSandbox& sandbox,
                                                  const FugureLieTopology& topology) {
    FugureExtractionResult result;
    const PillarStateVector& base = sandbox.get_contained_psv();

    std::array<float, NumPillars> inv_rotation = compute_inverse_rotation(topology);

    for (int p = 0; p < NumPillars; ++p) {
        float phase = inv_rotation[p];
        result.true_payload[p] = bloch_rotate(base[p], -phase);
    }

    result.lie_topology = topology;
    result.extraction_confidence = compute_extraction_confidence(topology, result.true_payload);
    return result;
}

std::array<float, NumPillars> FugureExtractor::compute_inverse_rotation(const FugureLieTopology& topology) const {
    std::array<float, NumPillars> phase_shift{};
    if (topology.pillar_snapshots.empty()) return phase_shift;

    const auto& saturated = topology.pillar_snapshots.back();
    const auto& initial = topology.pillar_snapshots.front();

    for (int p = 0; p < NumPillars; ++p) {
        float theta_initial = bloch_value_to_theta(initial[p]);
        float theta_saturated = bloch_value_to_theta(saturated[p]);
        phase_shift[p] = theta_saturated - theta_initial;
    }

    float flux_shift = phase_shift[Flux];
    float harm_shift = phase_shift[Harm];
    float dist_shift = phase_shift[Distortion];

    for (int p = 0; p < NumPillars; ++p) {
        float weight = 1.0f;
        if (p == Resistance) weight = 2.0f;
        else if (p == Willpower) weight = 1.5f;
        phase_shift[p] = (phase_shift[p] * weight + flux_shift * 0.3f + harm_shift * 0.2f + dist_shift * 0.1f) / (weight + 0.6f);
    }

    return phase_shift;
}

float FugureExtractor::compute_extraction_confidence(const FugureLieTopology& topology,
                                                      const PillarStateVector& extracted) const {
    float depth_boundary = topology.relation_boundary.empty() ? 1.0f : topology.relation_boundary.back();

    float coherence = 0.0f;
    for (int p = 0; p < NumPillars; ++p) {
        coherence += extracted[p];
    }
    coherence /= static_cast<float>(NumPillars);

    float negative_space = topology.negative_space_volume;
    float volume_confidence = std::min(1.0f, negative_space * 5.0f);

    float depth_penalty = std::fabs(depth_boundary - 0.5f) * 2.0f;

    return std::max(0.0f, std::min(1.0f, (coherence * 0.4f + volume_confidence * 0.4f) * (1.0f - depth_penalty * 0.2f)));
}

FugureRuntime::FugureRuntime() : sandbox_(), accelerator_(), tracer_(), extractor_() {}

FugureExtractionResult FugureRuntime::process(const PillarStateVector& input_psv,
                                               const std::array<float, FUGURE_WHT_DIM>& input_wht,
                                               const std::array<float, FUGURE_HOPF_FIBER_DIM>& input_hopf) {
    sandbox_.ingest(input_psv, input_wht, input_hopf);

    FugureDistortionMetrics metrics = accelerator_.extrapolate(sandbox_);

    FugureLieTopology topology = tracer_.trace(sandbox_, metrics);

    FugureExtractionResult result = extractor_.extract(sandbox_, topology);

    if (topology.negative_space_volume > 0.1f) {
        broadcast_immunization(topology);
        result.immunized = true;
    }

    return result;
}

void FugureRuntime::broadcast_immunization(const FugureLieTopology& topology) {
    immunization_memory_.push_back(topology);
}

FugureRuntime& FugureRuntime::instance() {
    static FugureRuntime runtime;
    return runtime;
}
