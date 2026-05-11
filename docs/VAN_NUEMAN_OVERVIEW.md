# **Van_Nueman**  
### *A voxel‑based cognitive simulation engine for governance‑constrained AI agents.*

Van_Nueman is a **multi‑layer voxel simulation engine** designed to demonstrate the **Pillar‑Constrained Multi‑Scale Representative Model (PCMSRM)** inside a living, evolving world. It integrates:

- voxel‑based world generation  
- chunked spatial partitioning  
- multi‑agent cognition  
- pillar‑driven governance constraints  
- emergent systemic behavior  
- multi‑scale simulation loops  

This project serves as the **runtime environment** and **visual demonstration layer** for the Pillar AI Configuration Suite.

---

## 🌍 **Core Concept**

Where **Pillar AI** defines the *cognitive architecture*,  
**Van_Nueman** provides the *world* in which that architecture comes alive.

Agents in Van_Nueman:

- perceive voxel environments  
- maintain internal pillar‑state vectors  
- act under governance constraints  
- interact with terrain, structures, and each other  
- generate emergent behavior patterns  
- exhibit systemic failure modes (Harm, Distortion, Resistance)  
- evolve over time through multi‑scale loops  

This is not a grid‑world.  
It is a **voxel‑based cognitive ecosystem**.

---

## 🧱 **Architecture Overview**

```
Van_Nueman/
├── world/
│   ├── voxel_types.md          # Material definitions, physical rules
│   ├── chunking.md             # Spatial partitioning, LOD, memory model
│   ├── worldgen.md             # Procedural generation rules
│   └── environment_state.md    # Temperature, pressure, resources, hazards
│
├── agents/
│   ├── cognition.md            # Internal state vectors, decision loops
│   ├── perception.md           # Voxel sensing, line-of-sight, awareness
│   ├── behavior.md             # Pillar-driven action selection
│   └── interactions.md         # Agent-agent and agent-world interactions
│
├── simulation/
│   ├── tick_loop.md            # Micro/meso/macro simulation cycles
│   ├── emergent_patterns.md    # Group dynamics, collapse modes
│   └── pillar_integration.md   # PCMSRM integration layer
│
├── api/
│   ├── rest_api.md             # Planned REST interface
│   ├── websocket_api.md        # Real-time simulation streaming
│   └── llm_bridge.md           # Pillar AI → Van_Nueman bridge
│
└── ui/
    ├── rendering.md            # Voxel rendering pipeline
    ├── overlays.md             # Pillar HUD, agent state visualization
    └── controls.md             # Camera, debug tools
```

---

## 🧠 **Cognitive Model (PCMSRM Integration)**

Each agent maintains a **14‑dimensional pillar vector**:

```
[Awareness, Willpower, Force, Influence, Resistance, Integrity,
 Cohesion, Relation, Presence, Warmth, Memory, Attraction,
 Harm, Distortion]
```

These values influence:

- perception weighting  
- action selection  
- risk tolerance  
- group cohesion  
- communication patterns  
- susceptibility to distortion  
- likelihood of systemic collapse  

The simulation loop updates pillar states at:

- **micro‑scale** (agent decisions)  
- **meso‑scale** (group dynamics)  
- **macro‑scale** (world evolution)  

This is where the "multi‑scale" in PCMSRM becomes visible.

---

## 🧩 **Voxel Engine Features**

### **1. Chunked World**
- Infinite or bounded world  
- Chunk streaming  
- Locality‑aware updates  
- Efficient memory footprint  

### **2. Multi‑Layer Voxels**
Each voxel can contain:

- material  
- temperature  
- pressure  
- resource density  
- hazard level  
- structural integrity  

### **3. Procedural Worldgen**
Supports:

- terrain layers  
- resource veins  
- environmental hazards  
- biome‑like variation  

### **4. Environmental Simulation**
Includes:

- diffusion  
- erosion  
- structural collapse  
- resource regeneration  
- hazard propagation  

---

## 🤖 **Agent System**

Agents include:

### **Perception**
- voxel sampling  
- line‑of‑sight  
- environmental scanning  
- agent proximity detection  

### **Cognition**
- pillar‑weighted decision loops  
- internal memory  
- short‑term vs long‑term goals  
- distortion susceptibility  

### **Behavior**
- movement  
- resource gathering  
- construction  
- communication  
- conflict  
- cooperation  
- avoidance  

### **Emergent Patterns**
- group formation  
- collapse cascades  
- runaway distortion  
- high‑cohesion clusters  
- harm propagation  

---

## 🔌 **API Layer (In Progress)**

Van_Nueman exposes a planned API for:

### **REST**
- world queries  
- agent state queries  
- simulation control  

### **WebSocket**
- real‑time simulation streaming  
- agent telemetry  
- pillar‑state updates  

### **LLM Bridge**
Connects Pillar AI → Van_Nueman:

- build contexts from pillar configs  
- send agent decisions to local LLMs  
- receive pillar‑aware reasoning  
- update agent state vectors  

This enables **local LLM‑driven agents** inside a voxel world.

---

## 🎮 **UI Layer**

The UI includes:

- voxel renderer  
- camera controls  
- debug overlays  
- pillar HUD  
- agent state visualization  
- simulation timeline  

This makes Van_Nueman a **visual demonstration platform** for PCMSRM.

---

## 🚧 **Current Status**

Van_Nueman is **under active development**.  
Core systems implemented or partially implemented:

- voxel definitions  
- chunking model  
- worldgen rules  
- agent cognition model  
- pillar integration layer  
- simulation loop design  
- rendering pipeline design  
- API specification  

Upcoming:

- full simulation loop  
- voxel renderer  
- agent behaviors  
- REST/WebSocket API  
- LLM integration  
- demo scenarios  

---

## 🧭 **Roadmap**

- [ ] Implement voxel renderer  
- [ ] Implement chunk streaming  
- [ ] Implement agent movement  
- [ ] Implement perception model  
- [ ] Implement pillar‑driven behavior selection  
- [ ] Add simulation tick loop  
- [ ] Add UI overlays (pillar HUD)  
- [ ] Add REST API  
- [ ] Add WebSocket streaming  
- [ ] Add LLM bridge to Pillar AI  
- [ ] Add demo scenarios  
- [ ] Add video demo  

---

## 🔗 **Related Project**

### **Pillar AI Configuration Suite**  
Defines the cognitive architecture used by Van_Nueman.  
[https://github.com/UrukuTelal/PillarConstrainedAICollaboration](https://github.com/UrukuTelal/PillarConstrainedAICollaboration)

---

## 📜 **License**

Open for local AI experimentation and simulation research.
