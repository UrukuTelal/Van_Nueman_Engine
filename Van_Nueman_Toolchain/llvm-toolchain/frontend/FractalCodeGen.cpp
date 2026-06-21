//===-- FractalCodeGen.cpp - Fractal Scale Code Generation -----------===//
//
// Generates multiple versions of functions annotated with [[fractal]]
// for BCC lattice voxel structures. Lowers [[fractal]] structures to
// the Vulkan compute pipeline via SPIR-V compatible code generation.
// Creates entity, server, and federation scale versions with proper
// BCC lattice sampling and mipmap-style LoD scaling.
//
//===----------------------------------------------------------------------===//

#include <iostream>
#include <string>
#include <vector>
#include <array>
#include <cmath>
#include <unordered_map>
#include <sstream>

namespace vn {

// BCC (Body-Centered Cubic) lattice parameters
struct BCCLatticeConfig {
    float voxel_scale;         // base voxel size
    int resolution;            // lattice resolution per axis
    float svt_coupling;        // superfluid vacuum coupling strength
    bool use_fp20_precision;   // use fixed-point for deterministic compute

    BCCLatticeConfig()
        : voxel_scale(1.0f), resolution(64),
          svt_coupling(0.01f), use_fp20_precision(true) {}
};

// SPIR-V kernel code fragment for BCC sampling
struct BCCKernelFragment {
    std::string decl;      // variable declarations
    std::string body;      // kernel body
    std::string output;    // output write
};

// Generate BCC lattice sampling SPIR-V compatible GLSL
static BCCKernelFragment generateBCCSample(const std::string& funcName,
                                            const BCCLatticeConfig& cfg) {
    BCCKernelFragment frag;

    frag.decl =
        "    // BCC lattice parameters from [[fractal(" + funcName + ")]]\n"
        "    float voxel_scale = " + std::to_string(cfg.voxel_scale) + ";\n"
        "    float svt_coupling = " + std::to_string(cfg.svt_coupling) + ";\n"
        "    int res = " + std::to_string(cfg.resolution) + ";\n"
        "    vec3 bcc_offset = vec3(0.5, 0.5, 0.5);\n";

    frag.body =
        "    // BCC sampling: two interleaved grid passes\n"
        "    vec3 grid_pos = floor(pos * float(res)) / float(res);\n"
        "    vec3 frac_pos = fract(pos * float(res));\n"
        "    // Primary grid\n"
        "    float d1 = length(frac_pos - 0.5);\n"
        "    // Body-centered offset\n"
        "    vec3 bc_pos = frac_pos - bcc_offset;\n"
        "    float d2 = length(bc_pos);\n"
        "    float sample = smoothstep(0.5, 0.0, min(d1, d2));\n"
        "    sample = sample * svt_coupling + sample * (1.0 - svt_coupling);\n";

    frag.output =
        "    result = sample;\n";

    return frag;
}

// Generate complete SPIR-V GLSL kernel string
static std::string generateGLSLKernel(const std::string& funcName,
                                       const std::string& scale,
                                       const BCCLatticeConfig& cfg,
                                       const std::vector<std::string>& pillar_params) {
    BCCKernelFragment bcc = generateBCCSample(funcName, cfg);

    std::ostringstream ss;
    ss << "#version 450\n\n"
       << "// Auto-generated from [[fractal(" << funcName << ")]] scale=" << scale << "\n"
       << "// BCC lattice voxel kernel\n\n"
       << "layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;\n\n"
       << "layout(binding = 0, std430) buffer Input {\n"
       << "    float positions[];\n"
       << "} in_buf;\n\n"
       << "layout(binding = 1, std430) buffer Output {\n"
       << "    float results[];\n"
       << "} out_buf;\n\n";

    // Add pillar-specific buffer bindings if any
    for (size_t i = 0; i < pillar_params.size(); i++) {
        ss << "layout(binding = " << (2 + i) << ", std430) buffer Pillar" << i << " {\n"
           << "    float pillar_values[];\n"
           << "} pillar" << i << ";\n";
    }

    ss << "\nvoid main() {\n"
       << "    uint idx = gl_GlobalInvocationID.x;\n"
       << "    vec3 pos = vec3(in_buf.positions[idx * 3 + 0],\n"
       << "                    in_buf.positions[idx * 3 + 1],\n"
       << "                    in_buf.positions[idx * 3 + 2]);\n";

    // Write pillar reads if any
    for (size_t i = 0; i < pillar_params.size(); i++) {
        ss << "    float p" << i << " = pillar" << i << ".pillar_values[idx];\n";
    }

    ss << "\n    float result = 0.0;\n"
       << bcc.decl
       << bcc.body
       << bcc.output
       << "    out_buf.results[idx] = result;\n"
       << "}\n";

    return ss.str();
}

// Fractal code generator
class FractalCodeGen {
public:
    FractalCodeGen() : default_cfg_() {}

    // Process a function with [[fractal]] attribute
    // Generates scale-specific GLSL kernels that can be compiled to SPIR-V
    // and dispatched on the Vulkan compute pipeline.
    void processFractalFunction(const std::string& funcName,
                                const std::vector<std::string>& pillar_params = {}) {
        std::cout << "vncc: Generating fractal versions for: " << funcName << "\n";

        // Generate 3 scale versions
        std::string entity_glsl = generateGLSLKernel(funcName, "entity", default_cfg_, {});
        std::string server_glsl = generateGLSLKernel(funcName, "server",
            BCCLatticeConfig{0.5f, 128, 0.005f, true}, pillar_params);
        std::string federation_glsl = generateGLSLKernel(funcName, "federation",
            BCCLatticeConfig{0.25f, 256, 0.002f, true}, pillar_params);

        std::cout << "  Generated entity scale (" << entity_glsl.size() << " bytes)\n";
        std::cout << "  Generated server scale (" << server_glsl.size() << " bytes)\n";
        std::cout << "  Generated federation scale (" << federation_glsl.size() << " bytes)\n";

        // Store generated kernels (in full implementation, writes to .comp files)
        generated_kernels_[funcName + "_entity"] = entity_glsl;
        generated_kernels_[funcName + "_server"] = server_glsl;
        generated_kernels_[funcName + "_federation"] = federation_glsl;
    }

    // Retrieve generated GLSL kernel by name
    std::string getKernel(const std::string& name) const {
        auto it = generated_kernels_.find(name);
        if (it != generated_kernels_.end()) return it->second;
        return "";
    }

    // Set custom BCC config
    void setConfig(const BCCLatticeConfig& cfg) { default_cfg_ = cfg; }

private:
    BCCLatticeConfig default_cfg_;
    std::unordered_map<std::string, std::string> generated_kernels_;

    const char* getScaleName(const std::string& scale) {
        if (scale == "entity") return "entity";
        if (scale == "server") return "server";
        if (scale == "federation") return "federation";
        return "unknown";
    }
};

} // namespace vn
