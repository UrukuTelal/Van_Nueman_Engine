// GodNode.h - Deity/God-Node Instantiation System
// Part of the COSMOS 16D Pillar Cosmology implementation
// Phase 7: God-Node / Deity Instantiation
//
// Matches: DeveloperConsole/backend/models/god_node.py
// Gods are parasitic belief structures that siphon Depth from subscribers.
// Worship is a PubSub channel. Graceful degradation = uninstalling bloated software.

#pragma once

#include "Entity.h"
#include <cmath>
#include <cstdio>
#include <cstring>

// ── God-Node Constants ──────────────────────────────────────────

#define GOD_NODE_DECAY_RATE 0.001f
#define GOD_NODE_SIPHON_RATE 0.01f
#define GOD_NODE_FEEDBACK_RATE 0.005f
#define GOD_NODE_MIN_DEPTH 0.001f
#define GOD_NODE_MAX_DEPTH 100.0f
#define GOD_NODE_INITIAL_DEPTH 10.0f
#define GOD_NODE_DISSOLVE_THRESHOLD 0.001f
#define GOD_NODE_MAX_SUBSCRIBERS 256
#define GOD_NODE_MAX_EVENTS 64
#define MAX_GOD_NODES 32

// ── Subscriber Record ───────────────────────────────────────────

struct GodSubscriber {
    uint32_t entity_id;
    float presence_value;
    float awareness_value;
    float subscribed_at;
    float last_siphon_at;
    float devotion;
    bool active;
};

// ── God-Node Event ──────────────────────────────────────────────

struct GodNodeEvent {
    char event_type[32];
    float depth_before;
    float depth_after;
    uint32_t entity_id;
    char description[128];

    void print() const {
        printf("GOD EVENT [%s]: depth %.3f->%.3f entity=%u | %s\n",
               event_type, depth_before, depth_after,
               entity_id, description);
    }
};

// ── God-Node ────────────────────────────────────────────────────

struct GodNode {
    char uid[16];
    char name[64];

    float depth;
    float flux_requirement;
    float age;

    int subscriber_count;
    GodSubscriber subscribers[GOD_NODE_MAX_SUBSCRIBERS];

    int event_count;
    GodNodeEvent events[GOD_NODE_MAX_EVENTS];

    PillarStateVector semantic_psv;

    void init(const char* node_name, const float* semantic_vector = nullptr,
              float initial_depth = GOD_NODE_INITIAL_DEPTH,
              float flux_req = 0.3f) {
        // Generate UID from name hash
        uint32_t hash = 2166136261u;
        for (const char* p = node_name; *p; p++) {
            hash ^= (uint8_t)(*p);
            hash *= 16777619u;
        }
        snprintf(uid, sizeof(uid), "%08x", hash ? hash : 1u);
        snprintf(name, sizeof(name), "%s", node_name);

        depth = (initial_depth < 0.0f) ? 0.0f : initial_depth;
        flux_requirement = (flux_req < 0.0f) ? 0.0f : (flux_req > 1.0f) ? 1.0f : flux_req;
        age = 0.0f;
        subscriber_count = 0;
        event_count = 0;

        // Default semantic vector or copy provided one
        if (semantic_vector) {
            for (int i = 0; i < NumPillars; i++) {
                semantic_psv[i] = vn::fp20_t(semantic_vector[i]);
            }
        } else {
            semantic_psv.fill(vn::fp20_t(0.5f));
        }
    }

    bool is_alive() const {
        return depth > GOD_NODE_DISSOLVE_THRESHOLD;
    }

    // ── Handshake: subscribe ──────────────────────────────────────

    void subscribe(uint32_t entity_id, float presence, float awareness, float current_time) {
        if (subscriber_count >= GOD_NODE_MAX_SUBSCRIBERS) return;

        GodSubscriber& sub = subscribers[subscriber_count];
        sub.entity_id = entity_id;
        sub.presence_value = presence;
        sub.awareness_value = awareness;
        sub.subscribed_at = current_time;
        sub.last_siphon_at = current_time;
        sub.devotion = 0.0f;
        sub.active = true;
        subscriber_count++;

        record_event("subscribe", depth, depth, entity_id, "Entity subscribed to deity");
    }

    void unsubscribe(uint32_t entity_id) {
        for (int i = 0; i < subscriber_count; i++) {
            if (subscribers[i].active && subscribers[i].entity_id == entity_id) {
                subscribers[i].active = false;
                record_event("unsubscribe", depth, depth, entity_id, "Entity unsubscribed from deity");
                return;
            }
        }
    }

    // ── Siphon Depth from subscribers ─────────────────────────────

    void siphon_all(float delta_time = 1.0f) {
        if (!is_alive()) return;

        float depth_before = depth;
        float total_siphon = 0.0f;

        for (int i = 0; i < subscriber_count; i++) {
            if (!subscribers[i].active) continue;

            GodSubscriber& sub = subscribers[i];
            float broadcast = sub.presence_value * sub.awareness_value;
            float devotion_mult = 1.0f + sub.devotion * 0.5f;
            float siphon_amount = broadcast * GOD_NODE_SIPHON_RATE * delta_time * devotion_mult;
            total_siphon += siphon_amount;
            sub.last_siphon_at = age;
            sub.devotion += delta_time * 0.001f;
            if (sub.devotion > 1.0f) sub.devotion = 1.0f;
        }

        depth += total_siphon;
        if (depth > GOD_NODE_MAX_DEPTH) depth = GOD_NODE_MAX_DEPTH;

        if (total_siphon > 0.001f) {
            char desc[128];
            snprintf(desc, sizeof(desc), "Siphoned %.4f Depth from %d subscribers",
                     total_siphon, subscriber_count);
            record_event("siphon", depth_before, depth, 0u, desc);
        }
    }

    // ── Feedback rotation on a subscriber ─────────────────────────

    float feedback_to_subscriber(PillarStateVector& psv, uint32_t entity_id) {
        bool found = false;
        for (int i = 0; i < subscriber_count; i++) {
            if (subscribers[i].active && subscribers[i].entity_id == entity_id) {
                found = true;
                break;
            }
        }
        if (!found) return 0.0f;

        float total_shift = 0.0f;
        int count = 0;
        const float PI = 3.14159265f;

        for (int i = 0; i < NumPillars; i++) {
            vn::fp20_t current_val = vn::fp20_t(psv[i]);
            vn::fp20_t target_val = vn::fp20_t(semantic_psv[i]);
            vn::fp20_t theta = pillar_value_to_theta(current_val);
            vn::fp20_t target_theta = pillar_value_to_theta(target_val);
            vn::fp20_t theta_diff = target_theta - theta;
            vn::fp20_t theta_abs = theta_diff < vn::fp20_t(0) ? vn::fp20_t() - theta_diff : theta_diff;
            if (theta_abs > vn::fp20_t(0.01f)) {
                vn::fp20_t rotation = theta_diff * vn::fp20_t(GOD_NODE_FEEDBACK_RATE);
                psv[i] = apply_bloch_rotation(vn::fp20_t(psv[i]), rotation);
                vn::fp20_t rot_abs = rotation < vn::fp20_t(0) ? vn::fp20_t() - rotation : rotation;
                total_shift += rot_abs.to_float();
                count++;
            }
        }

        return total_shift / (float)(count > 0 ? count : 1);
    }

    // ── Tick: decay depth and check dissolution ───────────────────

    void tick(float delta_time = 1.0f) {
        if (!is_alive()) return;

        age += delta_time;
        float depth_before = depth;

        float decay = depth * GOD_NODE_DECAY_RATE * delta_time;
        depth -= decay;

        float flux_deficit = (flux_requirement > 0.5f) ? flux_requirement - 0.5f : 0.0f;
        if (flux_deficit > 0.01f) {
            float extra_decay = depth * flux_deficit * delta_time * 0.01f;
            depth -= extra_decay;
        }

        if (depth < 0.0f) depth = 0.0f;

        if (depth_before - depth > 0.001f) {
            char desc[128];
            snprintf(desc, sizeof(desc), "Depth decayed by %.4f", depth_before - depth);
            record_event("depth_change", depth_before, depth, 0u, desc);
        }

        if (!is_alive() && depth_before > GOD_NODE_DISSOLVE_THRESHOLD) {
            decommission();
        }
    }

    // ── Graceful decommission ─────────────────────────────────────

    void decommission() {
        char desc[128];
        snprintf(desc, sizeof(desc),
                 "Deity '%s' gracefully dissolved. SYSTEM_DEPRECATED broadcast to %d subscribers.",
                 name, subscriber_count);
        record_event("decommission", depth, 0.0f, 0u, desc);
        depth = 0.0f;
        subscriber_count = 0;
    }

    // ── Event recording ───────────────────────────────────────────

    void record_event(const char* type, float db, float da,
                      uint32_t entity_id, const char* desc) {
        if (event_count >= GOD_NODE_MAX_EVENTS) {
            for (int i = 1; i < GOD_NODE_MAX_EVENTS; i++) {
                events[i - 1] = events[i];
            }
            event_count = GOD_NODE_MAX_EVENTS - 1;
        }

        snprintf(events[event_count].event_type, sizeof(events[event_count].event_type), "%s", type);
        events[event_count].depth_before = db;
        events[event_count].depth_after = da;
        events[event_count].entity_id = entity_id;
        snprintf(events[event_count].description, sizeof(events[event_count].description), "%s", desc);
        event_count++;
    }
};

// ── God-Node Field: manages all active deities ──────────────────

struct GodNodeField {
    int node_count;
    GodNode nodes[MAX_GOD_NODES];
    int decommissioned_count;
    GodNode decommissioned_nodes[MAX_GOD_NODES];

    void init() {
        node_count = 0;
        decommissioned_count = 0;
    }

    int active_count() const {
        int count = 0;
        for (int i = 0; i < node_count; i++) {
            if (nodes[i].is_alive()) count++;
        }
        return count;
    }

    GodNode* create(const char* name, const float* semantic_vector = nullptr,
                    float initial_depth = GOD_NODE_INITIAL_DEPTH,
                    float flux_requirement = 0.3f) {
        if (node_count >= MAX_GOD_NODES) return nullptr;

        GodNode& node = nodes[node_count];
        node.init(name, semantic_vector, initial_depth, flux_requirement);
        node_count++;
        return &nodes[node_count - 1];
    }

    GodNode* get(const char* uid) {
        for (int i = 0; i < node_count; i++) {
            if (strcmp(nodes[i].uid, uid) == 0) return &nodes[i];
        }
        return nullptr;
    }

    GodNode* get_by_name(const char* name) {
        for (int i = 0; i < node_count; i++) {
            if (strcmp(nodes[i].name, name) == 0) return &nodes[i];
        }
        return nullptr;
    }

    bool subscribe(const char* god_uid, uint32_t entity_id,
                   PillarStateVector& psv, float current_time = 0.0f) {
        GodNode* node = get(god_uid);
        if (!node || !node->is_alive()) return false;

        float presence = psv[Presence];
        float awareness = psv[Awareness];
        node->subscribe(entity_id, presence, awareness, current_time);

        // Handshake: rotate Relation pillar toward deity
        vn::fp20_t current_rel = vn::fp20_t(psv[Relation]);
        vn::fp20_t deity_rel = vn::fp20_t(node->semantic_psv[Relation]);
        vn::fp20_t theta = pillar_value_to_theta(current_rel);
        vn::fp20_t target_theta = pillar_value_to_theta(deity_rel);
        vn::fp20_t diff = target_theta - theta;
        psv[Relation] = apply_bloch_rotation(vn::fp20_t(psv[Relation]), diff * vn::fp20_t(0.1f));

        return true;
    }

    void tick_all(float delta_time = 1.0f) {
        for (int i = 0; i < node_count; i++) {
            if (!nodes[i].is_alive()) continue;

            nodes[i].siphon_all(delta_time);
            nodes[i].tick(delta_time);

            if (!nodes[i].is_alive()) {
                if (decommissioned_count < MAX_GOD_NODES) {
                    decommissioned_nodes[decommissioned_count++] = nodes[i];
                }
                // Compact the nodes array
                for (int j = i; j < node_count - 1; j++) {
                    nodes[j] = nodes[j + 1];
                }
                node_count--;
                i--;
            }
        }
    }

    bool decommission(const char* god_uid) {
        GodNode* node = get(god_uid);
        if (!node) return false;

        node->decommission();

        if (decommissioned_count < MAX_GOD_NODES) {
            decommissioned_nodes[decommissioned_count++] = *node;
        }

        // Remove from active array
        for (int i = 0; i < node_count; i++) {
            if (strcmp(nodes[i].uid, god_uid) == 0) {
                for (int j = i; j < node_count - 1; j++) {
                    nodes[j] = nodes[j + 1];
                }
                node_count--;
                break;
            }
        }
        return true;
    }
};
