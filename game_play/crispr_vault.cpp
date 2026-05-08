// crispr_vault.cpp - CRISPR Vault implementation
// Store discovered life genomes, custom creature DNA, trade/share with others

#include "crispr_vault.h"
#include <ctime>
#include <cstdlib>
#include <cmath>

CRISPRVault::CRISPRVault(uint32_t player_id) : 
    player_id_(player_id), next_genome_id_(3000), capacity_(128) {
    genomes_.reserve(capacity_);
}

uint32_t CRISPRVault::add_discovered(const char* species_name, uint32_t planet_id,
                                     const float pillar_template[16], float fitness) {
    if (genomes_.size() >= capacity_) {
        return 0;  // Vault full
    }
    
    GenomeEntry entry;
    entry.id = next_genome_id_++;
    strncpy(entry.species_name, species_name, 127);
    entry.species_name[127] = '\0';
    entry.description[0] = '\0';
    entry.planet_id = planet_id;
    entry.fitness_score = fitness;
    entry.generation = 0;
    entry.is_custom = false;
    entry.body_size = 0.5f;
    entry.metabolism = 0.5f;
    entry.adaptation = 0.3f;
    entry.discovered_timestamp = static_cast<uint32_t>(time(nullptr));
    
    if (pillar_template) {
        for (int i = 0; i < 16; i++) {
            entry.pillar_template[i] = pillar_template[i];
        }
    }
    
    genomes_.push_back(entry);
    return entry.id;
}

uint32_t CRISPRVault::create_custom(const char* creature_name,
                                    const uint32_t* parent_genome_ids, uint32_t parent_count) {
    if (genomes_.size() >= capacity_ || parent_count == 0) {
        return 0;
    }
    
    GenomeEntry entry;
    entry.id = next_genome_id_++;
    strncpy(entry.species_name, creature_name, 127);
    entry.species_name[127] = '\0';
    entry.description[0] = '\0';
    entry.planet_id = 0;  // Custom creatures have no planet
    entry.fitness_score = 0.0f;
    entry.generation = 1;
    entry.is_custom = true;
    entry.body_size = 0.5f;
    entry.metabolism = 0.5f;
    entry.adaptation = 0.3f;
    entry.discovered_timestamp = static_cast<uint32_t>(time(nullptr));
    
    // Mix parent genomes
    float mixed_pillars[16] = {0.5f};
    mix_genomes(parent_genome_ids, parent_count, mixed_pillars, entry.fitness_score);
    
    for (int i = 0; i < 16; i++) {
        entry.pillar_template[i] = mixed_pillars[i];
    }
    
    genomes_.push_back(entry);
    return entry.id;
}

const GenomeEntry* CRISPRVault::get_genome(uint32_t genome_id) const {
    for (const auto& g : genomes_) {
        if (g.id == genome_id) return &g;
    }
    return nullptr;
}

uint32_t CRISPRVault::get_discovered_count() const {
    uint32_t count = 0;
    for (const auto& g : genomes_) {
        if (!g.is_custom) count++;
    }
    return count;
}

uint32_t CRISPRVault::get_custom_count() const {
    uint32_t count = 0;
    for (const auto& g : genomes_) {
        if (g.is_custom) count++;
    }
    return count;
}

bool CRISPRVault::export_genome(uint32_t genome_id, char* out_buffer, uint32_t buffer_size) const {
    const GenomeEntry* g = get_genome(genome_id);
    if (!g || buffer_size < 512) return false;
    
    int len = snprintf(out_buffer, buffer_size,
        "GENOME:%u|NAME:%s|FITNESS:%.2f|GENERATION:%u|CUSTOM:%d|PILLARS:%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f",
        g->id, g->species_name, g->fitness_score, g->generation, g->is_custom,
        g->pillar_template[0], g->pillar_template[1], g->pillar_template[2], g->pillar_template[3],
        g->pillar_template[4], g->pillar_template[5], g->pillar_template[6], g->pillar_template[7],
        g->pillar_template[8], g->pillar_template[9], g->pillar_template[10], g->pillar_template[11],
        g->pillar_template[12], g->pillar_template[13], g->pillar_template[14], g->pillar_template[15]);
    
    return len > 0;
}

uint32_t CRISPRVault::import_genome(const char* genome_data) {
    // Parse exported genome format
    // Format: GENOME:id|NAME:name|FITNESS:f|GENERATION:g|CUSTOM:0/1|PILLARS:p0,p1,...
    if (strncmp(genome_data, "GENOME:", 7) != 0) return 0;
    
    GenomeEntry entry;
    entry.id = next_genome_id_++;
    entry.is_custom = false;
    entry.discovered_timestamp = static_cast<uint32_t>(time(nullptr));
    
    // Simplified parsing - in production use proper parser
    const char* name_start = strstr(genome_data, "|NAME:");
    if (name_start) {
        name_start += 6;
        const char* name_end = strchr(name_start, '|');
        int name_len = name_end ? (int)(name_end - name_start) : 127;
        if (name_len > 127) name_len = 127;
        strncpy(entry.species_name, name_start, name_len);
        entry.species_name[name_len] = '\0';
    }
    
    for (int i = 0; i < 16; i++) {
        entry.pillar_template[i] = 0.5f;
    }
    
    genomes_.push_back(entry);
    return entry.id;
}

bool CRISPRVault::delete_genome(uint32_t genome_id) {
    for (auto it = genomes_.begin(); it != genomes_.end(); ++it) {
        if (it->id == genome_id) {
            genomes_.erase(it);
            return true;
        }
    }
    return false;
}

void CRISPRVault::mix_genomes(const uint32_t* parent_ids, uint32_t count,
                               float out_pillars[16], float& out_fitness) {
    // Average parent pillar templates
    for (int i = 0; i < 16; i++) {
        out_pillars[i] = 0.0f;
    }
    out_fitness = 0.0f;
    
    uint32_t valid_parents = 0;
    for (uint32_t i = 0; i < count; i++) {
        const GenomeEntry* parent = get_genome(parent_ids[i]);
        if (parent) {
            for (int j = 0; j < 16; j++) {
                out_pillars[j] += parent->pillar_template[j];
            }
            out_fitness += parent->fitness_score;
            valid_parents++;
        }
    }
    
    if (valid_parents > 0) {
        for (int i = 0; i < 16; i++) {
            out_pillars[i] /= valid_parents;
            // Add small mutation
            out_pillars[i] += (rand() % 20 - 10) / 100.0f;
            if (out_pillars[i] < 0.0f) out_pillars[i] = 0.0f;
            if (out_pillars[i] > 1.0f) out_pillars[i] = 1.0f;
        }
        out_fitness /= valid_parents;
    }
}
