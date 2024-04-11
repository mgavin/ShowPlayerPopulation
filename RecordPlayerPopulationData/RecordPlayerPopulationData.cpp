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

        init_datafile();

        // so tired...
        LOG("{}", last_time);

        using namespace std::chrono_literals;
        std::chrono::system_clock clk;
        if ((clk.now() - last_time) < 5min) {
                LOG("TOO SOON!");
        }

        cvarManager->registerCvar(
                "rppd_enable_topscroll",
                "0",
                "Flag to determine showing the top ticker scroll showing player population");

        cvarManager->registerCvar("rppd_auto_enable", "0", "Flag to determine if matchmaking should be automatic");

        HookedEvents::AddHookedEvent("Function Engine.Actor.Timer", [this](std::string ev) {
                // THIS IS ACTUALLY A SECONDS TIMER
                if (seconds_counter > 1e6)  // preventing overflow. just cuz.
                        seconds_counter = 0;
                seconds_counter++;
                bool               auto_enable = cvarManager->getCvar("rppd_auto_enable").getBoolValue();
                MatchmakingWrapper mw          = gameWrapper->GetMatchmakingWrapper();
                if (mw && auto_enable) {
                        // the one second delay between searching isn't necessary
                        // (no wait time is necessary)
                        // to trigger the "get playlist populations" function running every 5 seconds.
                        // now to test how long it lasts
                        if (seconds_counter % 300 == 0) {
                                mw.StartMatchmaking(PlaylistCategory::CASUAL);
                                mw.CancelMatchmaking();
                        }
                }

                // MAYBE THERE'S ACTUALLY ANOTHER FUNCTION?
        });
        // cvarManager->registerNotifier(
        //         "RPPD_CHECKNOW",
        //         [this](std::vector<std::string> args) { CHECK_NOW(); },
        //         "CHECK, RIGHT NOW, THE PLAYER POPULATION!",
        //         NULL);

        HookedEvents::AddHookedEvent(
                "Function ProjectX.OnlineGamePopulation_X.GetPlaylistPopulations",
                // I would like to know how long this lasts.
                // this seems to be a thing that bakkesmod does? but I can't figure that out necessarily
                // (aka running the function scanner) without bakkesmod running

                // like, this doesn't even start getting called every 5 seconds until you queue for a game

                [this](std::string ev) {
                        LOG("getplaylistpopulations: seconds counter: {}", seconds_counter);
                        MatchmakingWrapper mw = gameWrapper->GetMatchmakingWrapper();
                        if (mw) {
                                int tmpp = mw.GetTotalPlayersOnline();
                                if (TOTAL_POP != tmpp) {
                                        TOTAL_POP = tmpp;
                                        LOG("POPULATION UPDATED AT TIME: {}, TIMES UPDATED: {},  POPULATION: {}",
                                            get_current_datetime_str(),
                                            updated_count,
                                            TOTAL_POP);
                                        updated_count++;

                                        write_population();
                                }
                        }
                });
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

        CVarWrapper auto_enable  = cvarManager->getCvar("rppd_auto_enable");
        bool        bauto_enable = auto_enable.getBoolValue();
        if (ImGui::Checkbox("Enable auto matchmaking refresh every 300 seconds?", &bauto_enable)) {
                auto_enable.setValue(bauto_enable);
        }

        if (ImGui::Button("CHECK NOW")) {
                gameWrapper->Execute([this](GameWrapper * gw) { CHECK_NOW(); });
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
                std::vector<std::string>      header {"DATETIME", "TOTALPOPULATION", "TOTALPLAYERSONLINE"};
                // header.emplace_back(
                //         begin(bmhelper::playlist_ids_str),
                //         end(bmhelper::playlist_ids_str),
                //         [](const auto & ele) -> std::string { return ele.second; });
                // I WANTED THIS TO BE IN A SIMPLER FUNCTION THAT ALREADY EXISTED
                // LIKE A CUSTOM EMPLACE_BACK
                auto                          vv = std::views::values(bmhelper::playlist_ids_str);
                std::copy(begin(vv), end(vv), std::back_inserter(header));
                recordwriter << header;
        } else {
                csv::CSVReader recordreader {RECORD_POPULATION_FILE.string()};
                csv::CSVRow    row;

                // go to last row
                while (recordreader.read_row(row))
                        ;
                // last_time = row["DATETIME"].get<std::chrono::time_point<std::chrono::system_clock>>();
                std::string tmpdt = row["DATETIME"].get<std::string>();
                last_time         = get_timepoint_from_str(tmpdt);
        }
}

void RecordPlayerPopulationData::CHECK_NOW() {
        MatchmakingWrapper mw = gameWrapper->GetMatchmakingWrapper();
        if (mw) {
                mw.StartMatchmaking(PlaylistCategory::CASUAL);
                mw.CancelMatchmaking();
        }
}

void RecordPlayerPopulationData::write_population() {
        std::ofstream                 file {RECORD_POPULATION_FILE, std::ios::app};
        csv::CSVWriter<std::ofstream> recordwriter {file};
        MatchmakingWrapper            mw = gameWrapper->GetMatchmakingWrapper();
        if (mw) {
                std::vector<int> counts;
                counts.push_back(mw.GetTotalPopulation());
                counts.push_back(mw.GetTotalPlayersOnline());
                for (const auto & element : bmhelper::playlist_ids_str) {
                        counts.push_back(mw.GetPlayerCount(element.first));
                }

                std::vector<std::string> strs;
                strs.reserve(bmhelper::playlist_ids_str.size() + 1);
                strs.push_back(get_current_datetime_str());
                std::transform(
                        begin(counts),
                        end(counts),
                        std::back_inserter(strs),
                        [](const auto & elem) -> std::string { return std::to_string(elem); });

                recordwriter << strs;
        }
}

/// <summary>
/// RETURNS THE DATETIME AS A STRING!
/// </summary>
/// <returns>_now_ represented as a datetime string</returns>
std::string RecordPlayerPopulationData::get_current_datetime_str() {
        const std::chrono::zoned_time t {std::chrono::current_zone(), std::chrono::system_clock::now()};
        return std::vformat(DATETIME_FORMAT_STR, std::make_format_args(t));
}

/// <summary>
/// RETURNS THE TIMEPOINT AS INTERPRETED FROM A DATETIME STRING!
/// </summary>
/// <param name="str">a string representation of the datetime</param>
/// <returns>a time_point representing the string</returns>
std::chrono::time_point<std::chrono::system_clock> RecordPlayerPopulationData::get_timepoint_from_str(std::string str) {
        std::chrono::utc_time<std::chrono::system_clock::duration> tmpd;
        std::istringstream                                         ss(str);
        ss >> std::chrono::parse(DATETIME_PARSE_STR, tmpd);
        std::chrono::time_point<std::chrono::system_clock, std::chrono::system_clock::duration> tmptp {
                tmpd.time_since_epoch()};
        return tmptp;
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