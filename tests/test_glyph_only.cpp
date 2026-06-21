#include <cstdio>
#include "../fll/include/ThoughtGlyphBridge.h"
#include "test_thought_glyph_bridge.h"

int main() {
    printf("Glyph-only test runner\n");
    int failures = vn::tests::test_thought_glyph_bridge_tests();
    if (failures == 0)
        printf("ALL THOUGHT GLYPH BRIDGE TESTS PASS\n");
    else
        printf("SOME TESTS FAILED: %d failures\n", failures);
    return failures;
}
