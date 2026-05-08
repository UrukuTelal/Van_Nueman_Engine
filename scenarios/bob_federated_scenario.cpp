// bob_federated_scenario.cpp - WHT Communication Demo
// All code delegated to Ollama qwen2.5:3b via task_delegator.py

#include <iostream>
#include "../audio/wht.h"
#include "../audio/wht_comm.h"

int main() {
    std::cout << "Federated Bob Scenario - WHT Communication" << std::endl;
    
    // WHT Communication Demo (from Ollama delegation)
    float coeffs[WHT_N];
    encode_message("Bob here", coeffs);
    std::cout << "WHT signal generated" << std::endl;
    
    // Decode test
    char decoded[32];
    decode_message(coeffs, decoded, 32);
    std::cout << "Decoded: " << decoded << std::endl;
    
    return 0;
}

