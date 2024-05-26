#include <Windows.h>

#include <shellapi.h>
#include <stringapiset.h>
#include <winnls.h>
#include "ShowPlayerPopulation.h"

#include <format>
#include <fstream>
#include <functional>
#include <ranges>
#include <thread>

#include "csv.hpp"
#include "implot.h"

#include "HookedEvents.h"
#include "internal/csv_row.hpp"
#include "Logger.h"

BAKKESMOD_PLUGIN(ShowPlayerPopulation, "ShowPlayerPopulation", "2.0.0", /*UNUSED*/ NULL);
std::shared_ptr<CVarManagerWrapper> _globalCVarManager;

/// <summary>
/// do the following when your plugin is loaded
/// </summary>
void ShowPlayerPopulation::onLoad() {
        // initialize things
        _globalCVarManager        = cvarManager;
        HookedEvents::gameWrapper = gameWrapper;

        // init time zone for graphs
        _putenv_s("TZ", tz->name().data());

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
                auto vv = std::views::values(bm_helper::playlist_ids_str);
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
                                if (bm_helper::playlist_str_ids.contains(str)) {
                                        playlist_pop[bm_helper::playlist_str_ids[str]] = row[str].get<int>();
                                }
                        }
                        bank.push_back(
                                token {get_timepoint_from_str(row["DATETIME"].get<std::string>()),
                                       row["TOTALPOPULATION"].get<int>(),
                                       row["TOTALPLAYERSONLINE"].get<int>(),
                                       playlist_pop});
                }

                // just in case the input file had entries out of whack (which it shouldn't)
                std::sort(begin(bank), end(bank), [](const token & t1, const token & t2) {
                        return t1.zt.get_local_time() < t2.zt.get_local_time();
                });
        }
}

/// <summary>
/// This will house initialization code for all the cvars used for the plugin.
/// </summary>
void ShowPlayerPopulation::init_cvars() {
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

        CVarWrapper lock_overlay_columns_cv = cvarManager->registerCvar(
                CMD_PREFIX + "flag_lock_overlay_columns",
                "0",
                "Flag for locking the overlay's columns",
                false);
        lock_overlay_columns_cv.addOnValueChanged([this](std::string old_value, CVarWrapper new_value) {
                lock_overlay_columns = new_value.getBoolValue();
        });

        CVarWrapper show_overlay_borders_cv = cvarManager->registerCvar(
                CMD_PREFIX + "flag_show_overlay_borders",
                "0",
                "Flag for showing the overlay's borders",
                false);
        show_overlay_borders_cv.addOnValueChanged([this](std::string old_value, CVarWrapper new_value) {
                show_overlay_borders = new_value.getBoolValue();
        });

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
                "0",
                "Should data be kept indefinitely?",
                false);
        keep_cvar.addOnValueChanged(
                [this](std::string old_value, CVarWrapper new_value) { keep_all_data = new_value.getBoolValue(); });
        keep_all_data = keep_cvar.getBoolValue();
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
                                // an attempt to find the best point at which to call this
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

        // clear graph data, just in case it came from a previous time where it had data
        // if it doesn't have data already, then this effectively does nothing
        clear_graph_total_pop_data();
        clear_graph_total_in_game_data();
        clear_graph_data();
        clear_graph_flags();

        for (const token & t : bank) {
                float thenf = duration<float, seconds::period> {t.zt.get_sys_time().time_since_epoch()}.count();
                graph_total_pop_data->xs.push_back(thenf);
                graph_total_pop_data->ys.push_back(static_cast<float>(t.total_pop));

                int total_in_game = 0;
                for (const auto & entry : t.playlist_pop) {
                        PlaylistId playid = entry.first;
                        int        pop    = entry.second;
                        if (pop < DONT_SHOW_POP_BELOW_THRESHOLD) {
                                continue;
                        }

                        (*graph_data)[playid].xs.push_back(thenf);
                        (*graph_data)[playid].ys.push_back(static_cast<float>(pop));
                        graph_flags[playid] = true;

                        total_in_game += pop;
                }

                graph_total_in_game_data->xs.push_back(thenf);
                graph_total_in_game_data->ys.push_back(total_in_game);

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
                for (const auto & element : bm_helper::playlist_ids_str) {
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

        using namespace std::chrono;
        using namespace std::chrono_literals;

        float timef = duration<float, seconds::period> {t.zt.get_sys_time().time_since_epoch()}.count();

        graph_total_pop_data->xs.push_back(timef);
        graph_total_pop_data->ys.push_back(static_cast<float>(t.total_pop));

        int total_in_game = 0;
        for (const auto & entry : t.playlist_pop) {
                PlaylistId playid = entry.first;
                int        pop    = entry.second;
                if (pop < DONT_SHOW_POP_BELOW_THRESHOLD) {
                        continue;
                }

                (*graph_data)[playid].xs.push_back(timef);
                (*graph_data)[playid].ys.push_back(static_cast<float>(pop));
                graph_flags[playid] = true;

                total_in_game += pop;
        }

        graph_total_in_game_data->xs.push_back(timef);
        graph_total_in_game_data->ys.push_back(total_in_game);

        has_graph_data = true;
}

/// <summary>
/// Put the data in a representable format.
/// </summary>
void ShowPlayerPopulation::prepare_data() {
        // prepare data to be shown
        const std::map<PlaylistId, int> & playlist_population = get_last_bank_entry().playlist_pop;

        // clear persistent data that's used elsewhere :}
        population_data.clear();
        TOTAL_IN_GAME_POP = 0;

        // get max number of lines to display in a category on the overlay
        size_t mxlines = 0;
        for (const auto & neat : SHOWN_PLAYLIST_POPS) {
                mxlines = std::max(bm_helper::playlist_categories[neat].size(), mxlines);
        }

        // put those lines into a data structure
        // (this is solely done to avoid empty lines in the overlay display.)
        for (int line = 0; line < mxlines; ++line) {
                for (const auto & playstr : SHOWN_PLAYLIST_POPS) {
                        if (line >= bm_helper::playlist_categories[playstr].size()) {
                                continue;
                        } else {
                                PlaylistId id  = bm_helper::playlist_categories[playstr][line];
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

        // get_first_entry or bank.front() works here due to shortcutting when asking
        // bank.empty()
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
                        if (bm_helper::playlist_str_ids.contains(col)) {
                                PlaylistId playid = bm_helper::playlist_str_ids[col];
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
                // I wanted to have some sort of "timeout" to basically keep people from
                // spamming their shit? turn this into some sort of "disabled" timer in the
                // future ~_~
                LOG("It's too soon to manually request another check of populations. {} "
                    "seconds until available.",
                    5min - time_waited);
                return;
        }

        MatchmakingWrapper mw = gameWrapper->GetMatchmakingWrapper();
        if (mw) {
                // It's my theory that the playlist selection and the category selected here
                // result in something being "selected", yet nothing gets selected because it's
                // not technically valid, but this prevents the "Error: Select a playlist" error
                // from popping up -_-

                // I'm so over this. Fuck it and fuck this broken API.
                // mw.SetPlaylistSelection(Playlist::EXTRAS_SNOWDAY, true);

                // there isn't an extras playlist anymore...
                mw.StartMatchmaking(PlaylistCategory::EXTRAS);
                mw.CancelMatchmaking();
        }
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
        if (off > 0.0f) {
                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + off);
        }
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
        ImGui::Text("%s", name_);
        ImGui::PopStyleColor();
        if (ImGui::IsItemHovered()) {
                if (ImGui::IsMouseClicked(0)) {
                        // What if the URL length is greater than int but less than size_t?
                        // well then the program should crash, but this is fine.
                        const int nchar =
                                std::clamp(static_cast<int>(std::strlen(URL_)), 0, std::numeric_limits<int>::max());
                        wchar_t * URL = new wchar_t[nchar + 1];
                        MultiByteToWideChar(CP_UTF8, 0, URL_, nchar, URL, nchar);
                        ShellExecuteW(NULL, L"open", URL, NULL, NULL, SW_SHOWNORMAL);

                        delete[] URL;
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

        CVarWrapper shoverlay  = cvarManager->getCvar(CMD_PREFIX + "flag_show_overlay");
        bool        bshoverlay = shoverlay.getBoolValue();
        if (ImGui::Checkbox("Show the overlay?", &bshoverlay)) {
                shoverlay.setValue(bshoverlay);
        }

        ImGui::NewLine();
        ImGui::Separator();
        ImGui::Indent(80.0f);

        // widgets for being able to choose the overlay's color settings
        ImGuiColorEditFlags cef = ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_AlphaPreview
                                  | ImGuiColorEditFlags_Float | ImGuiColorEditFlags_DisplayRGB;
        bool open_popup1 = ImGui::Button("Choose color for the overlay background.");
        ImGui::SameLine(0, ImGui::GetStyle().ItemInnerSpacing.x);
        open_popup1 |= ImGui::ColorButton("OverlayColor", chosen_overlay_color, cef);
        if (open_popup1) {
                ImGui::OpenPopup("overlaycolorpicker");
        }
        with_Popup("overlaycolorpicker") {
                ImGui::Text("Choose the background color for the overlay.");
                ImGui::Separator();
                ImGui::ColorPicker4(
                        "##picker",
                        (float *)&chosen_overlay_color,
                        cef | ImGuiColorEditFlags_NoSidePreview | ImGuiColorEditFlags_NoSmallPreview);
        }
        ImGui::SameLine(0, ImGui::GetStyle().ItemSpacing.x * 14);

        bool open_popup2 = ImGui::Button("Choose color for the text on the overlay.");
        ImGui::SameLine(0, ImGui::GetStyle().ItemInnerSpacing.x);
        open_popup2 |= ImGui::ColorButton("OverlayTextColor", chosen_overlay_text_color, cef);
        if (open_popup2) {
                ImGui::OpenPopup("overlaytextcolorpicker");
        }
        with_Popup("overlaytextcolorpicker") {
                ImGui::Text("Choose the text color for the overlay.");
                ImGui::Separator();
                ImGui::ColorPicker4(
                        "##picker",
                        (float *)&chosen_overlay_text_color,
                        cef | ImGuiColorEditFlags_NoSidePreview | ImGuiColorEditFlags_NoSmallPreview);
        }

        ImGui::Separator();
        ImGui::Unindent(80.0f);
        ImGui::NewLine();

        // Button for manually querying to check the population
        ImGui::SetCursorPosX(322.0f);
        if (ImGui::Button("CHECK POPULATION NOW")) {
                gameWrapper->Execute([this](GameWrapper * gw) { CHECK_NOW(); });
        }

        ImGui::NewLine();

        // Checkbox flags for different options
        if (ImGui::Checkbox("Lock overlay?", &lock_overlay)) {
                CVarWrapper cvw  = cvarManager->getCvar(CMD_PREFIX + "flag_lock_overlay");
                CVarWrapper cvwc = cvarManager->getCvar(CMD_PREFIX + "flag_lock_overlay_columns");
                cvw.setValue(lock_overlay);
                if (!cvwc.getBoolValue()) {
                        cvwc.setValue(lock_overlay);
                }
        }

        ImGui::SameLine(0, 50.0f);

        if (ImGui::Checkbox("Lock overlay columns?", &lock_overlay_columns)) {
                CVarWrapper cvw  = cvarManager->getCvar(CMD_PREFIX + "flag_lock_overlay");
                CVarWrapper cvwc = cvarManager->getCvar(CMD_PREFIX + "flag_lock_overlay_columns");
                CVarWrapper cvwb = cvarManager->getCvar(CMD_PREFIX + "flag_show_overlay_borders");
                cvwc.setValue(lock_overlay_columns | cvwb.getBoolValue());
                if (cvw.getBoolValue()) {
                        cvw.setValue(lock_overlay_columns);
                }
        }

        ImGui::SameLine(0, 50.0f);

        if (ImGui::Checkbox("Hide overlay column's borders?", &show_overlay_borders)) {
                CVarWrapper cvw  = cvarManager->getCvar(CMD_PREFIX + "flag_show_overlay_borders");
                CVarWrapper cvwc = cvarManager->getCvar(CMD_PREFIX + "flag_lock_overlay_columns");
                cvw.setValue(show_overlay_borders);
                if (!cvwc.getBoolValue()) {
                        cvwc.setValue(show_overlay_borders);
                }
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

        ImGui::NewLine();
        ImGui::Separator();

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

        maybe_Disabled(bkeep) {  // if bkeep = true, the following is rendered inactive
                ImGui::SameLine(300.0f, 200.0f);
                static bool popup = false;
                if (ImGui::Button("PRUNE DATA FILE?")) {
                        popup = true;
                        ImGui::OpenPopup("PRUNE_DATA_CONFIRM");
                }

                with_PopupModal(
                        "PRUNE_DATA_CONFIRM",
                        &popup,
                        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoDecoration) {
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
                                prune_data();

                                // update current and graph data structures with pruned data
                                prepare_data();
                                init_graph_data();
                                ImGui::CloseCurrentPopup();
                        }
                        ImGui::SameLine(0.0f, 50.0f);
                        if (ImGui::Button("no", ImVec2(80, 20))) {
                                write_data_to_file();
                                ImGui::CloseCurrentPopup();
                        }
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
        }

        ImGui::NewLine();

        ImVec2 tmpts = ImGui::CalcTextSize(
                "DISCLAIMER:  THE FOLLOWING GRAPHED DATA IS ONLY BASED ON VALUES THAT HAVE "
                "BEEN SAVED LOCALLY.");

        AlignForWidth(tmpts.x);

        ImGui::TextUnformatted("DISCLAIMER:  THE FOLLOWING GRAPHED DATA IS");

        ImGui::SameLine();

        ImGui::TextUnformatted("ONLY");
        AddUnderline(col_white);

        ImGui::SameLine();

        ImGui::TextUnformatted("BASED ON VALUES THAT HAVE BEEN SAVED LOCALLY.");

        set_StyleColor(ImGuiCol_Header, ImVec4(0.0f, 0.2f, 0.8f, 1.0f));
        // ImPlotLimits plot_limits;
        // plot_limits.X.Min = plot_limits.X.Max = plot_limits.Y.Min = plot_limits.Y.Max = 0;
        if (has_graph_data && (data_header_is_open = ImGui::CollapsingHeader("Data"))) {
                float width  = 0.0f;
                width       += ImGui::CalcTextSize("Double-click on plot to re-orient data.").x;
                width       += 50.0f;
                width       += ImGui::CalcTextSize(
                                 "Double right-click on plot for options, such as to set "
                                       "bounds.")
                                 .x;

                AlignForWidth(width);

                ImGui::TextUnformatted("Double-click on plot to re-orient data.");

                ImGui::SameLine(0.0f, 50.0f);

                ImGui::TextUnformatted("Double right-click on plot for options, such as to set bounds.");

                // this is more tightly coupled to implot, being that it understands here that
                // these values will be used for the plot that ultimately serves a function
                ImPlot::SetNextPlotLimits(
                        graph_total_pop_data->xs.front(),
                        graph_total_pop_data->xs.back(),
                        0,
                        std::ranges::max(graph_total_pop_data->ys),
                        ImGuiCond_FirstUseEver);
                // transform functions must be set before adding ImPlotAxisFlags_CustomFormat flag to an axis
                ImPlot::GetStyle().x_label_tf = &graphed_data_t::xlabel_transform_func;
                ImPlot::GetStyle().x_mouse_tf = &graphed_data_t::xval_mouse_func;
                if (ImPlot::BeginPlot(
                            "Population Numbers over Time",
                            "time",
                            "pop",
                            ImVec2(-1, 350),
                            ImPlotFlags_Default,
                            ImPlotAxisFlags_Default | ImPlotAxisFlags_CustomFormat,
                            ImPlotAxisFlags_Default | ImPlotAxisFlags_LockMin)) {
                        if (graph_total_pop) {
                                ImPlot::PlotLine(
                                        "TOTAL POPULATION",
                                        graph_total_pop_data->xs.data(),
                                        graph_total_pop_data->ys.data(),
                                        static_cast<int>(std::size(graph_total_pop_data->xs)));
                        }

                        if (graph_total_in_game) {
                                ImPlot::PlotLine(
                                        "TOTAL IN A GAME",
                                        graph_total_in_game_data->xs.data(),
                                        graph_total_in_game_data->ys.data(),
                                        static_cast<int>(std::size(graph_total_in_game_data->xs)));
                        }

                        for (const auto & entry : graph_flags_selected) {
                                if (!entry.second) {
                                        continue;
                                }
                                ImPlot::PlotLine(
                                        bm_helper::playlist_ids_str_spaced[entry.first].c_str(),
                                        (*graph_data)[entry.first].xs.data(),
                                        (*graph_data)[entry.first].ys.data(),
                                        static_cast<int>(std::size((*graph_data)[entry.first].xs)));
                        }
                        ImPlot::EndPlot();
                }
                // A list of selectable elements to select what shows up in the graph
                set_StyleColor(ImGuiCol_Header, ImVec4(0.19f, 0.85f, 0.12f, 0.7f));
                ImGui::BeginColumns(
                        "graphselectables_total_pop",
                        6,
                        ImGuiColumnsFlags_NoResize | ImGuiColumnsFlags_NoBorder);
                ImGui::Selectable("TOTAL POPULATION", &graph_total_pop);
                ImGui::NextColumn();
                ImGui::Selectable("TOTAL IN A GAME", &graph_total_in_game);
                ImGui::EndColumns();

                ImGui::SameLine();

                ImGui::BeginColumns(
                        "graphselectables_help_text",
                        3,
                        ImGuiColumnsFlags_NoResize | ImGuiColumnsFlags_NoBorder);

                ImGui::NextColumn();

                ImGui::TextUnformatted("Click a category to see it graphed.");

                ImGui::NextColumn();
                std::string num_avail_points = std::vformat(
                        "There are {} available data points.",
                        std::make_format_args(graph_total_pop_data->xs.size()));
                AlignForWidth(ImGui::CalcTextSize(num_avail_points.c_str()).x, 1.0f);
                ImGui::TextUnformatted(num_avail_points.c_str());
                ImGui::EndColumns();

                ImGui::BeginColumns("graphselectables", 6, ImGuiColumnsFlags_NoResize);
                size_t mxlines = 0;
                for (const std::string & header : SHOWN_PLAYLIST_POPS) {
                        ImGui::TextUnformatted(header.c_str());
                        AddUnderline(col_white);
                        ImGui::NextColumn();
                        mxlines = std::max(mxlines, bm_helper::playlist_categories[header].size());
                }
                for (int line = 0; line < mxlines; ++line) {
                        for (const std::string & category : SHOWN_PLAYLIST_POPS) {
                                if (line < bm_helper::playlist_categories[category].size()) {
                                        bool enabled = graph_flags[bm_helper::playlist_categories[category][line]];
                                        group_Disabled(!enabled);
                                        PlaylistId playid = bm_helper::playlist_categories[category][line];
                                        ImGui::Selectable(
                                                bm_helper::playlist_ids_str_spaced[playid].c_str(),
                                                &graph_flags_selected[playid]);
                                }
                                ImGui::NextColumn();
                        }
                }
                ImGui::EndColumns();
        }

        // ADD A BIG OL' DISCLAIMER-EXPLANATION DOWN HERE ON HOW THINGS WORK!
        if (ImGui::CollapsingHeader("How does this work?")) {
                with_Child("##howitworks", ImVec2(700, 300)) {
                        ImGui::TextWrapped(
                                "This plugin works by asking bakkesmod for playlist population "
                                "information through its "
                                "MatchmakingWrapper. But, to get the match making wrapper "
                                "happy to be asked questions, "
                                "some things happen first. To begin, you need to be connected "
                                "to psyonix servers. From "
                                "there, match making is initiated and then immediately "
                                "cancelled. This produces the "
                                "effective state that the plugin relies on without "
                                "showing/starting that <queued "
                                "matchmaking> event that plays -- where the game plays a sound "
                                "and puts 'searching' at "
                                "the top. When the matchmaking servers are queried for a "
                                "match, they begin to send "
                                "your client population information.");

                        ImGui::TextWrapped(
                                "This information for a population count is queried from your "
                                "client every 5 seconds "
                                "through Rocket League's own functions. Even though the actual "
                                "population numbers tend "
                                "to update every 5 minutes, querying every 5 seconds is done "
                                "to accommodate for "
                                "basically checking in the middle of the last update.");

                        ImGui::TextWrapped(
                                "Bakkesmod takes that information and makes it available to be "
                                "queried. This plugin "
                                "just asks for that population information when your client "
                                "asks for it. If there's an "
                                "update to be shown, then the population is shown through a "
                                "window/overlay and "
                                "reflected in the data you can see in the graphs under the "
                                "Data tab.");

                        // MAKE A VIDEO?
                        ImGui::TextUnformatted("If you need me to tell you in video format, then look");
                        TextURL("HERE", "https://youtu.be/WP_fkUnbRVU", true, false);
                }
        }

        if (ImGui::CollapsingHeader("Some questions you may have...")) {
                // SOME TEXT EXPLAINING SOME QUESTIONS
                static const float INDENT_OFFSET = 40.0f;

                // Question 1
                ImGui::Indent(INDENT_OFFSET);

                ImGui::TextUnformatted("WHERE IS THE DATA SAVED?");
                AddUnderline(col_white);

                ImGui::Unindent(INDENT_OFFSET);

                ImGui::TextUnformatted(
                        std::vformat("{}", std::make_format_args(RECORD_POPULATION_FILE.generic_string())).c_str());

                ImGui::NewLine();

                // Question 2
                ImGui::Indent(INDENT_OFFSET);

                ImGui::TextUnformatted("WHAT IF I CRASH AND HAVE A PROBLEM?");
                AddUnderline(col_white);

                ImGui::Unindent(INDENT_OFFSET);

                ImGui::TextUnformatted("Raise an issue on the github page: ");
                TextURL("HERE", "https://github.com/mgavin/ShowPlayerPopulation", true, false);

                ImGui::NewLine();

                // Question 3
                ImGui::Indent(INDENT_OFFSET);

                ImGui::TextUnformatted("WHAT IS PRUNING?");
                AddUnderline(col_white);

                ImGui::Unindent(INDENT_OFFSET);

                ImGui::TextWrapped(
                        "Pruning is the process where data is fitted within the timeframe "
                        "between now and the amount "
                        "of hours(currently %d) you've chosen to keep. This process is "
                        "irreversible from the in-game "
                        "menu. Make a backup save of data if you want backups.",
                        hours_kept);

                ImGui::NewLine();

                // Question 4
                ImGui::Indent(INDENT_OFFSET);

                ImGui::TextUnformatted("WHAT IS 'KEEP INDEFINITELY?'");
                AddUnderline(col_white);

                ImGui::Unindent(INDENT_OFFSET);

                ImGui::TextWrapped(
                        "This simply stops the plugin from pruning/trimming the data when the "
                        "plugin is unloaded or "
                        "the game is exited.");

                ImGui::NewLine();

                // Question 5
                ImGui::Indent(INDENT_OFFSET);

                ImGui::TextUnformatted("WHAT IS THE MAIN MENU?");
                AddUnderline(col_white);

                ImGui::Unindent(INDENT_OFFSET);

                ImGui::TextWrapped("It's where you are when you're able to select the [Play] button.");

                ImGui::NewLine();

                // Question 6
                ImGui::Indent(INDENT_OFFSET);

                ImGui::TextUnformatted("WHAT IS THE PLAYLIST MENU?");
                AddUnderline(col_white);

                ImGui::Unindent(INDENT_OFFSET);

                ImGui::TextWrapped("It's where you are when you're able to select different game modes.");

                ImGui::NewLine();

                // Question 7
                ImGui::Indent(INDENT_OFFSET);

                ImGui::TextUnformatted("WHAT IS THE PAUSE MENU IN GAME?");
                AddUnderline(col_white);

                ImGui::Unindent(INDENT_OFFSET);

                ImGui::TextWrapped(
                        "It's where you are when you are in a game and are able to select "
                        "[Exit to Main Menu]");

                ImGui::NewLine();

                // Question 8
                ImGui::Indent(INDENT_OFFSET);

                ImGui::TextUnformatted("WHEN IS THE POPULATION INFORMATION SAVED?");
                AddUnderline(col_white);

                ImGui::Unindent(INDENT_OFFSET);

                ImGui::TextWrapped(
                        "The population data is saved when the plugin is unloaded or Rocket League is exited.");

                ImGui::NewLine();
        }
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

void ShowPlayerPopulation::clear_graph_total_pop_data() {
        graph_total_pop_data->xs.clear();
        graph_total_pop_data->ys.clear();
}

void ShowPlayerPopulation::clear_graph_total_in_game_data() {
        graph_total_in_game_data->xs.clear();
        graph_total_in_game_data->ys.clear();
}

void ShowPlayerPopulation::clear_graph_data() {
        for (auto & item : std::ranges::views::values(*graph_data)) {
                item.xs.clear();
                item.ys.clear();
        }
}

void ShowPlayerPopulation::clear_graph_flags() {
        for (auto & item : std::ranges::views::values(graph_flags)) {
                item = false;
        }
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
        plugin_ctx = reinterpret_cast<ImGuiContext *>(ctx);
        ImGui::SetCurrentContext(plugin_ctx);
}

/// <summary>
/// Used for... getting the plugin name, for like, when rendering settings in the bakkesmod
/// plugin page
/// </summary>
std::string ShowPlayerPopulation::GetPluginName() {
        return "Show Player Population";
}

/* ------------------------- BEGIN TOGGLEMENU CODE ----------------------------- */

/// <summary>
/// (ImGui) Code called while rendering your menu window
/// </summary>
void ShowPlayerPopulation::Render() {
        {
                static bool first_time = true;
                if (first_time) {
                        ImGuiContext * igc = ImGui::GetCurrentContext();
                        if (igc != nullptr) {
                                igc->Style.Alpha = 1.0f;
                        }
                        first_time = !first_time;
                }
        }
        if ((in_main_menu && show_in_main_menu) || (in_game_menu && show_in_game_menu)
            || (in_playlist_menu && show_in_playlist_menu)) {
                ImGui::SetNextWindowSize(ImVec2(ImGui::GetIO().DisplaySize.x - 200, 225), ImGuiCond_FirstUseEver);
                ImGui::SetNextWindowPos(ImVec2(10, 2), ImGuiCond_FirstUseEver);
                set_StyleColor(ImGuiCol_WindowBg, chosen_overlay_color);
                ImGuiWindowFlags flags = ImGuiWindowFlags_None | ImGuiWindowFlags_NoCollapse;
                if (lock_overlay) {
                        flags |= ImGuiWindowFlags_NoInputs;
                }
                with_Window("Hey, cutie", NULL, flags) {
                        set_StyleColor(ImGuiCol_Text, chosen_overlay_text_color);
                        ImGui::SetWindowFontScale(1.3f);
                        CenterImGuiText(std::vformat(
                                                "PLAYLIST POPULATIONS! LAST UPDATED: {0:%r} {0:%D}",
                                                std::make_format_args(get_last_bank_entry().zt))
                                                .c_str());

                        ImGui::NewLine();

                        set_StyleColor(ImGuiCol_ChildBg, chosen_overlay_color);
                        with_StyleVar(ImGuiStyleVar_WindowPadding, {20, 0}) {
                                with_Child(
                                        "popnumbers",
                                        ImVec2 {0, 0},
                                        false,
                                        ImGuiWindowFlags_AlwaysUseWindowPadding | ImGuiWindowFlags_NoBackground) {
                                        if (ImGui::GetWindowWidth() <= (ImGui::GetIO().DisplaySize.x / 2.0f)) {
                                                // less than or equal to half of the width of
                                                // the screen = "vertical layout"
                                                ImGui::BeginColumns(
                                                        "populationnums_vert",
                                                        2,
                                                        ((lock_overlay_columns) ? ImGuiColumnsFlags_NoResize : 0)
                                                                | ((show_overlay_borders) ? ImGuiColumnsFlags_NoBorder
                                                                                          : 0));
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
                                                        const auto & pop = population_data[str];
                                                        for (const auto & popv : pop) {
                                                                playlist_pops.push_back({popv.first, popv.second});
                                                        }
                                                }

                                                for (const std::pair<PlaylistId, int> & ppops : playlist_pops) {
                                                        std::string playliststr =
                                                                bm_helper::playlist_ids_str_spaced[ppops.first];
                                                        int pop = ppops.second;

                                                        ImGui::TextUnformatted(
                                                                std::vformat("{}:", std::make_format_args(playliststr))
                                                                        .c_str());

                                                        ImGui::NextColumn();

                                                        CenterImGuiText(std::vformat("{}", std::make_format_args(pop)));

                                                        ImGui::NextColumn();
                                                }
                                                ImGui::EndColumns();
                                        } else {
                                                // greater than half of the width of the screen
                                                // = "horizontal layout"
                                                ImGui::BeginColumns(
                                                        "pop_nums_vert",
                                                        6,
                                                        ImGuiColumnsFlags_NoResize | ImGuiColumnsFlags_NoBorder);
                                                CenterImGuiText("Total Players Online:");
                                                AddUnderline(col_black);

                                                ImGui::NextColumn();

                                                CenterImGuiText(std::to_string(get_last_bank_entry().total_pop));

                                                ImGui::NextColumn();

                                                CenterImGuiText("Total Population in a Game:");
                                                AddUnderline(col_black);

                                                ImGui::NextColumn();

                                                CenterImGuiText(std::to_string(TOTAL_IN_GAME_POP));

                                                ImGui::EndColumns();

                                                size_t mxlines = 0;
                                                for (const auto & x : population_data) {
                                                        mxlines = std::max(mxlines, x.second.size());
                                                }

                                                ImGui::BeginColumns(
                                                        "pop_nums_horiz",
                                                        12,
                                                        ((lock_overlay_columns) ? ImGuiColumnsFlags_NoResize : 0)
                                                                | ((show_overlay_borders) ? ImGuiColumnsFlags_NoBorder
                                                                                          : 0));
                                                for (int line = 0; line < mxlines; ++line) {
                                                        for (const auto & playstr : SHOWN_PLAYLIST_POPS) {
                                                                if (line >= population_data[playstr].size()) {
                                                                        // nothing to show :(
                                                                        ImGui::NextColumn();
                                                                        ImGui::NextColumn();
                                                                        continue;
                                                                } else {
                                                                        PlaylistId id =
                                                                                population_data[playstr][line].first;
                                                                        int pop = population_data[playstr][line].second;
                                                                        ImGui::TextUnformatted(
                                                                                std::vformat(
                                                                                        "{}:",
                                                                                        std::make_format_args(
                                                                                                bm_helper::
                                                                                                        playlist_ids_str_spaced
                                                                                                                [id]))
                                                                                        .c_str());

                                                                        ImGui::NextColumn();

                                                                        std::string str = std::vformat(
                                                                                "{}",
                                                                                std::make_format_args(pop));
                                                                        CenterImGuiText(str);

                                                                        ImGui::NextColumn();
                                                                }
                                                        }
                                                }

                                                // every frame avoided with this call
                                                static bool exec_once = true;
                                                if (h_cols.colws[0] >= 0.0f && exec_once) {
                                                        exec_once = !exec_once;  // turn off
                                                        for (int i = 0; i < 12; ++i) {
                                                                ImGui::SetColumnWidth(i, h_cols.colws[i]);
                                                                ImGui::SetColumnOffset(i, h_cols.colos[i]);
                                                        }
                                                }
                                                if (!lock_overlay_columns) {
                                                        for (int i = 0; i < 12; ++i) {
                                                                h_cols.colws[i] = ImGui::GetColumnWidth(i);
                                                                h_cols.colos[i] = ImGui::GetColumnOffset(i);

                                                                // called to get ImGui to call savehandlers when
                                                                // it saves info itself
                                                                ImGui::MarkIniSettingsDirty();
                                                        }
                                                }
                                                ImGui::EndColumns();
                                        }
                                }
                        }
                }
        }
}

// Settings Handlers that will mostly be copied code from ImGui internals
static void * ImGuiSettingsReadOpen(ImGuiContext * ctx, ImGuiSettingsHandler * handler, const char * name) {
        // really, only one entry should exist here. There's no  support for multiple
        // instantiations

        if (strcmp(name, "HorizontalColumnsData") != 0) {
                // should never reach here
                // throw std::exception {"somehow imgui setting but like, not"};
        }
        return reinterpret_cast<void *>(&ShowPlayerPopulation::h_cols);
}

static void ImGuiSettingsReadLine(ImGuiContext *, ImGuiSettingsHandler *, void * entry, const char * line) {
        ShowPlayerPopulation::h_cols = *(imgui_helper::OverlayHorizontalColumnsSettings *)entry;
        float w1, w2, w3, w4, w5, w6, w7, w8, w9, w10, w11, w12;
        float o1, o2, o3, o4, o5, o6, o7, o8, o9, o10, o11, o12;
        if (sscanf(line,
                   "width1=%f,width2=%f,width3=%f,width4=%f,width5=%f,width6=%f,width7=%f,"
                   "width8=%f,width9=%f,"
                   "width10=%"
                   "f,width11=%f,width12=%f",
                   &w1,
                   &w2,
                   &w3,
                   &w4,
                   &w5,
                   &w6,
                   &w7,
                   &w8,
                   &w9,
                   &w10,
                   &w11,
                   &w12)
            == 12) {
                ShowPlayerPopulation::h_cols.colws[0]  = w1;
                ShowPlayerPopulation::h_cols.colws[1]  = w2;
                ShowPlayerPopulation::h_cols.colws[2]  = w3;
                ShowPlayerPopulation::h_cols.colws[3]  = w4;
                ShowPlayerPopulation::h_cols.colws[4]  = w5;
                ShowPlayerPopulation::h_cols.colws[5]  = w6;
                ShowPlayerPopulation::h_cols.colws[6]  = w7;
                ShowPlayerPopulation::h_cols.colws[7]  = w8;
                ShowPlayerPopulation::h_cols.colws[8]  = w9;
                ShowPlayerPopulation::h_cols.colws[9]  = w10;
                ShowPlayerPopulation::h_cols.colws[10] = w11;
                ShowPlayerPopulation::h_cols.colws[11] = w12;
        }
        if (sscanf(line,
                   "offset1=%f,offset2=%f,offset3=%f,offset4=%f,offset5=%f,offset6=%f,offset7=%"
                   "f,offset8=%f,"
                   "offset9=%f,"
                   "offset10=%f,offset11=%f,offset12=%f",
                   &o1,
                   &o2,
                   &o3,
                   &o4,
                   &o5,
                   &o6,
                   &o7,
                   &o8,
                   &o9,
                   &o10,
                   &o11,
                   &o12)
            == 12) {
                ShowPlayerPopulation::h_cols.colos[0]  = o1;
                ShowPlayerPopulation::h_cols.colos[1]  = o2;
                ShowPlayerPopulation::h_cols.colos[2]  = o3;
                ShowPlayerPopulation::h_cols.colos[3]  = o4;
                ShowPlayerPopulation::h_cols.colos[4]  = o5;
                ShowPlayerPopulation::h_cols.colos[5]  = o6;
                ShowPlayerPopulation::h_cols.colos[6]  = o7;
                ShowPlayerPopulation::h_cols.colos[7]  = o8;
                ShowPlayerPopulation::h_cols.colos[8]  = o9;
                ShowPlayerPopulation::h_cols.colos[9]  = o10;
                ShowPlayerPopulation::h_cols.colos[10] = o11;
                ShowPlayerPopulation::h_cols.colos[11] = o12;
        }
}

static void ImGuiSettingsWriteAll(ImGuiContext * ctx, ImGuiSettingsHandler * handler, ImGuiTextBuffer * buf) {
        buf->reserve(buf->size() + sizeof(ShowPlayerPopulation::h_cols));
        buf->appendf("[%s][%s]\n", handler->TypeName, "HorizontalColumnsData");
        buf->appendf("width%d=%0.3f", 1, ShowPlayerPopulation::h_cols.colws[0]);

        // 12 is the number of horizontal columns represented in this data structure
        for (int i = 1; i < 12; ++i) {
                buf->appendf(",width%d=%0.3f", i + 1, ShowPlayerPopulation::h_cols.colws[i]);
        }

        buf->append("\n");
        buf->appendf("offset%d=%0.3f", 1, ShowPlayerPopulation::h_cols.colos[0]);

        for (int i = 1; i < 12; ++i) {
                buf->appendf(",offset%d=%0.3f", i + 1, ShowPlayerPopulation::h_cols.colos[i]);
        }
        buf->append("\n");
}

/// <summary>
/// do the following on popup open
/// </summary>
void ShowPlayerPopulation::OnOpen() {
        is_overlay_open = true;

        ImGuiSettingsHandler ini_handler;
        ini_handler.TypeName   = "ShowPlayerPopulation";
        ini_handler.TypeHash   = ImHashStr("ShowPlayerPopulation");
        ini_handler.ReadOpenFn = &ImGuiSettingsReadOpen;
        ini_handler.ReadLineFn = &ImGuiSettingsReadLine;
        ini_handler.WriteAllFn = &ImGuiSettingsWriteAll;

        (plugin_ctx->SettingsHandlers).push_back(ini_handler);
}

/// <summary>
/// do the following on popup menu close
/// </summary>
void ShowPlayerPopulation::OnClose() {
        is_overlay_open = false;

        ImGuiSettingsHandler * igsh = ImGui::FindSettingsHandler("ShowPlayerPopulation");
        if (igsh == nullptr) {
                return;
        }

        (plugin_ctx->SettingsHandlers).erase(igsh);
}

/// <summary>
/// Returns the name of the menu to refer to it by
/// </summary>
/// <returns>The name used refered to by togglemenu</returns>
std::string ShowPlayerPopulation::GetMenuName() {
        return "ShowPlayerPopulation";
}

/// <summary>
/// Returns a std::string to show as the title
/// </summary>
/// <returns>The title of the menu</returns>
std::string ShowPlayerPopulation::GetMenuTitle() {
        return "";
}

/// <summary>
/// Is it the active overlay(window)?
/// Might make this susceptible to being closed with Esc
/// Makes it "active", aka, able to be manipulated when the plugin menu is closed.
/// </summary>
/// <returns>True/False for being the active overlay</returns>
bool ShowPlayerPopulation::IsActiveOverlay() {
        return false;
}

/// <summary>
/// Should this block input from the rest of the program?
/// (aka RocketLeague and BakkesMod windows)
/// </summary>
/// <returns>True/False for if bakkesmod should block input. Could be
/// `return ImGui::GetIO().WantCaptureMouse ||
/// ImGui::GetIO().WantCaptureKeyboard;`</returns>
bool ShowPlayerPopulation::ShouldBlockInput() {
        return false;
}

/* -------------------------- END TOGGLEMENU CODE ----------------------------- */

/// <summary>
///  do the following when your plugin is unloaded
///  destroy things
///  dont throw here
///
///  DONT DO ANYTHING HERE BECAUSE IT ISNT GUARANTEED (UNLESS YOU MANUALLY UNLOAD PLUGIN)
///  SO IT'S JUST KIND OF FOR EASE OF DEVELOPMENT SINCE REBUILD = UNLOAD/LOAD
/// </summary>
void ShowPlayerPopulation::onUnload() noexcept {
        prune_data();
        write_data_to_file();
}
