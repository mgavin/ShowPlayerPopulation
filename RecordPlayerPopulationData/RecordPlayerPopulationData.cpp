#include <format>
#include "RecordPlayerPopulationData.h"
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