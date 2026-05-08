#ifndef WHT_COMM_H
#define WHT_COMM_H

#include "wht.h"

void encode_message(const char* message, float* coeffs);
void decode_message(const float* coeffs, char* message, int max_len);

#endif // WHT_COMM_H
