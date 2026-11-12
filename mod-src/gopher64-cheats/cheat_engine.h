#ifndef GOPHER64_CHEAT_ENGINE_H
#define GOPHER64_CHEAT_ENGINE_H

typedef unsigned char CheatByte;
typedef unsigned short CheatHalf;
typedef unsigned int CheatWord;

typedef struct {
    CheatWord address;
    CheatHalf value;
    CheatByte type;
} CheatCode;

typedef struct {
    const char* id;
    const CheatCode* codes;
    CheatHalf code_count;
    CheatHalf choice_count;
} Cheat;

typedef struct {
    CheatHalf first;
    CheatHalf count;
} CheatGroup;

enum {
    CHEAT_WRITE_BYTE = 0x80,
    CHEAT_WRITE_HALF = 0x81,
    CHEAT_WALLET = 0x82,
    CHEAT_EQUAL_BYTE = 0xD0,
    CHEAT_RDRAM_SIZE = 0x800000,
};

static int apply_cheat_codes(volatile CheatByte* memory, const CheatCode* codes, CheatWord count) {
    int valid = 1;

    for (CheatWord i = 0; i < count; i++) {
        const CheatCode* code = &codes[i];
        if (code->address >= CHEAT_RDRAM_SIZE ||
            (code->type == CHEAT_WRITE_HALF && code->address + 1 >= CHEAT_RDRAM_SIZE)) {
            return 0;
        }

        switch (code->type) {
            case CHEAT_EQUAL_BYTE:
                valid = memory[code->address] == (CheatByte)code->value;
                break;
            case CHEAT_WRITE_BYTE:
                if (valid) {
                    memory[code->address] = (CheatByte)code->value;
                }
                valid = 1;
                break;
            case CHEAT_WRITE_HALF:
                if (valid) {
                    memory[code->address] = (CheatByte)(code->value >> 8);
                    memory[code->address + 1] = (CheatByte)code->value;
                }
                valid = 1;
                break;
            case CHEAT_WALLET:
                if (valid) {
                    memory[code->address] =
                        (memory[code->address] & (CheatByte)~0x30) | (code->value & 0x30);
                }
                valid = 1;
                break;
            default:
                return 0;
        }
    }

    return 1;
}

#endif
