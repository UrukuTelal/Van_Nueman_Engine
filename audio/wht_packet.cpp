#include "wht_packet.h"
#include "wht.h"
#include <cstring>

void encode_pillar_vector(const float pillars[NUM_PILLARS], float signal[WHT_N]) {
    int i;
    for(i=0;i<NUM_PILLARS;i++) signal[i]=pillars[i];
    fwht(signal, WHT_LOG2_N);
}

void decode_pillar_vector(const float signal[WHT_N], float pillars[NUM_PILLARS]) {
    float temp[WHT_N];
    memcpy(temp, signal, WHT_N*sizeof(float));
    ifwht(temp, WHT_LOG2_N);
    int i;
    for(i=0;i<NUM_PILLARS;i++) pillars[i]=temp[i];
}
