#include <iostream>
#include "../audio/wht.h"
#include "../audio/wht_packet.h"
int main() {
    WHTPacket p;
    p.sender_id=1;
    float pillars[NumPillars];
    for(int i=0;i<NumPillars;i++)
        pillars[i]=(float)(i+1)/16.0f;
    encode_pillar_vector(pillars, p.pillar_signal);
    float pillars2[NumPillars];
    decode_pillar_vector(p.pillar_signal, pillars2);
    std::cout << pillars2[14] << std::endl;
    std::cout << pillars2[15] << std::endl;
    return 0;
}
