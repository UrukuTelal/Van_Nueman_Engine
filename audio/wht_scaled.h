#pragma once

#include "../vn/ScaledInt.h"
#include <cstdint>

void fwht_fp(vn::fp20_t* data, int log2n, bool normalize = false);
void ifwht_fp(vn::fp20_t* data, int log2n, bool normalize = false);
