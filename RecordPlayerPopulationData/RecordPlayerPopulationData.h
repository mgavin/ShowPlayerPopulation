#pragma once
#include <chrono>
#include <fstream>
#include "bakkesmod/imgui/imgui.h"
#include "bakkesmod/plugin/bakkesmodplugin.h"
#include "bakkesmod/plugin/PluginSettingsWindow.h"
#include "bmhelper.h"
#include "csv.hpp"
#include "HookedEvents.h"

class RecordPlayerPopulationData :
        public BakkesMod::Plugin::BakkesModPlugin,
        public BakkesMod::Plugin::PluginSettingsWindow {
private:
        std::chrono::time_point<std::chrono::system_clock> last_time;

        const std::filesystem::path RECORD_POPULATION_FILE =
                gameWrapper->GetDataFolder().append("RecordPopulationData.csv");

        void init_datafile();

public:
        void onLoad() override;
        void onUnload() override;

        void        RenderSettings() override;
        std::string GetPluginName() override;
        void        SetImGuiContext(uintptr_t ctx) override;

        //
        // inherit from
        //				public BakkesMod::Plugin::PluginWindow
        //	 for the following
        // void        OnOpen() override;
        // void        OnClose() override;
        // void        Render() override;
        // std::string GetMenuName() override;
        // std::string GetMenuTitle() override;
        // bool        IsActiveOverlay() override;
        // bool        ShouldBlockInput() override;
};
