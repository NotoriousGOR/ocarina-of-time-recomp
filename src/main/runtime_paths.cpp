#include "runtime_paths.h"

#include <system_error>

std::filesystem::path prepare_oot_runtime_path(const std::filesystem::path& shared_path) {
    namespace fs = std::filesystem;
    const auto runtime_path = shared_path / "oot";
    const auto shared_saves = shared_path / "saves";
    const auto runtime_saves = runtime_path / "saves";

    fs::create_directories(shared_saves);
    fs::create_directories(runtime_path);
    if (fs::exists(runtime_saves)) {
        if (!fs::equivalent(runtime_saves, shared_saves)) {
            throw fs::filesystem_error(
                "OoT save directory does not point to the shared saves",
                runtime_saves, shared_saves, std::make_error_code(std::errc::file_exists));
        }
    }
    else {
        fs::create_directory_symlink("../saves", runtime_saves);
    }

    for (const auto* name : {"mods.json", "mod_config"}) {
        const auto source = shared_path / name;
        const auto destination = runtime_path / name;
        if (fs::exists(source) && !fs::exists(destination)) {
            fs::copy(source, destination, fs::copy_options::recursive);
        }
    }
    return runtime_path;
}
