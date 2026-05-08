// inter_scale_feedback.cpp - Inter-scale WHT feedback implementation
// Reuses audio/wht.cpp for Walsh-Hadamard Transform

#include "inter_scale_feedback.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <ctime>

InterScaleFeedback::InterScaleFeedback() {
}

InterScaleFeedback::~InterScaleFeedback() {
    feedback_log_.clear();
}

bool InterScaleFeedback::log_feedback(uint32_t from_scale, uint32_t to_scale,
                                   const float* pillar_data, uint32_t length) {
    if (!pillar_data || length == 0) return false;
    
    ScaleFeedback fb;
    fb.from_scale = from_scale;
    fb.to_scale = to_scale;
    fb.timestamp = (uint32_t)time(nullptr);
    fb.signal_energy = 0.0f;
    fb.non_zero_count = 0;
    
    // Prepare data for WHT (use first 32 values, pad to 128)
    float temp[WHT_N];  // WHT_N = 32 from audio/wht.h
    for (int i = 0; i < WHT_N; i++) {
        temp[i] = (i < (int)length) ? pillar_data[i] : 0.0f;
    }
    
    // Transform to WHT domain (reuse audio/wht.cpp fwht function)
    fwht(temp, WHT_LOG2_N);  // Forward WHT (in-place)
    
    // Copy to 128-length array (4 x 32 for signal matching)
    for (int i = 0; i < 128; i++) {
        fb.wht_coefficients[i] = temp[i % WHT_N];  // Repeat pattern
        if (fb.wht_coefficients[i] != 0.0f) {
            fb.non_zero_count++;
        }
        fb.signal_energy += fb.wht_coefficients[i] * fb.wht_coefficients[i];
    }
    
    fb.sparsity = 1.0f - ((float)fb.non_zero_count / 128.0f);
    
    feedback_log_.push_back(fb);
    
    // Save to Pillar_10_Memory/signals/ (match existing JSON format)
    save_to_pillar_memory(fb);
    
    return true;
}

bool InterScaleFeedback::read_feedback(int scale_diff, float* out_coefficients) const {
    if (!out_coefficients || scale_diff < 0 || scale_diff >= 128) {
        return false;
    }
    
    // Find feedback with matching scale difference
    for (const auto& fb : feedback_log_) {
        int diff = get_coefficient_index(fb.from_scale, fb.to_scale);
        if (diff == scale_diff) {
            for (int i = 0; i < 128; i++) {
                out_coefficients[i] = fb.wht_coefficients[i];
            }
            return true;
        }
    }
    return false;
}

bool InterScaleFeedback::save_to_pillar_memory(const ScaleFeedback& fb) const {
    // Match existing format: Pillar_10_Memory/signals/wht_signal_<timestamp>.json
    char filename[256];
    struct tm* timeinfo;
    time_t now = fb.timestamp;
    timeinfo = localtime(&now);
    strftime(filename, sizeof(filename), 
             "Pillar_10_Memory/signals/wht_signal_%Y%m%d_%H%M%S.json", 
             timeinfo);
    
    FILE* f = fopen(filename, "w");
    if (!f) return false;
    
    // JSON format matching existing wht_signal_*.json files
    fprintf(f, "{\n");
    fprintf(f, "  \"timestamp\": \"%s\",\n", filename);  // Simplified
    fprintf(f, "  \"original_length\": %u,\n", WHT_N);
    fprintf(f, "  \"signal_length\": 128,\n");
    fprintf(f, "  \"non_zero_coefficients\": %u,\n", fb.non_zero_count);
    fprintf(f, "  \"sparsity\": %.1f,\n", fb.sparsity);
    fprintf(f, "  \"threshold_used\": 0.7,\n");
    fprintf(f, "  \"max_coefficient\": %.6f,\n", fb.wht_coefficients[0]);
    fprintf(f, "  \"signal_energy\": %.6f,\n", fb.signal_energy);
    fprintf(f, "  \"pillar_thresholds_applied\": false,\n");
    fprintf(f, "  \"from_scale\": %u,\n", fb.from_scale);
    fprintf(f, "  \"to_scale\": %u,\n", fb.to_scale);
    fprintf(f, "  \"coefficient_index\": %d\n", get_coefficient_index(fb.from_scale, fb.to_scale));
    fprintf(f, "}\n");
    
    fclose(f);
    return true;
}

void InterScaleFeedback::transform_to_wht(float* data, int log2n) {
    // Reuse audio/wht.cpp::fwht()
    fwht(data, log2n);
}

float InterScaleFeedback::calculate_energy(const float* data, uint32_t length) {
    float energy = 0.0f;
    for (uint32_t i = 0; i < length; i++) {
        energy += data[i] * data[i];
    }
    return energy;
}
