// emergency_console.cpp - Emergency Console implementation
// Python-scriptable console, always available

#include "emergency_console.h"
#include <cstdio>
#include <cstdlib>
#include <ctime>

// Predefined helper scripts
const char* EmergencyConsole::SCAN_SCRIPT = 
    "import json\n"
    "result = {'scan_complete': True, 'bodies_found': 0, 'resources': []}\n"
    "print(json.dumps(result))\n";

const char* EmergencyConsole::VITALS_SCRIPT = 
    "import json\n"
    "result = {'vitals_ok': True, 'energy': 100, 'integrity': 100}\n"
    "print(json.dumps(result))\n";

const char* EmergencyConsole::TRAJECTORY_SCRIPT = 
    "import json\n"
    "result = {'trajectory_calculated': True, 'eta_hours': 0, 'fuel_required': 0}\n"
    "print(json.dumps(result))\n";

EmergencyConsole::EmergencyConsole(uint32_t player_id) :
    player_id_(player_id), py_interpreter_state_(nullptr) {
    init_python();
}

EmergencyConsole::~EmergencyConsole() {
    shutdown_python();
}

bool EmergencyConsole::init_python() {
    // In production, this would call Py_Initialize()
    // For now, simulate availability
    py_interpreter_state_ = (void*)1;  // Non-null = initialized
    return true;
}

void EmergencyConsole::shutdown_python() {
    // In production: Py_Finalize()
    py_interpreter_state_ = nullptr;
}

bool EmergencyConsole::execute_script(const char* script, char* output_buffer, uint32_t buffer_size) {
    if (!py_interpreter_state_) {
        snprintf(output_buffer, buffer_size, "Error: Python not initialized");
        last_result_.success = false;
        last_result_.error_code = 1;
        return false;
    }
    
    auto start = clock();
    bool result = run_python_code(script, output_buffer, buffer_size);
    auto end = clock();
    
    last_result_.success = result;
    last_result_.execution_time_ms = 1000.0f * (end - start) / CLOCKS_PER_SEC;
    last_result_.error_code = result ? 0 : 1;
    
    strncpy(last_result_.output, output_buffer, 1023);
    last_result_.output[1023] = '\0';
    
    return result;
}

bool EmergencyConsole::execute_expression(const char* expression, char* result, uint32_t result_size) {
    // Wrap expression in print()
    char script[1024];
    snprintf(script, sizeof(script), "print(%s)\n", expression);
    return execute_script(script, result, result_size);
}

bool EmergencyConsole::run_script_file(const char* filepath, char* output_buffer, uint32_t buffer_size) {
    FILE* f = fopen(filepath, "r");
    if (!f) {
        snprintf(output_buffer, buffer_size, "Error: Cannot open file %s", filepath);
        return false;
    }
    
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    char* script = (char*)malloc(len + 1);
    if (!script) {
        fclose(f);
        return false;
    }
    
    fread(script, 1, len, f);
    script[len] = '\0';
    fclose(f);
    
    bool result = execute_script(script, output_buffer, buffer_size);
    free(script);
    return result;
}

bool EmergencyConsole::run_python_code(const char* code, char* output, uint32_t max_output) {
    // In production, this would use Python C API:
    // PyRun_SimpleString(code);
    // For now, simulate execution
    
    snprintf(output, max_output, "Script executed (simulated).\nCode length: %u bytes\nResult: OK", 
             (uint32_t)strlen(code));
    
    return true;  // Simulated success
}

bool EmergencyConsole::get_pillar_state(uint32_t entity_id, float out_pillars[16]) const {
    // In production: query entity system
    for (int i = 0; i < 16; i++) {
        out_pillars[i] = 0.5f;  // Default
    }
    return true;
}

bool EmergencyConsole::set_pillar_state(uint32_t entity_id, const float pillars[16]) {
    // In production: update entity system
    return true;
}

void EmergencyConsole::get_ship_status(char* status_json, uint32_t buffer_size) const {
    snprintf(status_json, buffer_size, 
             "{\"player_id\":%u,\"console_available\":true,\"ship_status\":\"nominal\"}",
             player_id_);
}

bool EmergencyConsole::scan_system() {
    char output[1024];
    return execute_script(SCAN_SCRIPT, output, sizeof(output));
}

bool EmergencyConsole::check_vitals() {
    char output[1024];
    return execute_script(VITALS_SCRIPT, output, sizeof(output));
}

bool EmergencyConsole::calculate_trajectory() {
    char output[1024];
    return execute_script(TRAJECTORY_SCRIPT, output, sizeof(output));
}

void EmergencyConsole::get_api_docs(char* docs_buffer, uint32_t buffer_size) const {
    snprintf(docs_buffer, buffer_size,
             "Emergency Console API:\n"
             "- execute_script(script, output, size) -> bool\n"
             "- execute_expression(expr, result, size) -> bool\n"
             "- run_script_file(path, output, size) -> bool\n"
             "- scan_system() -> bool\n"
             "- check_vitals() -> bool\n"
             "- calculate_trajectory() -> bool\n"
             "- get_ship_status(buffer, size)\n"
             "- get_pillar_state(id, pillars) -> bool\n"
             "- set_pillar_state(id, pillars) -> bool\n"
             "Console is ALWAYS available, even when ship is powered down.\n");
}
