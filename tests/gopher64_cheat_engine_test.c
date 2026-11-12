#undef NDEBUG
#include <assert.h>
#include "../mod-src/gopher64-cheats/cheat_engine.h"

int main(void) {
    CheatByte memory[16] = { 0 };
    const CheatCode codes[] = {
        { 0, 1, CHEAT_EQUAL_BYTE },
        { 1, 0xAB, CHEAT_WRITE_BYTE },
        { 2, 0x1234, CHEAT_WRITE_HALF },
        { 4, 0x67, CHEAT_WALLET },
    };

    memory[4] = 0x8B;
    assert(apply_cheat_codes(memory, codes, 4));
    assert(memory[1] == 0);
    assert(memory[2] == 0x12 && memory[3] == 0x34);
    assert(memory[4] == 0xAB);

    memory[0] = 1;
    assert(apply_cheat_codes(memory, codes, 4));
    assert(memory[1] == 0xAB);

    const CheatCode invalid[] = { { CHEAT_RDRAM_SIZE - 1, 0x1234, CHEAT_WRITE_HALF } };
    assert(!apply_cheat_codes(memory, invalid, 1));
    return 0;
}
