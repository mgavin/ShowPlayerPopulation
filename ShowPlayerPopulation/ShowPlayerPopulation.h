#pragma once
#include <chrono>

#include "bakkesmod/plugin/bakkesmodplugin.h"
#include "bakkesmod/plugin/pluginsettingswindow.h"
#include "bakkesmod/plugin/pluginwindow.h"

#include "imgui.h"
#include "imgui_internal.h"

#include "bm_helper.h"
#include "imgui_helper.h"

class ShowPlayerPopulation :
        public BakkesMod::Plugin::BakkesModPlugin,
        public BakkesMod::Plugin::PluginSettingsWindow,
        public BakkesMod::Plugin::PluginWindow {
private:
        // variables pertaining to the plugin's functionality

        static inline const int DONT_SHOW_POP_BELOW_THRESHOLD = 10;
        // Why less than 10?
        // Developers tend to jump into modes at their whim
        // The population numbers are still tracked no matter who is in the
        // mode, so you would see 1 or 2 people in a mode that isn't available
        // to the public. I'm guessing, if they don't want to show population
        // data to begin with, maybe they don't want people stalking those
        // people? like, "why are they blah blah when blah blah?" ... idk. It
        // just doesn't seem very relevant to track the 1 or 2 or 9 people
        // playing in an inaccessible mode. .... except FaceIt for some reason.
        // and 10 ... because... to give an opportunity to
        // catch enough people in the custom training editor

        static inline const std::string          CMD_PREFIX                 = "spp_";
        static inline const std::chrono::seconds GRAPH_DATA_MASSAGE_TIMEOUT = std::chrono::seconds {15};
        const std::filesystem::path              RECORD_POPULATION_FILE =
                gameWrapper->GetDataFolder().append("ShowPlayerPopulation\\RecordPopulationData.csv");
        const std::filesystem::path POP_NUMBER_PLACEMENTS_FILE =
                gameWrapper->GetDataFolder().append("ShowPlayerPopulation\\FirstTimePopulationNumberPlacements.txt");
        const std::string DATETIME_FORMAT_STR = "{0:%F}T{0:%T%z}";
        const std::string DATETIME_PARSE_STR  = "%FT%T%z";

        // flags for different points in the plugin
        bool is_overlay_open       = false;
        bool lock_overlay          = false;
        bool show_overlay_header   = true;
        bool show_overlay_borders  = false;
        bool lock_overlay_columns  = false;
        bool in_main_menu          = false;
        bool in_playlist_menu      = false;
        bool in_game_menu          = false;
        bool show_in_main_menu     = false;
        bool show_in_playlist_menu = false;
        bool show_in_game_menu     = false;
        bool DO_CHECK              = false;

        // ImGui related variables
        ImGuiContext * plugin_ctx;
        ImFont *       overlay_font_18           = nullptr;
        ImFont *       overlay_font_22           = nullptr;
        ImVec4         chosen_overlay_color      = {1.0f, 1.0f, 1.0f, 0.9f};
        ImVec4         chosen_overlay_text_color = {0.0f, 0.0f, 0.0f, 1.0f};
        ImColor        col_black                 = ImColor {
                ImVec4 {0.0f, 0.0f, 0.0f, 1.0f}
        };
        ImColor col_white = ImColor {
                ImVec4 {1.0f, 1.0f, 1.0f, 1.0f}
        };

        // miscellaneous helper data. graphing should go here.
        struct graphed_data_t {  // three pair
                graphed_data_t()                                     = default;
                graphed_data_t & operator=(const graphed_data_t &) & = default;
                graphed_data_t & operator=(graphed_data_t &&) &      = default;
                graphed_data_t(const graphed_data_t &)               = default;
                graphed_data_t(graphed_data_t &&)                    = default;

                std::vector<float> xs;
                std::vector<float> ys;

                /// <summary>
                /// takes a float value and converts it to a string representing a datetime
                /// I think I should allow 2 rows - 32 characters max
                ///
                /// The general format is - takes a "tick value" (a float) and
                /// translates it into a custom string.
                /// </summary>
                /// <param name="dur">The amount of time since the UNIX epoch</param>
                /// <returns></returns>
                inline static std::string xlabel_transform_func(float inp) {
                        std::chrono::zoned_time tp {
                                std::chrono::current_zone(),
                                std::chrono::sys_time {
                                        std::chrono::duration<float, std::chrono::seconds::period> {inp}}};
                        return std::vformat("{0:%X}\n{0:%x}", std::make_format_args(tp));
                }
        };
        const std::vector<std::string> SHOWN_PLAYLIST_POPS =
                {"Casual", "Competitive", "Tournament", "Training", "Offline", "Private Match"};
        std::map<std::string, std::vector<std::pair<PlaylistId, int>>> population_data;
        int                                                            TOTAL_IN_GAME_POP = 0;
        std::chrono::zoned_seconds      last_massage_update {std::chrono::current_zone()};
        bool                            has_graph_data       = false;
        bool                            data_header_is_open  = false;
        bool                            graph_total_pop      = true;
        std::shared_ptr<graphed_data_t> graph_total_pop_data = std::make_shared<graphed_data_t>();  // {times, xs, ys}
        std::shared_ptr<std::map<PlaylistId, graphed_data_t>> graph_data =
                std::make_shared<std::map<PlaylistId, graphed_data_t>>();  // [PlaylistId] -> {times, xs, ys}
        std::map<PlaylistId, bool> graph_flags = []() {
                std::map<PlaylistId, bool> tmp;
                for (const auto & item : bm_helper::playlist_ids_str) {
                        tmp[item.first] = false;
                };
                return tmp;
        }();
        std::map<PlaylistId, bool> graph_flags_selected {graph_flags};

        // members pertaining to data functionality
        struct token {
                token()                            = default;
                token & operator=(const token &) & = default;
                token & operator=(token &&) &      = default;
                token(const token &)               = default;
                token(token &&)                    = default;
                token(std::chrono::zoned_seconds z, int tp, int tpo, std::map<PlaylistId, int> pp) :
                        zt(std::move(z)),
                        total_pop(std::move(tp)),
                        total_players_online(std::move(tpo)),
                        playlist_pop(std::move(pp)) {}

                std::chrono::zoned_seconds zt;
                int                        total_pop;
                int                        total_players_online;  // I've never seen it be unequal to total_pop
                std::map<PlaylistId, int>  playlist_pop;
        };

        // a bank full of tokens
        // it holds all the data during the operation of the plugin
        // ... instead of reading in and out of a file...
        // would be neat to separate out an interface
        // and that would take designing another class to basically
        // -> record, -> write -> read -> save... one class implementation would read / write to
        // the file for every operation; another implementation would just use the queue... but
        // I'm not doing that right now

        // it's a deque, as to support iteration through its elements
        std::deque<token> bank;

        bool      keep_all_data = false;
        const int hours_min     = 0;
        const int hours_max     = 168;
        int       hours_kept    = 24;

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

        // member functions pertaining to general functionality
        // init
        void init_datafile();
        void init_cvars();
        void init_hooked_events();
        void init_graph_data();
        void init_settings_handler();

        // provided functionality
        void record_population();
        void add_last_entry_to_graph_data();
        void massage_graph_data_operations(
                bool &,
                bool &,
                std::shared_ptr<graphed_data_t> &,
                std::shared_ptr<std::map<PlaylistId, graphed_data_t>> &);
        void massage_graph_data();
        void prepare_data();
        void prune_data();
        void write_data_to_file();
        void CHECK_NOW();

        // debug/print
        void print_bank_info();
        void print_graph_data();

        // helper functions
        void SET_WHICH_MENU_I_AM_IN();

        // deque -help
        ShowPlayerPopulation::token get_first_bank_entry();
        ShowPlayerPopulation::token get_last_bank_entry();

        // clear -help
        void clear_graph_total_pop_data();
        void clear_graph_data();
        void clear_graph_flags();

        // chrono -help
        std::string                get_current_datetime_str();
        std::chrono::zoned_seconds get_timepoint_from_str(std::string);

        // bakkesmod -help
        void add_notifier(
                std::string                                   cmd_name,
                std::function<void(std::vector<std::string>)> do_func,
                std::string                                   desc,
                unsigned char                                 PERMISSIONS);

        friend void * ImGuiSettingsReadOpen(ImGuiContext * ctx, ImGuiSettingsHandler * handler, const char * name);
        friend void   ImGuiSettingsReadLine(ImGuiContext *, ImGuiSettingsHandler *, void * entry, const char * line);
        friend void   ImGuiSettingsWriteAll(ImGuiContext * ctx, ImGuiSettingsHandler * handler, ImGuiTextBuffer * buf);
        static inline imgui_helper::OverlayHorizontalColumnsSettings h_cols = {{-1}};

public:
        void onLoad() override;
        void onUnload() noexcept override;

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
