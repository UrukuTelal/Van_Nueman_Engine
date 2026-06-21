#ifndef WHT_PACKET_H
#define WHT_PACKET_H

#include <wht.h>
#include "../include/Entity.h"

struct WHTPacket {
    uint32_t sender_id;
    uint32_t receiver_id;
    float pillar_signal[WHT_N];
    float timestamp;
    int message_type;
};

void encode_pillar_vector(const float pillars[NumPillars], float signal[WHT_N]);
void decode_pillar_vector(const float signal[WHT_N], float pillars[NumPillars]);

#endif // WHT_PACKET_H
