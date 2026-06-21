#pragma once

#include "../agents/cognition.h"
#include "../biology/society_network.h"
#include "../biology/voxel_organism.h"
#include "../include/Entity.h"
#include "../audio/wht_scaled.h"
#include <vector>
#include <cmath>
#include <cstdint>
#include <cstdlib>

struct DreamSandbox {
    float pillar_scale[NumPillars];
    float physics_looseness;
    float coherence;

    DreamSandbox() : physics_looseness(0.3f), coherence(1.0f) {
        for (int i = 0; i < NumPillars; i++) pillar_scale[i] = 1.0f;
    }
};

struct InternalInsight {
    PillarVector delta;
    float clarity;
    uint32_t dream_episode_index;
    uint32_t tick_formed;

    InternalInsight() : clarity(0), dream_episode_index(0), tick_formed(0) {
        delta.fill(vn::fp20_t(0));
    }
};

struct PlayObjective {
    PillarVector target_pillar_delta;
    std::string description;
    float expected_difficulty;
    bool attempted;

    PlayObjective() : expected_difficulty(0.5f), attempted(false) {
        target_pillar_delta.fill(vn::fp20_t(0));
    }
};

enum class ArtifactType : uint8_t {
    Undefined = 0,
    Tool = 1,
    WrittenRecord = 2,
    Art = 3,
    Software = 4,
    CulturalPractice = 5,
    SpiritualInsight = 6
};

inline const char* artifact_type_name(ArtifactType t) {
    switch (t) {
        case ArtifactType::Tool: return "Tool";
        case ArtifactType::WrittenRecord: return "WrittenRecord";
        case ArtifactType::Art: return "Art";
        case ArtifactType::Software: return "Software";
        case ArtifactType::CulturalPractice: return "CulturalPractice";
        case ArtifactType::SpiritualInsight: return "SpiritualInsight";
        default: return "Undefined";
    }
}

struct Artifact {
    ArtifactType type;
    PillarVector essence;
    float quality;
    float complexity;
    uint32_t tick_created;

    Artifact() : type(ArtifactType::Undefined), quality(0), complexity(0), tick_created(0) {
        essence.fill(vn::fp20_t(0));
    }
};

struct TeachingResult {
    PillarVector knowledge_delta;
    float quality_score;
    bool student_understood;

    TeachingResult() : quality_score(0), student_understood(false) {
        knowledge_delta.fill(vn::fp20_t(0));
    }
};

struct LivedExperience {
    PillarVector pillar_imprint;
    uint32_t tick;
    float intensity;

    LivedExperience() : tick(0), intensity(0) {
        pillar_imprint.fill(vn::fp20_t(0));
    }
};

enum class DreamPipelineStage : uint8_t {
    Idle = 0,
    Dreaming = 1,
    ProcessingInternally = 2,
    FormingPlayObjective = 3,
    Playing = 4,
    Learning = 5,
    Externalizing = 6,
    Teaching = 7
};

inline const char* pipeline_stage_name(DreamPipelineStage s) {
    switch (s) {
        case DreamPipelineStage::Idle: return "Idle";
        case DreamPipelineStage::Dreaming: return "Dreaming";
        case DreamPipelineStage::ProcessingInternally: return "ProcessingInternally";
        case DreamPipelineStage::FormingPlayObjective: return "FormingPlayObjective";
        case DreamPipelineStage::Playing: return "Playing";
        case DreamPipelineStage::Learning: return "Learning";
        case DreamPipelineStage::Externalizing: return "Externalizing";
        case DreamPipelineStage::Teaching: return "Teaching";
        default: return "Unknown";
    }
}

struct DreamPipeline {
    DreamPipelineStage stage;
    uint32_t stage_ticks;
    LivedExperience recent_experiences[16];
    uint32_t experience_count;
    uint32_t experience_head;
    std::vector<InternalInsight> pending_insights;
    std::vector<PlayObjective> play_queue;
    std::vector<Artifact> artifacts;
    float pipeline_progress;

    DreamPipeline() : stage(DreamPipelineStage::Idle), stage_ticks(0),
                      experience_count(0), experience_head(0), pipeline_progress(0) {}
};

class DreamEngine {
public:
    DreamEngine() {}

    void record_experience(DreamPipeline& pipe, const PillarStateVector& psv, float intensity) {
        LivedExperience& le = pipe.recent_experiences[pipe.experience_head];
        for (int i = 0; i < NumPillars; i++)
            le.pillar_imprint[i] = psv.pillars[i];
        le.tick = pipe.stage_ticks;
        le.intensity = intensity;
        pipe.experience_head = (pipe.experience_head + 1) % 16;
        if (pipe.experience_count < 16) pipe.experience_count++;
    }

    void generate_dream(DreamState& ds, const DreamPipeline& pipe,
                        const PillarStateVector& entity_pillars,
                        const DreamSandbox* external_influence) {
        ds.dream_content.clear();

        DreamEpisode episode;
        episode.duration_ticks = 10 + (std::rand() % 20);
        episode.emotional_valence = entity_pillars[Warmth] * 2.0f - 1.0f;
        episode.resolution = ResolutionState::UNRESOLVED;

        DreamSandbox sandbox;
        if (external_influence) sandbox = *external_influence;

        for (int p = 0; p < NumPillars; p++) {
            float base = entity_pillars[p];
            float lived = 0.5f;
            if (pipe.experience_count > 0) {
                for (uint32_t e = 0; e < pipe.experience_count; e++) {
                    lived += pipe.recent_experiences[e].pillar_imprint[p];
                }
                lived /= (pipe.experience_count + 1);
            }

            float altered = base * sandbox.pillar_scale[p] * (1.0f - sandbox.physics_looseness * 0.5f)
                          + lived * sandbox.physics_looseness
                          + (static_cast<float>(std::rand() % 200 - 100) / 1000.0f) * sandbox.physics_looseness;

            if (altered < 0.0f) altered = 0.0f;
            if (altered > 1.0f) altered = 1.0f;
            episode.simulated_pillars[p] = vn::fp20_t(altered);
        }

        ds.dream_content.push_back(episode);
        ds.is_dreaming = true;
        ds.rem_cycles++;
    }

    void check_lucid(DreamState& ds, const PillarStateVector& entity_pillars) {
        float willpower = entity_pillars[Willpower];
        float awareness = entity_pillars[Awareness];
        float lucid_threshold = 0.5f + 0.2f * (1.0f - awareness);

        if (willpower > lucid_threshold && ds.is_dreaming && ds.rem_cycles > 2) {
            ds.current_level = DreamLevel::LUCID;
            ds.lucid_level = (willpower - lucid_threshold) / (1.0f - lucid_threshold);
            if (ds.lucid_level > 1.0f) ds.lucid_level = 1.0f;
        }
    }

    InternalInsight process_internally(const DreamState& ds,
                                        const PillarStateVector& entity_pillars,
                                        uint32_t current_tick) {
        InternalInsight insight;
        if (ds.dream_content.empty()) return insight;

        const DreamEpisode& latest = ds.dream_content.back();
        float awareness = entity_pillars[Awareness];
        float memory = entity_pillars[Memory];

        insight.clarity = (awareness * 0.6f + memory * 0.4f) * ds.lucid_level * 0.5f + 0.1f;
        if (insight.clarity > 1.0f) insight.clarity = 1.0f;

        for (int p = 0; p < NumPillars; p++) {
            float diff = latest.simulated_pillars[p] - entity_pillars[p];
            insight.delta[p] = vn::fp20_t(diff * insight.clarity * 0.3f);
        }

        insight.dream_episode_index = static_cast<uint32_t>(ds.dream_content.size() - 1);
        insight.tick_formed = current_tick;
        return insight;
    }

    PlayObjective insight_to_play_objective(const InternalInsight& insight,
                                             const PillarStateVector& entity_pillars) {
        PlayObjective obj;
        float willpower = entity_pillars[Willpower];
        float force = entity_pillars[Force];

        for (int p = 0; p < NumPillars; p++) {
            float shift = insight.delta[p] * (0.5f + willpower * 0.5f);
            obj.target_pillar_delta[p] = vn::fp20_t(shift);
        }

        float total_delta = 0.0f;
        for (int p = 0; p < NumPillars; p++)
            total_delta += fabsf(insight.delta[p]);
        obj.expected_difficulty = total_delta / (1.0f + force);
        if (obj.expected_difficulty > 1.0f) obj.expected_difficulty = 1.0f;

        return obj;
    }

    float play_to_learning(PillarStateVector& entity_pillars,
                            const PlayObjective& objective, float dt) {
        float total_shift = 0.0f;
        float memory_factor = entity_pillars[Memory] * 0.5f + 0.5f;

        for (int p = 0; p < NumPillars; p++) {
            float delta = objective.target_pillar_delta[p];
            float absorbed = delta * memory_factor * dt * 0.1f;
            float current = entity_pillars[p];
            current += absorbed;
            if (current < 0.0f) current = 0.0f;
            if (current > 1.0f) current = 1.0f;
            entity_pillars[p] = vn::fp20_t(current);
            total_shift += fabsf(absorbed);
        }
        return total_shift;
    }

    Artifact externalize(const PillarStateVector& entity_pillars, ArtifactType type,
                         uint32_t current_tick) {
        Artifact artifact;
        artifact.type = type;
        artifact.tick_created = current_tick;
        float memory = entity_pillars[Memory];
        float awareness = entity_pillars[Awareness];
        float integrity = entity_pillars[Integrity];

        artifact.quality = (memory * 0.4f + awareness * 0.3f + integrity * 0.3f);
        if (artifact.quality > 1.0f) artifact.quality = 1.0f;

        artifact.complexity = (memory + awareness) * 0.5f;
        if (artifact.complexity > 1.0f) artifact.complexity = 1.0f;

        for (int p = 0; p < NumPillars; p++)
            artifact.essence[p] = entity_pillars[p];

        return artifact;
    }

    TeachingResult teach(const Artifact& artifact,
                          const PillarStateVector& teacher_pillars,
                          const PillarStateVector& student_pillars) {
        TeachingResult result;

        float teacher_warmth = teacher_pillars[Warmth];
        float teacher_relation = teacher_pillars[Relation];
        float teacher_distortion = teacher_pillars[Distortion];
        float student_awareness = student_pillars[Awareness];
        float student_willpower = student_pillars[Willpower];
        float student_memory = student_pillars[Memory];

        float rapport = teacher_warmth * teacher_relation;
        float receptivity = student_awareness * (0.5f + student_willpower * 0.5f);
        float clarity = 1.0f - teacher_distortion * 0.5f;

        result.quality_score = rapport * receptivity * clarity * artifact.quality;
        if (result.quality_score > 1.0f) result.quality_score = 1.0f;

        result.student_understood = result.quality_score > 0.2f;

        for (int p = 0; p < NumPillars; p++) {
            float knowledge = artifact.essence[p];
            float transferred = knowledge * result.quality_score * student_memory;
            result.knowledge_delta[p] = vn::fp20_t(transferred);
        }

        return result;
    }

    void propagate_knowledge(const Artifact& artifact,
                              SocietyNetwork& network, uint32_t teacher_node_id,
                              const PillarStateVector& teacher_pillars) {
        const SocietyNode& teacher = network.get_node(teacher_node_id);

        for (uint32_t neighbor_id : teacher.connected_nodes) {
            const SocietyNode& student_node = network.get_node(neighbor_id);
            if (!student_node.organism || !student_node.organism->is_alive()) continue;

            PillarStateVector student_pillars = student_node.organism->pillar_state();
            TeachingResult tr = teach(artifact, teacher_pillars, student_pillars);

            if (tr.student_understood) {
                ResourcePacket packet;
                for (int p = 0; p < NumPillars; p++)
                    packet.information += tr.knowledge_delta[p];
                packet.information /= NumPillars;
            }
        }
    }

    bool pipeline_tick(DreamPipeline& pipe, DreamState& ds,
                       PillarStateVector& entity_pillars,
                       SocietyNetwork* network, uint32_t entity_node_id,
                       float dt, uint32_t current_tick,
                       const DreamSandbox* external_influence) {
        pipe.stage_ticks++;

        switch (pipe.stage) {
            case DreamPipelineStage::Idle: {
                float awareness = entity_pillars[Awareness];
                float warmth = entity_pillars[Warmth];
                if (warmth < 0.3f && awareness < 0.5f && pipe.stage_ticks > 50) {
                    pipe.stage = DreamPipelineStage::Dreaming;
                    pipe.stage_ticks = 0;
                    generate_dream(ds, pipe, entity_pillars, external_influence);
                }
                break;
            }

            case DreamPipelineStage::Dreaming: {
                check_lucid(ds, entity_pillars);
                DreamEpisode& current = ds.dream_content.back();
                current.duration_ticks--;

                if (ds.current_level == DreamLevel::LUCID) {
                    ds.lucid_level += dt * 0.01f;
                    if (ds.lucid_level > 1.0f) ds.lucid_level = 1.0f;
                }

                if (current.duration_ticks <= 0 || pipe.stage_ticks > 100) {
                    ds.is_dreaming = false;
                    pipe.stage = DreamPipelineStage::ProcessingInternally;
                    pipe.stage_ticks = 0;
                }
                break;
            }

            case DreamPipelineStage::ProcessingInternally: {
                if (pipe.stage_ticks >= 5) {
                    InternalInsight insight = process_internally(ds, entity_pillars, current_tick);
                    if (insight.clarity > 0.1f) {
                        pipe.pending_insights.push_back(insight);
                    }
                    pipe.stage = DreamPipelineStage::FormingPlayObjective;
                    pipe.stage_ticks = 0;
                }
                break;
            }

            case DreamPipelineStage::FormingPlayObjective: {
                if (!pipe.pending_insights.empty()) {
                    InternalInsight insight = pipe.pending_insights.back();
                    pipe.pending_insights.pop_back();
                    PlayObjective obj = insight_to_play_objective(insight, entity_pillars);
                    pipe.play_queue.push_back(obj);
                }
                if (pipe.play_queue.empty()) {
                    pipe.stage = DreamPipelineStage::Idle;
                } else {
                    pipe.stage = DreamPipelineStage::Playing;
                }
                pipe.stage_ticks = 0;
                break;
            }

            case DreamPipelineStage::Playing: {
                if (!pipe.play_queue.empty() && pipe.stage_ticks < 30) {
                    PlayObjective& obj = pipe.play_queue.front();
                    float shift = play_to_learning(entity_pillars, obj, dt);
                    pipe.pipeline_progress += shift;
                    if (pipe.stage_ticks >= 20 || pipe.pipeline_progress > obj.expected_difficulty) {
                        obj.attempted = true;
                        pipe.stage = DreamPipelineStage::Learning;
                        pipe.stage_ticks = 0;
                    }
                } else {
                    pipe.stage = DreamPipelineStage::Learning;
                    pipe.stage_ticks = 0;
                }
                break;
            }

            case DreamPipelineStage::Learning: {
                if (pipe.stage_ticks >= 10) {
                    if (!pipe.play_queue.empty()) pipe.play_queue.erase(pipe.play_queue.begin());
                    pipe.stage = DreamPipelineStage::Externalizing;
                    pipe.stage_ticks = 0;
                }
                break;
            }

            case DreamPipelineStage::Externalizing: {
                if (pipe.stage_ticks >= 15) {
                    ArtifactType types[] = {
                        ArtifactType::Tool, ArtifactType::WrittenRecord,
                        ArtifactType::Art, ArtifactType::Software,
                        ArtifactType::CulturalPractice, ArtifactType::SpiritualInsight
                    };
                    ArtifactType chosen = types[std::rand() % 6];
                    Artifact art = externalize(entity_pillars, chosen, current_tick);
                    pipe.artifacts.push_back(art);
                    pipe.stage = DreamPipelineStage::Teaching;
                    pipe.stage_ticks = 0;
                    pipe.pipeline_progress = 0.0f;
                }
                break;
            }

            case DreamPipelineStage::Teaching: {
                if (!pipe.artifacts.empty() && network && pipe.stage_ticks >= 5) {
                    Artifact& art = pipe.artifacts.back();
                    propagate_knowledge(art, *network, entity_node_id, entity_pillars);
                }
                if (pipe.stage_ticks >= 20) {
                    pipe.stage = DreamPipelineStage::Idle;
                    pipe.stage_ticks = 0;
                }
                break;
            }
        }

        return pipe.stage != DreamPipelineStage::Idle;
    }
};
