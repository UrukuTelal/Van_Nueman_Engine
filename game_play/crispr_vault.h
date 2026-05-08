// crispr_vault.h - CRISPR Vault for storing discovered life genomes
// From GAMEPLAY.md: "CRISPR vault (pre-installed ship module for genome storage)"
// Store discovered life, custom creature DNA, trade/share with others

#pragma once

#include <cstdint>
#include <cstddef>
#include <vector>
#include <cstring>

// Genome/DNA entry in the vault
struct GenomeEntry {
    uint32_t id;
    char species_name[128];
    char description[256];
    uint32_t planet_id;           // Where it was discovered
    float fitness_score;
    uint32_t generation;
    bool is_custom;                // Custom-designed vs discovered
    
    // Pillar template for this genome
    float pillar_template[16];    // NUM_PILLARS = 16
    
    // Physical traits (for creature generation)
    float body_size;               // 0.0 - 1.0
    float metabolism;               // Energy efficiency
    float adaptation;              // Environmental adaptation rate
    
    uint32_t discovered_timestamp;
};

// CRISPR Vault system
class CRISPRVault {
public:
    CRISPRVault(uint32_t player_id);
    
    // Add discovered specimen to vault
    uint32_t add_discovered(const char* species_name, uint32_t planet_id,
                           const float pillar_template[16], float fitness);
    
    // Create custom creature from existing genomes
    uint32_t create_custom(const char* creature_name, 
                          const uint32_t* parent_genome_ids, uint32_t parent_count);
    
    // Get genome by ID
    const GenomeEntry* get_genome(uint32_t genome_id) const;
    
    // List all genomes
    const std::vector<GenomeEntry>& get_all_genomes() const { return genomes_; }
    
    // Count specimens
    uint32_t get_discovered_count() const;
    uint32_t get_custom_count() const;
    
    // Export genome for sharing/trading
    bool export_genome(uint32_t genome_id, char* out_buffer, uint32_t buffer_size) const;
    
    // Import genome from trade
    uint32_t import_genome(const char* genome_data);
    
    // Delete genome (free up vault space)
    bool delete_genome(uint32_t genome_id);
    
    // Get vault capacity
    uint32_t get_capacity() const { return capacity_; }
    uint32_t get_used() const { return static_cast<uint32_t>(genomes_.size()); }
    
private:
    uint32_t player_id_;
    std::vector<GenomeEntry> genomes_;
    uint32_t next_genome_id_;
    uint32_t capacity_;              // Max genomes the vault can hold
    
    // Helper: mix parent genomes to create custom creature
    void mix_genomes(const uint32_t* parent_ids, uint32_t count, 
                    float out_pillars[16], float& out_fitness);
};
