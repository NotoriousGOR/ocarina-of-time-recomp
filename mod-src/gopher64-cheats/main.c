#include "modding.h"
#include "cheat_engine.h"

RECOMP_IMPORT("*", CheatWord recomp_get_config_u32(const char* key));
RECOMP_IMPORT("*", int recomp_printf(const char* format, ...));

#include "cheat_catalog.h"

RECOMP_HOOK("Play_Update") void apply_gopher64_cheats(void) {
    static int reported_error = 0;
    static int reported_conflict = 0;
    volatile CheatByte* memory = (volatile CheatByte*)0x80000000u;

    for (CheatWord i = 0; i < CHEAT_GROUP_COUNT; i++) {
        const CheatGroup* group = &cheat_groups[i];
        const Cheat* selected = 0;
        int conflict = 0;
        for (CheatWord j = 0; j < group->count; j++) {
            const Cheat* cheat = &cheats[group->first + j];
            CheatWord choice = recomp_get_config_u32(cheat->id);
            if (choice == 0 || choice > cheat->choice_count) {
                continue;
            }
            if (selected) {
                if (!reported_conflict) {
                    recomp_printf("Gopher64 cheats: conflicting options %s and %s; both ignored\n",
                                  selected->id, cheat->id);
                    reported_conflict = 1;
                }
                conflict = 1;
                break;
            }
            selected = cheat;
        }
        if (conflict || !selected) {
            continue;
        }

        if (!apply_cheat_codes(memory, selected->codes, selected->code_count) && !reported_error) {
            recomp_printf("Gopher64 cheats: invalid code in %s\n", selected->id);
            reported_error = 1;
        }
    }
}
