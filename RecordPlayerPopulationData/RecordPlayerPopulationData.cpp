#include "RecordPlayerPopulationData.h"
#include <format>
#include "HookedEvents.h"
#include "Logger.h"

BAKKESMOD_PLUGIN(RecordPlayerPopulationData, "RecordPlayerPopulationData", "0.0.0", /*UNUSED*/ NULL);
std::shared_ptr<CVarManagerWrapper> _globalCvarManager;

/// <summary>
/// do the following when your plugin is loaded
/// </summary>
void RecordPlayerPopulationData::onLoad() {
        // initialize things
        _globalCvarManager        = cvarManager;
        HookedEvents::gameWrapper = gameWrapper;

        init_datafile();

        // so tired...
        LOG("{}", last_time);
}

/// <summary>
///  do the following when your plugin is unloaded
/// </summary>
void RecordPlayerPopulationData::onUnload() {
        // destroy things
        // dont throw here
}

/// <summary>
/// This call usually includes ImGui code that is shown and rendered (repeatedly,
/// on every frame rendered) when your plugin is selected in the plugin
/// manager. AFAIK, if your plugin doesn't have an associated *.set file for its
/// settings, this will be used instead.
/// </summary>
void RecordPlayerPopulationData::RenderSettings() {
        // for imgui plugin window
        CVarWrapper ticker       = cvarManager->getCvar("rppd_enable_topscroll");
        bool        top_scroller = ticker.getBoolValue();
        if (ImGui::Checkbox("Enable ticker scrolling at the top?", &top_scroller)) {
                ticker.setValue(top_scroller);
        }
}

/// <summary>
/// "SetImGuiContext happens when the plugin’s ImGui is initialized."
/// https://wiki.bakkesplugins.com/imgui/custom_fonts/
///
/// also:
/// "Don't call this yourself, BM will call this function with a pointer
/// to the current ImGui context"
/// ...
///
/// so ¯\(°_o)/¯
/// </summary>
/// <param name="ctx">AFAIK The pointer to the ImGui context</param>
void RecordPlayerPopulationData::SetImGuiContext(uintptr_t ctx) {
        ImGui::SetCurrentContext(reinterpret_cast<ImGuiContext *>(ctx));
}

std::string RecordPlayerPopulationData::GetPluginName() {
        return "RecordPlayerPopulationData";
}

void RecordPlayerPopulationData::init_datafile() {
        std::ofstream file {RECORD_POPULATION_FILE, std::ios::app};
        if (!file.good()) {
                throw std::filesystem::filesystem_error("DATA FILE NOT GOOD! UNRECOVERABLE!~", std::error_code());
        }
        if (!std::filesystem::exists(RECORD_POPULATION_FILE) || std::filesystem::is_empty(RECORD_POPULATION_FILE)) {
                csv::CSVWriter<std::ofstream> recordwriter {file};
                std::vector<std::string>      header {"DATETIME"};
                header.emplace_back(
                        begin(bmhelper::playlist_ids_str),
                        end(bmhelper::playlist_ids_str),
                        [](const auto & ele) -> std::string { return ele.second; });
                recordwriter << header;
        } else {
                csv::CSVReader recordreader {RECORD_POPULATION_FILE.string()};
                csv::CSVRow    row;

                // go to last row
                while (recordreader.read_row(row))
                        ;
                last_time = row["DATETIME"].get<std::chrono::time_point<std::chrono::system_clock>>();
        }
}

std::string get_current_datetime() {
        std::chrono::system_clock clock;
        return std::format("{0:%F}T{0:%R%z}", clock.now());
}

/*
 * for when you've inherited from BakkesMod::Plugin::PluginWindow.
 * this lets  you do "togglemenu (GetMenuName())" in BakkesMod's console...
 * ie
 * if the following GetMenuName() returns "xyz", then you can refer to your
 * plugin's window in game through "togglemenu xyz"
 */

/*
/// <summary>
/// do the following on togglemenu open
/// </summary>
void RecordPlayerPopulationData::OnOpen() {};

/// <summary>
/// do the following on menu close
/// </summary>
void RecordPlayerPopulationData::OnClose() {};

/// <summary>
/// (ImGui) Code called while rendering your menu window
/// </summary>
void RecordPlayerPopulationData::Render() {};

/// <summary>
/// Returns the name of the menu to refer to it by
/// </summary>
/// <returns>The name used refered to by togglemenu</returns>
std::string RecordPlayerPopulationData::GetMenuName() {
        return "$safeprojectname";
};

/// <summary>
/// Returns a std::string to show as the title
/// </summary>
/// <returns>The title of the menu</returns>
std::string RecordPlayerPopulationData::GetMenuTitle() {
        return "";
};

/// <summary>
/// Is it the active overlay(window)?
/// </summary>
/// <returns>True/False for being the active overlay</returns>
bool RecordPlayerPopulationData::IsActiveOverlay() {
        return true;
};

/// <summary>
/// Should this block input from the rest of the program?
/// (aka RocketLeague and BakkesMod windows)
/// </summary>
/// <returns>True/False for if bakkesmod should block input</returns>
bool RecordPlayerPopulationData::ShouldBlockInput() {
        return false;
};
*/