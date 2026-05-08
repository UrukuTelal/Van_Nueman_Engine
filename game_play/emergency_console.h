// emergency_console.h - Python-scriptable console always available
// From GAMEPLAY.md: "Emergency Console - Python scriptable. Full logic + automation. Always available (even powered down)."

#pragma once

#include <cstdint>
#include <cstddef>
#include <vector>
#include <cstring>

// Script result
struct ScriptResult {
    bool success;
    char output[1024];
    float execution_time_ms;
    uint32_t error_code;
};

// Emergency Console - always available Python scripting interface
class EmergencyConsole {
public:
    EmergencyConsole(uint32_t player_id);
    ~EmergencyConsole();
    
    // Execute Python script (non-blocking for complex scripts)
    bool execute_script(const char* script, char* output_buffer, uint32_t buffer_size);
    
    // Execute simple expression (blocking, returns result)
    bool execute_expression(const char* expression, char* result, uint32_t result_size);
    
    // Load and run script from file
    bool run_script_file(const char* filepath, char* output_buffer, uint32_t buffer_size);
    
    // Check if console is available (always true - even when ship powered down)
    bool is_available() const { return true; }
    
    // Get last script result
    const ScriptResult& get_last_result() const { return last_result_; }
    
    // Access to game state (for Python scripts)
    bool get_pillar_state(uint32_t entity_id, float out_pillars[16]) const;
    bool set_pillar_state(uint32_t entity_id, const float pillars[16]);
    
    // Ship status (always accessible)
    void get_ship_status(char* status_json, uint32_t buffer_size) const;
    
    // Quick helper scripts
    bool scan_system();           // Quick survey
    bool check_vitals();           // Check ship/pilot vitals
    bool calculate_trajectory();    // Navigate to target
    
    // Get available API functions for scripting
    void get_api_docs(char* docs_buffer, uint32_t buffer_size) const;
    
private:
    uint32_t player_id_;
    ScriptResult last_result_;
    
    // Python interpreter state (embedded)
    void* py_interpreter_state_;  // Opaque pointer to Python C API state
    
    // Internal execution
    bool init_python();
    void shutdown_python();
    bool run_python_code(const char* code, char* output, uint32_t max_output);
    
    // Predefined helper scripts
    static const char* SCAN_SCRIPT;
    static const char* VITALS_SCRIPT;
    static const char* TRAJECTORY_SCRIPT;
};
