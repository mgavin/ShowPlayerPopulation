#pragma once
#include <chrono>

#include "bakkesmod/plugin/bakkesmodplugin.h"
#include "bakkesmod/plugin/pluginsettingswindow.h"
#include "bakkesmod/plugin/pluginwindow.h"

#include "bmhelper.h"
#include "imgui.h"

/*
 * SAMPLE:
 *
 * 13219:[19:48:07] [bakkesmod] [class ShowPlayerPopulation] POPULATION UPDATED AT TIME:
 2024-04-08T00:48:07.8930216+0000, TIMES UPDATED: 0,  POPULATION: 326275
 * 13272:[19:49:46] [bakkesmod] [class ShowPlayerPopulation] POPULATION UPDATED AT TIME:
 2024-04-08T00:49:46.8816692+0000, TIMES UPDATED: 1,  POPULATION: 325712
 * 13426:[19:54:48] [bakkesmod] [class ShowPlayerPopulation] POPULATION UPDATED AT TIME:
 2024-04-08T00:54:48.8792777+0000, TIMES UPDATED: 2,  POPULATION: 324136
 * 13578:[19:59:50] [bakkesmod] [class ShowPlayerPopulation] POPULATION UPDATED AT TIME:
 2024-04-08T00:59:50.8810702+0000, TIMES UPDATED: 3,  POPULATION: 322139
 */

class ShowPlayerPopulation :
        public BakkesMod::Plugin::BakkesModPlugin,
        public BakkesMod::Plugin::PluginSettingsWindow,
        public BakkesMod::Plugin::PluginWindow {
private:
        static const std::string    cmd_prefix;
        const std::filesystem::path RECORD_POPULATION_FILE =
                gameWrapper->GetDataFolder().append("ShowPlayerPopulation/RecordPopulationData.csv");
        const std::filesystem::path POP_NUMBER_PLACEMENTS_FILE =
                gameWrapper->GetDataFolder().append("ShowPlayerPopulation/FirstTimePopulationNumberPlacements.txt");
        const std::string DATETIME_FORMAT_STR = "{0:%F}T{0:%T%z}";
        const std::string DATETIME_PARSE_STR  = "%FT%T%z";

        std::chrono::zoned_time<std::chrono::system_clock::duration> last_time {std::chrono::current_zone()};
        int                                                          TOTAL_POP = 0;

        // flags for different points in the plugin
        bool is_overlay_open  = false;
        bool in_main_menu     = false;
        bool in_playlist_menu = false;
        bool in_game_menu     = false;
        bool DO_CHECK         = false;

        // data
        const std::vector<std::string> SHOWN_PLAYLIST_POPS =
                {"Casual", "Competitive", "Tournament", "Training", "Offline", "Private Match"};
        std::map<PlaylistId, int> playlist_population;

        const int hours_min  = 0;
        const int hours_max  = 168;
        int       hours_kept = 24;

        // flags for showing numbers above playlists
        // ordered 1-6, top left to bottom right
        bool slot1 = false;
        bool slot2 = false;
        bool slot3 = false;
        bool slot4 = false;
        bool slot5 = false;
        bool slot6 = false;
        bool show_all;

        ImVec2 onepos, twopos, threepos, fourpos, fivepos, sixpos;
        ImVec2 slot1_init_pos, slot2_init_pos, slot3_init_pos, slot4_init_pos, slot5_init_pos, slot6_init_pos;
        void   SNAPSHOT_PLAYLIST_POSITIONS();
        void   GET_DEFAULT_POP_NUMBER_PLACEMENTS();

        // these may end up going away
        bool showstats;
        bool curiouser;

        // member functions
        void                                               init_datafile();
        void                                               CHECK_NOW();
        void                                               write_population();
        void                                               massage_data();
        std::string                                        get_current_datetime_str();
        std::chrono::time_point<std::chrono::system_clock> get_timepoint_from_str(std::string);
        void                                               SET_WHICH_MENU_I_AM_IN();
        void                                               center_imgui_text(const std::string & text);
        void                                               add_underline(ImColor col_);

        void add_notifier(
                std::string                                   cmd_name,
                std::function<void(std::vector<std::string>)> do_func,
                std::string                                   desc,
                unsigned char                                 PERMISSIONS) const;

public:
        void onLoad() override;
        void onUnload() override;

        void        RenderSettings() override;
        std::string GetPluginName() override;
        void        SetImGuiContext(uintptr_t ctx) override;

        void        ShowOverlay();
        void        UnregisterOverlay();
        //
        // inherit from
        //				public BakkesMod::Plugin::PluginWindow
        //	 for the following
        void        OnOpen() override;
        void        OnClose() override;
        void        Render() override;
        std::string GetMenuName() override;
        std::string GetMenuTitle() override;
        bool        IsActiveOverlay() override;
        bool        ShouldBlockInput() override;
};
