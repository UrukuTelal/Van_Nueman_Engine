#pragma once

#include <cstdint>
#include <cstddef>
#include <memory>
#include <vector>
#include "../biology/organism_types.h"

#if defined(_WIN32) || defined(_WIN64)
    #ifdef VAN_NUEMAN_EXPORTS
        #define VAN_NUEMAN_API __declspec(dllexport)
    #else
        #define VAN_NUEMAN_API __declspec(dllimport)
    #endif
#else
    #define VAN_NUEMAN_API
#endif

#if defined(__CUDA_ARCH__)
    #define HOST_DEVICE __device__
    #define DEVICE __device__
    #define HOST __host__
#else
    #define HOST_DEVICE
    #define DEVICE
    #define HOST
#endif

namespace Van_Nueman {

struct EntityID {
    uint32_t index : 24;
    uint32_t generation : 8;
    
    __declspec(dllexport) bool is_valid() const { return index != 0xFFFFFF; }
    __declspec(dllexport) static EntityID invalid() { return {0xFFFFFF, 0}; }
};

constexpr size_t MAX_CA_GRID_DIM = 4096;
constexpr size_t MAX_CELL_ENTITIES = 65536;

struct CAGridSoA {
    float* energy;
    float* matter;
    float* velocity_x;
    float* velocity_y;
    float* velocity_z;
    uint8_t* organism_type;
    uint8_t* life_state;
    uint8_t* shadow_state;
    uint8_t* dream_state;
    uint32_t* entity_id;
    
    struct Flags {
        uint8_t alive : 1;
        uint8_t reproducing : 1;
        uint8_t decaying : 1;
        uint8_t in_dream : 1;
        uint8_t in_shadow : 1;
        uint8_t metamorphosing : 1;
        uint8_t reserved : 2;
    };
    Flags* flags;
    
    size_t width;
    size_t height;
    size_t depth;
    size_t total_cells;
};

struct FluxData {
    float* energy_flux_x;
    float* energy_flux_y;
    float* energy_flux_z;
    float* matter_flux_x;
    float* matter_flux_y;
    float* matter_flux_z;
    float* net_energy_change;
    float* net_matter_change;
};

struct PillarGridSoA {
    float* alignment;
    float* resonance;
    float* stability;
    float* entropy;
    float* coherence;
    float* flux;
    float* manifestation;
    float* dissolution;
    float* pressure;
};

class IComputeBackend {
public:
    virtual ~IComputeBackend() = default;
    virtual bool initialize() = 0;
    virtual void shutdown() = 0;
    virtual bool is_available() const = 0;
    virtual const char* backend_name() const = 0;
};

class CCAComputeBackend : public IComputeBackend {
public:
    bool initialize() override;
    void shutdown() override;
    bool is_available() const override;
    const char* backend_name() const override { return "CUDA"; }
    
    bool upload_grid(const CAGridSoA& grid);
    bool upload_pillars(const PillarGridSoA& pillars);
    bool execute_step(const CAGridSoA& grid, const PillarGridSoA& pillars, 
                      float delta_time, size_t step);
    bool download_results(CAGridSoA& grid);
    void* get_cuda_stream() const;
    
private:
    bool allocate_buffers(size_t cell_count);
    void free_buffers();
    
    void* d_grid_storage;
    void* d_pillar_storage;
    void* d_flux_storage;
    void* d_temp_storage;
    void* cuda_stream;
    size_t allocated_cells;
    bool cuda_available;
};

class CpuComputeBackend : public IComputeBackend {
public:
    bool initialize() override;
    void shutdown() override;
    bool is_available() const override;
    const char* backend_name() const override { return "CPU"; }
    
    void execute_step_cpu(CAGridSoA& grid, const PillarGridSoA& pillars, 
                          FluxData& flux, float delta_time);
    
private:
    void process_cell_cpu(size_t idx, CAGridSoA& grid, const PillarGridSoA& pillars,
                          FluxData& flux, float delta_time);
    void apply_organism_rules_cpu(size_t idx, CAGridSoA& grid, const PillarGridSoA& pillars,
                                   float delta_time);
    void calculate_flux_cpu(size_t idx, CAGridSoA& grid, FluxData& flux);
};

class CAComputeDispatcher {
public:
    __declspec(dllexport) static CAComputeDispatcher& get();
    
    __declspec(dllexport) bool initialize();
    __declspec(dllexport) void shutdown();
    __declspec(dllexport) bool is_gpu_available() const;
    __declspec(dllexport) const char* active_backend() const;
    
    __declspec(dllexport) bool upload_grid(const CAGridSoA& grid);
    __declspec(dllexport) bool upload_pillars(const PillarGridSoA& pillars);
    __declspec(dllexport) bool execute_step(CAGridSoA& grid, const PillarGridSoA& pillars,
                                            float delta_time, size_t step);
    __declspec(dllexport) bool download_results(CAGridSoA& grid);
    
    __declspec(dllexport) void execute_step_cpu_fallback(CAGridSoA& grid, 
                                                         const PillarGridSoA& pillars,
                                                         FluxData& flux,
                                                         float delta_time);
    
    __declspec(dllexport) bool supports_double_precision() const;
    __declspec(dllexport) size_t max_grid_dim() const;
    
private:
    CAComputeDispatcher() = default;
    ~CAComputeDispatcher() = default;
    CAComputeDispatcher(const CAComputeDispatcher&) = delete;
    CAComputeDispatcher& operator=(const CAComputeDispatcher&) = delete;
    
    std::unique_ptr<IComputeBackend> gpu_backend;
    std::unique_ptr<CpuComputeBackend> cpu_backend;
    bool initialized;
    bool use_gpu;
};

struct ALifeEvolutionState {
    float generation;
    float fitness;
    float mutation_rate;
    float adaptation_score;
    uint32_t lineage_id;
    
    __declspec(dllexport) static ALifeEvolutionState default_state() {
        return ALifeEvolutionState{0.0f, 1.0f, 0.001f, 0.5f, 0};
    }
};

class ALifeEvolutionIntegrator {
public:
    __declspec(dllexport) static void evolve_organism(EntityID entity,
                                                     const PillarState& pillars,
                                                     ALifeEvolutionState& state,
                                                     float delta_time);
    
    __declspec(dllexport) static float calculate_fitness(uint32_t type_id,
                                                          const float pillar_states[8],
                                                          float energy,
                                                          float matter);
    
    __declspec(dllexport) static uint32_t mutate_type(uint32_t parent_type,
                                                       const float pillar_states[8],
                                                       float mutation_rate);
};

__declspec(dllexport) bool ca_grid_allocate(CAGridSoA& grid, size_t width, size_t height, size_t depth = 1);
__declspec(dllexport) void ca_grid_deallocate(CAGridSoA& grid);
__declspec(dllexport) bool pillar_grid_allocate(PillarGridSoA& pillars, size_t width, size_t height, size_t depth = 1);
__declspec(dllexport) void pillar_grid_deallocate(PillarGridSoA& pillars);
__declspec(dllexport) bool flux_data_allocate(FluxData& flux, size_t cell_count);
__declspec(dllexport) void flux_data_deallocate(FluxData& flux);

__declspec(dllexport) size_t get_cell_index(const CAGridSoA& grid, size_t x, size_t y, size_t z = 0);
__declspec(dllexport) void get_cell_coords(const CAGridSoA& grid, size_t index, size_t& x, size_t& y, size_t& z);

}
