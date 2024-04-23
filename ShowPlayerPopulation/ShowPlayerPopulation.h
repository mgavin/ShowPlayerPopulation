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

        // flags for different points in the plugin
        bool is_overlay_open       = false;
        bool lock_overlay          = false;
        bool in_main_menu          = false;
        bool in_playlist_menu      = false;
        bool in_game_menu          = false;
        bool show_in_main_menu     = false;
        bool show_in_playlist_menu = false;
        bool show_in_game_menu     = false;
        bool DO_CHECK              = false;

        ImFont * overlay_font_18 = nullptr;
        ImFont * overlay_font_22 = nullptr;
        ImColor  col_black       = ImColor {
                ImVec4 {0.0f, 0.0f, 0.0f, 1.0f}
        };
        // data
        const std::vector<std::string> SHOWN_PLAYLIST_POPS =
                {"Casual", "Competitive", "Tournament", "Training", "Offline", "Private Match"};
        std::map<std::string, std::vector<std::pair<PlaylistId, int>>> population_data;
        int                                                            TOTAL_IN_GAME_POP = 0;

        struct token {
                using sc = std::chrono::system_clock;
                std::chrono::zoned_time<sc::duration> zt;
                int                                   total_pop;
                int                       total_players_online;  // I've never seen it be unequal to total_pop
                std::map<PlaylistId, int> playlist_pop;
        };
        // idk why it's called a bank
        // a bank full of tokens
        // it holds all the data during the operation of the plugin
        // ... instead of reading in and out of a file
        // would be neat to separate out an interface
        // and that would take desining another class to basically
        // -> record, -> write -> read -> save... one class implementation would read / write to the file for every
        // operation; another implementation would just use the queue...
        // but I'm not doing that right now

        // it's a deque, as to support iteration through its elements
        std::deque<token> bank;

        bool keep_all_data = false;

        /*
         * To make pruning work: put everything in a fucking queue
         * check the queue for times, keep track of the queue, update file as necessary
         *
         */

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
        void   print_bank_info();
        void   GET_DEFAULT_POP_NUMBER_PLACEMENTS();

        // these may end up going away
        bool showstats;
        bool curiouser;

        // member functions
        void init_datafile();
        void init_cvars();
        void init_hooked_events();
        void CHECK_NOW();
        void record_population();
        void prepare_data();
        void prune_data();
        void write_data_to_file();

        ShowPlayerPopulation::token                        get_first_entry();
        ShowPlayerPopulation::token                        get_last_entry();
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
