//      __________        ___               ______            _
//     / ____/ __ \____  / (_)___  ___     / ____/___  ____ _(_)___  ___
//    / /_  / / / / __ \/ / / __ \/ _ \   / __/ / __ \/ __ `/ / __ \/ _ `
//   / __/ / /_/ / / / / / / / / /  __/  / /___/ / / / /_/ / / / / /  __/
//  /_/    \____/_/ /_/_/_/_/ /_/\___/  /_____/_/ /_/\__, /_/_/ /_/\___/
//                                                  /____/
// FOnline Engine
// https://fonline.ru
// https://github.com/cvet/fonline
//
// MIT License
//
// Copyright (c) 2006 - 2025, Anton Tsvetinskiy aka cvet <cvet@tut.by>
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//

#include "Settings.h"
#include "AnyData.h"
#include "ConfigFile.h"
#include "ImGuiStuff.h"
#include "Version-Include.h"
#include "WinApi-Include.h"

FO_BEGIN_NAMESPACE();

template<typename T>
static void SetEntry(T& entry, string_view value, bool append)
{
    FO_STACK_TRACE_ENTRY();

    if (!append) {
        entry = {};
    }

    if constexpr (std::same_as<T, string>) {
        if (append && !entry.empty()) {
            entry += " ";
        }

        const auto any_value = AnyData::ParseValue(string(value), false, false, AnyData::ValueType::String);
        entry += any_value.AsString();
    }
    else if constexpr (std::same_as<T, bool>) {
        const auto any_value = AnyData::ParseValue(string(value), false, false, AnyData::ValueType::Bool);
        entry |= any_value.AsBool();
    }
    else if constexpr (std::floating_point<T>) {
        const auto any_value = AnyData::ParseValue(string(value), false, false, AnyData::ValueType::Float64);
        entry += numeric_cast<float32>(any_value.AsDouble());
    }
    else if constexpr (std::is_enum_v<T>) {
        const auto any_value = AnyData::ParseValue(string(value), false, false, AnyData::ValueType::Int64);
        entry = static_cast<T>(static_cast<int64>(entry) | any_value.AsInt64());
    }
    else if constexpr (is_strong_type<T>) {
        const auto any_value = AnyData::ParseValue(string(value), false, false, AnyData::ValueType::Int64);
        entry = T {numeric_cast<typename T::underlying_type>(any_value.AsInt64())};
    }
    else if constexpr (is_valid_property_plain_type<T>) {
        const auto any_value = AnyData::ParseValue(string(value), false, false, AnyData::ValueType::String);
        istringstream istr(any_value.AsString());
        istr >> entry;
    }
    else {
        const auto any_value = AnyData::ParseValue(string(value), false, false, AnyData::ValueType::Int64);
        entry += numeric_cast<T>(any_value.AsInt64());
    }
}

template<typename T>
static void SetEntry(vector<T>& entry, string_view value, bool append)
{
    FO_STACK_TRACE_ENTRY();

    if (!append) {
        entry.clear();
    }

    if constexpr (std::same_as<T, string>) {
        const auto arr_value = AnyData::ParseValue(string(value), false, true, AnyData::ValueType::String);
        const auto& arr = arr_value.AsArray();

        for (const auto& arr_entry : arr) {
            entry.emplace_back(arr_entry.AsString());
        }
    }
    else if constexpr (std::same_as<T, bool>) {
        const auto arr_value = AnyData::ParseValue(string(value), false, true, AnyData::ValueType::Bool);
        const auto& arr = arr_value.AsArray();

        for (const auto& arr_entry : arr) {
            entry.emplace_back(arr_entry.AsBool());
        }
    }
    else if constexpr (std::floating_point<T>) {
        const auto arr_value = AnyData::ParseValue(string(value), false, true, AnyData::ValueType::Float64);
        const auto& arr = arr_value.AsArray();

        for (const auto& arr_entry : arr) {
            entry.emplace_back(numeric_cast<float32>(arr_entry.AsDouble()));
        }
    }
    else if constexpr (std::is_enum_v<T>) {
        const auto arr_value = AnyData::ParseValue(string(value), false, true, AnyData::ValueType::Int64);
        const auto& arr = arr_value.AsArray();

        for (const auto& arr_entry : arr) {
            entry.emplace_back(numeric_cast<std::underlying_type_t<T>>(arr_entry.AsInt64()));
        }
    }
    else {
        const auto arr_value = AnyData::ParseValue(string(value), false, true, AnyData::ValueType::Int64);
        const auto& arr = arr_value.AsArray();

        for (const auto& arr_entry : arr) {
            entry.emplace_back(numeric_cast<T>(arr_entry.AsInt64()));
        }
    }
}

template<typename T>
static void DrawEntry(string_view name, const T& entry)
{
    FO_STACK_TRACE_ENTRY();

    ImGui::TextUnformatted(strex("{}: {}", name, entry).c_str());
}

template<typename T>
static void DrawEditableEntry(string_view name, T& entry)
{
    FO_STACK_TRACE_ENTRY();

    // Todo: improve editable entries
    DrawEntry(name, entry);
}

GlobalSettings::GlobalSettings(bool baking_mode) :
    _bakingMode {baking_mode}
{
    FO_STACK_TRACE_ENTRY();

    if (_bakingMode) {
        // Auto settings
        _appliedSettings.emplace("ApplyConfig");
        _appliedSettings.emplace("ApplySubConfig");
        _appliedSettings.emplace("UnpackagedSubConfig");
        _appliedSettings.emplace("CommandLine");
        _appliedSettings.emplace("CommandLineArgs");
        _appliedSettings.emplace("GitBranch");
        _appliedSettings.emplace("WebBuild");
        _appliedSettings.emplace("WindowsBuild");
        _appliedSettings.emplace("LinuxBuild");
        _appliedSettings.emplace("MacOsBuild");
        _appliedSettings.emplace("AndroidBuild");
        _appliedSettings.emplace("IOsBuild");
        _appliedSettings.emplace("DesktopBuild");
        _appliedSettings.emplace("TabletBuild");
        _appliedSettings.emplace("MapHexagonal");
        _appliedSettings.emplace("MapSquare");
        _appliedSettings.emplace("MapDirCount");
        _appliedSettings.emplace("Packaged");
        _appliedSettings.emplace("DebugBuild");
        _appliedSettings.emplace("RenderDebug");
        _appliedSettings.emplace("MonitorWidth");
        _appliedSettings.emplace("MonitorHeight");
        _appliedSettings.emplace("ClientResourceEntries");
        _appliedSettings.emplace("MapperResourceEntries");
        _appliedSettings.emplace("ServerResourceEntries");
        _appliedSettings.emplace("MousePos");
        _appliedSettings.emplace("DummyIntVec");
        _appliedSettings.emplace("Ping");
        _appliedSettings.emplace("ScrollMouseUp");
        _appliedSettings.emplace("ScrollMouseDown");
        _appliedSettings.emplace("ScrollMouseLeft");
        _appliedSettings.emplace("ScrollMouseRight");
        _appliedSettings.emplace("ScrollKeybUp");
        _appliedSettings.emplace("ScrollKeybDown");
        _appliedSettings.emplace("ScrollKeybLeft");
        _appliedSettings.emplace("ScrollKeybRight");
    }
}

void GlobalSettings::ApplyConfigAtPath(string_view config_name, string_view config_dir)
{
    FO_STACK_TRACE_ENTRY();

    if (config_name.empty()) {
        return;
    }

    const string config_path = strex(config_dir).combine_path(config_name);

    if (auto settings_file = DiskFileSystem::OpenFile(config_path, false)) {
        _appliedConfigs.emplace_back(config_path);

        string settings_content;
        settings_content.resize(settings_file.GetSize());
        settings_file.Read(settings_content.data(), settings_content.size());

        auto config = ConfigFile(config_name, settings_content);
        ApplyConfigFile(config, config_dir);
    }
    else {
        throw SettingsException("Config not found", config_path);
    }
}

void GlobalSettings::ApplyConfigFile(ConfigFile& config, string_view config_dir)
{
    FO_STACK_TRACE_ENTRY();

    for (auto&& [key, value] : config.GetSection("")) {
        SetValue(key, value, config_dir);
    }

    AddResourcePacks(config.GetSections("ResourcePack"), config_dir);
    AddSubConfigs(config.GetSections("SubConfig"), config_dir);
}

void GlobalSettings::ApplyCommandLine(int32 argc, char** argv)
{
    FO_STACK_TRACE_ENTRY();

    for (int32 i = 0; i < argc; i++) {
        if (i == 0 && argv[0][0] != '-') {
            continue;
        }

        if (argv[i][0] == '-') {
            const string key = strex("{}", argv[i]).trim().str().substr(argv[i][1] == '-' ? 2 : 1);
            const auto value = i < argc - 1 && argv[i + 1][0] != '-' ? strex("{}", argv[i + 1]).trim().str() : "1";

            if (key != "ApplyConfig" && key != "ApplySubConfig") {
                WriteLog(LogType::Info, "Set {} to {}", key, value);
                SetValue(key, value);
            }
        }
    }
}

void GlobalSettings::ApplyInternalConfig()
{
    FO_STACK_TRACE_ENTRY();

    alignas(uint32_t) static volatile constexpr char INTERNAL_CONFIG[10048] = {//
        "###InternalConfig###1234"
        "41286155307835455016484608701376538077188313611362680125654503016610364267856432315362321687524827003543310136007017135126412670001512002223663802155145813757843176642001252585570115622615121535465850"
        "46668306511265018560744841646068831048222685488534334733385807281647141284122346530433142633100608600367566145663032777281706637044077576533888571721582561638451337516501026262258836778308258014283233"
        "32564008247064526584436302274853261547475250621752188836057545652881245344120746018127416363550458727030465332627110745365156326362834858627720885837176087003476157150415767735742458473052607173666524"
        "58400447464788787531182165032560083831018107262505426337048465658460837433440231522137358638056753486031841706566383030012108551256416162537624774345517711676161563772601023262256634353858041856108125"
        "02242668362238554305583822117417638400550083645544726272320873113518766246257730830112461368456881004026126416182167417232767505456085332242766535283067283045580567187223070366474827101618645367823483"
        "85257348814446723665720335301577758188525778550204442604103204851716236743772147118281444416483115825382318647476845358528832358418571188110212824770711348284378146620534311573041183557107015687328271"
        "13073271162508083461583210830470656181764154267466781822830146477026842844036800362531743383528137357542757230333544000403425487676323674461375414825475514210642064720446418743410277336370407046043707"
        "86167421810214381216800111126086325261076675415426728778705733344070103804230845335831576447343082615353030202652341031846722342704256186208651745223753605687408664306270724134004055207356086255660803"
        "27707670715677451237730536018648617072221540760137037114227748188470087312605368427384156718225522644157434643306682777642077342688053008001185821650586447237066556821033636673002322327213834604424172"
        "03852738324447828583556685431872022851873258557500655504186786377241235641247730753020016733434464266816020317346475264255477265735505406842811268183032678457802845584522284038580728665844051400844562"
        "68716400287186104140360521616642500466066654172718756857800550568772657556713643207661525445751053045787737285630207062526021600457258134722835775403167232712423080384121581801275380102308061313204635"
        "40348542843875522544080033303656078163401351104677340436487301506612004046121185186334741135660815307011766864372427524552640631155137623224604725632181531116536027425410806652178681314515012562864631"
        "64530624383108184550770685013618088414104587154331241456857220022826734807463582088170846036187465027723820788240782631733382244628046257738664640022411034565487867707548041843081157076203055278064537"
        "35772550227267854216712202357000252603833065814242141004335230367345345738128303003753146866228852871877572563163676151274065437767118540105407687647673114700545673343771785028654223157734501783130858"
        "38484270250584632616583121784010167250030022724522868245461686725610101050478315723213627673578035856137478581837750120316788551846634151151301745324286663201873462512555783008332036268568384566541225"
        "00513368216228416850700105326352181450781371323354318211154835705466522578384840030831035664021515418373651513057775358012632000622858584866828066842567203446422322415834252081082526118056512362472887"
        "23732707552744124201568670346717174216760120177818510101043611227428884737443076608830248345173431851876013423478775152705421885885186511740882737077312073486621436006623570614454322416263671013716164"
        "38538130830118716253575035881034347462147512178674758637220482572665718847222070301854372365275773882182255182518324547642580458760756118441063518581758516758861567184526137754714178282634056814071538"
        "03115088156223583205837887302062288866555427620768623300827033571633866502208726857580677484666812224821774140722666826747844184604156413720760233574327455347516260528041044524221323385867652706576015"
        "14708128704788770737303054106048277570501270740723816601212881587018041050364714558811281317422734386434078166588557008038585757428661582644073738416387512837234735501743864067134753554387854602674486"
        "04405446112884287652055305137872336128582282405715551025280508566328473447407516105710103752524854683182638227345482583036582077328532008614034340320818881656261653252431128365362222616181000243535346"
        "84682640638601531247451673858705125204020402715058007121651601256458525283120322474481080444847144277358871546472555647550318838087132655283033762844436245580544684250728663675160610065442261846331317"
        "51186612185582388448153757025067242418513802235524881554358766477046258600331285005487713035163447001867651170353447385431664257465344542348254704138871581567511646387858744338436275807120755507024587"
        "20282654545836818701845124567236134802834220583620655316851126062746826045576604177752872754636761108353508240138688644208628863707566151454524425364783387622363017627111858688803128232250055452342328"
        "47476550733852574571236664324710037448886866626702542601228015333327008108004882885328852470010710148112054766754164730158631711332657664361583500503307032087536230143345386807757453447286264051530664"
        "35200282345514305330443684401413308656322213867822061350460002586105224148138757721470488888874864035124483614730711054552340555688440873762641601733441577074216578178547674256361154876101828031887606"
        "76458163562308066605568587146227617515376506216854033247230037171448070861673070122522355367216263251012678280411764455044563232582103674483221767774731704523263851240644216714210774474310166420764113"
        "28205625232463040347415107686264156472256258546850556520328153800222836768851732748740466703652726870332303352126331341767844335020283723258688074827736644360458024560500605821024353040747222635241538"
        "85323477013318458212700603278784346028582867262111660375645865687653210068733433713761342786638776480674435643153068523772370134024772342018856535368701183505506451003021848830370858411657786014647546"
        "58607748825133678287504466865747625777402455656264482681782117376451886408621247446405161331128126641623715683240081476056433843838277081243458637736537438030127616862841222563230543126525807328857358"
        "83414841024065214236773245615214230007518147368757357473535581748163882872130875721013441286557707663202517488816252161804322003471635866651858142127601303303754033782265000513848688872504718156448061"
        "24841014887056042282664416778640755724607678350351514875638132512636513601228816365378705885684264218366801835786704458875743242551143222064607553168706154648312237512731027671877443316356262045775535"
        "27404647803736111842763412423228462257064242381714407554018656067346283085384144537284162173252047810218586101205722537587540858343131572671213035410587308722278678577817804883125048778364487077674658"
        "60468522714322362842425745756120250564047542018103637844646757101833713160453254423438787725767487806727333315425186025445477852528018887457138017637561617371048760680734027631657873128288811371610210"
        "87614147746657886020753528667810071347556642027113818435277320487786371057831557800444784644320820214218703166324741772145581684168234341623058724101831707431248482688501607363341887726708203516742871"
        "68626830813574510363430411640571484803255162828425655701258372178270770528380838505714017078311606848676876143405082608738278827004785664413075272160250551181510338333668756774068370835730734616772208"
        "70748656583018226470810750147642420407208761424076857860700614240220404504247512375248870422726430043164006831382678327323800065404155870438507364453668888626326221511831551482554356467700014825344436"
        "08216247162318678424835244350312187630142350872778131864121254270315676033787601462620448676171283216532617713767521880588123742618060860072160237181653754583658188532212030023878802651048001117327705"
        "84711081051276858760727011373215862854285187854561762372852180186764035603284671306466844812042447726277478383832206780107238724028736711652553326853424866446163224165760326467404474145742643304128860"
        "83273311568057762825415624350605246471178061206287471383688121502648244522031012665333444434016755061288878380102316460810150777181150582187588152564754045733085663342583183205817524567257422885753602"
        "83430314403616434873305638823872043775307063142244333758705401281257503354737885815044216217628712455600527362742467124417873017267164401853170246647732373265280680115478774018480035416332232631500754"
        "67125348424242336360636575344821065783763888728483167478206586104523126011870101167073084215616263157144347061327741655514750017084358863750466730476436111518546020531616453214412181146752843462230833"
        "26750503217743050413562780575835618030165646663403572022522564800253814502252185282462448828104534855361415810760616741188126413144285050352645665316357317332652773772656711346032280708824628853706620"
        "84887208511405515550232551786535820101848286470331082837805787125183825021206705027518103625663052284336667648878771524274774507628465705432021752748005403256644788465880537706865563080555720823042137"
        "82158053845830616327667456411050332214423626411788365081122342858558642057383020346242018201717034862505656580080252540161063412557830318237038552535667025565511365267125626800455402867078877577773680"
        "30718153003324730501601785875021843025030167162635568835352747363600424174851331530167580513705484113438315402668771708457077343600805508501670827500582486261813541126574017757350543507762738824427715"
        "35461164661734406222252616804543733282352437111745053221080567656281348212130632865388721510434654724435436253155424105128877123588768068740340125323744213126860326078045145625567112526830411768526340"
        "24648148246678522060481052655554851588267453176277855837712188027841236543604383325733123658702285763636720047815357772555553257737045383588566212518256717602827737246807016530337364137380402741020714"
        "54345768483411525646078080005153536037776707357111258854757712757351038431650116366861625827868182261103337245252331173516057363588862768787032067823486684804277742252245232035874684165387061878208840"
        "74777412784838367274016411823766525581268636704881211851784481328162168512236587346022011800322835823704630313618432460384064665630835335332013484342786462058374530581327175585042577848256556510468363"
        "###InternalConfigEnd###"};

    const auto config_str = strex().assignVolatile(INTERNAL_CONFIG, sizeof(INTERNAL_CONFIG)).str();

    if (strex(config_str).starts_with("###InternalConfig###")) {
        throw SettingsException("Internal config not patched");
    }

    auto config = ConfigFile("InternalConfig.fomain", config_str);
    ApplyConfigFile(config, "");
}

void GlobalSettings::ApplyAutoSettings()
{
    FO_STACK_TRACE_ENTRY();

    const_cast<bool&>(Packaged) = IsPackaged();

#if FO_WEB
    const_cast<bool&>(WebBuild) = true;
#else
    const_cast<bool&>(WebBuild) = false;
#endif
#if FO_WINDOWS
    const_cast<bool&>(WindowsBuild) = true;
#else
    const_cast<bool&>(WindowsBuild) = false;
#endif
#if FO_LINUX
    const_cast<bool&>(LinuxBuild) = true;
#else
    const_cast<bool&>(LinuxBuild) = false;
#endif
#if FO_MAC
    const_cast<bool&>(MacOsBuild) = true;
#else
    const_cast<bool&>(MacOsBuild) = false;
#endif
#if FO_ANDROID
    const_cast<bool&>(AndroidBuild) = true;
#else
    const_cast<bool&>(AndroidBuild) = false;
#endif
#if FO_IOS
    const_cast<bool&>(IOsBuild) = true;
#else
    const_cast<bool&>(IOsBuild) = false;
#endif
    const_cast<bool&>(DesktopBuild) = WindowsBuild || LinuxBuild || MacOsBuild;
    const_cast<bool&>(TabletBuild) = AndroidBuild || IOsBuild;

#if FO_WINDOWS
    if (::GetSystemMetrics(SM_TABLETPC) != 0) {
        const_cast<bool&>(DesktopBuild) = false;
        const_cast<bool&>(TabletBuild) = true;
    }
#endif

    const_cast<bool&>(MapHexagonal) = GameSettings::HEXAGONAL_GEOMETRY;
    const_cast<bool&>(MapSquare) = GameSettings::SQUARE_GEOMETRY;
    const_cast<int32&>(MapDirCount) = GameSettings::MAP_DIR_COUNT;

#if FO_DEBUG
    const_cast<bool&>(DebugBuild) = true;
    const_cast<bool&>(RenderDebug) = true;
#endif

    if (MapDirectDraw) {
        const_cast<bool&>(MapZoomEnabled) = false;
    }

    const_cast<string&>(GitBranch) = FO_GIT_BRANCH;
}

void GlobalSettings::ApplySubConfigSection(string_view name)
{
    FO_STACK_TRACE_ENTRY();

    const auto find_predicate = [&](const SubConfigInfo& cfg) { return cfg.Name == name; };
    const auto it = std::ranges::find_if(_subConfigs, find_predicate);

    if (it == _subConfigs.end()) {
        throw SettingsException("Sub config not found", name);
    }

    for (auto&& [key, value] : it->Settings) {
        SetValue(key, value, it->ConfigDir);
    }
}

auto GlobalSettings::GetCustomSetting(string_view name) const -> const string&
{
    const auto it = _customSettings.find(name);

    if (it != _customSettings.end()) {
        return it->second;
    }

    return _empty;
}

void GlobalSettings::SetCustomSetting(string_view name, string value)
{
    FO_STACK_TRACE_ENTRY();

    _customSettings[string(name)] = std::move(value);
}

void GlobalSettings::SetValue(const string& setting_name, const string& setting_value, string_view config_dir)
{
    FO_STACK_TRACE_ENTRY();

    const bool append = !setting_value.empty() && setting_value[0] == '+';
    string_view value = append ? string_view(setting_value).substr(1) : setting_value;

    // Resolve environment variables and files
    string resolved_value;
    size_t prev_pos = 0;
    size_t pos = setting_value.find('$');

    if (pos != string::npos) {
        while (pos != string::npos) {
            const bool is_env = setting_value.compare(pos, "$ENV{"_len, "$ENV{") == 0;
            const bool is_file = setting_value.compare(pos, "$FILE{"_len, "$FILE{") == 0;
            const bool is_target_env = setting_value.compare(pos, "$TARGET_ENV{"_len, "$TARGET_ENV{") == 0;
            const bool is_target_file = setting_value.compare(pos, "$TARGET_FILE{"_len, "$TARGET_FILE{") == 0;
            const size_t len = is_env ? "$ENV{"_len : (is_file ? "$FILE{"_len : (is_target_env ? "$TARGET_ENV{"_len : "$TARGET_FILE{"_len));

            if (is_env || is_file || (!_bakingMode && (is_target_env || is_target_file))) {
                pos += len;
                size_t end_pos = setting_value.find('}', pos);

                if (end_pos != string::npos) {
                    const string name = setting_value.substr(pos, end_pos - pos);

                    if (is_env || is_target_env) {
                        const char* env = !name.empty() ? std::getenv(name.c_str()) : nullptr;

                        if (env != nullptr) {
                            resolved_value += setting_value.substr(prev_pos, pos - prev_pos - len) + string(env);
                            end_pos++;
                        }
                        else {
                            WriteLog(LogType::Warning, "Environment variable {} for setting {} is not found", name, setting_name);
                            resolved_value += setting_value.substr(prev_pos, pos - prev_pos) + name;
                        }
                    }
                    else {
                        const string file_path = strex(config_dir).combine_path(name);
                        auto file = DiskFileSystem::OpenFile(file_path, false);

                        if (file) {
                            string file_content;
                            file_content.resize(file.GetSize());
                            file.Read(file_content.data(), file_content.size());
                            file_content = strex(file_content).trim();

                            resolved_value += setting_value.substr(prev_pos, pos - prev_pos - len) + string(file_content);
                            end_pos++;
                        }
                        else {
                            WriteLog(LogType::Warning, "File {} for setting {} is not found", file_path, setting_name);
                            resolved_value += setting_value.substr(prev_pos, pos - prev_pos) + name;
                        }
                    }

                    prev_pos = end_pos;
                    pos = setting_value.find('$', end_pos);
                }
                else {
                    throw SettingsException("Not closed $ tag in settings", setting_name, setting_value);
                }
            }
            else {
                pos = setting_value.find('$', pos + 1);
            }
        }

        if (prev_pos != string::npos) {
            resolved_value += setting_value.substr(prev_pos);
        }

        value = resolved_value;
    }

#define SET_SETTING(sett) \
    SetEntry(sett, value, append); \
    break
#define FIXED_SETTING(type, name, ...) \
    case const_hash(#name): \
        SET_SETTING(const_cast<type&>(name))
#define VARIABLE_SETTING(type, name, ...) \
    case const_hash(#name): \
        SET_SETTING(name)
#define SETTING_GROUP(name, ...)
#define SETTING_GROUP_END()

    switch (const_hash(setting_name.c_str())) {
#include "Settings-Include.h"
    default:
        _customSettings[setting_name] = setting_value;
        break;
    }

#undef SET_SETTING

    if (_bakingMode) {
        _appliedSettings.emplace(setting_name);
    }
}

void GlobalSettings::AddResourcePacks(const vector<map<string, string>*>& res_packs, string_view config_dir)
{
    FO_STACK_TRACE_ENTRY();

    for (const auto* res_pack : res_packs) {
        const auto get_map_value = [&](string_view key) -> string {
            const auto it = res_pack->find(key);
            return it != res_pack->end() ? it->second : string();
        };

        ResourcePackInfo pack_info;

        if (auto name = get_map_value("Name"); !name.empty()) {
            pack_info.Name = std::move(name);
        }
        else {
            throw SettingsException("Resource pack name not specifed");
        }

        if (auto server_only = get_map_value("ServerOnly"); !server_only.empty()) {
            pack_info.ServerOnly = strex(server_only).to_bool();
        }
        if (auto client_only = get_map_value("ClientOnly"); !client_only.empty()) {
            pack_info.ClientOnly = strex(client_only).to_bool();
        }
        if (auto mapper_only = get_map_value("MapperOnly"); !mapper_only.empty()) {
            pack_info.MapperOnly = strex(mapper_only).to_bool();
        }
        if (std::bit_cast<int8>(pack_info.ServerOnly) + std::bit_cast<int8>(pack_info.ClientOnly) + std::bit_cast<int8>(pack_info.MapperOnly) > 1) {
            throw SettingsException("Resource pack can be common or server, client or mapper only");
        }

        if (auto inpurt_dir = get_map_value("InputDir"); !inpurt_dir.empty()) {
            for (auto& dir : strex(inpurt_dir).split(' ')) {
                dir = strex(config_dir).combine_path(dir);
                pack_info.InputDir.emplace_back(std::move(dir));
            }
        }
        if (auto input_file = get_map_value("InputFile"); !input_file.empty()) {
            for (auto& fname : strex(input_file).split(' ')) {
                fname = strex(config_dir).combine_path(fname);
                pack_info.InputFile.emplace_back(std::move(fname));
            }
        }
        if (auto recursive_input = get_map_value("RecursiveInput"); !recursive_input.empty()) {
            pack_info.RecursiveInput = strex(recursive_input).to_bool();
        }
        if (auto bake_order = get_map_value("BakeOrder"); !bake_order.empty()) {
            pack_info.BakeOrder = strex(bake_order).to_int32();
        }

        if (pack_info.ServerOnly) {
            const_cast<vector<string>&>(ServerResourceEntries).emplace_back(pack_info.Name);
        }
        else if (pack_info.ClientOnly) {
            const_cast<vector<string>&>(ClientResourceEntries).emplace_back(pack_info.Name);
        }
        else if (pack_info.MapperOnly) {
            const_cast<vector<string>&>(MapperResourceEntries).emplace_back(pack_info.Name);
        }
        else {
            const_cast<vector<string>&>(ServerResourceEntries).emplace_back(pack_info.Name);
            const_cast<vector<string>&>(ClientResourceEntries).emplace_back(pack_info.Name);
        }

        _resourcePacks.emplace_back(std::move(pack_info));
    }
}

void GlobalSettings::AddSubConfigs(const vector<map<string, string>*>& sub_configs, string_view config_dir)
{
    FO_STACK_TRACE_ENTRY();

    for (const auto* sub_config : sub_configs) {
        const auto get_map_value = [&](string_view key) -> string {
            const auto it = sub_config->find(key);
            return it != sub_config->end() ? it->second : string();
        };

        SubConfigInfo config_info;
        config_info.ConfigDir = config_dir;

        if (auto name = get_map_value("Name"); !name.empty()) {
            config_info.Name = std::move(name);
        }
        else {
            throw SettingsException("Sub config name not specifed");
        }

        if (auto parents = strex(get_map_value("Parent")).split(' '); !parents.empty()) {
            for (const auto& parent : parents) {
                const auto find_predicate = [&](const SubConfigInfo& cfg) { return cfg.Name == parent; };
                const auto it = std::ranges::find_if(_subConfigs, find_predicate);

                if (it == _subConfigs.end()) {
                    throw SettingsException("Parent sub config not found", parent);
                }

                config_info.Settings = it->Settings;
            }
        }

        for (auto&& [key, value] : *sub_config) {
            if (key != "Name" && key != "Parent") {
                config_info.Settings[key] = value;
            }
        }

        _subConfigs.emplace_back(std::move(config_info));
    }
}

auto GlobalSettings::Save() const -> map<string, string>
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(_bakingMode);

    map<string, string> result;

    for (auto&& [key, value] : _customSettings) {
        if (_appliedSettings.count(key) != 0) {
            result.emplace(key, value);
        }
    }

    const auto add_setting = [&](string_view name, const auto& value) {
        if (_appliedSettings.count(name) != 0) {
            result.emplace(name, strex("{}", value));
        }
    };

#define FIXED_SETTING(type, name, ...) add_setting(#name, name)
#define VARIABLE_SETTING(type, name, ...) add_setting(#name, name)
#define SETTING_GROUP(name, ...)
#define SETTING_GROUP_END()
#include "Settings-Include.h"

    return result;
}

void GlobalSettings::Draw(bool editable)
{
    FO_STACK_TRACE_ENTRY();

#define FIXED_SETTING(type, name, ...) \
    if (editable) { \
        DrawEditableEntry(#name, const_cast<type&>(name)); \
    } \
    else { \
        DrawEntry(#name, name); \
    }
#define VARIABLE_SETTING(type, name, ...) \
    if (editable) { \
        DrawEditableEntry(#name, name); \
    } \
    else { \
        DrawEntry(#name, name); \
    }
#define SETTING_GROUP(name, ...)
#define SETTING_GROUP_END()
#include "Settings-Include.h"
}

auto BaseSettings::GetResourcePacks() const -> span<const ResourcePackInfo>
{
    FO_STACK_TRACE_ENTRY();

    if (_resourcePacks.empty()) {
        throw SettingsException("No information about resource packs found");
    }

    return _resourcePacks;
}

FO_END_NAMESPACE();
