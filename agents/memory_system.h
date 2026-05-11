#ifndef VAN_NUEMAN_AGENTS_MEMORY_SYSTEM_H
#define VAN_NUEMAN_AGENTS_MEMORY_SYSTEM_H

#include <cstdint>
#include <array>
#include <vector>
#include <string>
#include <memory>
#include <unordered_map>
#include <functional>
#include <algorithm>
#include <optional>
#include <chrono>
#include "cognition.h"

constexpr int MEMORY_DIMENSION = 512;
constexpr int MAX_MEMORIES_PER_AGENT = 100000;
constexpr float DEFAULT_CONSOLIDATION_THRESHOLD = 0.7f;

enum class MemoryType {
    EPISODIC,
    SEMANTIC,
    PROCEDURAL,
    COGNITIVE,
    DREAM
};

struct MemoryItem {
    std::string id;
    std::string content;
    std::array<float, MEMORY_DIMENSION> embedding;
    MemoryType type;
    float importance;
    uint32_t tick_created;
    uint32_t tick_last_accessed;
    bool consolidated;
    std::unordered_map<std::string, std::string> metadata;
    PillarVector associated_pillars;
};

struct MemoryQuery {
    std::string query_text;
    std::array<float, MEMORY_DIMENSION> query_embedding;
    std::optional<MemoryType> type_filter;
    std::optional<uint32_t> tick_range_start;
    std::optional<uint32_t> tick_range_end;
    uint32_t max_results;
    bool include_consolidated;
};

struct MemoryRetrievalResult {
    std::shared_ptr<MemoryItem> item;
    float score;
    uint32_t relevance_rank;
};

class MemoryStore {
public:
    MemoryStore(int agent_id, const std::string& db_path = "");
    ~MemoryStore();

    std::string store(const std::string& content, MemoryType type,
                      const PillarVector& pillars, float importance = 0.5f,
                      const std::unordered_map<std::string, std::string>& metadata = {});

    std::string store_cognitive(const CognitiveState& state, uint32_t tick);

    std::string store_dream(const DreamState& dream_state, uint32_t tick);

    std::vector<MemoryRetrievalResult> retrieve(const MemoryQuery& query) const;

    std::vector<MemoryRetrievalResult> recall(const std::string& query_text, uint32_t k = 5) const;

    std::optional<std::shared_ptr<MemoryItem>> get(const std::string& memory_id) const;

    bool remove(const std::string& memory_id);

    int consolidate(float threshold = DEFAULT_CONSOLIDATION_THRESHOLD);

    int prune(uint32_t max_memories = MAX_MEMORIES_PER_AGENT);

    void apply_pillar_weights(std::vector<MemoryRetrievalResult>& results,
                              const std::array<float, NUM_PILLARS>& weights) const;

    struct Statistics {
        uint32_t total_memories;
        uint32_t consolidated_count;
        uint32_t episodic_count;
        uint32_t semantic_count;
        uint32_t procedural_count;
        uint32_t cognitive_count;
        uint32_t dream_count;
        float average_importance;
        uint32_t oldest_tick;
        uint32_t newest_tick;
    };

    Statistics get_statistics() const;

    void set_embedding_callback(std::function<std::array<float, MEMORY_DIMENSION>(const std::string&)> callback);

private:
    int agent_id_;
    std::string db_path_;
    std::vector<std::shared_ptr<MemoryItem>> memories_;
    std::unordered_map<std::string, size_t> memory_index_;
    uint32_t current_tick_;
    std::function<std::array<float, MEMORY_DIMENSION>(const std::string&)> embedding_callback_;

    std::array<float, MEMORY_DIMENSION> generate_embedding(const std::string& text) const;

    float cosine_similarity(const std::array<float, MEMORY_DIMENSION>& a,
                            const std::array<float, MEMORY_DIMENSION>& b) const;

    void update_metadata(std::shared_ptr<MemoryItem> item);

    void persist_to_disk() const;

    void load_from_disk();
};

class DreamConsolidator {
public:
    DreamConsolidator(MemoryStore& store);

    void consolidate_dreams(float importance_threshold = 0.6f);

    void process_shadow_patterns(const DreamState& dream_state);

    void extract_semantic_memories(const DreamEpisode& episode);

private:
    MemoryStore& store_;
    std::vector<std::string> processed_dream_ids_;
};

class PillarWeightCalculator {
public:
    static std::array<float, NUM_PILLARS> calculate_retrieval_weights(
        const PillarVector& agent_pillars,
        const std::string& retrieval_context);

    static float calculate_importance(
        const PillarVector& pillars_before,
        const PillarVector& pillars_after,
        float emotional_significance);
};

inline std::string memory_id_from_content(const std::string& content, uint32_t tick, int agent_id) {
    std::string combined = std::to_string(agent_id) + content + std::to_string(tick);
    std::hash<std::string> hasher;
    size_t hash = hasher(combined);
    return std::to_string(hash).substr(0, 16);
}

#endif // VAN_NUEMAN_AGENTS_MEMORY_SYSTEM_H