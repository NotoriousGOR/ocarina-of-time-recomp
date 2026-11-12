#include "recomp_ui.h"
#include "zelda_config.h"
#include "zelda_support.h"
#include "librecomp/game.hpp"
#include "librecomp/helpers.hpp"
#include "ultramodern/ultramodern.hpp"
#include "RmlUi/Core.h"
#include <filesystem>
#include <string>

static std::string version_string;
static Rml::DataModelHandle launcher_model_handle;

bool oot_rom_valid = false;
bool majora_selected = true;

extern std::vector<recomp::GameEntry> supported_games;

static std::string oot_rom_error;

static void detect_oot_rom() {
    auto& game_id = supported_games[0].game_id;
    if (recomp::is_rom_valid(game_id)) {
        oot_rom_valid = true;
        oot_rom_error.clear();
        return;
    }

    const auto oot_rom_path =
            zelda64::get_app_folder_path() / "roms" / "oot.ntsc-u.rev1.z64";
    std::error_code ec;
    if (!std::filesystem::exists(oot_rom_path, ec)) {
        oot_rom_valid = false;
        if (ec) {
            oot_rom_error = "Could not check the Ocarina of Time ROM path.";
        }
        else {
            oot_rom_error = "Copy your NTSC-U Rev 1 ROM to:\n" + oot_rom_path.string();
        }
        return;
    }

    const auto error = recomp::select_rom(oot_rom_path, game_id);
    if (error == recomp::RomValidationError::Good) {
        recomp::check_all_stored_roms();
        oot_rom_valid = recomp::is_rom_valid(game_id);
        oot_rom_error.clear();
        return;
    }

    oot_rom_valid = false;
    switch (error) {
        case recomp::RomValidationError::FailedToOpen:
            oot_rom_error = "Could not open the ROM at:\n" + oot_rom_path.string();
            break;
        case recomp::RomValidationError::NotARom:
            oot_rom_error = "The file at the Ocarina of Time ROM path is not a valid N64 ROM.";
            break;
        case recomp::RomValidationError::IncorrectRom:
            oot_rom_error = "The ROM at the Ocarina of Time ROM path is not Ocarina of Time.";
            break;
        case recomp::RomValidationError::IncorrectVersion:
            oot_rom_error = "Ocarina of Time requires the North American Rev 1 (NTSC 1.1) ROM.";
            break;
        case recomp::RomValidationError::NotYet:
            oot_rom_error = "This ROM version is not supported.";
            break;
        case recomp::RomValidationError::OtherError:
            oot_rom_error = "An unknown error occurred while loading the Ocarina of Time ROM.";
            break;
        case recomp::RomValidationError::Good:
            break;
    }
}

recompui::ContextId launcher_context;
static bool autostart_requested = false;
static bool start_map_select = false;
static bool open_mods_requested = false;

void recompui::request_autostart(bool map_select) {
    autostart_requested = true;
    start_map_select = map_select;
}

void recompui::request_open_mods() {
    open_mods_requested = true;
}

static void open_mods_menu() {
    recompui::set_config_tab(recompui::ConfigTab::Mods);
    recompui::hide_all_contexts();
    recompui::show_context(recompui::get_config_context_id(), "");
}

static void launch_majora_app() {
#ifdef __APPLE__
    if (!zelda64::launch_majora_app()) {
        recompui::message_box(
                "Install Zelda 64: Recompiled in ~/Applications or /Applications to launch Majora's Mask.");
    }
#else
    recompui::message_box("Launching Majora's Mask from this app is supported on macOS only.");
#endif
}

// Called by the Setup_InitImpl patch to choose between the intro and the developer map select.
extern "C" void recomp_get_start_map_select(uint8_t* rdram, recomp_context* ctx) {
    _return(ctx, static_cast<s32>(start_map_select));
}

static void select_game(bool select_majora) {
    if (majora_selected != select_majora) {
        majora_selected = select_majora;
        launcher_model_handle.DirtyVariable("majora_selected");
    }
}

bool recompui::try_autostart() {
    if (open_mods_requested) {
        open_mods_requested = false;
        open_mods_menu();
        return true;
    }
    if (!autostart_requested) {
        return false;
    }
    detect_oot_rom();
    if (!oot_rom_valid) {
        return false;
    }
    autostart_requested = false;
    recomp::start_game(supported_games[0].game_id);
    return true;
}

recompui::ContextId recompui::get_launcher_context_id() {
	return launcher_context;
}

class LauncherMenu : public recompui::MenuController {
public:
    LauncherMenu() {
        detect_oot_rom();
    }
    ~LauncherMenu() override {

    }
    void load_document() override {
		launcher_context = recompui::create_context(zelda64::get_asset_path("launcher.rml"));
    }
    void register_events(recompui::UiEventListenerInstancer& listener) override {
        recompui::register_event(listener, "select_oot",
            [](const std::string& param, Rml::Event& event) {
                select_game(false);
            }
        );
        recompui::register_event(listener, "select_majora",
            [](const std::string& param, Rml::Event& event) {
                select_game(true);
            }
        );
        recompui::register_event(listener, "start_game",
            [](const std::string& param, Rml::Event& event) {
                if (majora_selected) {
                    launch_majora_app();
                    return;
                }
                detect_oot_rom();
                if (!oot_rom_valid) {
                    recompui::message_box(oot_rom_error.c_str());
                    return;
                }
                start_map_select = false;
                recomp::start_game(supported_games[0].game_id);
                recompui::hide_all_contexts();
            }
        );
        recompui::register_event(listener, "open_controls",
            [](const std::string& param, Rml::Event& event) {
                recompui::set_config_tab(recompui::ConfigTab::Controls);
                recompui::hide_all_contexts();
                recompui::show_context(recompui::get_config_context_id(), "");
            }
        );
        recompui::register_event(listener, "open_settings",
            [](const std::string& param, Rml::Event& event) {
                recompui::set_config_tab(recompui::ConfigTab::General);
                recompui::hide_all_contexts();
                recompui::show_context(recompui::get_config_context_id(), "");
            }
        );
        recompui::register_event(listener, "open_mods",
            [](const std::string &param, Rml::Event &event) {
                if (majora_selected) {
                    launch_majora_app();
                }
                else {
                    open_mods_menu();
                }
            }
        );
        recompui::register_event(listener, "exit_game",
            [](const std::string& param, Rml::Event& event) {
                ultramodern::quit();
            }
        );
    }
    void make_bindings(Rml::Context* context) override {
        Rml::DataModelConstructor constructor = context->CreateDataModel("launcher_model");

        constructor.Bind("majora_selected", &majora_selected);

        version_string = recomp::get_project_version().to_string();
        constructor.Bind("version_number", &version_string);
        launcher_model_handle = constructor.GetModelHandle();
    }
};

std::unique_ptr<recompui::MenuController> recompui::create_launcher_menu() {
    return std::make_unique<LauncherMenu>();
}
