#include <Windows.h>

#include <shellapi.h>
#include "ShowPlayerPopulation.h"

#include <format>
#include <fstream>
#include <ranges>
#include <ratio>

#include "bakkesmod/imgui/imgui_internal.h"

#include "csv.hpp"
#include "implot.h"

#include "HookedEvents.h"
#include "internal/csv_row.hpp"
#include "Logger.h"

/*
 *
 *  - cleanup code
 * todo: alpha release:
 *  - take out superfluous options used for testing
 *       .   make a new branch to clean up for "release"
 *
 */

const std::string          ShowPlayerPopulation::CMD_PREFIX                 = "spp_";
const std::chrono::seconds ShowPlayerPopulation::GRAPH_DATA_MASSAGE_TIMEOUT = std::chrono::seconds {15};

BAKKESMOD_PLUGIN(ShowPlayerPopulation, "ShowPlayerPopulation", "0.100.16", /*UNUSED*/ NULL);
std::shared_ptr<CVarManagerWrapper> _globalCvarManager;

/// <summary>
/// do the following when your plugin is loaded
///
/// is cvar manager only like, guaranteed in here? what the fuck??
/// </summary>
void ShowPlayerPopulation::onLoad() {
        plot_limits.X.Min = plot_limits.X.Max = plot_limits.Y.Min = plot_limits.Y.Max = 0;

        // initialize things
        _globalCvarManager        = cvarManager;
        HookedEvents::gameWrapper = gameWrapper;

        // ImGuiIO & io = ImGui::GetIO();
        // overlay_font_22 = io.Fonts->AddFontFromFileTTF(
        //         (gameWrapper->GetDataFolder().append("fonts/SourceSerifPro-Black.ttf")).string().c_str(),
        //         22.0f);
        // overlay_font_18 = io.Fonts->AddFontFromFileTTF(
        //         (gameWrapper->GetDataFolder().append("fonts/SourceSerifPro-Black.ttf")).string().c_str(),
        //         18.0f);

        init_datafile();
        init_cvars();
        init_hooked_events();
        init_graph_data();
        prepare_data();
        SET_WHICH_MENU_I_AM_IN();
}

void ShowPlayerPopulation::init_datafile() {
        if (!std::filesystem::exists(gameWrapper->GetDataFolder().append("ShowPlayerPopulation/"))) {
                // create plugin data directory if it doesn't exist
                std::filesystem::create_directory(gameWrapper->GetDataFolder().append("ShowPlayerPopulation/"));
        }

        std::ofstream file {RECORD_POPULATION_FILE, std::ios::app};
        if (!file.good()) {
                throw std::filesystem::filesystem_error("DATA FILE NOT GOOD! UNRECOVERABLE!~", std::error_code());
        }
        if (!std::filesystem::exists(RECORD_POPULATION_FILE) || std::filesystem::is_empty(RECORD_POPULATION_FILE)) {
                // a first run of the plugin
                csv::CSVWriter<std::ofstream> recordwriter {file};
                std::vector<std::string>      header {"DATETIME", "TOTALPOPULATION", "TOTALPLAYERSONLINE"};

                // EASILY COPY THE HEADERS OVER
                auto vv = std::views::values(bmhelper::playlist_ids_str);
                std::copy(begin(vv), end(vv), std::back_inserter(header));
                recordwriter << header;
        } else {
                // the data file exists.
                csv::CSVReader recordreader {RECORD_POPULATION_FILE.string()};
                csv::CSVRow    row;

                // read in all rows to be kept in memory
                const std::vector<std::string> header = recordreader.get_col_names();
                while (recordreader.read_row(row)) {
                        std::map<PlaylistId, int> playlist_pop;
                        for (const auto & str : header) {
                                if (bmhelper::playlist_str_ids.contains(str)) {
                                        playlist_pop[bmhelper::playlist_str_ids[str]] = row[str].get<int>();
                                }
                        }
                        bank.push_back(
                                token {.zt        = get_timepoint_from_str(row["DATETIME"].get<std::string>()),
                                       .total_pop = row["TOTALPOPULATION"].get<int>(),
                                       .total_players_online = row["TOTALPLAYERSONLINE"].get<int>(),
                                       .playlist_pop         = playlist_pop});
                }

                // just in case the input file had entries out of whack (which it shouldn't)
                std::sort(begin(bank), end(bank), [](const token & t1, const token & t2) {
                        return t1.zt.get_local_time() < t2.zt.get_local_time();
                });
        }

        // not working,yet
        GET_DEFAULT_POP_NUMBER_PLACEMENTS();
}

/// <summary>
/// This will house initialization code for all the cvars used for the plugin.
/// </summary>
void ShowPlayerPopulation::init_cvars() {
        // cvarManager->registerCvar(
        //         cmd_prefix + "enable_topscroll",
        //         "0",
        //         "Flag to determine showing the top ticker scroll showing player
        //         population");

        CVarWrapper show_overlay_cvar = cvarManager->registerCvar(
                CMD_PREFIX + "flag_show_overlay",
                "0",
                "Flag to determine if the overlay should be shown",
                false);
        show_overlay_cvar.addOnValueChanged([this](std::string old_value, CVarWrapper new_value) {
                if (is_overlay_open != new_value.getBoolValue()) {
                        gameWrapper->Execute([this](GameWrapper * gw) {
                                // look, sometimes cvarManager just gets fucking lost.
                                cvarManager->executeCommand("togglemenu ShowPlayerPopulation", false);
                        });
                }
        });

        CVarWrapper lock_overlay_cv =
                cvarManager->registerCvar(CMD_PREFIX + "flag_lock_overlay", "0", "Flag for locking the overlay", false);
        lock_overlay_cv.addOnValueChanged(
                [this](std::string old_value, CVarWrapper new_value) { lock_overlay = new_value.getBoolValue(); });

        CVarWrapper show_menu_cv = cvarManager->registerCvar(
                CMD_PREFIX + "flag_show_in_menu",
                "0",
                "Flag for showing the overlay in the main menu",
                false);
        show_menu_cv.addOnValueChanged(
                [this](std::string old_value, CVarWrapper new_value) { show_in_main_menu = new_value.getBoolValue(); });

        CVarWrapper show_playlist_menu_cv = cvarManager->registerCvar(
                CMD_PREFIX + "flag_show_in_playlist_menu",
                "0",
                "Flag for showing the overlay during playlist selection",
                false);
        show_playlist_menu_cv.addOnValueChanged([this](std::string old_value, CVarWrapper new_value) {
                show_in_playlist_menu = new_value.getBoolValue();
        });

        CVarWrapper show_game_menu_cv = cvarManager->registerCvar(
                CMD_PREFIX + "flag_show_in_game_menu",
                "0",
                "Flag for showing the overlay in game while paused",
                false);
        show_game_menu_cv.addOnValueChanged(
                [this](std::string old_value, CVarWrapper new_value) { show_in_game_menu = new_value.getBoolValue(); });

        CVarWrapper hrs_cvar = cvarManager->registerCvar(
                CMD_PREFIX + "hours_kept",
                "24",
                "Number of hours to keep of population data.",
                false);
        hrs_cvar.addOnValueChanged(
                [this](std::string old_value, CVarWrapper new_value) { hours_kept = new_value.getIntValue(); });

        CVarWrapper keep_cvar = cvarManager->registerCvar(
                CMD_PREFIX + "keep_indefinitely",
                "1",
                "Should data be kept indefinitely?",
                false);
        keep_cvar.addOnValueChanged(
                [this](std::string old_value, CVarWrapper new_value) { keep_all_data = new_value.getBoolValue(); });
}

/// <summary>
/// This will house initialization code for all the hooked events
/// </summary>
void ShowPlayerPopulation::init_hooked_events() {
        HookedEvents::AddHookedEvent(
                "Function ProjectX.OnlineGamePopulation_X.GetPlaylistPopulations",
                // conveniently stops after disconnected
                [this](std::string ev) {
                        MatchmakingWrapper mw = gameWrapper->GetMatchmakingWrapper();
                        if (mw) {
                                int tmpp = mw.GetTotalPlayersOnline();
                                // sadly this lags ... querying all those playlists at once :(
                                if (get_last_bank_entry().total_pop != tmpp && tmpp != 0
                                    && !gameWrapper->IsInOnlineGame()) {
                                        // THERE'S DATA!
                                        record_population();
                                        prepare_data();
                                        add_last_entry_to_graph_data();
                                }
                        }
                        data_header_is_open = false;
                });

        HookedEvents::AddHookedEvent(
                "Function ProjectX.PsyNetConnection_X.EventDisconnected",
                [](std::string eventName) {
                        // conveniently stops the playlist population query
                });

        HookedEvents::AddHookedEvent(
                "Function ProjectX.PsyNetConnection_X.EventConnected",
                [this](std::string eventName) { DO_CHECK = true; });

        HookedEvents::AddHookedEvent(
                "Function TAGame.StatusObserver_MenuStack_TA.HandleMenuChange",
                [this](std::string eventName) {
                        SET_WHICH_MENU_I_AM_IN();
                        if (in_main_menu && DO_CHECK) {
                                // roundabout way to try and avoid the "set playlist" error.
                                CHECK_NOW();
                                DO_CHECK = false;
                        }
                });

        HookedEvents::AddHookedEvent("Function Engine.GameInfo.PreExit", [this](std::string eventName) {
                // ASSURED CLEANUP
                prune_data();
                write_data_to_file();
        });
}

/// <summary>
/// PREPARE DATA STRUCTURES FOR GRAPHING
/// </summary>
void ShowPlayerPopulation::init_graph_data() {
        using namespace std::chrono;
        using namespace std::chrono_literals;

        zoned_seconds now {current_zone(), time_point_cast<seconds>(system_clock::now())};
        for (const token & t : bank) {
                std::chrono::zoned_seconds then      = t.zt;
                float                      time_diff = static_cast<float>(
                        duration_cast<minutes>(then.get_local_time() - now.get_local_time()).count());
                graph_total_pop_data.t.push_back(then);
                graph_total_pop_data.xs.push_back(time_diff);
                graph_total_pop_data.ys.push_back(t.total_pop);

                for (const auto & entry : t.playlist_pop) {
                        PlaylistId playid = entry.first;
                        int        pop    = entry.second;
                        if (pop < DONT_SHOW_POP_BELOW_THRESHOLD) {
                                continue;
                        }

                        graph_data[playid].t.push_back(then);
                        graph_data[playid].xs.push_back(time_diff);
                        graph_data[playid].ys.push_back(pop);
                        graph_flags[playid] = true;
                }

                has_graph_data = true;
        }
}

/// <summary>
/// Updates the data to include the latest population records.
/// </summary>
void ShowPlayerPopulation::record_population() {
        using namespace std::chrono;

        MatchmakingWrapper mw = gameWrapper->GetMatchmakingWrapper();

        if (mw) {
                token entry;
                entry.total_pop            = mw.GetTotalPopulation();
                entry.total_players_online = mw.GetTotalPlayersOnline();
                for (const auto & element : bmhelper::playlist_ids_str) {
                        entry.playlist_pop[element.first] = mw.GetPlayerCount(static_cast<PlaylistIds>(element.first));
                }
                entry.zt = zoned_seconds {current_zone(), time_point_cast<seconds>(system_clock::now())};
                bank.push_back(std::move(entry));
        }
}

/// <summary>
/// Puts the last population data point into the graph data.
/// </summary>
void ShowPlayerPopulation::add_last_entry_to_graph_data() {
        const token & t = get_last_bank_entry();
        if (t.total_pop == 0) {
                // protection against adding an empty entry
                // if population really is 0, rocket league is dead.
                return;
        }

        // I can get away with "0s" as xs.back() because it's ostensibly "now"-ish
        graph_total_pop_data.t.push_back(t.zt);
        graph_total_pop_data.xs.push_back(0.0f);
        graph_total_pop_data.ys.push_back(static_cast<float>(t.total_pop));

        for (const auto & entry : t.playlist_pop) {
                PlaylistId playid = entry.first;
                int        pop    = entry.second;
                if (pop < DONT_SHOW_POP_BELOW_THRESHOLD) {
                        continue;
                }

                graph_data[playid].t.push_back(t.zt);
                graph_data[playid].xs.push_back(0.0f);
                graph_data[playid].ys.push_back(static_cast<float>(pop));
                graph_flags[playid] = true;
        }
        has_graph_data = true;
}

/// <summary>
/// Updates the entries in the graph data to have a consistent time offset from "now".
/// </summary>
void ShowPlayerPopulation::massage_graph_data() {
        if (!data_header_is_open) {
                return;
        }

        using namespace std::chrono;
        using namespace std::chrono_literals;

        const token & t = get_last_bank_entry();
        zoned_seconds now {current_zone(), time_point_cast<seconds>(system_clock::now())};

        // only be updatable every 15 seconds
        if (now.get_local_time() - last_massage_update.get_local_time() < GRAPH_DATA_MASSAGE_TIMEOUT) {
                return;
        }

        // update every time difference in the list...
        for (size_t i : std::ranges::views::iota(0, static_cast<int>(graph_total_pop_data.t.size()))) {
                graph_total_pop_data.xs[i] =
                        static_cast<float>(duration_cast<minutes, float, seconds::period>(
                                                   graph_total_pop_data.t[i].get_local_time() - now.get_local_time())
                                                   .count());
        }

        for (const auto & entry : t.playlist_pop) {
                PlaylistId playid = entry.first;
                for (size_t i : std::ranges::views::iota(0, static_cast<int>(graph_data[playid].t.size()))) {
                        graph_data[playid].xs[i] = static_cast<float>(
                                duration_cast<minutes, float, seconds::period>(
                                        graph_total_pop_data.t[i].get_local_time() - now.get_local_time())
                                        .count());
                }
        }

        last_massage_update = now;
}

/// <summary>
/// Put the data in a representable format.
/// </summary>
void ShowPlayerPopulation::prepare_data() {
        const std::map<PlaylistId, int> & playlist_population = get_last_bank_entry().playlist_pop;

        // prepare data to be shown

        // clear persistent data that's used elsewhere :}
        population_data.clear();
        TOTAL_IN_GAME_POP = 0;

        // get max number of lines to display in a category on the overlay
        size_t mxlines = 0;
        for (const auto & neat : SHOWN_PLAYLIST_POPS) {
                mxlines = std::max(bmhelper::playlist_categories[neat].size(), mxlines);
        }

        // put those lines into a data structure
        // (this is solely done to avoid empty lines in the overlay display.)
        for (int line = 0; line < mxlines; ++line) {
                for (const auto & playstr : SHOWN_PLAYLIST_POPS) {
                        if (line >= bmhelper::playlist_categories[playstr].size()) {
                                continue;
                        } else {
                                PlaylistId id  = bmhelper::playlist_categories[playstr][line];
                                int        pop = 0;
                                if (playlist_population.contains(id)) {
                                        pop = playlist_population.at(id);
                                }
                                if (pop < DONT_SHOW_POP_BELOW_THRESHOLD) {
                                        continue;
                                }
                                population_data[playstr].push_back({id, pop});
                                TOTAL_IN_GAME_POP += pop;
                        }
                }
        }
}

/// <summary>
/// Puts the data in a state that adheres to specifications
/// Ensures that all recorded events fit within a certain timeframe.
/// </summary>
void ShowPlayerPopulation::prune_data() {
        if (keep_all_data) {
                return;
        }

        if (hours_kept == 0) {
                bank.clear();
        }

        // since the list is sorted, pop off the top
        using namespace std::chrono;
        using namespace std::chrono_literals;
        zoned_seconds now {current_zone(), time_point_cast<seconds>(system_clock::now())};
        hours         h {hours_kept};

        // get_first_entry or bank.front() works here due to shortcutting when asking bank.empty()
        while (!bank.empty() && (now.get_local_time() - bank.front().zt.get_local_time()) > h) {
                bank.pop_front();
        }
}

/************* NO REDUNDANCY CHECKING FOR DATA IN THIS PROGRAM &&&&&&&&&&&&&&&***********/
void ShowPlayerPopulation::write_data_to_file() {
        std::ifstream            ifile {RECORD_POPULATION_FILE};
        std::vector<std::string> header = csv::CSVReader {ifile}.get_col_names();  // THE ONLY "SANITY" {SO FAR}
        ifile.close();

        std::ofstream                 ofile {RECORD_POPULATION_FILE};
        csv::CSVWriter<std::ofstream> recordwriter {ofile};

        recordwriter << header;

        for (const auto & item : bank) {
                std::vector<std::string> data_written;
                data_written.push_back(std::vformat(DATETIME_FORMAT_STR, std::make_format_args(item.zt)));
                data_written.push_back(std::to_string(item.total_pop));
                data_written.push_back(std::to_string(item.total_players_online));
                for (const auto & col : header) {
                        if (bmhelper::playlist_str_ids.contains(col)) {
                                PlaylistId playid = bmhelper::playlist_str_ids[col];
                                data_written.push_back(std::to_string(item.playlist_pop.at(playid)));
                        }
                }
                recordwriter << data_written;
        }
        ofile.close();
}

void ShowPlayerPopulation::CHECK_NOW() {
        using namespace std::chrono;
        using namespace std::chrono_literals;

        zoned_seconds now {current_zone(), time_point_cast<seconds>(system_clock::now())};
        if (auto time_waited = (now.get_local_time() - get_last_bank_entry().zt.get_local_time());
            time_waited <= 5min) {
                // I wanted to have some sort of "timeout" to basically keep people from spamming their shit?
                // turn this into some sort of "disabled" timer in the future ~_~
                LOG("It's too soon to manually request another check of populations. {} seconds until available.",
                    5min - time_waited);
                return;
        }

        MatchmakingWrapper mw = gameWrapper->GetMatchmakingWrapper();
        if (mw) {
                // EXTRAS because (I believe) it gets around the "Please select a playlist" notification popping
                // up... maybe since there's no "EXTRAS" playlist category anymore? ... When I had this at
                // casual, I was seeing an annoying error popup sometimes.
                mw.StartMatchmaking(PlaylistCategory::EXTRAS);
                mw.CancelMatchmaking();
        }
}

/// <summary>
/// helper function to print data
/// used in conjunction with LOG
/// this is a member function because I want access to *this data
/// </summary>
void ShowPlayerPopulation::print_bank_info() {
        // this is a member function because I want access to *this data
        LOG("NUM ENTRIES IN BANK: {}", bank.size());
        LOG("HOURS BETWEEN LAST AND FIRST: {:%H}",
            get_last_bank_entry().zt.get_local_time() - get_first_bank_entry().zt.get_local_time());
}

void ShowPlayerPopulation::print_graph_data() {
        std::vector<float> xs = graph_total_pop_data.xs;
        std::vector<float> ys = graph_total_pop_data.ys;
        for (int i = 0; i < xs.size(); ++i) {
                LOG("{} {} {}", graph_total_pop_data.t[i], xs[i], ys[i]);
        }
}

/// <summary>
/// ImGui helper function that pushes some values that make an item appear "inactive"
/// </summary>
static inline void PushItemDisabled() {
        ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
}

/// <summary>
/// ImGui helper function that pops some values that make an item appear "inactive"
/// </summary>
static inline void PopItemDisabled() {
        ImGui::PopItemFlag();
        ImGui::PopStyleVar();
}

/// <summary>
/// https://mastodon.gamedev.place/@dougbinks/99009293355650878
/// </summary>
static inline void AddUnderline(ImColor col_) {
        ImVec2 min = ImGui::GetItemRectMin();
        ImVec2 max = ImGui::GetItemRectMax();
        min.y      = max.y;
        ImGui::GetWindowDrawList()->AddLine(min, max, col_, 1.0f);
}

/// <summary>
/// This is for helping with IMGUI stuff
///
/// copied from: https://github.com/ocornut/imgui/discussions/3862
/// </summary>
/// <param name="width">total width of items</param>
/// <param name="alignment">where on the line to align</param>
static inline void AlignForWidth(float width, float alignment = 0.5f) {
        float avail = ImGui::GetContentRegionAvail().x;
        float off   = (avail - width) * alignment;
        if (off > 0.0f)
                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + off);
}

/// <summary>
/// inspiration:
/// https://stackoverflow.com/questions/64653747/how-to-center-align-text-horizontally
/// </summary>
/// <param name="text">The text to center.</param>
static inline void CenterImGuiText(const std::string & text) {
        AlignForWidth(ImGui::CalcTextSize(text.c_str()).x);
        ImGui::TextUnformatted(text.c_str());
}

/// <summary>
/// taken from https://gist.github.com/dougbinks/ef0962ef6ebe2cadae76c4e9f0586c69
/// "hyperlink urls"
/// </summary>
static inline void TextURL(const char * name_, const char * URL_, uint8_t SameLineBefore_, uint8_t SameLineAfter_) {
        if (1 == SameLineBefore_) {
                ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
        }
        ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(0, 165, 255, 255));
        ImGui::Text(name_);
        ImGui::PopStyleColor();
        if (ImGui::IsItemHovered()) {
                if (ImGui::IsMouseClicked(0)) {
                        ShellExecute(NULL, "open", URL_, NULL, NULL, SW_SHOWNORMAL);
                }
                AddUnderline(ImGui::GetStyle().Colors[ImGuiCol_ButtonHovered]);
                ImGui::SetTooltip("  Open in browser\n%s", URL_);
        } else {
                AddUnderline(ImGui::GetStyle().Colors[ImGuiCol_Button]);
        }
        if (1 == SameLineAfter_) {
                ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
        }
}

/// <summary>
/// This call usually includes ImGui code that is shown and rendered (repeatedly,
/// on every frame rendered) when your plugin is selected in the plugin
/// manager. AFAIK, if your plugin doesn't have an associated *.set file for its
/// settings, this will be used instead.
/// </summary>
void ShowPlayerPopulation::RenderSettings() {
        using namespace std::chrono;

        // CVarWrapper ticker       = cvarManager->getCvar("rppd_enable_topscroll");
        // bool        top_scroller = ticker.getBoolValue();
        // if (ImGui::Checkbox("Enable ticker scrolling at the top?", &top_scroller)) {
        //        ticker.setValue(top_scroller);
        //}
        CVarWrapper shoverlay  = cvarManager->getCvar(CMD_PREFIX + "flag_show_overlay");
        bool        bshoverlay = shoverlay.getBoolValue();
        if (ImGui::Checkbox("Show the overlay?", &bshoverlay)) {
                shoverlay.setValue(bshoverlay);
        }

        ImGui::ColorPicker4(
                "PICK A COLOR!",
                &chosen_overlay_color.x,
                ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_AlphaPreview | ImGuiColorEditFlags_Float
                        | ImGuiColorEditFlags_DisplayRGB);
        // show in main menu, show in game, show in playlist menu | flags
        // still need color / transparency /location locking options for the overlays

        if (ImGui::Button("CHECK POPULATION NOW")) {
                gameWrapper->Execute([this](GameWrapper * gw) { CHECK_NOW(); });
        }

        if (ImGui::Checkbox("Lock overlay?", &lock_overlay)) {
                CVarWrapper cvw = cvarManager->getCvar(CMD_PREFIX + "flag_lock_overlay");
                cvw.setValue(lock_overlay);
        }

        if (ImGui::Checkbox("Show in main menu?", &show_in_main_menu)) {
                CVarWrapper cvw = cvarManager->getCvar(CMD_PREFIX + "flag_show_in_menu");
                cvw.setValue(show_in_main_menu);
        }
        ImGui::SameLine(0, 50.0f);
        if (ImGui::Checkbox("Show in playlist menu?", &show_in_playlist_menu)) {
                CVarWrapper cvw = cvarManager->getCvar(CMD_PREFIX + "flag_show_in_playlist_menu");
                cvw.setValue(show_in_playlist_menu);
        }
        ImGui::SameLine(0, 50.0f);
        if (ImGui::Checkbox("Show during pause menu in game?", &show_in_game_menu)) {
                CVarWrapper cvw = cvarManager->getCvar(CMD_PREFIX + "flag_show_in_game_menu");
                cvw.setValue(show_in_game_menu);
        }

        if (ImGui::Checkbox("SHOW ALL?", &show_all)) {
                if (show_all) {
                        slot1 = slot2 = slot3 = slot4 = slot5 = slot6 = true;
                } else {
                        slot1 = slot2 = slot3 = slot4 = slot5 = slot6 = false;
                }
        }

        if (ImGui::Checkbox("show window stats", &curiouser)) {
                if (curiouser) {
                        showstats = true;
                } else {
                        showstats = false;
                }
        }

        /*
         * On another note, a button that "snapshots" where things *are placed on the
         * overlay, and checkboxes that enable disable movement of windows.
         */
        if (ImGui::Button("SNAPSHOT PLAYLIST NUMBER POSITIONS")) {
                SNAPSHOT_PLAYLIST_POSITIONS();
        }

        ImGui::Text("%d hours selected. (%d days, %d hours)", hours_kept, hours_kept / 24, hours_kept % 24);

        ImGui::SameLine(100.0f, 200.0f);
        CVarWrapper keep_data_cvar = cvarManager->getCvar(CMD_PREFIX + "keep_indefinitely");
        bool        bkeep          = keep_data_cvar.getBoolValue();
        if (ImGui::Checkbox("KEEP DATA INDEFINITELY", &bkeep)) {
                keep_data_cvar.setValue(bkeep);
        }
        if (ImGui::IsItemHovered()) {
                ImGui::BeginTooltip();
                ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
                ImGui::TextUnformatted("IF SET, DATA WILL");
                ImGui::SameLine();
                ImGui::TextUnformatted("NOT");
                AddUnderline(col_white);
                ImGui::SameLine();
                ImGui::TextUnformatted("BE TRIMMED WHEN GAME EXITS");
                ImGui::PopTextWrapPos();
                ImGui::EndTooltip();
        }

        if (bkeep) {
                PushItemDisabled();
        }

        ImGui::SameLine(300.0f, 200.0f);
        static bool popup = false;
        if (ImGui::Button("PRUNE DATA FILE?")) {
                popup = true;
                ImGui::OpenPopup("PRUNE_DATA_CONFIRM");
        }
        if (ImGui::BeginPopupModal(
                    "PRUNE_DATA_CONFIRM",
                    &popup,
                    ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoDecoration)) {
                ImGui::SetWindowSize(ImVec2(300.0f, 60.0f), ImGuiCond_Always);
                CenterImGuiText("Are you sure you want to prune your data?");
                ImGuiStyle & style  = ImGui::GetStyle();
                float        width  = 0.0f;
                width              += 80.0f;  // fixed size button 1
                width              += style.ItemSpacing.x;
                width              += 50.0f;  // because of the sameline + spacing
                width              += 80.0f;  // fixed size button 2
                AlignForWidth(width, 0.5f);
                if (ImGui::Button("yes", ImVec2(80, 20))) {
                        ImGui::CloseCurrentPopup();
                        print_bank_info();
                        prune_data();
                }
                ImGui::SameLine(0.0f, 50.0f);
                if (ImGui::Button("no", ImVec2(80, 20))) {
                        // write_data_to_file();
                        print_graph_data();
                        ImGui::CloseCurrentPopup();
                }
                ImGui::EndPopup();
        }

        CVarWrapper hrs_cvar = cvarManager->getCvar(CMD_PREFIX + "hours_kept");
        int         hrs      = hrs_cvar.getIntValue();
        ImGui::SliderScalar(
                "How long should data be kept(hours)",
                ImGuiDataType_U8,
                &hrs,
                &hours_min,
                &hours_max,
                "%d");
        hrs = std::max(hours_min, std::min(hours_max, hrs));
        hrs_cvar.setValue(hrs);
        if (bkeep) {
                PopItemDisabled();
        }

        ImGui::NewLine();
        ImVec2 tmpts = ImGui::CalcTextSize(
                "DISCLAIMER:  THE FOLLOWING GRAPHED DATA IS ONLY BASED ON VALUES THAT HAVE BEEN SAVED LOCALLY.");
        AlignForWidth(tmpts.x);
        ImGui::TextUnformatted("DISCLAIMER:  THE FOLLOWING GRAPHED DATA IS");
        ImGui::SameLine();
        ImGui::TextUnformatted("ONLY");
        AddUnderline(col_white);
        ImGui::SameLine();
        ImGui::TextUnformatted("BASED ON VALUES THAT HAVE BEEN SAVED LOCALLY.");

        ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.0f, 0.2f, 0.8f, 1.0f));

        plot_limits.X.Min = plot_limits.X.Max = plot_limits.Y.Min = plot_limits.Y.Max = 0;
        if (has_graph_data && (data_header_is_open = ImGui::CollapsingHeader("Data"))) {
                massage_graph_data();

                float       width = 0.0f;
                std::string when_updates_txt =
                        std::vformat("The graph updates every {}", std::make_format_args(GRAPH_DATA_MASSAGE_TIMEOUT));
                width += ImGui::CalcTextSize("Double-click on plot to re-orient data.").x;
                width += 50.0f;
                width += ImGui::CalcTextSize("Double right-click on plot for options, such as to set bounds.").x;
                width += 50.0f;
                width += ImGui::CalcTextSize(when_updates_txt.c_str()).x;
                AlignForWidth(width);
                ImGui::TextUnformatted("Double-click on plot to re-orient data.");
                ImGui::SameLine(0.0f, 50.0f);
                ImGui::TextUnformatted("Double right-click on plot for options, such as to set bounds.");
                ImGui::SameLine(0.0f, 50.0f);
                ImGui::TextUnformatted(when_updates_txt.c_str());

                ImPlot::SetNextPlotLimits(
                        graph_total_pop_data.xs.front(),
                        0,
                        0,
                        std::ranges::max(graph_total_pop_data.ys),
                        ImGuiCond_FirstUseEver);
                if (ImPlot::BeginPlot(
                            "Population Numbers over Time",
                            "time in number of minutes ago (0 = now) (1440 minutes = 1 day)",
                            "pop",
                            ImVec2(-1, 0),
                            ImPlotFlags_Default,
                            ImPlotAxisFlags_Default,
                            ImPlotAxisFlags_Default | ImPlotAxisFlags_LockMin)) {
                        if (graph_total_pop) {
                                ImPlot::PlotLine(
                                        "TOTAL POPULATION",
                                        graph_total_pop_data.xs.data(),
                                        graph_total_pop_data.ys.data(),
                                        graph_total_pop_data.xs.size());
                        }

                        for (const auto & entry : graph_flags_selected) {
                                if (!entry.second) {
                                        continue;
                                }
                                ImPlot::PlotLine(
                                        bmhelper::playlist_ids_str_spaced[entry.first].c_str(),
                                        graph_data[entry.first].xs.data(),
                                        graph_data[entry.first].ys.data(),
                                        graph_data[entry.first].xs.size());
                        }
                        plot_limits = ImPlot::GetPlotLimits();

                        duration<float, minutes::period> left_minutes {plot_limits.X.Min};
                        zoned_seconds                    im_dead {current_zone()};
                        im_dead = time_point_cast<seconds>(last_massage_update.get_local_time())
                                  + duration_cast<seconds, float, minutes::period>(left_minutes);
                        std::string left_dt = std::vformat("{0:%D} {0:%r}", std::make_format_args(im_dead));

                        duration<float, minutes::period> right_minutes {plot_limits.X.Max};
                        zoned_seconds                    im_alive {current_zone()};
                        im_alive = time_point_cast<seconds>(last_massage_update.get_local_time())
                                   + duration_cast<seconds, float, minutes::period>(right_minutes);
                        std::string right_dt = std::vformat("{0:%D} {0:%r}", std::make_format_args(im_alive));

                        ImVec2 left_dt_sz  = ImGui::CalcTextSize(left_dt.c_str());
                        ImVec2 right_dt_sz = ImGui::CalcTextSize(right_dt.c_str());
                        ImVec2 pos         = ImPlot::GetPlotPos();
                        ImVec2 psize       = ImPlot::GetPlotSize();
                        ImPlot::PushPlotClipRect();
                        ImGui::GetWindowDrawList()->AddText(
                                pos + ImVec2 {0, psize.y} - ImVec2 {0.0f, left_dt_sz.y} + ImVec2 {15.0f, -15.0f},
                                IM_COL32(255, 255, 255, 255),
                                left_dt.c_str());
                        ImGui::GetWindowDrawList()->AddText(
                                pos + ImVec2 {psize.x, 0} + ImVec2 {-right_dt_sz.x, right_dt_sz.y}
                                        + ImVec2 {-15.0, 5.0f},
                                IM_COL32(255, 255, 255, 255),
                                right_dt.c_str());
                        ImPlot::PopPlotClipRect();
                        ImPlot::EndPlot();
                }
                // Maybe a list with selectable elements
                ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.5f, 0.1f, 0.0f, 0.7f));
                ImGui::BeginColumns(
                        "graphselectables_total_pop",
                        6,
                        ImGuiColumnsFlags_NoResize | ImGuiColumnsFlags_NoBorder);
                ImGui::Selectable("TOTAL POPULATION", &graph_total_pop);
                ImGui::EndColumns();
                ImGui::SameLine();
                ImGui::BeginColumns(
                        "graphselectables_help_text",
                        3,
                        ImGuiColumnsFlags_NoResize | ImGuiColumnsFlags_NoBorder);
                ImGui::NextColumn();
                ImGui::TextUnformatted("Click a category to see it graphed.");
                ImGui::NextColumn();
                ImGui::TextUnformatted(std::vformat(
                                               "There are {} available data points.",
                                               std::make_format_args(graph_total_pop_data.xs.size()))
                                               .c_str());
                ImGui::EndColumns();

                ImGui::BeginColumns("graphselectables", 6, ImGuiColumnsFlags_NoResize);
                size_t mxlines = 0;
                for (const std::string & header : SHOWN_PLAYLIST_POPS) {
                        ImGui::TextUnformatted(header.c_str());
                        AddUnderline(col_white);
                        ImGui::NextColumn();
                        mxlines = std::max(mxlines, bmhelper::playlist_categories[header].size());
                }
                for (int line = 0; line < mxlines; ++line) {
                        for (const std::string & category : SHOWN_PLAYLIST_POPS) {
                                if (line < bmhelper::playlist_categories[category].size()) {
                                        PlaylistId playid  = bmhelper::playlist_categories[category][line];
                                        bool       enabled = graph_flags[bmhelper::playlist_categories[category][line]];
                                        if (!enabled) {
                                                PushItemDisabled();
                                        }
                                        ImGui::Selectable(
                                                bmhelper::playlist_ids_str_spaced[playid].c_str(),
                                                &graph_flags_selected[playid]);

                                        if (!enabled) {
                                                PopItemDisabled();
                                        }
                                }
                                ImGui::NextColumn();
                        }
                }
                ImGui::EndColumns();
                ImGui::PopStyleColor();
        }

        // ADD A BIG OL' DISCLAIMER-EXPLANATION DOWN HERE ON HOW THINGS WORK!
        if (ImGui::CollapsingHeader("How does this work?")) {
                // ImGuiTextExplainingHowThisWorks();
                ImGui::TextUnformatted("Well...");
        }

        if (ImGui::CollapsingHeader("Some questions you may have...")) {
                // SOME TEXT EXPLAINING SOME QUESTIONS
                ImGui::SetWindowFontScale(1.1f);
                ImGui::Indent(80.0f);
                ImGui::TextUnformatted("WHERE IS THE DATA SAVED?");
                AddUnderline(col_white);
                ImGui::Unindent(80.0f);
                ImGui::TextUnformatted(
                        std::vformat("{}", std::make_format_args(RECORD_POPULATION_FILE.generic_string())).c_str());
                ImGui::NewLine();

                ImGui::Indent(80.0f);
                ImGui::TextUnformatted("WHAT IF I CRASH AND HAVE A PROBLEM?");
                AddUnderline(col_white);
                ImGui::Unindent(80.0f);
                ImGui::TextUnformatted("Raise an issue on the github page: ");
                TextURL("HERE", "https://github.com/mgavin/ShowPlayerPopulation", true, false);
                ImGui::NewLine();
                ImGui::SetWindowFontScale(1.0f);
        }
        ImGui::PopStyleColor();
}

/// <summary>
/// WHY? BECAUSE .front() ON AN EMPTY DEQUE IS UB
/// https://en.cppreference.com/w/cpp/container/deque/front
/// AND I WOULD RATHER IT RETURN AN EMPTY ENTRY
/// WHEN ASSUMEDLY GETTING THE FIRST ENTRY (SOMETIMES)
///
/// THIS IS SOLELY TO ADDRESS WHEN THE BANK IS EMPTY
/// </summary>
ShowPlayerPopulation::token ShowPlayerPopulation::get_first_bank_entry() {
        if (bank.empty()) {
                return token {};
        }

        return bank.front();
}

/// <summary>
/// WHY? BECAUSE .back() ON AN EMPTY DEQUE IS UB
/// https://en.cppreference.com/w/cpp/container/deque/back
/// AND I WOULD RATHER IT RETURN AN EMPTY ENTRY
/// WHEN ASSUMEDLY GETTING THE LAST ENTRY
///
/// THIS IS SOLELY TO ADDRESS WHEN THE BANK IS EMPTY
/// </summary>
ShowPlayerPopulation::token ShowPlayerPopulation::get_last_bank_entry() {
        if (bank.empty()) {
                return token {};
        }

        return bank.back();
}

/// <summary>
/// RETURNS THE DATETIME AS A STRING!
/// </summary>
/// <returns>_now_ represented as a datetime string</returns>
std::string ShowPlayerPopulation::get_current_datetime_str() {
        using namespace std::chrono;

        const zoned_seconds now {current_zone(), time_point_cast<seconds>(system_clock::now())};
        return std::vformat(DATETIME_FORMAT_STR, std::make_format_args(now));
}

/// <summary>
/// RETURNS THE TIMEPOINT AS INTERPRETED FROM A DATETIME STRING!
/// </summary>
/// <param name="str">a string representation of the datetime</param>
/// <returns>a time_point representing the string</returns>
std::chrono::zoned_seconds ShowPlayerPopulation::get_timepoint_from_str(std::string str) {
        using namespace std::chrono;

        sys_time<seconds>  tmpd;
        std::istringstream ss(str);
        ss >> parse(DATETIME_PARSE_STR, tmpd);
        return zoned_seconds {current_zone(), tmpd};
}

/// <summary>
/// Sets flags that help specify which portion of the menu the user is in.
/// </summary>
void ShowPlayerPopulation::SET_WHICH_MENU_I_AM_IN() {
        // clear flags
        in_main_menu = in_playlist_menu = in_game_menu = false;

        // check what menu we're in when this gets triggered
        MenuStackWrapper msw = gameWrapper->GetMenuStack();
        if (msw) {
                std::string menu_name = msw.GetTopMenu();
                if (menu_name == "RootMenuMovie") {
                        // set when on the main menu
                        in_main_menu = true;
                } else if (menu_name == "PlayMenuV4Movie") {
                        // set after hitting [Play] on the main menu
                        in_playlist_menu = true;
                } else if (menu_name == "MidGameMenuMovie") {
                        // set when opening up the menu during a game
                        in_game_menu = true;
                }
        }
}

/// <summary>
/// Records to a file the position of some windows I wanted to utilize in the future.
/// this is a member function because I want access to *this data
/// </summary>
void ShowPlayerPopulation::SNAPSHOT_PLAYLIST_POSITIONS() {
        // THIS SHOULD BE DONE AT MAX SCALE!
        std::ofstream file {POP_NUMBER_PLACEMENTS_FILE, std::ios::app};
        Vector2       disp = gameWrapper->GetScreenSize();
        if (disp.X == -1 && disp.Y == -1) {
                ImVec2 dsps = ImGui::GetIO().DisplaySize;
                disp.X      = dsps.x;
                disp.Y      = dsps.y;
        }
        file << std::vformat(
                "[{}x{}] {} {} {} {} {} {} {} {} {} {} {} {}",
                std::make_format_args(
                        disp.X,
                        disp.Y,
                        static_cast<int>(onepos.x),
                        static_cast<int>(onepos.y),
                        static_cast<int>(twopos.x),
                        static_cast<int>(twopos.y),
                        static_cast<int>(threepos.x),
                        static_cast<int>(threepos.y),
                        static_cast<int>(fourpos.x),
                        static_cast<int>(fourpos.y),
                        static_cast<int>(fivepos.x),
                        static_cast<int>(fivepos.y),
                        static_cast<int>(sixpos.x),
                        static_cast<int>(sixpos.y)))
             << std::endl;
}

/// <summary>
/// Helper function to read from a file, the placements for the windows...
/// not used yet.
/// </summary>
void ShowPlayerPopulation::GET_DEFAULT_POP_NUMBER_PLACEMENTS() {
        // init resolutions
        std::ifstream res_file {POP_NUMBER_PLACEMENTS_FILE};
        // format will be [2560x1440] x1 y1 x2 y2 x3 y3 x4 y4 x5 y5 x6 y6
        int           dispx, dispy;
        int           x1, y1;
        int           x2, y2;
        int           x3, y3;
        int           x4, y4;
        int           x5, y5;
        int           x6, y6;
        std::string   input;
        Vector2       screensize = gameWrapper->GetScreenSize();
        if (screensize.X == -1 && screensize.Y == -1) {
                ImVec2 dsps  = ImGui::GetIO().DisplaySize;
                screensize.X = dsps.x;
                screensize.Y = dsps.y;
        }
        while (std::getline(res_file, input)) {
                std::sscanf(
                        input.c_str(),
                        "[%dx%d] %d %d %d %d %d %d %d %d %d %d %d %d",
                        &dispx,
                        &dispy,
                        &x1,
                        &y1,
                        &x2,
                        &y2,
                        &x3,
                        &y3,
                        &x4,
                        &y4,
                        &x5,
                        &y5,
                        &x6,
                        &y6);
                if (screensize.X == dispx && screensize.Y == dispy) {
                        slot1_init_pos.x = static_cast<float>(x1), slot1_init_pos.y = static_cast<float>(y1);
                        slot2_init_pos.x = static_cast<float>(x2), slot2_init_pos.y = static_cast<float>(y2);
                        slot3_init_pos.x = static_cast<float>(x3), slot3_init_pos.y = static_cast<float>(y3);
                        slot4_init_pos.x = static_cast<float>(x4), slot4_init_pos.y = static_cast<float>(y4);
                        slot5_init_pos.x = static_cast<float>(x5), slot5_init_pos.y = static_cast<float>(y5);
                        slot6_init_pos.x = static_cast<float>(x6), slot6_init_pos.y = static_cast<float>(y6);
                }
        }

        // TRY TO "SCALE"!
        const float final_scale  = gameWrapper->GetDisplayScale() * gameWrapper->GetInterfaceScale();
        slot1_init_pos          *= final_scale;
        slot2_init_pos          *= final_scale;
        slot3_init_pos          *= final_scale;
        slot4_init_pos          *= final_scale;
        slot5_init_pos          *= final_scale;
        slot6_init_pos          *= final_scale;
}

/// <summary>
/// "SetImGuiContext happens when the plugins ImGui is initialized."
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
void ShowPlayerPopulation::SetImGuiContext(uintptr_t ctx) {
        ImGui::SetCurrentContext(reinterpret_cast<ImGuiContext *>(ctx));
}

/// <summary>
/// Used for... getting the plugin name, for like, when rendering settings in the bakkesmod plugin page
/// </summary>
std::string ShowPlayerPopulation::GetPluginName() {
        return "Show Player Population";
}

/// <summary>
/// Helper function to add a notifier. Not sure if I'll use this.
/// </summary>
void ShowPlayerPopulation::add_notifier(
        std::string                                   cmd_name,
        std::function<void(std::vector<std::string>)> do_func,
        std::string                                   desc,
        byte                                          PERMISSIONS = NULL) const {
        cvarManager->registerNotifier(CMD_PREFIX + cmd_name, do_func, desc, PERMISSIONS);
}

/* ------------------------- BEGIN TOGGLEMENU CODE ----------------------------- */

/// <summary>
/// (ImGui) Code called while rendering your menu window
/// </summary>
void ShowPlayerPopulation::Render() {
        if ((in_main_menu && show_in_main_menu) || (in_game_menu && show_in_game_menu)
            || (in_playlist_menu && show_in_playlist_menu)) {
                // SHOW THE DAMN NUMBERS, JIM!
                ImGui::SetNextWindowSize(ImVec2(100, 100), ImGuiCond_FirstUseEver);
                ImGui::SetNextWindowPos(ImVec2(100, 100), ImGuiCond_FirstUseEver);
                ImGui::PushStyleColor(ImGuiCol_WindowBg, chosen_overlay_color);
                ImGuiWindowFlags flags = ImGuiWindowFlags_None | ImGuiWindowFlags_NoCollapse;
                if (lock_overlay) {
                        flags |= ImGuiWindowFlags_NoInputs;
                }
                ImGui::Begin("Hey, cutie", NULL, flags);
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0, 0, 0, 1));
                ImGui::SetWindowFontScale(1.3f);
                // ImGui::PushFont(overlay_font_22);
                CenterImGuiText(std::vformat(
                                        "POPULATIONS! LAST UPDATED: {0:%r} {0:%D}",
                                        std::make_format_args(get_last_bank_entry().zt))
                                        .c_str());
                ImGui::NewLine();
                ImGui::Indent(20.0f);
                if (ImGui::GetWindowWidth() <= (ImGui::GetIO().DisplaySize.x / 2.0f)) {
                        // less than or equal to half of the width of the screen
                        // = "vertical layout"
                        ImGui::BeginColumns("populationnums_vert", 2, ImGuiColumnsFlags_NoResize);
                        ImGui::TextUnformatted("Total Players Online:");
                        AddUnderline(col_black);
                        ImGui::NextColumn();
                        CenterImGuiText(std::to_string(get_last_bank_entry().total_pop));
                        ImGui::NextColumn();
                        ImGui::TextUnformatted("Total Population in a Game:");
                        AddUnderline(col_black);
                        ImGui::NextColumn();
                        CenterImGuiText(std::to_string(TOTAL_IN_GAME_POP));
                        ImGui::NextColumn();

                        std::vector<std::pair<PlaylistId, int>> playlist_pops;

                        for (const auto & str : SHOWN_PLAYLIST_POPS) {
                                auto pop = population_data[str];
                                for (const auto & popv : pop) {
                                        playlist_pops.push_back({popv.first, popv.second});
                                }
                        }

                        // ImGui::PushFont(overlay_font_18);
                        for (const std::pair<PlaylistId, int> & ppops : playlist_pops) {
                                std::string playliststr = bmhelper::playlist_ids_str_spaced[ppops.first];
                                int         pop         = ppops.second;

                                ImGui::TextUnformatted(std::vformat("{}:", std::make_format_args(playliststr)).c_str());
                                ImGui::NextColumn();
                                CenterImGuiText(std::vformat("{}", std::make_format_args(pop)));
                                ImGui::NextColumn();
                        }
                        // ImGui::PopFont();
                        ImGui::EndColumns();
                } else {
                        // greater than half of the width of the screen
                        // "horizontal layout"

                        ImGui::BeginColumns(
                                "pop_horiz_tot",
                                6,
                                ImGuiColumnsFlags_NoResize | ImGuiColumnsFlags_NoBorder);
                        AlignForWidth(ImGui::GetCursorPos().x + ImGui::CalcTextSize("Total Players Online:").x);
                        ImGui::TextUnformatted("Total Players Online:");
                        AddUnderline(col_black);
                        ImGui::NextColumn();
                        CenterImGuiText(std::to_string(get_last_bank_entry().total_pop));
                        ImGui::NextColumn();
                        AlignForWidth(ImGui::GetCursorPos().x + ImGui::CalcTextSize("Total Population in a Game:").x);
                        ImGui::TextUnformatted("Total Population in a Game:");
                        AddUnderline(col_black);
                        ImGui::NextColumn();
                        CenterImGuiText(std::to_string(TOTAL_IN_GAME_POP));
                        ImGui::NextColumn();
                        ImGui::EndColumns();

                        size_t mxlines = 0;
                        for (const auto & x : population_data) {
                                mxlines = std::max(mxlines, x.second.size());
                        }
                        // ImGui::PushFont(overlay_font_18);
                        ImGui::BeginColumns("populationnums_horiz", 12, ImGuiColumnsFlags_NoResize);
                        for (int line = 0; line < mxlines; ++line) {
                                for (const auto & playstr : SHOWN_PLAYLIST_POPS) {
                                        if (line >= population_data[playstr].size()) {
                                                // nothing to show :(
                                                ImGui::NextColumn();
                                                ImGui::NextColumn();
                                                continue;
                                        } else {
                                                PlaylistId id  = population_data[playstr][line].first;
                                                int        pop = population_data[playstr][line].second;
                                                ImGui::TextUnformatted(
                                                        std::vformat(
                                                                "{}:",
                                                                std::make_format_args(
                                                                        bmhelper::playlist_ids_str_spaced[id]))
                                                                .c_str());
                                                ImGui::NextColumn();
                                                // MANUAL CENTERING? WTF ... the width isnt being correctly set.
                                                std::string str = std::vformat("{}", std::make_format_args(pop));
                                                ImGui::SetCursorPosX(
                                                        ImGui::GetCursorPosX()
                                                        + ((ImGui::GetContentRegionAvailWidth()
                                                            - ImGui::CalcTextSize(str.c_str()).x)
                                                           * 0.5));
                                                ImGui::TextUnformatted(str.c_str());
                                                ImGui::NextColumn();
                                        }
                                }
                        }
                        // ImGui::PopFont();
                        ImGui::EndColumns();
                }
                // ImGui::PopFont();
                ImGui::PopStyleColor();
                ImGui::PopStyleColor();
                ImGui::End();
        }

        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
        ImVec2      onebar, twobar, threebar, fourbar, fivebar, sixbar;
        const float WIN_HEIGHT = 40.0f;
        const float WIN_WIDTH  = 247.0f;
        // THE ImGuiCond_Always ON POSITIONS RIGHT NOW IS FOR TESTING!!!!!!!!!!!
        // LATER THEY WILL BE ImGuiCond_FirstUseEver
        if (slot1) {
                ImGui::SetNextWindowSize(ImVec2(WIN_WIDTH, WIN_HEIGHT), ImGuiCond_FirstUseEver);
                ImGui::SetNextWindowPos(slot1_init_pos, ImGuiCond_FirstUseEver);
                ImGui::Begin("show1", &slot1, ImGuiWindowFlags_NoTitleBar);
                onepos = ImGui::GetWindowPos();
                onebar = ImGui::GetWindowSize();
                ImGui::End();
        }
        if (slot2) {
                ImGui::SetNextWindowSize(ImVec2(WIN_WIDTH, WIN_HEIGHT), ImGuiCond_FirstUseEver);
                ImGui::SetNextWindowPos(slot2_init_pos, ImGuiCond_FirstUseEver);
                ImGui::Begin("show2", &slot2, ImGuiWindowFlags_NoTitleBar);
                twopos = ImGui::GetWindowPos();
                twobar = ImGui::GetWindowSize();
                ImGui::End();
        }
        if (slot3) {
                ImGui::SetNextWindowSize(ImVec2(WIN_WIDTH, WIN_HEIGHT), ImGuiCond_FirstUseEver);
                ImGui::SetNextWindowPos(slot3_init_pos, ImGuiCond_FirstUseEver);
                ImGui::Begin("show3", &slot3, ImGuiWindowFlags_NoTitleBar);
                threepos = ImGui::GetWindowPos();
                threebar = ImGui::GetWindowSize();
                ImGui::End();
        }
        if (slot4) {
                ImGui::SetNextWindowSize(ImVec2(WIN_WIDTH, WIN_HEIGHT), ImGuiCond_FirstUseEver);
                ImGui::SetNextWindowPos(slot4_init_pos, ImGuiCond_FirstUseEver);
                ImGui::Begin("show4", &slot4, ImGuiWindowFlags_NoTitleBar);
                fourpos = ImGui::GetWindowPos();
                fourbar = ImGui::GetWindowSize();
                ImGui::End();
        }
        if (slot5) {
                ImGui::SetNextWindowSize(ImVec2(WIN_WIDTH, WIN_HEIGHT), ImGuiCond_FirstUseEver);
                ImGui::SetNextWindowPos(slot5_init_pos, ImGuiCond_FirstUseEver);
                ImGui::Begin("show5", &slot5, ImGuiWindowFlags_NoTitleBar);
                fivepos = ImGui::GetWindowPos();
                fivebar = ImGui::GetWindowSize();
                ImGui::End();
        }
        if (slot6) {
                ImGui::SetNextWindowSize(ImVec2(WIN_WIDTH, WIN_HEIGHT), ImGuiCond_FirstUseEver);
                ImGui::SetNextWindowPos(slot6_init_pos, ImGuiCond_FirstUseEver);
                ImGui::Begin("show6", &slot6, ImGuiWindowFlags_NoTitleBar);
                sixpos = ImGui::GetWindowPos();
                sixbar = ImGui::GetWindowSize();
                ImGui::End();
        }
        if (showstats) {
                ImGui::SetNextWindowSize(ImVec2(100, 100), ImGuiCond_FirstUseEver);
                ImGui::SetNextWindowPos(ImVec2(70, 70), ImGuiCond_FirstUseEver);
                ImGui::Begin("stats", NULL);
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0, 0, 0, 1));
                CenterImGuiText(std::vformat(
                        "SHOW1| X: {} . Y: {} | WIDTH: {} . HEIGHT: {}",
                        std::make_format_args(onepos.x, onepos.y, onebar.x, onebar.y)));
                CenterImGuiText(std::vformat(
                        "SHOW1| X: {} . Y: {} | WIDTH: {} . HEIGHT: {}",
                        std::make_format_args(twopos.x, twopos.y, twobar.x, twobar.y)));
                CenterImGuiText(std::vformat(
                        "SHOW1| X: {} . Y: {} | WIDTH: {} . HEIGHT: {}",
                        std::make_format_args(threepos.x, threepos.y, threebar.x, threebar.y)));
                CenterImGuiText(std::vformat(
                        "SHOW1| X: {} . Y: {} | WIDTH: {} . HEIGHT: {}",
                        std::make_format_args(fourpos.x, fourpos.y, fourbar.x, fourbar.y)));
                CenterImGuiText(std::vformat(
                        "SHOW1| X: {} . Y: {} | WIDTH: {} . HEIGHT: {}",
                        std::make_format_args(fivepos.x, fivepos.y, fivebar.x, fivebar.y)));
                CenterImGuiText(std::vformat(
                        "SHOW1| X: {} . Y: {} | WIDTH: {} . HEIGHT: {}",
                        std::make_format_args(sixpos.x, sixpos.y, sixbar.x, sixbar.y)));
                ImGui::End();
                ImGui::PopStyleColor();
        }

        ImGui::PopStyleColor();
};

/// <summary>
/// do the following on menu open
/// </summary>
void ShowPlayerPopulation::OnOpen() {
        is_overlay_open = true;
};

/// <summary>
/// do the following on menu close
/// </summary>
void ShowPlayerPopulation::OnClose() {
        is_overlay_open = false;
};

/// <summary>
/// Returns the name of the menu to refer to it by
/// </summary>
/// <returns>The name used refered to by togglemenu</returns>
std::string ShowPlayerPopulation::GetMenuName() {
        return "ShowPlayerPopulation";
};

/// <summary>
/// Returns a std::string to show as the title
/// </summary>
/// <returns>The title of the menu</returns>
std::string ShowPlayerPopulation::GetMenuTitle() {
        return "";
};

/// <summary>
/// Is it the active overlay(window)?
/// Might make this susceptible to being closed with Esc
/// Makes it "active", aka, able to be manipulated when the plugin menu is closed.
/// </summary>
/// <returns>True/False for being the active overlay</returns>
bool ShowPlayerPopulation::IsActiveOverlay() {
        return false;
};

/// <summary>
/// Should this block input from the rest of the program?
/// (aka RocketLeague and BakkesMod windows)
/// </summary>
/// <returns>True/False for if bakkesmod should block input. Could be
/// `return ImGui::GetIO().WantCaptureMouse ||
/// ImGui::GetIO().WantCaptureKeyboard;`</returns>
bool ShowPlayerPopulation::ShouldBlockInput() {
        return false;
};

/* -------------------------- END TOGGLEMENU CODE ----------------------------- */

/// <summary>
///  do the following when your plugin is unloaded
/// </summary>
void ShowPlayerPopulation::onUnload() {
        // destroy things
        // dont throw here

        // DONT DO ANYTHING HERE BECAUSE IT ISNT GUARANTEED (UNLESS YOU MANUALLY UNLOAD PLUGIN)
        // SO IT'S JUST KIND OF FOR EASE OF DEVELOPMENT SINCE REBUILD = UNLOAD/LOAD
        prune_data();
        write_data_to_file();
}
