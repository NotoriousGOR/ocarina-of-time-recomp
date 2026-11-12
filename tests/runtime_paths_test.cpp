#include "../src/main/runtime_paths.h"

#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <unistd.h>

namespace fs = std::filesystem;

static std::string read(const fs::path& path) {
    std::ifstream input(path);
    return {std::istreambuf_iterator<char>{input}, {}};
}

int main() {
    const auto shared = fs::temp_directory_path() / ("oot-runtime-path-test-" + std::to_string(getpid()));
    if (!fs::create_directory(shared)) return 1;
    fs::create_directory(shared / "saves");
    fs::create_directory(shared / "mod_config");
    std::ofstream(shared / "saves/save.bin") << "existing save";
    std::ofstream(shared / "mods.json") << "original settings";
    std::ofstream(shared / "mod_config/oot_test.json") << "original option";

    const auto oot = prepare_oot_runtime_path(shared);
    if (!fs::equivalent(oot / "saves", shared / "saves") ||
        read(oot / "saves/save.bin") != "existing save" ||
        read(oot / "mods.json") != "original settings" ||
        read(oot / "mod_config/oot_test.json") != "original option") return 1;

    std::ofstream(oot / "saves/save.bin") << "updated save";
    std::ofstream(oot / "mods.json") << "updated settings";
    std::ofstream(oot / "mod_config/oot_test.json") << "updated option";
    prepare_oot_runtime_path(shared);
    if (read(shared / "saves/save.bin") != "updated save" ||
        read(shared / "mods.json") != "original settings" ||
        read(shared / "mod_config/oot_test.json") != "original option" ||
        read(oot / "mods.json") != "updated settings" ||
        read(oot / "mod_config/oot_test.json") != "updated option") return 1;

    fs::remove(oot / "saves");
    fs::create_directory(oot / "saves");
    bool conflict_rejected = false;
    try {
        prepare_oot_runtime_path(shared);
    }
    catch (const fs::filesystem_error&) {
        conflict_rejected = true;
    }
    if (!conflict_rejected || read(shared / "saves/save.bin") != "updated save") return 1;

    fs::remove(oot / "saves");
    fs::remove(oot / "mod_config/oot_test.json");
    fs::remove(oot / "mod_config");
    fs::remove(oot / "mods.json");
    fs::remove(oot);
    fs::remove(shared / "mod_config/oot_test.json");
    fs::remove(shared / "mod_config");
    fs::remove(shared / "mods.json");
    fs::remove(shared / "saves/save.bin");
    fs::remove(shared / "saves");
    fs::remove(shared);
    std::cout << "Separate OoT mod settings preserve shared saves\n";
}
