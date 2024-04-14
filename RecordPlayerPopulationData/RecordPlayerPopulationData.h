#pragma once
#include <chrono>
#include <format>
#include <fstream>
#include <ranges>
#include "bakkesmod/imgui/imgui.h"
#include "bakkesmod/plugin/bakkesmodplugin.h"
#include "bakkesmod/plugin/PluginSettingsWindow.h"
#include "bmhelper.h"
#include "csv.hpp"
#include "HookedEvents.h"

/*
 * SAMPLE:
 *
 * 13219:[19:48:07] [bakkesmod] [class RecordPlayerPopulationData] POPULATION UPDATED AT TIME:
 2024-04-08T00:48:07.8930216+0000, TIMES UPDATED: 0,  POPULATION: 326275
 * 13272:[19:49:46] [bakkesmod] [class RecordPlayerPopulationData] POPULATION UPDATED AT TIME:
 2024-04-08T00:49:46.8816692+0000, TIMES UPDATED: 1,  POPULATION: 325712
 * 13426:[19:54:48] [bakkesmod] [class RecordPlayerPopulationData] POPULATION UPDATED AT TIME:
 2024-04-08T00:54:48.8792777+0000, TIMES UPDATED: 2,  POPULATION: 324136
 * 13578:[19:59:50] [bakkesmod] [class RecordPlayerPopulationData] POPULATION UPDATED AT TIME:
 2024-04-08T00:59:50.8810702+0000, TIMES UPDATED: 3,  POPULATION: 322139
 */

class RecordPlayerPopulationData :
        public BakkesMod::Plugin::BakkesModPlugin,
        public BakkesMod::Plugin::PluginSettingsWindow {
private:
        static const std::string                           cmd_prefix;
        std::chrono::time_point<std::chrono::system_clock> last_time;

        const std::filesystem::path RECORD_POPULATION_FILE =
                gameWrapper->GetDataFolder().append("RecordPopulationData.csv");
        const std::string DATETIME_FORMAT_STR = "{0:%F}T{0:%T%z}";
        const std::string DATETIME_PARSE_STR  = "%FT%T%z";

        void                                               init_datafile();
        void                                               CHECK_NOW();
        void                                               write_population();
        std::string                                        get_current_datetime_str();
        std::chrono::time_point<std::chrono::system_clock> get_timepoint_from_str(std::string);
        int                                                TOTAL_POP               = 0;
        int                                                updated_count           = 1;
        int                                                seconds_counter         = 0;
        bool                                               is_connected_to_servers = false;

        void add_notifier(
                std::string                                   cmd_name,
                std::function<void(std::vector<std::string>)> do_func,
                std::string                                   desc,
                byte                                          PERMISSIONS = NULL) const;

public:
        void onLoad() override;
        void onUnload() override;

        void        RenderSettings() override;
        std::string GetPluginName() override;
        void        SetImGuiContext(uintptr_t ctx) override;

        void ShowOverlay(std::string oldvalue, CVarWrapper cvar);

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

const std::string RecordPlayerPopulationData::cmd_prefix = "rppd_";