// VNES library — all functionality is header-only.
// This file provides the variant registry array definition
// and the Marching Cubes lookup tables (moved to .cpp to
// avoid MSVC constexpr initializer limits on headers).
#include "vnes_generator.h"

// ── Variant registry ───────────────────────────────────────
const VariantMetadata* const VNES_ALL_VARIANTS[VNES_VARIANT_COUNT] = {
    &VNE_A_250,
    &VNE_B_250,
    &VNE_C_250,
    &VNE_D_250,
    &VNE_E_250,
};

// ── Marching Cubes lookup tables ───────────────────────────
// Standard MC edge table: 256 entries, 12-bit mask per entry.
const uint16_t mc::edge_table[256] = {
    0x0, 0x109, 0x203, 0x30a, 0x406, 0x50f, 0x605, 0x70c,
    0x80c, 0x905, 0xa0f, 0xb06, 0xc0a, 0xd03, 0xe09, 0xf00,
    0x190, 0x99, 0x393, 0x29a, 0x596, 0x49f, 0x795, 0x69c,
    0x99c, 0x895, 0xb9f, 0xa96, 0xd9a, 0xc93, 0xf99, 0xe90,
    0x230, 0x339, 0x33, 0x13a, 0x636, 0x73f, 0x435, 0x53c,
    0xa3c, 0xb35, 0x83f, 0x936, 0xe3a, 0xf33, 0xc39, 0xd30,
    0x3a0, 0x2a9, 0x1a3, 0xaa, 0x7a6, 0x6af, 0x5a5, 0x4ac,
    0xbac, 0xaa5, 0x9af, 0x8a6, 0xfaa, 0xea3, 0xda9, 0xca0,
    0x460, 0x569, 0x663, 0x76a, 0x66, 0x16f, 0x265, 0x36c,
    0xc6c, 0xd65, 0xe6f, 0xf66, 0x86a, 0x963, 0xa69, 0xb60,
    0x5f0, 0x4f9, 0x7f3, 0x6fa, 0x1f6, 0xff, 0x3f5, 0x2fc,
    0xdfc, 0xcf5, 0xfff, 0xef6, 0x9fa, 0x8f3, 0xbf9, 0xaf0,
    0x650, 0x759, 0x453, 0x55a, 0x256, 0x35f, 0x55, 0x15c,
    0xe5c, 0xf55, 0xc5f, 0xd56, 0xa5a, 0xb53, 0x859, 0x950,
    0x7c0, 0x6c9, 0x5c3, 0x4ca, 0x3c6, 0x2cf, 0x1c5, 0xcc,
    0xfcc, 0xec5, 0xdcf, 0xcc6, 0xbca, 0xac3, 0x9c9, 0x8c0,
    0x8c0, 0x9c9, 0xac3, 0xbca, 0xcc6, 0xdcf, 0xec5, 0xfcc,
    0xcc, 0x1c5, 0x2cf, 0x3c6, 0x4ca, 0x5c3, 0x6c9, 0x7c0,
    0x950, 0x859, 0xb53, 0xa5a, 0xd56, 0xc5f, 0xf55, 0xe5c,
    0x15c, 0x55, 0x35f, 0x256, 0x55a, 0x453, 0x759, 0x650,
    0xaf0, 0xbf9, 0x8f3, 0x9fa, 0xef6, 0xfff, 0xcf5, 0xdfc,
    0x2fc, 0x3f5, 0xff, 0x1f6, 0x6fa, 0x7f3, 0x4f9, 0x5f0,
    0xb60, 0xa69, 0x963, 0x86a, 0xf66, 0xe6f, 0xd65, 0xc6c,
    0x36c, 0x265, 0x16f, 0x66, 0x76a, 0x663, 0x569, 0x460,
    0xca0, 0xda9, 0xea3, 0xfaa, 0x8a6, 0x9af, 0xaa5, 0xbac,
    0x4ac, 0x5a5, 0x6af, 0x7a6, 0xaa, 0x1a3, 0x2a9, 0x3a0,
    0xd30, 0xc39, 0xf33, 0xe3a, 0x936, 0x83f, 0xb35, 0xa3c,
    0x53c, 0x435, 0x73f, 0x636, 0x13a, 0x33, 0x339, 0x230,
    0xe90, 0xf99, 0xc93, 0xd9a, 0xa96, 0xb9f, 0x895, 0x99c,
    0x69c, 0x795, 0x49f, 0x596, 0x29a, 0x393, 0x99, 0x190,
    0xf00, 0xe09, 0xd03, 0xc0a, 0xb06, 0xa0f, 0x905, 0x80c,
    0x70c, 0x605, 0x50f, 0x406, 0x30a, 0x203, 0x109, 0x0,
};

// ── Individual MC row arrays ─────────────────────────────────
// Each row holds up to 16 edge indices (terminated by -1).
// Defined as separate 1D arrays so MSVC does not hit its
// per-initializer-list nesting limit for a single 2D array.

static const int8_t mc_r000[16] = {-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1};
static const int8_t mc_r001[16] = {0,8,3,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1};
static const int8_t mc_r002[16] = {0,1,9,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1};
static const int8_t mc_r003[16] = {1,8,3,9,8,1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1};
static const int8_t mc_r004[16] = {1,2,10,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1};
static const int8_t mc_r005[16] = {0,8,3,1,2,10,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1};
static const int8_t mc_r006[16] = {9,2,10,0,2,9,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1};
static const int8_t mc_r007[16] = {2,8,3,2,10,8,10,9,8,-1,-1,-1,-1,-1,-1,-1};
static const int8_t mc_r008[16] = {3,11,2,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1};
static const int8_t mc_r009[16] = {0,11,2,8,11,0,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1};
static const int8_t mc_r010[16] = {1,9,0,2,3,11,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1};
static const int8_t mc_r011[16] = {1,11,2,1,9,11,9,8,11,-1,-1,-1,-1,-1,-1,-1};
static const int8_t mc_r012[16] = {3,10,1,11,10,3,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1};
static const int8_t mc_r013[16] = {0,10,1,0,8,10,8,11,10,-1,-1,-1,-1,-1,-1,-1};
static const int8_t mc_r014[16] = {3,9,0,3,11,9,11,10,9,-1,-1,-1,-1,-1,-1,-1};
static const int8_t mc_r015[16] = {9,8,10,10,8,11,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1};
static const int8_t mc_r016[16] = {4,7,8,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1};
static const int8_t mc_r017[16] = {4,3,0,7,3,4,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1};
static const int8_t mc_r018[16] = {0,1,9,8,4,7,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1};
static const int8_t mc_r019[16] = {4,1,9,4,7,1,7,3,1,-1,-1,-1,-1,-1,-1,-1};
static const int8_t mc_r020[16] = {1,2,10,8,4,7,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1};
static const int8_t mc_r021[16] = {3,4,7,3,0,4,1,2,10,-1,-1,-1,-1,-1,-1,-1};
static const int8_t mc_r022[16] = {9,2,10,9,0,2,8,4,7,-1,-1,-1,-1,-1,-1,-1};
static const int8_t mc_r023[16] = {2,10,9,2,9,7,2,7,3,7,9,4,-1,-1,-1,-1};
static const int8_t mc_r024[16] = {8,4,7,3,11,2,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1};
static const int8_t mc_r025[16] = {11,4,7,11,2,4,2,0,4,-1,-1,-1,-1,-1,-1,-1};
static const int8_t mc_r026[16] = {9,0,1,8,4,7,2,3,11,-1,-1,-1,-1,-1,-1,-1};
static const int8_t mc_r027[16] = {4,7,11,9,4,11,9,11,2,9,2,1,-1,-1,-1,-1};
static const int8_t mc_r028[16] = {3,10,1,3,11,10,7,8,4,-1,-1,-1,-1,-1,-1,-1};
static const int8_t mc_r029[16] = {1,11,10,1,4,11,1,0,4,7,11,4,-1,-1,-1,-1};
static const int8_t mc_r030[16] = {4,7,8,9,0,11,9,11,10,11,0,3,-1,-1,-1,-1};
static const int8_t mc_r031[16] = {4,7,11,4,11,9,9,11,10,-1,-1,-1,-1,-1,-1,-1};
static const int8_t mc_r032[16] = {9,5,4,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1};
static const int8_t mc_r033[16] = {9,5,4,0,8,3,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1};
static const int8_t mc_r034[16] = {0,5,4,1,5,0,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1};
static const int8_t mc_r035[16] = {8,5,4,8,3,5,3,1,5,-1,-1,-1,-1,-1,-1,-1};
static const int8_t mc_r036[16] = {1,2,10,9,5,4,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1};
static const int8_t mc_r037[16] = {3,0,8,1,2,10,4,9,5,-1,-1,-1,-1,-1,-1,-1};
static const int8_t mc_r038[16] = {5,2,10,5,4,2,4,0,2,-1,-1,-1,-1,-1,-1,-1};
static const int8_t mc_r039[16] = {2,10,5,3,2,5,3,5,4,3,4,8,-1,-1,-1,-1};
static const int8_t mc_r040[16] = {9,5,4,2,3,11,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1};
static const int8_t mc_r041[16] = {0,11,2,0,8,11,4,9,5,-1,-1,-1,-1,-1,-1,-1};
static const int8_t mc_r042[16] = {0,5,4,0,1,5,2,3,11,-1,-1,-1,-1,-1,-1,-1};
static const int8_t mc_r043[16] = {2,1,5,2,5,8,2,8,11,4,8,5,-1,-1,-1,-1};
static const int8_t mc_r044[16] = {10,3,11,10,1,3,9,5,4,-1,-1,-1,-1,-1,-1,-1};
static const int8_t mc_r045[16] = {4,9,5,0,8,1,8,10,1,8,11,10,-1,-1,-1,-1};
static const int8_t mc_r046[16] = {5,4,0,5,0,11,5,11,10,11,0,3,-1,-1,-1,-1};
static const int8_t mc_r047[16] = {5,4,8,5,8,10,10,8,11,-1,-1,-1,-1,-1,-1,-1};
static const int8_t mc_r048[16] = {9,7,8,5,7,9,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1};
static const int8_t mc_r049[16] = {9,3,0,9,5,3,5,7,3,-1,-1,-1,-1,-1,-1,-1};
static const int8_t mc_r050[16] = {0,7,8,0,1,7,1,5,7,-1,-1,-1,-1,-1,-1,-1};
static const int8_t mc_r051[16] = {1,5,3,3,5,7,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1};
static const int8_t mc_r052[16] = {9,7,8,9,5,7,10,1,2,-1,-1,-1,-1,-1,-1,-1};
static const int8_t mc_r053[16] = {10,1,2,9,5,0,8,5,9,3,5,8,7,5,3,-1};
static const int8_t mc_r054[16] = {8,0,2,8,2,5,8,5,7,10,5,2,-1,-1,-1,-1};
static const int8_t mc_r055[16] = {2,10,5,2,5,3,3,5,7,-1,-1,-1,-1,-1,-1,-1};
static const int8_t mc_r056[16] = {7,9,5,7,8,9,3,11,2,-1,-1,-1,-1,-1,-1,-1};
static const int8_t mc_r057[16] = {9,5,7,9,7,2,9,2,0,2,7,11,-1,-1,-1,-1};
static const int8_t mc_r058[16] = {2,3,11,0,1,8,1,7,8,1,5,7,-1,-1,-1,-1};
static const int8_t mc_r059[16] = {11,2,1,11,1,7,7,1,5,-1,-1,-1,-1,-1,-1,-1};
static const int8_t mc_r060[16] = {9,5,8,8,5,7,10,1,3,10,3,11,-1,-1,-1,-1};
static const int8_t mc_r061[16] = {5,7,0,5,0,9,7,11,0,1,0,10,11,10,0,-1};
static const int8_t mc_r062[16] = {11,10,0,11,0,3,10,5,0,8,0,7,5,7,0,-1};
static const int8_t mc_r063[16] = {11,10,5,7,11,5,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1};
static const int8_t mc_r064[16] = {10,6,5,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1};
static const int8_t mc_r065[16] = {0,8,3,5,10,6,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1};
static const int8_t mc_r066[16] = {9,0,1,5,10,6,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1};
static const int8_t mc_r067[16] = {1,8,3,1,9,8,5,10,6,-1,-1,-1,-1,-1,-1,-1};
static const int8_t mc_r068[16] = {1,6,5,2,6,1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1};
static const int8_t mc_r069[16] = {1,6,5,1,2,6,3,0,8,-1,-1,-1,-1,-1,-1,-1};
static const int8_t mc_r070[16] = {9,6,5,9,0,6,0,2,6,-1,-1,-1,-1,-1,-1,-1};
static const int8_t mc_r071[16] = {5,9,8,5,8,2,5,2,6,3,2,8,-1,-1,-1,-1};
static const int8_t mc_r072[16] = {2,3,11,10,6,5,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1};
static const int8_t mc_r073[16] = {11,0,8,11,2,0,10,6,5,-1,-1,-1,-1,-1,-1,-1};
static const int8_t mc_r074[16] = {0,1,9,2,3,11,5,10,6,-1,-1,-1,-1,-1,-1,-1};
static const int8_t mc_r075[16] = {5,10,6,1,9,2,9,11,2,9,8,11,-1,-1,-1,-1};
static const int8_t mc_r076[16] = {6,3,11,6,5,3,5,1,3,-1,-1,-1,-1,-1,-1,-1};
static const int8_t mc_r077[16] = {0,8,11,0,11,5,0,5,1,5,11,6,-1,-1,-1,-1};
static const int8_t mc_r078[16] = {3,11,6,0,3,6,0,6,5,0,5,9,-1,-1,-1,-1};
static const int8_t mc_r079[16] = {6,5,9,6,9,11,11,9,8,-1,-1,-1,-1,-1,-1,-1};
static const int8_t mc_r080[16] = {5,10,6,4,7,8,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1};
static const int8_t mc_r081[16] = {4,3,0,4,7,3,6,5,10,-1,-1,-1,-1,-1,-1,-1};
static const int8_t mc_r082[16] = {1,9,0,5,10,6,8,4,7,-1,-1,-1,-1,-1,-1,-1};
static const int8_t mc_r083[16] = {10,6,5,1,9,7,1,7,3,7,9,4,-1,-1,-1,-1};
static const int8_t mc_r084[16] = {6,1,2,6,5,1,4,7,8,-1,-1,-1,-1,-1,-1,-1};
static const int8_t mc_r085[16] = {1,2,5,5,2,6,3,0,4,3,4,7,-1,-1,-1,-1};
static const int8_t mc_r086[16] = {8,4,7,9,0,5,0,6,5,0,2,6,-1,-1,-1,-1};
static const int8_t mc_r087[16] = {7,3,9,7,9,4,3,2,9,5,9,6,2,6,9,-1};
static const int8_t mc_r088[16] = {3,11,2,7,8,4,10,6,5,-1,-1,-1,-1,-1,-1,-1};
static const int8_t mc_r089[16] = {5,10,6,4,7,2,4,2,0,2,7,11,-1,-1,-1,-1};
static const int8_t mc_r090[16] = {0,1,9,4,7,8,2,3,11,5,10,6,-1,-1,-1,-1};
static const int8_t mc_r091[16] = {9,2,1,9,11,2,9,4,11,7,11,4,5,10,6,-1};
static const int8_t mc_r092[16] = {8,4,7,3,11,5,3,5,1,5,11,6,-1,-1,-1,-1};
static const int8_t mc_r093[16] = {5,1,11,5,11,6,1,0,11,7,11,4,0,4,11,-1};
static const int8_t mc_r094[16] = {0,5,9,0,3,5,3,6,5,3,11,6,0,7,8,-1};
static const int8_t mc_r095[16] = {5,9,6,6,9,11,11,9,4,11,4,7,-1,-1,-1,-1};
static const int8_t mc_r096[16] = {4,9,5,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1};
static const int8_t mc_r097[16] = {4,9,5,0,8,3,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1};
static const int8_t mc_r098[16] = {4,0,1,4,1,5,5,1,9,-1,-1,-1,-1,-1,-1,-1};
static const int8_t mc_r099[16] = {5,4,8,5,8,1,1,8,3,-1,-1,-1,-1,-1,-1,-1};
static const int8_t mc_r100[16] = {4,9,5,2,10,1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1};
static const int8_t mc_r101[16] = {4,9,5,0,8,3,1,2,10,-1,-1,-1,-1,-1,-1,-1};
static const int8_t mc_r102[16] = {4,2,10,4,10,5,0,2,4,-1,-1,-1,-1,-1,-1,-1};
static const int8_t mc_r103[16] = {10,5,4,10,4,2,2,4,8,2,8,3,-1,-1,-1,-1};
static const int8_t mc_r104[16] = {4,9,5,2,3,11,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1};
static const int8_t mc_r105[16] = {0,8,11,0,11,2,4,9,5,-1,-1,-1,-1,-1,-1,-1};
static const int8_t mc_r106[16] = {0,4,9,0,1,4,2,3,11,-1,-1,-1,-1,-1,-1,-1};
static const int8_t mc_r107[16] = {9,8,11,9,11,1,1,11,2,4,8,9,-1,-1,-1,-1};
static const int8_t mc_r108[16] = {4,9,5,3,11,1,3,1,10,1,11,3,-1,-1,-1,-1};
static const int8_t mc_r109[16] = {0,8,11,0,11,10,0,10,1,10,11,6,4,9,5,-1};
static const int8_t mc_r110[16] = {0,4,9,0,3,4,3,11,4,5,4,10,11,10,4,-1};
static const int8_t mc_r111[16] = {9,8,10,9,10,5,8,11,10,-1,-1,-1,-1,-1,-1,-1};
static const int8_t mc_r112[16] = {5,7,8,5,8,9,9,8,4,-1,-1,-1,-1,-1,-1,-1};
static const int8_t mc_r113[16] = {4,3,0,4,7,3,5,9,8,5,8,9,-1,-1,-1,-1};
static const int8_t mc_r114[16] = {1,7,8,1,8,0,5,7,1,-1,-1,-1,-1,-1,-1,-1};
static const int8_t mc_r115[16] = {1,5,7,3,1,7,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1};
static const int8_t mc_r116[16] = {1,2,10,5,7,9,7,8,9,4,9,7,-1,-1,-1,-1};
static const int8_t mc_r117[16] = {3,0,4,3,4,7,1,2,10,5,9,8,5,8,9,-1};
static const int8_t mc_r118[16] = {10,0,2,10,5,0,5,7,0,8,0,7,4,9,5,-1};
static const int8_t mc_r119[16] = {3,2,10,3,10,7,7,10,5,-1,-1,-1,-1,-1,-1,-1};
static const int8_t mc_r120[16] = {5,7,8,5,8,9,2,3,11,-1,-1,-1,-1,-1,-1,-1};
static const int8_t mc_r121[16] = {9,2,0,9,5,2,5,7,2,11,2,7,4,9,5,-1};
static const int8_t mc_r122[16] = {0,1,8,1,7,8,1,5,7,2,3,11,-1,-1,-1,-1};
static const int8_t mc_r123[16] = {9,2,1,9,5,2,11,2,7,5,7,2,-1,-1,-1,-1};
static const int8_t mc_r124[16] = {9,1,3,9,3,8,9,8,5,5,8,7,11,1,3,10};
static const int8_t mc_r125[16] = {1,0,5,5,0,9,7,11,1,5,1,11,5,11,6,10};
static const int8_t mc_r126[16] = {0,3,11,0,11,5,0,5,9,5,11,6,8,0,7,5};
static const int8_t mc_r127[16] = {11,6,5,7,11,5,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1};
static const int8_t mc_r128[16] = {10,11,5,11,6,5,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1};
static const int8_t mc_r129[16] = {0,8,3,5,10,11,5,11,6,11,10,6,-1,-1,-1,-1};
static const int8_t mc_r130[16] = {9,0,1,10,11,5,11,6,5,-1,-1,-1,-1,-1,-1,-1};
static const int8_t mc_r131[16] = {1,9,8,1,8,3,5,10,11,5,11,6,-1,-1,-1,-1};
static const int8_t mc_r132[16] = {1,11,5,1,5,10,2,11,1,-1,-1,-1,-1,-1,-1,-1};
static const int8_t mc_r133[16] = {0,8,3,1,2,11,1,11,5,11,6,5,-1,-1,-1,-1};
static const int8_t mc_r134[16] = {9,0,2,9,2,11,9,11,5,11,6,5,-1,-1,-1,-1};
static const int8_t mc_r135[16] = {5,9,8,5,8,11,5,11,6,11,8,3,-1,-1,-1,-1};
static const int8_t mc_r136[16] = {2,5,10,3,5,2,3,6,5,3,11,6,-1,-1,-1,-1};
static const int8_t mc_r137[16] = {0,8,11,0,11,6,0,6,5,0,5,2,2,5,10,-1};
static const int8_t mc_r138[16] = {0,1,9,2,3,5,3,6,5,3,11,6,-1,-1,-1,-1};
static const int8_t mc_r139[16] = {9,8,11,9,11,6,9,6,1,1,6,5,11,2,1,3};
static const int8_t mc_r140[16] = {1,3,5,3,11,5,5,11,6,-1,-1,-1,-1,-1,-1,-1};
static const int8_t mc_r141[16] = {1,0,8,1,8,11,1,11,5,5,11,6,8,0,5,1};
static const int8_t mc_r142[16] = {0,3,11,0,11,5,0,5,9,5,11,6,-1,-1,-1,-1};
static const int8_t mc_r143[16] = {9,8,11,9,11,5,5,11,6,-1,-1,-1,-1,-1,-1,-1};
static const int8_t mc_r144[16] = {10,8,5,11,8,10,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1};
static const int8_t mc_r145[16] = {10,0,8,10,8,5,11,0,10,-1,-1,-1,-1,-1,-1,-1};
static const int8_t mc_r146[16] = {0,1,9,10,8,5,11,8,10,-1,-1,-1,-1,-1,-1,-1};
static const int8_t mc_r147[16] = {1,9,8,1,8,10,1,10,11,10,8,5,-1,-1,-1,-1};
static const int8_t mc_r148[16] = {2,8,5,2,5,10,3,8,2,-1,-1,-1,-1,-1,-1,-1};
static const int8_t mc_r149[16] = {0,8,2,2,8,5,2,5,10,5,8,3,-1,-1,-1,-1};
static const int8_t mc_r150[16] = {9,0,2,9,2,10,9,10,5,10,2,3,9,5,8,-1};
static const int8_t mc_r151[16] = {9,8,5,5,8,3,5,3,10,10,3,2,-1,-1,-1,-1};
static const int8_t mc_r152[16] = {10,3,11,10,11,5,8,3,10,-1,-1,-1,-1,-1,-1,-1};
static const int8_t mc_r153[16] = {0,8,11,0,11,10,0,10,5,10,11,6,0,8,10,-1};
static const int8_t mc_r154[16] = {0,1,9,10,8,3,10,3,11,10,11,5,5,11,8,-1};
static const int8_t mc_r155[16] = {1,9,10,10,9,8,10,8,5,5,8,11,10,11,8,6};
static const int8_t mc_r156[16] = {1,3,8,1,8,10,10,8,5,-1,-1,-1,-1,-1,-1,-1};
static const int8_t mc_r157[16] = {0,8,1,1,8,10,10,8,5,-1,-1,-1,-1,-1,-1,-1};
static const int8_t mc_r158[16] = {0,3,11,0,11,5,0,5,9,5,11,6,0,9,5,-1};
static const int8_t mc_r159[16] = {9,8,5,5,8,11,5,11,6,-1,-1,-1,-1,-1,-1,-1};
static const int8_t mc_r160[16] = {8,6,11,8,11,3,6,5,10,6,10,8,-1,-1,-1,-1};
static const int8_t mc_r161[16] = {0,8,6,0,6,5,0,5,1,1,5,10,6,11,0,8};
static const int8_t mc_r162[16] = {1,9,0,8,6,11,8,3,6,6,5,10,8,6,3,-1};
static const int8_t mc_r163[16] = {1,9,6,1,6,10,9,8,6,11,6,5,8,11,6,-1};
static const int8_t mc_r164[16] = {8,6,11,8,3,6,6,5,10,6,10,8,-1,-1,-1,-1};
static const int8_t mc_r165[16] = {0,8,6,0,6,1,1,6,5,6,11,1,0,1,11,-1};
static const int8_t mc_r166[16] = {9,0,6,9,6,5,0,3,6,11,6,5,0,6,3,-1};
static const int8_t mc_r167[16] = {9,8,6,9,6,5,8,11,6,-1,-1,-1,-1,-1,-1,-1};
static const int8_t mc_r168[16] = {6,4,9,6,9,10,10,9,1,-1,-1,-1,-1,-1,-1,-1};
static const int8_t mc_r169[16] = {0,8,3,6,4,9,6,9,10,10,9,1,-1,-1,-1,-1};
static const int8_t mc_r170[16] = {0,4,9,0,1,4,1,6,4,10,6,1,-1,-1,-1,-1};
static const int8_t mc_r171[16] = {8,3,1,8,1,6,8,6,4,6,1,10,-1,-1,-1,-1};
static const int8_t mc_r172[16] = {2,4,9,2,9,1,6,4,2,-1,-1,-1,-1,-1,-1,-1};
static const int8_t mc_r173[16] = {0,8,3,2,4,9,2,9,1,6,4,2,-1,-1,-1,-1};
static const int8_t mc_r174[16] = {4,0,6,6,0,2,6,2,10,-1,-1,-1,-1,-1,-1,-1};
static const int8_t mc_r175[16] = {3,2,8,8,2,10,8,10,4,4,10,6,2,6,10,-1};
static const int8_t mc_r176[16] = {6,4,9,6,9,10,3,11,2,-1,-1,-1,-1,-1,-1,-1};
static const int8_t mc_r177[16] = {0,8,2,2,8,11,4,9,6,6,9,10,2,11,6,-1};
static const int8_t mc_r178[16] = {2,3,11,0,1,9,6,4,10,4,9,10,-1,-1,-1,-1};
static const int8_t mc_r179[16] = {1,9,11,1,11,2,9,4,11,6,11,10,4,6,11,-1};
static const int8_t mc_r180[16] = {4,9,1,4,1,3,4,3,6,6,3,11,1,10,4,6};
static const int8_t mc_r181[16] = {4,9,1,4,1,0,4,0,6,6,0,8,1,10,4,6};
static const int8_t mc_r182[16] = {0,3,11,0,11,6,0,6,4,4,6,10,0,4,6,-1};
static const int8_t mc_r183[16] = {4,6,11,4,11,8,6,10,11,-1,-1,-1,-1,-1,-1,-1};
static const int8_t mc_r184[16] = {10,8,9,10,9,5,11,8,10,-1,-1,-1,-1,-1,-1,-1};
static const int8_t mc_r185[16] = {10,5,9,10,9,8,10,8,11,8,9,0,10,11,8,-1};
static const int8_t mc_r186[16] = {10,5,1,1,5,0,11,10,1,8,11,1,0,1,8,-1};
static const int8_t mc_r187[16] = {10,5,1,1,5,3,3,5,8,11,10,1,3,11,1,8};
static const int8_t mc_r188[16] = {10,3,2,10,5,3,5,9,3,8,3,9,4,9,5,8};
static const int8_t mc_r189[16] = {0,8,3,10,5,4,10,4,2,2,4,9,10,2,5,4};
static const int8_t mc_r190[16] = {4,10,5,4,0,10,0,2,10,8,3,4,8,4,0,3};
static const int8_t mc_r191[16] = {10,5,4,10,4,2,2,4,8,2,8,3,-1,-1,-1,-1};
static const int8_t mc_r192[16] = {10,3,11,10,11,5,3,8,11,9,5,11,4,9,11,8};
static const int8_t mc_r193[16] = {0,8,11,0,11,5,0,5,9,5,11,10,4,9,5,8};
static const int8_t mc_r194[16] = {0,1,11,0,11,3,1,5,11,10,11,5,4,9,1,0};
static const int8_t mc_r195[16] = {1,9,4,1,4,5,1,5,10,10,5,6,11,1,10,2};
static const int8_t mc_r196[16] = {11,1,3,11,5,1,1,5,9,4,9,5,11,1,5,6};
static const int8_t mc_r197[16] = {0,8,1,1,8,10,10,8,5,4,9,1,10,5,1,6};
static const int8_t mc_r198[16] = {0,3,11,0,11,4,4,11,6,0,4,9,5,0,4,6};
static const int8_t mc_r199[16] = {4,6,11,4,11,8,6,10,11,8,11,9,4,8,11,5};
static const int8_t mc_r200[16] = {6,8,10,4,8,6,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1};
static const int8_t mc_r201[16] = {6,0,4,10,0,6,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1};
static const int8_t mc_r202[16] = {6,0,1,6,1,10,4,8,0,-1,-1,-1,-1,-1,-1,-1};
static const int8_t mc_r203[16] = {6,4,8,6,8,1,6,1,10,1,8,3,-1,-1,-1,-1};
static const int8_t mc_r204[16] = {6,2,3,6,3,8,6,8,4,2,6,8,-1,-1,-1,-1};
static const int8_t mc_r205[16] = {6,2,0,4,6,0,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1};
static const int8_t mc_r206[16] = {6,4,8,6,8,3,6,3,2,6,2,10,3,8,2,6};
static const int8_t mc_r207[16] = {6,2,0,6,0,4,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1};
static const int8_t mc_r208[16] = {6,8,10,4,8,6,3,11,2,-1,-1,-1,-1,-1,-1,-1};
static const int8_t mc_r209[16] = {6,0,4,6,2,0,2,11,0,3,11,2,6,8,10,-1};
static const int8_t mc_r210[16] = {1,9,0,6,4,8,10,6,3,10,3,11,3,6,8,10};
static const int8_t mc_r211[16] = {1,9,4,1,4,6,1,6,10,10,6,5,11,1,10,2};
static const int8_t mc_r212[16] = {3,11,2,8,4,6,8,6,10,8,10,3,3,10,1,-1};
static const int8_t mc_r213[16] = {1,0,4,1,4,10,10,4,6,11,1,10,2,11,10,-1};
static const int8_t mc_r214[16] = {0,3,11,0,11,6,0,6,4,4,6,10,0,4,6,-1};
static const int8_t mc_r215[16] = {4,6,11,4,11,8,6,10,11,-1,-1,-1,-1,-1,-1,-1};
static const int8_t mc_r216[16] = {10,8,9,10,9,5,11,8,10,-1,-1,-1,-1,-1,-1,-1};
static const int8_t mc_r217[16] = {10,5,9,10,9,8,10,8,11,8,9,0,10,11,8,-1};
static const int8_t mc_r218[16] = {10,5,1,1,5,0,11,10,1,8,11,1,0,1,8,-1};
static const int8_t mc_r219[16] = {10,5,1,1,5,3,3,5,8,11,10,1,3,11,1,8};
static const int8_t mc_r220[16] = {10,3,2,10,5,3,5,9,3,8,3,9,4,9,5,8};
static const int8_t mc_r221[16] = {0,8,3,10,5,4,10,4,2,2,4,9,10,2,5,4};
static const int8_t mc_r222[16] = {4,10,5,4,0,10,0,2,10,8,3,4,8,4,0,3};
static const int8_t mc_r223[16] = {10,5,4,10,4,2,2,4,8,2,8,3,-1,-1,-1,-1};
static const int8_t mc_r224[16] = {10,3,11,10,11,5,3,8,11,9,5,11,4,9,11,8};
static const int8_t mc_r225[16] = {0,8,11,0,11,5,0,5,9,5,11,10,4,9,5,8};
static const int8_t mc_r226[16] = {0,1,11,0,11,3,1,5,11,10,11,5,4,9,1,0};
static const int8_t mc_r227[16] = {1,9,4,1,4,5,1,5,10,10,5,6,11,1,10,2};
static const int8_t mc_r228[16] = {11,1,3,11,5,1,1,5,9,4,9,5,11,1,5,6};
static const int8_t mc_r229[16] = {0,8,1,1,8,10,10,8,5,4,9,1,10,5,1,6};
static const int8_t mc_r230[16] = {0,3,11,0,11,4,4,11,6,0,4,9,5,0,4,6};
static const int8_t mc_r231[16] = {4,6,11,4,11,8,6,10,11,8,11,9,4,8,11,5};
static const int8_t mc_r232[16] = {6,8,10,4,8,6,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1};
static const int8_t mc_r233[16] = {6,0,4,10,0,6,5,9,0,8,3,6,5,10,6,8};
static const int8_t mc_r234[16] = {0,6,5,0,5,9,0,1,6,10,6,5,-1,-1,-1,-1};
static const int8_t mc_r235[16] = {8,3,1,8,1,5,8,5,4,4,5,9,1,10,8,5};
static const int8_t mc_r236[16] = {8,4,5,8,5,3,3,5,1,5,10,1,8,3,5,-1};
static const int8_t mc_r237[16] = {9,6,2,9,2,1,4,6,9,6,5,10,6,2,11,9};
static const int8_t mc_r238[16] = {0,8,3,9,6,2,9,2,1,11,6,9,6,5,10,9};
static const int8_t mc_r239[16] = {4,6,10,4,10,0,0,10,1,6,5,10,4,0,10,-1};
static const int8_t mc_r240[16] = {8,3,2,8,2,4,4,2,10,4,10,5,2,6,8,4};
static const int8_t mc_r241[16] = {6,9,3,6,3,11,6,5,9,5,10,6,6,9,11,8};
static const int8_t mc_r242[16] = {0,8,11,0,11,5,0,5,9,5,11,6,0,9,5,10};
static const int8_t mc_r243[16] = {0,1,5,0,5,9,1,6,5,10,6,5,0,5,1,11};
static const int8_t mc_r244[16] = {1,9,4,1,4,8,1,8,3,8,4,6,1,3,8,11};
static const int8_t mc_r245[16] = {6,11,3,6,3,8,6,8,10,10,8,4,6,5,10,8};
static const int8_t mc_r246[16] = {0,8,1,1,8,10,10,8,5,5,8,4,6,5,10,11};
static const int8_t mc_r247[16] = {0,1,6,0,6,4,1,10,6,6,5,10,0,6,1,11};
static const int8_t mc_r248[16] = {1,10,6,1,6,4,1,4,8,1,8,3,4,6,5,1};
static const int8_t mc_r249[16] = {2,11,6,2,6,9,2,9,1,9,6,5,2,1,9,10};
static const int8_t mc_r250[16] = {0,8,11,0,11,9,0,9,1,9,11,6,0,1,9,5};
static const int8_t mc_r251[16] = {0,4,6,0,6,2,2,6,11,0,2,6,1,10,6,5};
static const int8_t mc_r252[16] = {8,4,6,8,6,11,11,6,5,8,11,6,1,3,8,10};
static const int8_t mc_r253[16] = {6,5,9,6,9,11,11,9,8,-1,-1,-1,-1,-1,-1,-1};
static const int8_t mc_r254[16] = {0,8,11,0,11,5,0,5,9,5,11,6,0,9,5,-1};
static const int8_t mc_r255[16] = {0,1,9,0,5,9,0,6,5,0,11,6,10,6,5,-1};

const int8_t* mc::tri_table() {
    static int8_t table[4096];
    static bool init = false;
    if (!init) {
        init = true;
        std::memcpy(table + 0, mc_r000, 16);
        std::memcpy(table + 16, mc_r001, 16);
        std::memcpy(table + 32, mc_r002, 16);
        std::memcpy(table + 48, mc_r003, 16);
        std::memcpy(table + 64, mc_r004, 16);
        std::memcpy(table + 80, mc_r005, 16);
        std::memcpy(table + 96, mc_r006, 16);
        std::memcpy(table + 112, mc_r007, 16);
        std::memcpy(table + 128, mc_r008, 16);
        std::memcpy(table + 144, mc_r009, 16);
        std::memcpy(table + 160, mc_r010, 16);
        std::memcpy(table + 176, mc_r011, 16);
        std::memcpy(table + 192, mc_r012, 16);
        std::memcpy(table + 208, mc_r013, 16);
        std::memcpy(table + 224, mc_r014, 16);
        std::memcpy(table + 240, mc_r015, 16);
        std::memcpy(table + 256, mc_r016, 16);
        std::memcpy(table + 272, mc_r017, 16);
        std::memcpy(table + 288, mc_r018, 16);
        std::memcpy(table + 304, mc_r019, 16);
        std::memcpy(table + 320, mc_r020, 16);
        std::memcpy(table + 336, mc_r021, 16);
        std::memcpy(table + 352, mc_r022, 16);
        std::memcpy(table + 368, mc_r023, 16);
        std::memcpy(table + 384, mc_r024, 16);
        std::memcpy(table + 400, mc_r025, 16);
        std::memcpy(table + 416, mc_r026, 16);
        std::memcpy(table + 432, mc_r027, 16);
        std::memcpy(table + 448, mc_r028, 16);
        std::memcpy(table + 464, mc_r029, 16);
        std::memcpy(table + 480, mc_r030, 16);
        std::memcpy(table + 496, mc_r031, 16);
        std::memcpy(table + 512, mc_r032, 16);
        std::memcpy(table + 528, mc_r033, 16);
        std::memcpy(table + 544, mc_r034, 16);
        std::memcpy(table + 560, mc_r035, 16);
        std::memcpy(table + 576, mc_r036, 16);
        std::memcpy(table + 592, mc_r037, 16);
        std::memcpy(table + 608, mc_r038, 16);
        std::memcpy(table + 624, mc_r039, 16);
        std::memcpy(table + 640, mc_r040, 16);
        std::memcpy(table + 656, mc_r041, 16);
        std::memcpy(table + 672, mc_r042, 16);
        std::memcpy(table + 688, mc_r043, 16);
        std::memcpy(table + 704, mc_r044, 16);
        std::memcpy(table + 720, mc_r045, 16);
        std::memcpy(table + 736, mc_r046, 16);
        std::memcpy(table + 752, mc_r047, 16);
        std::memcpy(table + 768, mc_r048, 16);
        std::memcpy(table + 784, mc_r049, 16);
        std::memcpy(table + 800, mc_r050, 16);
        std::memcpy(table + 816, mc_r051, 16);
        std::memcpy(table + 832, mc_r052, 16);
        std::memcpy(table + 848, mc_r053, 16);
        std::memcpy(table + 864, mc_r054, 16);
        std::memcpy(table + 880, mc_r055, 16);
        std::memcpy(table + 896, mc_r056, 16);
        std::memcpy(table + 912, mc_r057, 16);
        std::memcpy(table + 928, mc_r058, 16);
        std::memcpy(table + 944, mc_r059, 16);
        std::memcpy(table + 960, mc_r060, 16);
        std::memcpy(table + 976, mc_r061, 16);
        std::memcpy(table + 992, mc_r062, 16);
        std::memcpy(table + 1008, mc_r063, 16);
        std::memcpy(table + 1024, mc_r064, 16);
        std::memcpy(table + 1040, mc_r065, 16);
        std::memcpy(table + 1056, mc_r066, 16);
        std::memcpy(table + 1072, mc_r067, 16);
        std::memcpy(table + 1088, mc_r068, 16);
        std::memcpy(table + 1104, mc_r069, 16);
        std::memcpy(table + 1120, mc_r070, 16);
        std::memcpy(table + 1136, mc_r071, 16);
        std::memcpy(table + 1152, mc_r072, 16);
        std::memcpy(table + 1168, mc_r073, 16);
        std::memcpy(table + 1184, mc_r074, 16);
        std::memcpy(table + 1200, mc_r075, 16);
        std::memcpy(table + 1216, mc_r076, 16);
        std::memcpy(table + 1232, mc_r077, 16);
        std::memcpy(table + 1248, mc_r078, 16);
        std::memcpy(table + 1264, mc_r079, 16);
        std::memcpy(table + 1280, mc_r080, 16);
        std::memcpy(table + 1296, mc_r081, 16);
        std::memcpy(table + 1312, mc_r082, 16);
        std::memcpy(table + 1328, mc_r083, 16);
        std::memcpy(table + 1344, mc_r084, 16);
        std::memcpy(table + 1360, mc_r085, 16);
        std::memcpy(table + 1376, mc_r086, 16);
        std::memcpy(table + 1392, mc_r087, 16);
        std::memcpy(table + 1408, mc_r088, 16);
        std::memcpy(table + 1424, mc_r089, 16);
        std::memcpy(table + 1440, mc_r090, 16);
        std::memcpy(table + 1456, mc_r091, 16);
        std::memcpy(table + 1472, mc_r092, 16);
        std::memcpy(table + 1488, mc_r093, 16);
        std::memcpy(table + 1504, mc_r094, 16);
        std::memcpy(table + 1520, mc_r095, 16);
        std::memcpy(table + 1536, mc_r096, 16);
        std::memcpy(table + 1552, mc_r097, 16);
        std::memcpy(table + 1568, mc_r098, 16);
        std::memcpy(table + 1584, mc_r099, 16);
        std::memcpy(table + 1600, mc_r100, 16);
        std::memcpy(table + 1616, mc_r101, 16);
        std::memcpy(table + 1632, mc_r102, 16);
        std::memcpy(table + 1648, mc_r103, 16);
        std::memcpy(table + 1664, mc_r104, 16);
        std::memcpy(table + 1680, mc_r105, 16);
        std::memcpy(table + 1696, mc_r106, 16);
        std::memcpy(table + 1712, mc_r107, 16);
        std::memcpy(table + 1728, mc_r108, 16);
        std::memcpy(table + 1744, mc_r109, 16);
        std::memcpy(table + 1760, mc_r110, 16);
        std::memcpy(table + 1776, mc_r111, 16);
        std::memcpy(table + 1792, mc_r112, 16);
        std::memcpy(table + 1808, mc_r113, 16);
        std::memcpy(table + 1824, mc_r114, 16);
        std::memcpy(table + 1840, mc_r115, 16);
        std::memcpy(table + 1856, mc_r116, 16);
        std::memcpy(table + 1872, mc_r117, 16);
        std::memcpy(table + 1888, mc_r118, 16);
        std::memcpy(table + 1904, mc_r119, 16);
        std::memcpy(table + 1920, mc_r120, 16);
        std::memcpy(table + 1936, mc_r121, 16);
        std::memcpy(table + 1952, mc_r122, 16);
        std::memcpy(table + 1968, mc_r123, 16);
        std::memcpy(table + 1984, mc_r124, 16);
        std::memcpy(table + 2000, mc_r125, 16);
        std::memcpy(table + 2016, mc_r126, 16);
        std::memcpy(table + 2032, mc_r127, 16);
        std::memcpy(table + 2048, mc_r128, 16);
        std::memcpy(table + 2064, mc_r129, 16);
        std::memcpy(table + 2080, mc_r130, 16);
        std::memcpy(table + 2096, mc_r131, 16);
        std::memcpy(table + 2112, mc_r132, 16);
        std::memcpy(table + 2128, mc_r133, 16);
        std::memcpy(table + 2144, mc_r134, 16);
        std::memcpy(table + 2160, mc_r135, 16);
        std::memcpy(table + 2176, mc_r136, 16);
        std::memcpy(table + 2192, mc_r137, 16);
        std::memcpy(table + 2208, mc_r138, 16);
        std::memcpy(table + 2224, mc_r139, 16);
        std::memcpy(table + 2240, mc_r140, 16);
        std::memcpy(table + 2256, mc_r141, 16);
        std::memcpy(table + 2272, mc_r142, 16);
        std::memcpy(table + 2288, mc_r143, 16);
        std::memcpy(table + 2304, mc_r144, 16);
        std::memcpy(table + 2320, mc_r145, 16);
        std::memcpy(table + 2336, mc_r146, 16);
        std::memcpy(table + 2352, mc_r147, 16);
        std::memcpy(table + 2368, mc_r148, 16);
        std::memcpy(table + 2384, mc_r149, 16);
        std::memcpy(table + 2400, mc_r150, 16);
        std::memcpy(table + 2416, mc_r151, 16);
        std::memcpy(table + 2432, mc_r152, 16);
        std::memcpy(table + 2448, mc_r153, 16);
        std::memcpy(table + 2464, mc_r154, 16);
        std::memcpy(table + 2480, mc_r155, 16);
        std::memcpy(table + 2496, mc_r156, 16);
        std::memcpy(table + 2512, mc_r157, 16);
        std::memcpy(table + 2528, mc_r158, 16);
        std::memcpy(table + 2544, mc_r159, 16);
        std::memcpy(table + 2560, mc_r160, 16);
        std::memcpy(table + 2576, mc_r161, 16);
        std::memcpy(table + 2592, mc_r162, 16);
        std::memcpy(table + 2608, mc_r163, 16);
        std::memcpy(table + 2624, mc_r164, 16);
        std::memcpy(table + 2640, mc_r165, 16);
        std::memcpy(table + 2656, mc_r166, 16);
        std::memcpy(table + 2672, mc_r167, 16);
        std::memcpy(table + 2688, mc_r168, 16);
        std::memcpy(table + 2704, mc_r169, 16);
        std::memcpy(table + 2720, mc_r170, 16);
        std::memcpy(table + 2736, mc_r171, 16);
        std::memcpy(table + 2752, mc_r172, 16);
        std::memcpy(table + 2768, mc_r173, 16);
        std::memcpy(table + 2784, mc_r174, 16);
        std::memcpy(table + 2800, mc_r175, 16);
        std::memcpy(table + 2816, mc_r176, 16);
        std::memcpy(table + 2832, mc_r177, 16);
        std::memcpy(table + 2848, mc_r178, 16);
        std::memcpy(table + 2864, mc_r179, 16);
        std::memcpy(table + 2880, mc_r180, 16);
        std::memcpy(table + 2896, mc_r181, 16);
        std::memcpy(table + 2912, mc_r182, 16);
        std::memcpy(table + 2928, mc_r183, 16);
        std::memcpy(table + 2944, mc_r184, 16);
        std::memcpy(table + 2960, mc_r185, 16);
        std::memcpy(table + 2976, mc_r186, 16);
        std::memcpy(table + 2992, mc_r187, 16);
        std::memcpy(table + 3008, mc_r188, 16);
        std::memcpy(table + 3024, mc_r189, 16);
        std::memcpy(table + 3040, mc_r190, 16);
        std::memcpy(table + 3056, mc_r191, 16);
        std::memcpy(table + 3072, mc_r192, 16);
        std::memcpy(table + 3088, mc_r193, 16);
        std::memcpy(table + 3104, mc_r194, 16);
        std::memcpy(table + 3120, mc_r195, 16);
        std::memcpy(table + 3136, mc_r196, 16);
        std::memcpy(table + 3152, mc_r197, 16);
        std::memcpy(table + 3168, mc_r198, 16);
        std::memcpy(table + 3184, mc_r199, 16);
        std::memcpy(table + 3200, mc_r200, 16);
        std::memcpy(table + 3216, mc_r201, 16);
        std::memcpy(table + 3232, mc_r202, 16);
        std::memcpy(table + 3248, mc_r203, 16);
        std::memcpy(table + 3264, mc_r204, 16);
        std::memcpy(table + 3280, mc_r205, 16);
        std::memcpy(table + 3296, mc_r206, 16);
        std::memcpy(table + 3312, mc_r207, 16);
        std::memcpy(table + 3328, mc_r208, 16);
        std::memcpy(table + 3344, mc_r209, 16);
        std::memcpy(table + 3360, mc_r210, 16);
        std::memcpy(table + 3376, mc_r211, 16);
        std::memcpy(table + 3392, mc_r212, 16);
        std::memcpy(table + 3408, mc_r213, 16);
        std::memcpy(table + 3424, mc_r214, 16);
        std::memcpy(table + 3440, mc_r215, 16);
        std::memcpy(table + 3456, mc_r216, 16);
        std::memcpy(table + 3472, mc_r217, 16);
        std::memcpy(table + 3488, mc_r218, 16);
        std::memcpy(table + 3504, mc_r219, 16);
        std::memcpy(table + 3520, mc_r220, 16);
        std::memcpy(table + 3536, mc_r221, 16);
        std::memcpy(table + 3552, mc_r222, 16);
        std::memcpy(table + 3568, mc_r223, 16);
        std::memcpy(table + 3584, mc_r224, 16);
        std::memcpy(table + 3600, mc_r225, 16);
        std::memcpy(table + 3616, mc_r226, 16);
        std::memcpy(table + 3632, mc_r227, 16);
        std::memcpy(table + 3648, mc_r228, 16);
        std::memcpy(table + 3664, mc_r229, 16);
        std::memcpy(table + 3680, mc_r230, 16);
        std::memcpy(table + 3696, mc_r231, 16);
        std::memcpy(table + 3712, mc_r232, 16);
        std::memcpy(table + 3728, mc_r233, 16);
        std::memcpy(table + 3744, mc_r234, 16);
        std::memcpy(table + 3760, mc_r235, 16);
        std::memcpy(table + 3776, mc_r236, 16);
        std::memcpy(table + 3792, mc_r237, 16);
        std::memcpy(table + 3808, mc_r238, 16);
        std::memcpy(table + 3824, mc_r239, 16);
        std::memcpy(table + 3840, mc_r240, 16);
        std::memcpy(table + 3856, mc_r241, 16);
        std::memcpy(table + 3872, mc_r242, 16);
        std::memcpy(table + 3888, mc_r243, 16);
        std::memcpy(table + 3904, mc_r244, 16);
        std::memcpy(table + 3920, mc_r245, 16);
        std::memcpy(table + 3936, mc_r246, 16);
        std::memcpy(table + 3952, mc_r247, 16);
        std::memcpy(table + 3968, mc_r248, 16);
        std::memcpy(table + 3984, mc_r249, 16);
        std::memcpy(table + 4000, mc_r250, 16);
        std::memcpy(table + 4016, mc_r251, 16);
        std::memcpy(table + 4032, mc_r252, 16);
        std::memcpy(table + 4048, mc_r253, 16);
        std::memcpy(table + 4064, mc_r254, 16);
        std::memcpy(table + 4080, mc_r255, 16);
    }
    return table;
}

const uint8_t mc::edge_verts[12][2] = {
    {0,1}, {1,2}, {2,3}, {3,0},
    {4,5}, {5,6}, {6,7}, {7,4},
    {0,4}, {1,5}, {2,6}, {3,7},
};
