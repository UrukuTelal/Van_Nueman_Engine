#include "cellular_automata.h"
#include <cuda_runtime.h>
#include <device_launch_parameters.h>
#include <algorithm>
#include <cmath>
#include <memory>

#ifndef __CUDA_ARCH__
#include <Windows.h>
#endif

namespace Van_Nueman {

#define WARP_SIZE 32
#define MAX_THREADS_PER_BLOCK 256
#define THREADS_X 16
#define THREADS_Y 16

struct GridDeviceBuffer {
    float* d_energy;
    float* d_matter;
    float* d_velocity_x;
    float* d_velocity_y;
    float* d_velocity_z;
    uint8_t* d_organism_type;
    uint8_t* d_life_state;
    uint8_t* d_shadow_state;
    uint8_t* d_dream_state;
    uint32_t* d_entity_id;
    uint8_t* d_flags;
    
    float* d_energy_buffer;
    float* d_matter_buffer;
    float* d_shadow_buffer;
    float* d_dream_buffer;
    uint8_t* d_type_buffer;
    uint8_t* d_flags_buffer;
    
    size_t cell_count;
};

struct PillarDeviceBuffer {
    float* d_alignment;
    float* d_resonance;
    float* d_stability;
    float* d_entropy;
    float* d_coherence;
    float* d_flux;
    float* d_manifestation;
    float* d_dissolution;
};

__device__ __forceinline__ size_t get_cell_index_dev(size_t x, size_t y, size_t width, size_t height) {
    return y * width + x;
}

__device__ __forceinline__ void get_neighbor_indices(size_t x, size_t y, size_t width, size_t height,
                                                    size_t neighbors[8]) {
    size_t xp = (x + 1) % width;
    size_t xm = (x + width - 1) % width;
    size_t yp = (y + 1) % height;
    size_t ym = (y + height - 1) % height;
    
    neighbors[0] = get_cell_index_dev(xm, ym, width, height);
    neighbors[1] = get_cell_index_dev(x,   ym, width, height);
    neighbors[2] = get_cell_index_dev(xp,   ym, width, height);
    neighbors[3] = get_cell_index_dev(xm,   y,  width, height);
    neighbors[4] = get_cell_index_dev(xp,   y,  width, height);
    neighbors[5] = get_cell_index_dev(xm,   yp, width, height);
    neighbors[6] = get_cell_index_dev(x,    yp, width, height);
    neighbors[7] = get_cell_index_dev(xp,   yp, width, height);
}

__device__ __forceinline__ float clampf(float val, float min_val, float max_val) {
    return fmaxf(min_val, fminf(max_val, val));
}

__device__ __forceinline__ float lerpf(float a, float b, float t) {
    return a + t * (b - a);
}

__global__ void cellular_automata_step_kernel(
    float* __restrict__ energy,
    float* __restrict__ matter,
    uint8_t* __restrict__ organism_type,
    uint8_t* __restrict__ flags,
    uint8_t* __restrict__ shadow_state,
    uint8_t* __restrict__ dream_state,
    float* __restrict__ energy_buffer,
    float* __restrict__ shadow_buffer,
    float* __restrict__ dream_buffer,
    uint8_t* __restrict__ type_buffer,
    uint8_t* __restrict__ flags_buffer,
    const float* __restrict__ alignment,
    const float* __restrict__ resonance,
    const float* __restrict__ stability,
    const float* __restrict__ entropy,
    const float* __restrict__ coherence,
    const float* __restrict__ flux,
    const float* __restrict__ manifestation,
    const float* __restrict__ dissolution,
    size_t width,
    size_t height,
    float delta_time,
    float pillar_influence,
    size_t step
) {
    size_t x = blockIdx.x * blockDim.x + threadIdx.x;
    size_t y = blockIdx.y * blockDim.y + threadIdx.y;
    
    if (x >= width || y >= height) return;
    
    size_t idx = get_cell_index_dev(x, y, width, height);
    size_t neighbors[8];
    get_neighbor_indices(x, y, width, height, neighbors);
    
    float e = energy[idx];
    float m = matter[idx];
    float s = shadow_state[idx] / 255.0f;
    float d = dream_state[idx] / 255.0f;
    uint8_t ot = organism_type[idx];
    bool alive = (flags[idx] & 1) != 0;
    
    auto type_def = OrganismTypeRegistry::get_type(ot);
    
    float pillar_states[8] = {
        alignment[idx],
        resonance[idx],
        stability[idx],
        entropy[idx],
        coherence[idx],
        flux[idx],
        manifestation[idx],
        dissolution[idx]
    };
    
    float energy_delta = 0.0f;
    float matter_delta = 0.0f;
    
    if (alive) {
        float metabolism_factor = lerpf(type_def.metabolism.base_energy_consumption,
                                        type_def.metabolism.peak_energy_consumption,
                                        e / type_def.energy_threshold);
        energy_delta -= metabolism_factor * delta_time;
        
        if (type_def.metabolism.photosynthesis_rate > 0.0f) {
            float photosynth = type_def.metabolism.photosynthesis_rate * 
                              coherence[idx] * delta_time;
            energy_delta += photosynth;
        }
        
        float pred_rate = type_def.metabolism.predation_rate;
        if (pred_rate > 0.0f) {
            for (int i = 0; i < 8; ++i) {
                if ((flags[neighbors[i]] & 1) && organism_type[neighbors[i]] < ot) {
                    float prey_e = energy[neighbors[i]];
                    energy_delta += prey_e * pred_rate * delta_time * 0.1f;
                }
            }
        }
        
        energy_delta *= (1.0f + type_def.dream_attraction * d);
        energy_delta *= (1.0f - type_def.shadow_resistance * s);
        
        matter_delta += type_def.metabolism.matter_growth_rate * delta_time;
        matter_delta -= type_def.metabolism.matter_decay_rate * m * delta_time;
    }
    
    float neighbor_avg_energy = 0.0f;
    float neighbor_avg_matter = 0.0f;
    int alive_neighbors = 0;
    
    for (int i = 0; i < 8; ++i) {
        if (flags[neighbors[i]] & 1) {
            neighbor_avg_energy += energy[neighbors[i]];
            neighbor_avg_matter += matter[neighbors[i]];
            alive_neighbors++;
        }
    }
    
    if (alive_neighbors > 0) {
        neighbor_avg_energy /= alive_neighbors;
        neighbor_avg_matter /= alive_neighbors;
    }
    
    float diffusion_rate = 0.1f * pillar_influence;
    energy_delta += diffusion_rate * (neighbor_avg_energy - e);
    matter_delta += diffusion_rate * 0.5f * (neighbor_avg_matter - m);
    
    float new_energy = clampf(e + energy_delta, 0.0f, 1000.0f);
    float new_matter = clampf(m + matter_delta, 0.0f, 500.0f);
    
    energy_buffer[idx] = new_energy;
    matter[idx] = new_matter;
    
    float new_shadow = clampf(s * (1.0f - 0.01f * delta_time) + 
                              entropy[idx] * 0.005f * delta_time, 0.0f, 1.0f);
    float new_dream = clampf(d * (1.0f - 0.01f * delta_time) + 
                             manifestation[idx] * 0.01f * delta_time, 0.0f, 1.0f);
    
    shadow_buffer[idx] = new_shadow * 255.0f;
    dream_buffer[idx] = new_dream * 255.0f;
    type_buffer[idx] = ot;
    flags_buffer[idx] = flags[idx];
}

__global__ void apply_organism_rules_kernel(
    float* __restrict__ energy,
    float* __restrict__ matter,
    uint8_t* __restrict__ organism_type,
    uint8_t* __restrict__ flags,
    uint8_t* __restrict__ shadow_state,
    uint8_t* __restrict__ dream_state,
    float* __restrict__ energy_buffer,
    float* __restrict__ matter_buffer,
    uint8_t* __restrict__ type_buffer,
    uint8_t* __restrict__ flags_buffer,
    const float* __restrict__ alignment,
    const float* __restrict__ resonance,
    const float* __restrict__ stability,
    const float* __restrict__ entropy,
    const float* __restrict__ coherence,
    const float* __restrict__ flux,
    const float* __restrict__ manifestation,
    const float* __restrict__ dissolution,
    size_t width,
    size_t height,
    float delta_time,
    float time,
    uint32_t* __restrict__ entity_spawn_counts,
    size_t max_entities
) {
    size_t x = blockIdx.x * blockDim.x + threadIdx.x;
    size_t y = blockIdx.y * blockDim.y + threadIdx.y;
    
    if (x >= width || y >= height) return;
    
    size_t idx = get_cell_index_dev(x, y, width, height);
    size_t neighbors[8];
    get_neighbor_indices(x, y, width, height, neighbors);
    
    float e = energy[idx];
    float m = matter[idx];
    uint8_t ot = organism_type[idx];
    bool alive = (flags[idx] & 1) != 0;
    
    auto type_def = OrganismTypeRegistry::get_type(ot);
    
    float pillar_states[8] = {
        alignment[idx], resonance[idx], stability[idx], entropy[idx],
        coherence[idx], flux[idx], manifestation[idx], dissolution[idx]
    };
    
    float s = shadow_state[idx] / 255.0f;
    float d = dream_state[idx] / 255.0f;
    
    uint8_t new_flags = flags[idx];
    uint8_t new_type = ot;
    
    if (alive && ot > 0) {
        if (e < type_def.energy_threshold * 0.3f) {
            new_flags |= 0x04;
        }
        
        float reprob = OrganismTypeRegistry::calculate_reproduction_probability(
            ot, pillar_states, e, m, s, d);
        
        if (reprob > 0.1f && (new_flags & 0x02) == 0) {
            int empty_spots = 0;
            size_t empty_neighbors[8];
            for (int i = 0; i < 8; ++i) {
                if ((flags[neighbors[i]] & 1) == 0) {
                    empty_neighbors[empty_spots++] = neighbors[i];
                }
            }
            
            if (empty_spots > 0 && e > type_def.energy_threshold && 
                m > type_def.matter_threshold * 0.5f) {
                unsigned int seed = idx * 1103515245 + 
                                   static_cast<unsigned int>(time * 1000);
                float rand_val = fabsf(sin(seed)) / 1000.0f;
                
                if (rand_val < reprob) {
                    size_t spawn_idx = empty_neighbors[seed % empty_spots];
                    energy_buffer[spawn_idx] = e * 0.5f;
                    matter_buffer[spawn_idx] = m * 0.5f;
                    type_buffer[spawn_idx] = ot;
                    flags_buffer[spawn_idx] = 0x01;
                    
                    new_flags |= 0x02;
                }
            }
        }
        
        if (type_def.category == OrganismCategory::Phantom) {
            if (d > 0.7f && s < 0.3f) {
                new_flags = (new_flags & 0xFE) | 0x10;
            } else if (s > 0.6f) {
                new_flags |= 0x10;
            }
        }
        
        if (type_def.category == OrganismCategory::Sentient && d > 0.8f) {
            new_flags |= 0x20;
            if (m > type_def.matter_threshold * 2.0f) {
                new_type = (ot + 1) % 8;
            }
        }
    }
    
    matter_buffer[idx] = m;
    flags_buffer[idx] = new_flags;
    type_buffer[idx] = new_type;
}

__global__ void calculate_flux_kernel(
    float* __restrict__ energy,
    float* __restrict__ matter,
    float* __restrict__ velocity_x,
    float* __restrict__ velocity_y,
    float* __restrict__ velocity_z,
    float* __restrict__ energy_flux_x,
    float* __restrict__ energy_flux_y,
    float* __restrict__ matter_flux_x,
    float* __restrict__ matter_flux_y,
    float* __restrict__ net_energy_change,
    float* __restrict__ net_matter_change,
    const float* __restrict__ stability,
    const float* __restrict__ coherence,
    const float* __restrict__ flux,
    size_t width,
    size_t height,
    float delta_time
) {
    size_t x = blockIdx.x * blockDim.x + threadIdx.x;
    size_t y = blockIdx.y * blockDim.y + threadIdx.y;
    
    if (x >= width || y >= height) return;
    
    size_t idx = get_cell_index_dev(x, y, width, height);
    size_t xp = (x + 1) % width;
    size_t xm = (x + width - 1) % width;
    size_t yp = (y + 1) % height;
    size_t ym = (y + height - 1) % height;
    
    float e = energy[idx];
    float m = matter[idx];
    float stab = stability[idx];
    float coh = coherence[idx];
    float fl = flux[idx];
    
    float grad_e_x = (energy[get_cell_index_dev(xp, y, width, height)] - 
                      energy[get_cell_index_dev(xm, y, width, height)]) * 0.5f;
    float grad_e_y = (energy[get_cell_index_dev(x, yp, width, height)] - 
                      energy[get_cell_index_dev(x, ym, width, height)]) * 0.5f;
    
    float grad_m_x = (matter[get_cell_index_dev(xp, y, width, height)] - 
                     matter[get_cell_index_dev(xm, y, width, height)]) * 0.5f;
    float grad_m_y = (matter[get_cell_index_dev(x, yp, width, height)] - 
                     matter[get_cell_index_dev(x, ym, width, height)]) * 0.5f;
    
    float diffusion_coeff = lerpf(0.01f, 0.1f, coh) * stab;
    
    energy_flux_x[idx] = -diffusion_coeff * grad_e_x + velocity_x[idx] * e;
    energy_flux_y[idx] = -diffusion_coeff * grad_e_y + velocity_y[idx] * e;
    
    matter_flux_x[idx] = -diffusion_coeff * 0.5f * grad_m_x + velocity_x[idx] * m * 0.5f;
    matter_flux_y[idx] = -diffusion_coeff * 0.5f * grad_m_y + velocity_y[idx] * m * 0.5f;
    
    float vx = velocity_x[idx];
    float vy = velocity_y[idx];
    float vz = velocity_z[idx];
    
    float div_energy_flux = (energy_flux_x[get_cell_index_dev(xp, y, width, height)] -
                             energy_flux_x[get_cell_index_dev(xm, y, width, height)]) * 0.5f +
                            (energy_flux_y[get_cell_index_dev(x, yp, width, height)] -
                             energy_flux_y[get_cell_index_dev(x, ym, width, height)]) * 0.5f;
    
    float div_matter_flux = (matter_flux_x[get_cell_index_dev(xp, y, width, height)] -
                            matter_flux_x[get_cell_index_dev(xm, y, width, height)]) * 0.5f +
                           (matter_flux_y[get_cell_index_dev(x, yp, width, height)] -
                            matter_flux_y[get_cell_index_dev(x, ym, width, height)]) * 0.5f;
    
    net_energy_change[idx] = -div_energy_flux + fl * coh * e;
    net_matter_change[idx] = -div_matter_flux + fl * coh * m * 0.3f;
    
    energy[idx] = clampf(e + net_energy_change[idx] * delta_time, 0.0f, 1000.0f);
    matter[idx] = clampf(m + net_matter_change[idx] * delta_time, 0.0f, 500.0f);
}

__global__ void swap_buffers_kernel(
    float* __restrict__ energy,
    float* __restrict__ shadow_state,
    float* __restrict__ dream_state,
    uint8_t* __restrict__ organism_type,
    uint8_t* __restrict__ flags,
    float* __restrict__ energy_buffer,
    float* __restrict__ shadow_buffer,
    float* __restrict__ dream_buffer,
    uint8_t* __restrict__ type_buffer,
    uint8_t* __restrict__ flags_buffer,
    size_t cell_count
) {
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= cell_count) return;
    
    energy[idx] = energy_buffer[idx];
    shadow_state[idx] = shadow_buffer[idx];
    dream_state[idx] = dream_buffer[idx];
    organism_type[idx] = type_buffer[idx];
    flags[idx] = flags_buffer[idx];
}

bool CCAComputeBackend::initialize() {
    cuda_available = []() {
        int device_count = 0;
        cudaError_t err = cudaGetDeviceCount(&device_count);
        return (err == cudaSuccess && device_count > 0);
    }();
    
    if (!cuda_available) {
        return false;
    }
    
    cudaStreamCreate(&cuda_stream);
    allocated_cells = 0;
    
    return true;
}

void CCAComputeBackend::shutdown() {
    free_buffers();
    if (cuda_stream) {
        cudaStreamDestroy(cuda_stream);
        cuda_stream = nullptr;
    }
}

bool CCAComputeBackend::is_available() const {
    return cuda_available;
}

bool CCAComputeBackend::allocate_buffers(size_t cell_count) {
    if (cell_count <= allocated_cells) {
        return true;
    }
    
    free_buffers();
    
    GridDeviceBuffer* h_grid = new GridDeviceBuffer();
    PillarDeviceBuffer* h_pillars = new PillarDeviceBuffer();
    
    cudaMalloc(&d_grid_storage, sizeof(GridDeviceBuffer));
    cudaMalloc(&d_pillar_storage, sizeof(PillarDeviceBuffer));
    cudaMalloc(&d_flux_storage, sizeof(FluxData));
    cudaMalloc(&d_temp_storage, cell_count * sizeof(float));
    
    GridDeviceBuffer h_buf;
    h_buf.cell_count = cell_count;
    
    cudaMalloc(&h_buf.d_energy, cell_count * sizeof(float));
    cudaMalloc(&h_buf.d_matter, cell_count * sizeof(float));
    cudaMalloc(&h_buf.d_velocity_x, cell_count * sizeof(float));
    cudaMalloc(&h_buf.d_velocity_y, cell_count * sizeof(float));
    cudaMalloc(&h_buf.d_velocity_z, cell_count * sizeof(float));
    cudaMalloc(&h_buf.d_organism_type, cell_count * sizeof(uint8_t));
    cudaMalloc(&h_buf.d_life_state, cell_count * sizeof(uint8_t));
    cudaMalloc(&h_buf.d_shadow_state, cell_count * sizeof(uint8_t));
    cudaMalloc(&h_buf.d_dream_state, cell_count * sizeof(uint8_t));
    cudaMalloc(&h_buf.d_entity_id, cell_count * sizeof(uint32_t));
    cudaMalloc(&h_buf.d_flags, cell_count * sizeof(uint8_t));
    
    cudaMalloc(&h_buf.d_energy_buffer, cell_count * sizeof(float));
    cudaMalloc(&h_buf.d_shadow_buffer, cell_count * sizeof(float));
    cudaMalloc(&h_buf.d_dream_buffer, cell_count * sizeof(float));
    cudaMalloc(&h_buf.d_type_buffer, cell_count * sizeof(uint8_t));
    cudaMalloc(&h_buf.d_flags_buffer, cell_count * sizeof(uint8_t));
    
    cudaMemcpy(d_grid_storage, &h_buf, sizeof(GridDeviceBuffer), cudaMemcpyHostToDevice);
    
    PillarDeviceBuffer h_pil;
    cudaMalloc(&h_pil.d_alignment, cell_count * sizeof(float));
    cudaMalloc(&h_pil.d_resonance, cell_count * sizeof(float));
    cudaMalloc(&h_pil.d_stability, cell_count * sizeof(float));
    cudaMalloc(&h_pil.d_entropy, cell_count * sizeof(float));
    cudaMalloc(&h_pil.d_coherence, cell_count * sizeof(float));
    cudaMalloc(&h_pil.d_flux, cell_count * sizeof(float));
    cudaMalloc(&h_pil.d_manifestation, cell_count * sizeof(float));
    cudaMalloc(&h_pil.d_dissolution, cell_count * sizeof(float));
    
    cudaMemcpy(d_pillar_storage, &h_pil, sizeof(PillarDeviceBuffer), cudaMemcpyHostToDevice);
    
    FluxData h_flux;
    cudaMalloc(&h_flux.energy_flux_x, cell_count * sizeof(float));
    cudaMalloc(&h_flux.energy_flux_y, cell_count * sizeof(float));
    cudaMalloc(&h_flux.energy_flux_z, cell_count * sizeof(float));
    cudaMalloc(&h_flux.matter_flux_x, cell_count * sizeof(float));
    cudaMalloc(&h_flux.matter_flux_y, cell_count * sizeof(float));
    cudaMalloc(&h_flux.matter_flux_z, cell_count * sizeof(float));
    cudaMalloc(&h_flux.net_energy_change, cell_count * sizeof(float));
    cudaMalloc(&h_flux.net_matter_change, cell_count * sizeof(float));
    
    cudaMemcpy(d_flux_storage, &h_flux, sizeof(FluxData), cudaMemcpyHostToDevice);
    
    allocated_cells = cell_count;
    
    return true;
}

void CCAComputeBackend::free_buffers() {
    if (d_grid_storage) {
        GridDeviceBuffer h_buf;
        cudaMemcpy(&h_buf, d_grid_storage, sizeof(GridDeviceBuffer), cudaMemcpyDeviceToHost);
        
        cudaFree(h_buf.d_energy);
        cudaFree(h_buf.d_matter);
        cudaFree(h_buf.d_velocity_x);
        cudaFree(h_buf.d_velocity_y);
        cudaFree(h_buf.d_velocity_z);
        cudaFree(h_buf.d_organism_type);
        cudaFree(h_buf.d_life_state);
        cudaFree(h_buf.d_shadow_state);
        cudaFree(h_buf.d_dream_state);
        cudaFree(h_buf.d_entity_id);
        cudaFree(h_buf.d_flags);
        cudaFree(h_buf.d_energy_buffer);
        cudaFree(h_buf.d_shadow_buffer);
        cudaFree(h_buf.d_dream_buffer);
        cudaFree(h_buf.d_type_buffer);
        cudaFree(h_buf.d_flags_buffer);
        
        cudaFree(d_grid_storage);
        d_grid_storage = nullptr;
    }
    
    if (d_pillar_storage) {
        PillarDeviceBuffer h_pil;
        cudaMemcpy(&h_pil, d_pillar_storage, sizeof(PillarDeviceBuffer), cudaMemcpyDeviceToHost);
        
        cudaFree(h_pil.d_alignment);
        cudaFree(h_pil.d_resonance);
        cudaFree(h_pil.d_stability);
        cudaFree(h_pil.d_entropy);
        cudaFree(h_pil.d_coherence);
        cudaFree(h_pil.d_flux);
        cudaFree(h_pil.d_manifestation);
        cudaFree(h_pil.d_dissolution);
        
        cudaFree(d_pillar_storage);
        d_pillar_storage = nullptr;
    }
    
    if (d_flux_storage) {
        cudaFree(d_flux_storage);
        d_flux_storage = nullptr;
    }
    
    if (d_temp_storage) {
        cudaFree(d_temp_storage);
        d_temp_storage = nullptr;
    }
    
    allocated_cells = 0;
}

bool CCAComputeBackend::upload_grid(const CAGridSoA& grid) {
    GridDeviceBuffer h_buf;
    cudaMemcpy(&h_buf, d_grid_storage, sizeof(GridDeviceBuffer), cudaMemcpyDeviceToHost);
    
    cudaMemcpy(h_buf.d_energy, grid.energy, grid.total_cells * sizeof(float), cudaMemcpyHostToDevice);
    cudaMemcpy(h_buf.d_matter, grid.matter, grid.total_cells * sizeof(float), cudaMemcpyHostToDevice);
    cudaMemcpy(h_buf.d_velocity_x, grid.velocity_x, grid.total_cells * sizeof(float), cudaMemcpyHostToDevice);
    cudaMemcpy(h_buf.d_velocity_y, grid.velocity_y, grid.total_cells * sizeof(float), cudaMemcpyHostToDevice);
    cudaMemcpy(h_buf.d_velocity_z, grid.velocity_z, grid.total_cells * sizeof(float), cudaMemcpyHostToDevice);
    cudaMemcpy(h_buf.d_organism_type, grid.organism_type, grid.total_cells * sizeof(uint8_t), cudaMemcpyHostToDevice);
    cudaMemcpy(h_buf.d_life_state, grid.life_state, grid.total_cells * sizeof(uint8_t), cudaMemcpyHostToDevice);
    cudaMemcpy(h_buf.d_shadow_state, grid.shadow_state, grid.total_cells * sizeof(uint8_t), cudaMemcpyHostToDevice);
    cudaMemcpy(h_buf.d_dream_state, grid.dream_state, grid.total_cells * sizeof(uint8_t), cudaMemcpyHostToDevice);
    cudaMemcpy(h_buf.d_entity_id, grid.entity_id, grid.total_cells * sizeof(uint32_t), cudaMemcpyHostToDevice);
    cudaMemcpy(h_buf.d_flags, grid.flags, grid.total_cells * sizeof(uint8_t), cudaMemcpyHostToDevice);
    
    return true;
}

bool CCAComputeBackend::upload_pillars(const PillarGridSoA& pillars) {
    PillarDeviceBuffer h_pil;
    cudaMemcpy(&h_pil, d_pillar_storage, sizeof(PillarDeviceBuffer), cudaMemcpyDeviceToHost);
    
    size_t cells = pillars.alignment ? (pillars.total_cells > 0 ? pillars.total_cells : MAX_CA_GRID_DIM * MAX_CA_GRID_DIM) : 0;
    
    if (pillars.alignment) cudaMemcpy(h_pil.d_alignment, pillars.alignment, cells * sizeof(float), cudaMemcpyHostToDevice);
    if (pillars.resonance) cudaMemcpy(h_pil.d_resonance, pillars.resonance, cells * sizeof(float), cudaMemcpyHostToDevice);
    if (pillars.stability) cudaMemcpy(h_pil.d_stability, pillars.stability, cells * sizeof(float), cudaMemcpyHostToDevice);
    if (pillars.entropy) cudaMemcpy(h_pil.d_entropy, pillars.entropy, cells * sizeof(float), cudaMemcpyHostToDevice);
    if (pillars.coherence) cudaMemcpy(h_pil.d_coherence, pillars.coherence, cells * sizeof(float), cudaMemcpyHostToDevice);
    if (pillars.flux) cudaMemcpy(h_pil.d_flux, pillars.flux, cells * sizeof(float), cudaMemcpyHostToDevice);
    if (pillars.manifestation) cudaMemcpy(h_pil.d_manifestation, pillars.manifestation, cells * sizeof(float), cudaMemcpyHostToDevice);
    if (pillars.dissolution) cudaMemcpy(h_pil.d_dissolution, pillars.dissolution, cells * sizeof(float), cudaMemcpyHostToDevice);
    
    return true;
}

bool CCAComputeBackend::execute_step(const CAGridSoA& grid, const PillarGridSoA& pillars,
                                     float delta_time, size_t step) {
    if (!allocate_buffers(grid.total_cells)) {
        return false;
    }
    
    dim3 block_dims(THREADS_X, THREADS_Y);
    dim3 grid_dims((grid.width + THREADS_X - 1) / THREADS_X,
                   (grid.height + THREADS_Y - 1) / THREADS_Y);
    
    GridDeviceBuffer h_buf;
    cudaMemcpy(&h_buf, d_grid_storage, sizeof(GridDeviceBuffer), cudaMemcpyDeviceToHost);
    PillarDeviceBuffer h_pil;
    cudaMemcpy(&h_pil, d_pillar_storage, sizeof(PillarDeviceBuffer), cudaMemcpyDeviceToHost);
    
    float pillar_influence = 1.0f;
    
    cellular_automata_step_kernel<<<grid_dims, block_dims, 0, (cudaStream_t)cuda_stream>>>(
        h_buf.d_energy, h_buf.d_matter,
        h_buf.d_organism_type, h_buf.d_flags,
        h_buf.d_shadow_state, h_buf.d_dream_state,
        h_buf.d_energy_buffer,
        h_buf.d_shadow_buffer, h_buf.d_dream_buffer,
        h_buf.d_type_buffer, h_buf.d_flags_buffer,
        h_pil.d_alignment, h_pil.d_resonance, h_pil.d_stability,
        h_pil.d_entropy, h_pil.d_coherence, h_pil.d_flux,
        h_pil.d_manifestation, h_pil.d_dissolution,
        grid.width, grid.height, delta_time, pillar_influence, step
    );
    
    cudaStreamSynchronize((cudaStream_t)cuda_stream);
    
    apply_organism_rules_kernel<<<grid_dims, block_dims, 0, (cudaStream_t)cuda_stream>>>(
        h_buf.d_energy, h_buf.d_matter,
        h_buf.d_organism_type, h_buf.d_flags,
        h_buf.d_shadow_state, h_buf.d_dream_state,
        h_buf.d_energy_buffer, h_buf.d_matter_buffer,
        h_buf.d_type_buffer, h_buf.d_flags_buffer,
        h_pil.d_alignment, h_pil.d_resonance, h_pil.d_stability,
        h_pil.d_entropy, h_pil.d_coherence, h_pil.d_flux,
        h_pil.d_manifestation, h_pil.d_dissolution,
        grid.width, grid.height, delta_time,
        delta_time * (float)step, nullptr, grid.total_cells
    );
    
    cudaStreamSynchronize((cudaStream_t)cuda_stream);
    
    swap_buffers_kernel<<<(grid.total_cells + MAX_THREADS_PER_BLOCK - 1) / MAX_THREADS_PER_BLOCK,
                          MAX_THREADS_PER_BLOCK, 0, (cudaStream_t)cuda_stream>>>(
        h_buf.d_energy,
        h_buf.d_shadow_state, h_buf.d_dream_state,
        h_buf.d_organism_type, h_buf.d_flags,
        h_buf.d_energy_buffer,
        h_buf.d_shadow_buffer, h_buf.d_dream_buffer,
        h_buf.d_type_buffer, h_buf.d_flags_buffer,
        grid.total_cells
    );
    
    cudaStreamSynchronize((cudaStream_t)cuda_stream);
    
    return true;
}

bool CCAComputeBackend::download_results(CAGridSoA& grid) {
    GridDeviceBuffer h_buf;
    cudaMemcpy(&h_buf, d_grid_storage, sizeof(GridDeviceBuffer), cudaMemcpyDeviceToHost);
    
    cudaMemcpy(grid.energy, h_buf.d_energy, grid.total_cells * sizeof(float), cudaMemcpyDeviceToHost);
    cudaMemcpy(grid.matter, h_buf.d_matter, grid.total_cells * sizeof(float), cudaMemcpyDeviceToHost);
    cudaMemcpy(grid.shadow_state, h_buf.d_shadow_state, grid.total_cells * sizeof(uint8_t), cudaMemcpyDeviceToHost);
    cudaMemcpy(grid.dream_state, h_buf.d_dream_state, grid.total_cells * sizeof(uint8_t), cudaMemcpyDeviceToHost);
    cudaMemcpy(grid.organism_type, h_buf.d_organism_type, grid.total_cells * sizeof(uint8_t), cudaMemcpyDeviceToHost);
    cudaMemcpy(grid.flags, h_buf.d_flags, grid.total_cells * sizeof(uint8_t), cudaMemcpyDeviceToHost);
    
    return true;
}

void* CCAComputeBackend::get_cuda_stream() const {
    return cuda_stream;
}

bool CpuComputeBackend::initialize() {
    return true;
}

void CpuComputeBackend::shutdown() {
}

bool CpuComputeBackend::is_available() const {
    return true;
}

void CpuComputeBackend::execute_step_cpu(CAGridSoA& grid, const PillarGridSoA& pillars,
                                        FluxData& flux, float delta_time) {
    size_t cell_count = grid.total_cells;
    
    for (size_t idx = 0; idx < cell_count; ++idx) {
        process_cell_cpu(idx, grid, pillars, flux, delta_time);
    }
    
    for (size_t idx = 0; idx < cell_count; ++idx) {
        apply_organism_rules_cpu(idx, grid, pillars, delta_time);
    }
    
    for (size_t idx = 0; idx < cell_count; ++idx) {
        calculate_flux_cpu(idx, grid, flux);
    }
}

void CpuComputeBackend::process_cell_cpu(size_t idx, CAGridSoA& grid, const PillarGridSoA& pillars,
                                         FluxData& flux, float delta_time) {
    size_t x, y, z;
    get_cell_coords(grid, idx, x, y, z);
    
    size_t neighbors[8];
    size_t xp = (x + 1) % grid.width;
    size_t xm = (x + grid.width - 1) % grid.width;
    size_t yp = (y + 1) % grid.height;
    size_t ym = (y + grid.height - 1) % grid.height;
    
    neighbors[0] = get_cell_index(grid, xm, ym);
    neighbors[1] = get_cell_index(grid, x, ym);
    neighbors[2] = get_cell_index(grid, xp, ym);
    neighbors[3] = get_cell_index(grid, xm, y);
    neighbors[4] = get_cell_index(grid, xp, y);
    neighbors[5] = get_cell_index(grid, xm, yp);
    neighbors[6] = get_cell_index(grid, x, yp);
    neighbors[7] = get_cell_index(grid, xp, yp);
    
    float e = grid.energy[idx];
    float m = grid.matter[idx];
    uint8_t ot = grid.organism_type[idx];
    bool alive = (grid.flags[idx].alive);
    
    auto type_def = OrganismTypeRegistry::get_type(ot);
    
    float pillar_states[8] = {
        pillars.alignment ? pillars.alignment[idx] : 0.0f,
        pillars.resonance ? pillars.resonance[idx] : 0.0f,
        pillars.stability ? pillars.stability[idx] : 0.0f,
        pillars.entropy ? pillars.entropy[idx] : 0.0f,
        pillars.coherence ? pillars.coherence[idx] : 0.0f,
        pillars.flux ? pillars.flux[idx] : 0.0f,
        pillars.manifestation ? pillars.manifestation[idx] : 0.0f,
        pillars.dissolution ? pillars.dissolution[idx] : 0.0f
    };
    
    float s = grid.shadow_state[idx] / 255.0f;
    float d = grid.dream_state[idx] / 255.0f;
    
    float energy_delta = 0.0f;
    float matter_delta = 0.0f;
    
    if (alive) {
        float metabolism_factor = type_def.metabolism.base_energy_consumption;
        energy_delta -= metabolism_factor * delta_time;
        
        if (type_def.metabolism.photosynthesis_rate > 0.0f) {
            energy_delta += type_def.metabolism.photosynthesis_rate * pillar_states[4] * delta_time;
        }
        
        energy_delta *= (1.0f + type_def.dream_attraction * d);
        energy_delta *= (1.0f - type_def.shadow_resistance * s);
        
        matter_delta += type_def.metabolism.matter_growth_rate * delta_time;
        matter_delta -= type_def.metabolism.matter_decay_rate * m * delta_time;
    }
    
    float neighbor_avg_energy = 0.0f;
    float neighbor_avg_matter = 0.0f;
    int alive_neighbors = 0;
    
    for (int i = 0; i < 8; ++i) {
        if (grid.flags[neighbors[i]].alive) {
            neighbor_avg_energy += grid.energy[neighbors[i]];
            neighbor_avg_matter += grid.matter[neighbors[i]];
            alive_neighbors++;
        }
    }
    
    if (alive_neighbors > 0) {
        neighbor_avg_energy /= alive_neighbors;
        neighbor_avg_matter /= alive_neighbors;
    }
    
    float diffusion_rate = 0.1f;
    energy_delta += diffusion_rate * (neighbor_avg_energy - e);
    matter_delta += diffusion_rate * 0.5f * (neighbor_avg_matter - m);
    
    grid.energy[idx] = std::max(0.0f, std::min(1000.0f, e + energy_delta));
    grid.matter[idx] = std::max(0.0f, std::min(500.0f, m + matter_delta));
}

void CpuComputeBackend::apply_organism_rules_cpu(size_t idx, CAGridSoA& grid, 
                                                 const PillarGridSoA& pillars, float delta_time) {
    float e = grid.energy[idx];
    float m = grid.matter[idx];
    uint8_t ot = grid.organism_type[idx];
    bool alive = grid.flags[idx].alive;
    
    auto type_def = OrganismTypeRegistry::get_type(ot);
    
    float pillar_states[8] = {
        pillars.alignment ? pillars.alignment[idx] : 0.0f,
        pillars.resonance ? pillars.resonance[idx] : 0.0f,
        pillars.stability ? pillars.stability[idx] : 0.0f,
        pillars.entropy ? pillars.entropy[idx] : 0.0f,
        pillars.coherence ? pillars.coherence[idx] : 0.0f,
        pillars.flux ? pillars.flux[idx] : 0.0f,
        pillars.manifestation ? pillars.manifestation[idx] : 0.0f,
        pillars.dissolution ? pillars.dissolution[idx] : 0.0f
    };
    
    float s = grid.shadow_state[idx] / 255.0f;
    float d = grid.dream_state[idx] / 255.0f;
    
    if (alive && ot > 0) {
        if (e < type_def.energy_threshold * 0.3f) {
            grid.flags[idx].decaying = 1;
        }
        
        float reprob = OrganismTypeRegistry::calculate_reproduction_probability(
            ot, pillar_states, e, m, s, d);
        
        if (reprob > 0.1f && !grid.flags[idx].reproducing) {
        }
    }
}

void CpuComputeBackend::calculate_flux_cpu(size_t idx, CAGridSoA& grid, FluxData& flux) {
    if (!flux.energy_flux_x || !flux.energy_flux_y || !flux.net_energy_change) {
        return;
    }
    
    size_t x, y, z;
    get_cell_coords(grid, idx, x, y, z);
    
    size_t xp = (x + 1) % grid.width;
    size_t xm = (x + grid.width - 1) % grid.width;
    size_t yp = (y + 1) % grid.height;
    size_t ym = (y + grid.height - 1) % grid.height;
    
    float e = grid.energy[idx];
    float m = grid.matter[idx];
    
    float grad_e_x = (grid.energy[get_cell_index(grid, xp, y)] - 
                      grid.energy[get_cell_index(grid, xm, y)]) * 0.5f;
    float grad_e_y = (grid.energy[get_cell_index(grid, x, yp)] - 
                      grid.energy[get_cell_index(grid, x, ym)]) * 0.5f;
    
    float diffusion_coeff = 0.05f;
    
    flux.energy_flux_x[idx] = -diffusion_coeff * grad_e_x + grid.velocity_x[idx] * e;
    flux.energy_flux_y[idx] = -diffusion_coeff * grad_e_y + grid.velocity_y[idx] * e;
}

CAComputeDispatcher::~CAComputeDispatcher() {
    shutdown();
}

CAComputeDispatcher& CAComputeDispatcher::get() {
    static CAComputeDispatcher instance;
    return instance;
}

bool CAComputeDispatcher::initialize() {
    if (initialized) return true;
    
    cpu_backend = std::make_unique<CpuComputeBackend>();
    if (!cpu_backend->initialize()) {
        cpu_backend.reset();
        return false;
    }
    
    gpu_backend = std::make_unique<CCAComputeBackend>();
    if (gpu_backend->initialize()) {
        use_gpu = true;
    } else {
        gpu_backend.reset();
        use_gpu = false;
    }
    
    initialized = true;
    return true;
}

void CAComputeDispatcher::shutdown() {
    if (gpu_backend) {
        gpu_backend->shutdown();
        gpu_backend.reset();
    }
    if (cpu_backend) {
        cpu_backend->shutdown();
        cpu_backend.reset();
    }
    initialized = false;
}

bool CAComputeDispatcher::is_gpu_available() const {
    return gpu_backend && gpu_backend->is_available();
}

const char* CAComputeDispatcher::active_backend() const {
    return use_gpu ? "CUDA" : "CPU";
}

bool CAComputeDispatcher::upload_grid(const CAGridSoA& grid) {
    if (use_gpu && gpu_backend) {
        return gpu_backend->upload_grid(grid);
    }
    return true;
}

bool CAComputeDispatcher::upload_pillars(const PillarGridSoA& pillars) {
    if (use_gpu && gpu_backend) {
        return gpu_backend->upload_pillars(pillars);
    }
    return true;
}

bool CAComputeDispatcher::execute_step(CAGridSoA& grid, const PillarGridSoA& pillars,
                                       float delta_time, size_t step) {
    if (use_gpu && gpu_backend) {
        return gpu_backend->execute_step(grid, pillars, delta_time, step);
    } else if (cpu_backend) {
        static FluxData dummy_flux{};
        cpu_backend->execute_step_cpu(grid, pillars, dummy_flux, delta_time);
        return true;
    }
    return false;
}

bool CAComputeDispatcher::download_results(CAGridSoA& grid) {
    if (use_gpu && gpu_backend) {
        return gpu_backend->download_results(grid);
    }
    return true;
}

void CAComputeDispatcher::execute_step_cpu_fallback(CAGridSoA& grid, 
                                                     const PillarGridSoA& pillars,
                                                     FluxData& flux,
                                                     float delta_time) {
    if (cpu_backend) {
        cpu_backend->execute_step_cpu(grid, pillars, flux, delta_time);
    }
}

bool CAComputeDispatcher::supports_double_precision() const {
    if (use_gpu && gpu_backend) {
        int major = 0, minor = 0;
        cudaError_t err = cudaDriverGetVersion(&major);
        if (err != cudaSuccess) return false;
        return major >= 11;
    }
    return true;
}

size_t CAComputeDispatcher::max_grid_dim() const {
    return MAX_CA_GRID_DIM;
}

void ALifeEvolutionIntegrator::evolve_organism(EntityID entity,
                                              const PillarState& pillars,
                                              ALifeEvolutionState& state,
                                              float delta_time) {
    state.generation += delta_time * 0.1f;
    
    float pillar_avg = (pillars.alignment + pillars.resonance + pillars.stability +
                        pillars.entropy + pillars.coherence + pillars.flux +
                        pillars.manifestation + pillars.dissolution) / 8.0f;
    
    state.adaptation_score = lerpf(state.adaptation_score, pillar_avg, delta_time * 0.01f);
    state.fitness *= (1.0f + delta_time * 0.001f);
    
    if (state.mutation_rate < 0.1f) {
        state.mutation_rate += delta_time * 0.0001f;
    }
}

float ALifeEvolutionIntegrator::calculate_fitness(uint32_t type_id,
                                                  const float pillar_states[8],
                                                  float energy,
                                                  float matter) {
    auto def = OrganismTypeRegistry::get_type(type_id);
    float fitness = 1.0f;
    
    float pillar_score = 0.0f;
    for (int i = 0; i < 8; ++i) {
        pillar_score += pillar_states[i] * def.pillar_weights[i];
    }
    fitness *= (0.5f + 0.5f * pillar_score);
    
    if (energy > def.energy_threshold) fitness *= 1.5f;
    if (matter > def.matter_threshold) fitness *= 1.2f;
    
    return fitness;
}

uint32_t ALifeEvolutionIntegrator::mutate_type(uint32_t parent_type,
                                               const float pillar_states[8],
                                               float mutation_rate) {
    float rand_val = fabsf(sinf(parent_type * 1103515245)) * mutation_rate;
    
    if (rand_val < 0.01f) {
        return (parent_type + 1) % 8;
    }
    else if (rand_val < 0.02f) {
        return (parent_type + 7) % 8;
    }
    else if (rand_val < 0.03f) {
        int compatible_types[] = {0, 1, 2, 3, 4, 5, 6, 7};
        return compatible_types[(parent_type + 3) % 8];
    }
    
    return parent_type;
}

bool ca_grid_allocate(CAGridSoA& grid, size_t width, size_t height, size_t depth) {
    grid.width = width;
    grid.height = height;
    grid.depth = depth;
    grid.total_cells = width * height * depth;
    
    grid.energy = new float[grid.total_cells]();
    grid.matter = new float[grid.total_cells]();
    grid.velocity_x = new float[grid.total_cells]();
    grid.velocity_y = new float[grid.total_cells]();
    grid.velocity_z = new float[grid.total_cells]();
    grid.organism_type = new uint8_t[grid.total_cells]();
    grid.life_state = new uint8_t[grid.total_cells]();
    grid.shadow_state = new uint8_t[grid.total_cells]();
    grid.dream_state = new uint8_t[grid.total_cells]();
    grid.entity_id = new uint32_t[grid.total_cells]();
    grid.flags = new CAGridSoA::Flags[grid.total_cells]();
    
    return true;
}

void ca_grid_deallocate(CAGridSoA& grid) {
    delete[] grid.energy;
    delete[] grid.matter;
    delete[] grid.velocity_x;
    delete[] grid.velocity_y;
    delete[] grid.velocity_z;
    delete[] grid.organism_type;
    delete[] grid.life_state;
    delete[] grid.shadow_state;
    delete[] grid.dream_state;
    delete[] grid.entity_id;
    delete[] grid.flags;
    
    grid.energy = nullptr;
    grid.matter = nullptr;
    grid.velocity_x = nullptr;
    grid.velocity_y = nullptr;
    grid.velocity_z = nullptr;
    grid.organism_type = nullptr;
    grid.life_state = nullptr;
    grid.shadow_state = nullptr;
    grid.dream_state = nullptr;
    grid.entity_id = nullptr;
    grid.flags = nullptr;
    
    grid.total_cells = 0;
}

bool pillar_grid_allocate(PillarGridSoA& pillars, size_t width, size_t height, size_t depth) {
    pillars.alignment = new float[width * height * depth]();
    pillars.resonance = new float[width * height * depth]();
    pillars.stability = new float[width * height * depth]();
    pillars.entropy = new float[width * height * depth]();
    pillars.coherence = new float[width * height * depth]();
    pillars.flux = new float[width * height * depth]();
    pillars.manifestation = new float[width * height * depth]();
    pillars.dissolution = new float[width * height * depth]();
    
    pillars.total_cells = width * height * depth;
    
    return true;
}

void pillar_grid_deallocate(PillarGridSoA& pillars) {
    delete[] pillars.alignment;
    delete[] pillars.resonance;
    delete[] pillars.stability;
    delete[] pillars.entropy;
    delete[] pillars.coherence;
    delete[] pillars.flux;
    delete[] pillars.manifestation;
    delete[] pillars.dissolution;
    
    pillars.alignment = nullptr;
    pillars.resonance = nullptr;
    pillars.stability = nullptr;
    pillars.entropy = nullptr;
    pillars.coherence = nullptr;
    pillars.flux = nullptr;
    pillars.manifestation = nullptr;
    pillars.dissolution = nullptr;
    
    pillars.total_cells = 0;
}

bool flux_data_allocate(FluxData& flux, size_t cell_count) {
    flux.energy_flux_x = new float[cell_count]();
    flux.energy_flux_y = new float[cell_count]();
    flux.energy_flux_z = new float[cell_count]();
    flux.matter_flux_x = new float[cell_count]();
    flux.matter_flux_y = new float[cell_count]();
    flux.matter_flux_z = new float[cell_count]();
    flux.net_energy_change = new float[cell_count]();
    flux.net_matter_change = new float[cell_count]();
    
    return true;
}

void flux_data_deallocate(FluxData& flux) {
    delete[] flux.energy_flux_x;
    delete[] flux.energy_flux_y;
    delete[] flux.energy_flux_z;
    delete[] flux.matter_flux_x;
    delete[] flux.matter_flux_y;
    delete[] flux.matter_flux_z;
    delete[] flux.net_energy_change;
    delete[] flux.net_matter_change;
    
    flux.energy_flux_x = nullptr;
    flux.energy_flux_y = nullptr;
    flux.energy_flux_z = nullptr;
    flux.matter_flux_x = nullptr;
    flux.matter_flux_y = nullptr;
    flux.matter_flux_z = nullptr;
    flux.net_energy_change = nullptr;
    flux.net_matter_change = nullptr;
}

size_t get_cell_index(const CAGridSoA& grid, size_t x, size_t y, size_t z) {
    return (z * grid.height + y) * grid.width + x;
}

void get_cell_coords(const CAGridSoA& grid, size_t index, size_t& x, size_t& y, size_t& z) {
    z = index / (grid.width * grid.height);
    size_t remainder = index % (grid.width * grid.height);
    y = remainder / grid.width;
    x = remainder % grid.width;
}

}
