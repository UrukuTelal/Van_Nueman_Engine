#include "wht_comm.h"
#include "wht.h"
#include <cstring>

void encode_message(const char* message, float* coeffs) {
    if (!message || !coeffs) return;
    int len = static_cast<int>(strlen(message));
    if (len > WHT_N) len = WHT_N;
    for (int i = 0; i < len; i++) {
        coeffs[i] = (float)message[i];
    }
    for (int i = len; i < WHT_N; i++) {
        coeffs[i] = 0.0f;
    }
    fwht(coeffs, WHT_LOG2_N);
}

void decode_message(const float* coeffs, char* message, int max_len) {
    float temp[WHT_N];
    memcpy(temp, coeffs, WHT_N * sizeof(float));
    ifwht(temp, WHT_LOG2_N);
    for (int i = 0; i < max_len - 1 && i < WHT_N; i++) {
        message[i] = (char)temp[i];
    }
    message[max_len - 1] = 0;
}
