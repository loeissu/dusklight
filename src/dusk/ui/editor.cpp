#include "editor.hpp"

#include <RmlUi/Core.h>
#include <fmt/format.h>

#include "bool_button.hpp"
#include "button.hpp"
#include "d/actor/d_a_player.h"
#include "d/d_kankyo.h"
#include "d/d_meter2_info.h"
#include "dusk/map_loader_definitions.h"
#include "number_button.hpp"
#include "pane.hpp"
#include "select_button.hpp"
#include "string_button.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cctype>
#include <functional>
#include <limits>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace dusk::ui {

Rml::String stage_option_label(const MapEntry& map, bool showInternalNames) {
    return showInternalNames ? fmt::format("{} ({})", map.mapName, map.mapFile) : map.mapName;
}

Rml::String stage_label_for_file(const Rml::String& stageFile, bool showInternalNames) {
    for (const auto& region : gameRegions) {
        for (const auto& map : region.maps) {
            if (stageFile == map.mapFile) {
                return stage_option_label(map, showInternalNames);
            }
        }
    }
    return stageFile;
}

void populate_stage_picker(Pane& pane, std::function<Rml::String()> getStageFile,
    std::function<void(const char*)> setStageFile, bool showInternalNames) {
    pane.clear();
    for (const auto& region : gameRegions) {
        pane.add_section(region.regionName);
        for (const auto& map : region.maps) {
            pane.add_button({
                                .text = stage_option_label(map, showInternalNames),
                                .isSelected =
                                    [getStageFile, stageFile = map.mapFile] {
                                        return getStageFile() == stageFile;
                                    },
                            })
                .on_pressed([setStageFile, stageFile = map.mapFile] {
                    mDoAud_seStartMenu(kSoundItemChange);
                    setStageFile(stageFile);
                });
        }
    }
}

namespace {

bool has_save_data() {
    return dComIfGs_getSaveData() != nullptr;
}

dSv_player_status_a_c* get_player_status() {
    if (!has_save_data()) {
        return nullptr;
    }
    return &dComIfGs_getSaveData()->getPlayer().getPlayerStatusA();
}

dSv_player_status_b_c* get_player_status_b() {
    if (!has_save_data()) {
        return nullptr;
    }
    return &dComIfGs_getSaveData()->getPlayer().getPlayerStatusB();
}

dSv_player_return_place_c* get_player_return_place() {
    if (!has_save_data()) {
        return nullptr;
    }
    return &dComIfGs_getSaveData()->getPlayer().getPlayerReturnPlace();
}

dSv_horse_place_c* get_horse_place() {
    if (!has_save_data()) {
        return nullptr;
    }
    return &dComIfGs_getSaveData()->getPlayer().getHorsePlace();
}

dSv_player_item_c* get_player_item() {
    if (!has_save_data()) {
        return nullptr;
    }
    return &dComIfGs_getSaveData()->getPlayer().getItem();
}

dSv_player_item_record_c* get_player_item_record() {
    if (!has_save_data()) {
        return nullptr;
    }
    return &dComIfGs_getSaveData()->getPlayer().getItemRecord();
}

dSv_player_item_max_c* get_player_item_max() {
    if (!has_save_data()) {
        return nullptr;
    }
    return &dComIfGs_getSaveData()->getPlayer().getItemMax();
}

dSv_fishing_info_c* get_player_fishing_info() {
    if (!has_save_data()) {
        return nullptr;
    }
    return &dComIfGs_getSaveData()->getPlayer().getFishingInfo();
}

dSv_MiniGame_c* get_minigame() {
    if (!has_save_data()) {
        return nullptr;
    }
    return &dComIfGs_getSaveData()->getMiniGame();
}

dSv_player_config_c* get_player_config() {
    if (!has_save_data()) {
        return nullptr;
    }
    return &dComIfGs_getSaveData()->getPlayer().getConfig();
}

template <size_t Size>
Rml::String fixed_string(const char (&value)[Size]) {
    size_t length = 0;
    while (length < Size && value[length] != '\0') {
        ++length;
    }
    return Rml::String(value, length);
}

template <size_t Size>
void set_fixed_string(char (&dest)[Size], const Rml::String& value) {
    std::memset(dest, 0, Size);
    std::memcpy(dest, value.data(), std::min(value.size(), Size - 1));
}

void skip_whitespace(const char*& cursor) {
    while (std::isspace(static_cast<unsigned char>(*cursor))) {
        ++cursor;
    }
}

bool parse_float_token(const char*& cursor, float& parsedValue) {
    skip_whitespace(cursor);
    char* end = nullptr;
    parsedValue = std::strtof(cursor, &end);
    if (end == cursor) {
        return false;
    }
    cursor = end;
    skip_whitespace(cursor);
    return true;
}

bool consume_comma(const char*& cursor) {
    skip_whitespace(cursor);
    if (*cursor != ',') {
        return false;
    }
    ++cursor;
    return true;
}

bool parse_vec3(const Rml::String& value, float& x, float& y, float& z) {
    const char* cursor = value.c_str();
    if (!parse_float_token(cursor, x) || !consume_comma(cursor) || !parse_float_token(cursor, y) ||
        !consume_comma(cursor) || !parse_float_token(cursor, z))
    {
        return false;
    }
    skip_whitespace(cursor);
    return *cursor == '\0';
}

Rml::String get_player_name() {
    if (!has_save_data()) {
        return "";
    }
    return dComIfGs_getPlayerName().buffer;
}

void set_player_name(Rml::String name) {
    dComIfGs_setPlayerName(name.c_str());
}

Rml::String get_horse_name() {
    if (!has_save_data()) {
        return "";
    }
    return dComIfGs_getHorseName().buffer;
}

void set_horse_name(Rml::String name) {
    dComIfGs_setHorseName(name.c_str());
}

enum ItemType {
    ITEMTYPE_DEFAULT_e,
    ITEMTYPE_EQUIP_e,
};

struct itemInfo {
    std::string m_name;
    u8 m_type = ITEMTYPE_DEFAULT_e;
};

std::map<int, itemInfo> itemMap = {
    {dItemNo_HEART_e, {"心"}},
    {dItemNo_GREEN_RUPEE_e, {"绿卢比"}},
    {dItemNo_BLUE_RUPEE_e, {"蓝卢比"}},
    {dItemNo_YELLOW_RUPEE_e, {"黄卢比"}},
    {dItemNo_RED_RUPEE_e, {"红卢比"}},
    {dItemNo_PURPLE_RUPEE_e, {"紫卢比"}},
    {dItemNo_ORANGE_RUPEE_e, {"橙卢比"}},
    {dItemNo_SILVER_RUPEE_e, {"银卢比"}},
    {dItemNo_S_MAGIC_e, {"小魔法"}},
    {dItemNo_L_MAGIC_e, {"大魔法"}},
    {dItemNo_BOMB_5_e, {"炸弹（5）"}},
    {dItemNo_BOMB_10_e, {"炸弹（10）"}},
    {dItemNo_BOMB_20_e, {"炸弹（20）"}},
    {dItemNo_BOMB_30_e, {"炸弹（30）"}},
    {dItemNo_ARROW_10_e, {"箭（10）"}},
    {dItemNo_ARROW_20_e, {"箭（20）"}},
    {dItemNo_ARROW_30_e, {"箭（30）"}},
    {dItemNo_ARROW_1_e, {"箭（1）"}},
    {dItemNo_PACHINKO_SHOT_e, {"南瓜种子"}},
    {dItemNo_NOENTRY_19_e, {"保留"}},
    {dItemNo_NOENTRY_20_e, {"保留"}},
    {dItemNo_NOENTRY_21_e, {"保留"}},
    {dItemNo_WATER_BOMB_5_e, {"水炸弹（5）"}},
    {dItemNo_WATER_BOMB_10_e, {"水炸弹（10）"}},
    {dItemNo_WATER_BOMB_20_e, {"水炸弹（20）"}},
    {dItemNo_WATER_BOMB_30_e, {"水炸弹（30）"}},
    {dItemNo_BOMB_INSECT_5_e, {"炸弹虫（5）"}},
    {dItemNo_BOMB_INSECT_10_e, {"炸弹虫（10）"}},
    {dItemNo_BOMB_INSECT_20_e, {"炸弹虫（20）"}},
    {dItemNo_BOMB_INSECT_30_e, {"炸弹虫（30）"}},
    {dItemNo_RECOVERY_FAILY_e, {"妖精"}},
    {dItemNo_TRIPLE_HEART_e, {"三连心"}},
    {dItemNo_SMALL_KEY_e, {"小钥匙"}},
    {dItemNo_KAKERA_HEART_e, {"心之碎片"}},
    {dItemNo_UTAWA_HEART_e, {"心之容器"}},
    {dItemNo_MAP_e, {"迷宫地图"}},
    {dItemNo_COMPUS_e, {"罗盘"}},
    {dItemNo_DUNGEON_EXIT_e, {"乌可爷爷（初次）", ITEMTYPE_EQUIP_e}},
    {dItemNo_BOSS_KEY_e, {"头目钥匙"}},
    {dItemNo_DUNGEON_BACK_e, {"乌可 Jr.", ITEMTYPE_EQUIP_e}},
    {dItemNo_SWORD_e, {"奥登之剑"}},
    {dItemNo_MASTER_SWORD_e, {"大师之剑"}},
    {dItemNo_WOOD_SHIELD_e, {"奥登之盾"}},
    {dItemNo_SHIELD_e, {"木盾"}},
    {dItemNo_HYLIA_SHIELD_e, {"海利亚盾"}},
    {dItemNo_TKS_LETTER_e, {"乌可的信", ITEMTYPE_EQUIP_e}},
    {dItemNo_WEAR_CASUAL_e, {"奥登之服"}},
    {dItemNo_WEAR_KOKIRI_e, {"勇者之服"}},
    {dItemNo_ARMOR_e, {"魔法铠甲"}},
    {dItemNo_WEAR_ZORA_e, {"佐拉之铠"}},
    {dItemNo_MAGIC_LV1_e, {"魔法等级 1"}},
    {dItemNo_DUNGEON_EXIT_2_e, {"乌可爷爷", ITEMTYPE_EQUIP_e}},
    {dItemNo_WALLET_LV1_e, {"钱包"}},
    {dItemNo_WALLET_LV2_e, {"大钱包"}},
    {dItemNo_WALLET_LV3_e, {"巨型钱包"}},
    {dItemNo_NOENTRY_55_e, {"保留"}},
    {dItemNo_NOENTRY_56_e, {"保留"}},
    {dItemNo_NOENTRY_57_e, {"保留"}},
    {dItemNo_NOENTRY_58_e, {"保留"}},
    {dItemNo_NOENTRY_59_e, {"保留"}},
    {dItemNo_NOENTRY_60_e, {"保留"}},
    {dItemNo_ZORAS_JEWEL_e, {"珊瑚耳环", ITEMTYPE_EQUIP_e}},
    {dItemNo_HAWK_EYE_e, {"鹰眼", ITEMTYPE_EQUIP_e}},
    {dItemNo_WOOD_STICK_e, {"木剑"}},
    {dItemNo_BOOMERANG_e, {"疾风回旋镖", ITEMTYPE_EQUIP_e}},
    {dItemNo_SPINNER_e, {"陀螺", ITEMTYPE_EQUIP_e}},
    {dItemNo_IRONBALL_e, {"铁球锁链", ITEMTYPE_EQUIP_e}},
    {dItemNo_BOW_e, {"勇者之弓", ITEMTYPE_EQUIP_e}},
    {dItemNo_HOOKSHOT_e, {"钩爪", ITEMTYPE_EQUIP_e}},
    {dItemNo_HVY_BOOTS_e, {"铁靴", ITEMTYPE_EQUIP_e}},
    {dItemNo_COPY_ROD_e, {"支配之杖", ITEMTYPE_EQUIP_e}},
    {dItemNo_W_HOOKSHOT_e, {"双钩爪", ITEMTYPE_EQUIP_e}},
    {dItemNo_KANTERA_e, {"提灯", ITEMTYPE_EQUIP_e}},
    {dItemNo_LIGHT_SWORD_e, {"光之剑"}},
    {dItemNo_FISHING_ROD_1_e, {"钓竿", ITEMTYPE_EQUIP_e}},
    {dItemNo_PACHINKO_e, {"弹弓", ITEMTYPE_EQUIP_e}},
    {dItemNo_COPY_ROD_2_e, {"支配之杖（未蓄能）"}},
    {dItemNo_NOENTRY_77_e, {"保留"}},
    {dItemNo_NOENTRY_78_e, {"保留"}},
    {dItemNo_BOMB_BAG_LV2_e, {"巨型炸弹袋"}},
    {dItemNo_BOMB_BAG_LV1_e, {"空炸弹袋", ITEMTYPE_EQUIP_e}},
    {dItemNo_BOMB_IN_BAG_e, {"炸弹袋"}},
    {dItemNo_NOENTRY_82_e, {"保留"}},
    {dItemNo_LIGHT_ARROW_e, {"光之箭"}},
    {dItemNo_ARROW_LV1_e, {"箭袋"}},
    {dItemNo_ARROW_LV2_e, {"大箭袋"}},
    {dItemNo_ARROW_LV3_e, {"巨型箭袋"}},
    {dItemNo_NOENTRY_87_e, {"保留"}},
    {dItemNo_LURE_ROD_e, {"钓竿（拟饵）"}},
    {dItemNo_BOMB_ARROW_e, {"炸弹箭"}},
    {dItemNo_HAWK_ARROW_e, {"鹰箭"}},
    {dItemNo_BEE_ROD_e, {"钓竿（蜂幼虫）", ITEMTYPE_EQUIP_e}},
    {dItemNo_JEWEL_ROD_e, {"钓竿（耳环）", ITEMTYPE_EQUIP_e}},
    {dItemNo_WORM_ROD_e, {"钓竿（蠕虫）", ITEMTYPE_EQUIP_e}},
    {dItemNo_JEWEL_BEE_ROD_e, {"钓竿（耳环＋蜂幼虫）", ITEMTYPE_EQUIP_e}},
    {dItemNo_JEWEL_WORM_ROD_e, {"钓竿（耳环＋蠕虫）", ITEMTYPE_EQUIP_e}},
    {dItemNo_EMPTY_BOTTLE_e, {"空瓶", ITEMTYPE_EQUIP_e}},
    {dItemNo_RED_BOTTLE_e, {"红药水", ITEMTYPE_EQUIP_e}},
    {dItemNo_GREEN_BOTTLE_e, {"绿药水", ITEMTYPE_EQUIP_e}},
    {dItemNo_BLUE_BOTTLE_e, {"蓝药水", ITEMTYPE_EQUIP_e}},
    {dItemNo_MILK_BOTTLE_e, {"牛奶瓶", ITEMTYPE_EQUIP_e}},
    {dItemNo_HALF_MILK_BOTTLE_e, {"半瓶牛奶", ITEMTYPE_EQUIP_e}},
    {dItemNo_OIL_BOTTLE_e, {"提灯油", ITEMTYPE_EQUIP_e}},
    {dItemNo_WATER_BOTTLE_e, {"水瓶", ITEMTYPE_EQUIP_e}},
    {dItemNo_OIL_BOTTLE_2_e, {"提灯油（舀取）"}},
    {dItemNo_RED_BOTTLE_2_e, {"红药水（舀取）"}},
    {dItemNo_UGLY_SOUP_e, {"难喝的汤", ITEMTYPE_EQUIP_e}},
    {dItemNo_HOT_SPRING_e, {"温泉水", ITEMTYPE_EQUIP_e}},
    {dItemNo_FAIRY_e, {"妖精", ITEMTYPE_EQUIP_e}},
    {dItemNo_HOT_SPRING_2_e, {"温泉水（商店）"}},
    {dItemNo_OIL2_e, {"提灯油补给（舀取）"}},
    {dItemNo_OIL_e, {"提灯油补给（商店）"}},
    {dItemNo_NORMAL_BOMB_e, {"炸弹", ITEMTYPE_EQUIP_e}},
    {dItemNo_WATER_BOMB_e, {"水炸弹", ITEMTYPE_EQUIP_e}},
    {dItemNo_POKE_BOMB_e, {"炸弹虫", ITEMTYPE_EQUIP_e}},
    {dItemNo_FAIRY_DROP_e, {"大妖精之泪", ITEMTYPE_EQUIP_e}},
    {dItemNo_WORM_e, {"蠕虫", ITEMTYPE_EQUIP_e}},
    {dItemNo_DROP_BOTTLE_e, {"大妖精之泪（乔瓦尼）"}},
    {dItemNo_BEE_CHILD_e, {"蜂幼虫", ITEMTYPE_EQUIP_e}},
    {dItemNo_CHUCHU_RARE_e, {"稀有丘丘胶", ITEMTYPE_EQUIP_e}},
    {dItemNo_CHUCHU_RED_e, {"红丘丘胶", ITEMTYPE_EQUIP_e}},
    {dItemNo_CHUCHU_BLUE_e, {"蓝丘丘胶", ITEMTYPE_EQUIP_e}},
    {dItemNo_CHUCHU_GREEN_e, {"绿丘丘胶", ITEMTYPE_EQUIP_e}},
    {dItemNo_CHUCHU_YELLOW_e, {"黄丘丘胶", ITEMTYPE_EQUIP_e}},
    {dItemNo_CHUCHU_PURPLE_e, {"紫丘丘胶", ITEMTYPE_EQUIP_e}},
    {dItemNo_LV1_SOUP_e, {"清淡的汤", ITEMTYPE_EQUIP_e}},
    {dItemNo_LV2_SOUP_e, {"好喝的汤", ITEMTYPE_EQUIP_e}},
    {dItemNo_LV3_SOUP_e, {"极品汤", ITEMTYPE_EQUIP_e}},
    {dItemNo_LETTER_e, {"雷纳多的信", ITEMTYPE_EQUIP_e}},
    {dItemNo_BILL_e, {"账单", ITEMTYPE_EQUIP_e}},
    {dItemNo_WOOD_STATUE_e, {"木雕像", ITEMTYPE_EQUIP_e}},
    {dItemNo_IRIAS_PENDANT_e, {"伊莉亚的护身符", ITEMTYPE_EQUIP_e}},
    {dItemNo_HORSE_FLUTE_e, {"马哨", ITEMTYPE_EQUIP_e}},
    {dItemNo_NOENTRY_133_e, {"保留"}},
    {dItemNo_NOENTRY_134_e, {"保留"}},
    {dItemNo_NOENTRY_135_e, {"保留"}},
    {dItemNo_NOENTRY_136_e, {"保留"}},
    {dItemNo_NOENTRY_137_e, {"保留"}},
    {dItemNo_NOENTRY_138_e, {"保留"}},
    {dItemNo_NOENTRY_139_e, {"保留"}},
    {dItemNo_NOENTRY_140_e, {"保留"}},
    {dItemNo_NOENTRY_141_e, {"保留"}},
    {dItemNo_NOENTRY_142_e, {"保留"}},
    {dItemNo_NOENTRY_143_e, {"保留"}},
    {dItemNo_RAFRELS_MEMO_e, {"奥鲁的备忘录", ITEMTYPE_EQUIP_e}},
    {dItemNo_ASHS_SCRIBBLING_e, {"艾雪莉的素描", ITEMTYPE_EQUIP_e}},
    {dItemNo_NOENTRY_146_e, {"保留"}},
    {dItemNo_NOENTRY_147_e, {"保留"}},
    {dItemNo_NOENTRY_148_e, {"保留"}},
    {dItemNo_NOENTRY_149_e, {"保留"}},
    {dItemNo_NOENTRY_150_e, {"保留"}},
    {dItemNo_NOENTRY_151_e, {"保留"}},
    {dItemNo_NOENTRY_152_e, {"保留"}},
    {dItemNo_NOENTRY_153_e, {"保留"}},
    {dItemNo_NOENTRY_154_e, {"保留"}},
    {dItemNo_NOENTRY_155_e, {"保留"}},
    {dItemNo_CHUCHU_YELLOW2_e, {"提灯油补给（黄色丘丘）"}},
    {dItemNo_OIL_BOTTLE3_e, {"提灯油（科洛）"}},
    {dItemNo_SHOP_BEE_CHILD_e, {"蜂幼虫（商店）"}},
    {dItemNo_CHUCHU_BLACK_e, {"黑丘丘胶", ITEMTYPE_EQUIP_e}},
    {dItemNo_LIGHT_DROP_e, {"光之泪"}},
    {dItemNo_DROP_CONTAINER_e, {"光之容器（法隆）"}},
    {dItemNo_DROP_CONTAINER02_e, {"光之容器（艾尔丁）"}},
    {dItemNo_DROP_CONTAINER03_e, {"光之容器（拉聂耳）"}},
    {dItemNo_FILLED_CONTAINER_e, {"光之容器（已充满）"}},
    {dItemNo_MIRROR_PIECE_2_e, {"镜子碎片（雪峰遗迹）"}},
    {dItemNo_MIRROR_PIECE_3_e, {"镜子碎片（时之神殿）"}},
    {dItemNo_MIRROR_PIECE_4_e, {"镜子碎片（天空之城）"}},
    {dItemNo_NOENTRY_168_e, {"保留"}},
    {dItemNo_NOENTRY_169_e, {"保留"}},
    {dItemNo_NOENTRY_170_e, {"保留"}},
    {dItemNo_NOENTRY_171_e, {"保留"}},
    {dItemNo_NOENTRY_172_e, {"保留"}},
    {dItemNo_NOENTRY_173_e, {"保留"}},
    {dItemNo_NOENTRY_174_e, {"保留"}},
    {dItemNo_NOENTRY_175_e, {"保留"}},
    {dItemNo_SMELL_YELIA_POUCH_e, {"伊莉亚的香气"}},
    {dItemNo_SMELL_PUMPKIN_e, {"南瓜香气"}},
    {dItemNo_SMELL_POH_e, {"波伊的香气"}},
    {dItemNo_SMELL_FISH_e, {"臭鱼香气"}},
    {dItemNo_SMELL_CHILDREN_e, {"少年的香气"}},
    {dItemNo_SMELL_MEDICINE_e, {"药之香气"}},
    {dItemNo_NOENTRY_182_e, {"保留"}},
    {dItemNo_NOENTRY_183_e, {"保留"}},
    {dItemNo_NOENTRY_184_e, {"保留"}},
    {dItemNo_NOENTRY_185_e, {"保留"}},
    {dItemNo_NOENTRY_186_e, {"保留"}},
    {dItemNo_NOENTRY_187_e, {"保留"}},
    {dItemNo_NOENTRY_188_e, {"保留"}},
    {dItemNo_NOENTRY_189_e, {"保留"}},
    {dItemNo_NOENTRY_190_e, {"保留"}},
    {dItemNo_NOENTRY_191_e, {"保留"}},
    {dItemNo_M_BEETLE_e, {"甲虫（雄）"}},
    {dItemNo_F_BEETLE_e, {"甲虫（雌）"}},
    {dItemNo_M_BUTTERFLY_e, {"蝴蝶（雄）"}},
    {dItemNo_F_BUTTERFLY_e, {"蝴蝶（雌）"}},
    {dItemNo_M_STAG_BEETLE_e, {"锹形虫（雄）"}},
    {dItemNo_F_STAG_BEETLE_e, {"锹形虫（雌）"}},
    {dItemNo_M_GRASSHOPPER_e, {"蚱蜢（雄）"}},
    {dItemNo_F_GRASSHOPPER_e, {"蚱蜢（雌）"}},
    {dItemNo_M_NANAFUSHI_e, {"竹节虫（雄）"}},
    {dItemNo_F_NANAFUSHI_e, {"竹节虫（雌）"}},
    {dItemNo_M_DANGOMUSHI_e, {"鼠妇（雄）"}},
    {dItemNo_F_DANGOMUSHI_e, {"鼠妇（雌）"}},
    {dItemNo_M_MANTIS_e, {"螳螂（雄）"}},
    {dItemNo_F_MANTIS_e, {"螳螂（雌）"}},
    {dItemNo_M_LADYBUG_e, {"瓢虫（雄）"}},
    {dItemNo_F_LADYBUG_e, {"瓢虫（雌）"}},
    {dItemNo_M_SNAIL_e, {"蜗牛（雄）"}},
    {dItemNo_F_SNAIL_e, {"蜗牛（雌）"}},
    {dItemNo_M_DRAGONFLY_e, {"蜻蜓（雄）"}},
    {dItemNo_F_DRAGONFLY_e, {"蜻蜓（雌）"}},
    {dItemNo_M_ANT_e, {"蚂蚁（雄）"}},
    {dItemNo_F_ANT_e, {"蚂蚁（雌）"}},
    {dItemNo_M_MAYFLY_e, {"蜉蝣（雄）"}},
    {dItemNo_F_MAYFLY_e, {"蜉蝣（雌）"}},
    {dItemNo_NOENTRY_216_e, {"保留"}},
    {dItemNo_NOENTRY_217_e, {"保留"}},
    {dItemNo_NOENTRY_218_e, {"保留"}},
    {dItemNo_NOENTRY_219_e, {"保留"}},
    {dItemNo_NOENTRY_220_e, {"保留"}},
    {dItemNo_NOENTRY_221_e, {"保留"}},
    {dItemNo_NOENTRY_222_e, {"保留"}},
    {dItemNo_NOENTRY_223_e, {"保留"}},
    {dItemNo_POU_SPIRIT_e, {"波伊之魂"}},
    {dItemNo_NOENTRY_225_e, {"保留"}},
    {dItemNo_NOENTRY_226_e, {"保留"}},
    {dItemNo_NOENTRY_227_e, {"保留"}},
    {dItemNo_NOENTRY_228_e, {"保留"}},
    {dItemNo_NOENTRY_229_e, {"保留"}},
    {dItemNo_NOENTRY_230_e, {"保留"}},
    {dItemNo_NOENTRY_231_e, {"保留"}},
    {dItemNo_NOENTRY_232_e, {"保留"}},
    {dItemNo_ANCIENT_DOCUMENT_e, {"古代天空之书", ITEMTYPE_EQUIP_e}},
    {dItemNo_AIR_LETTER_e, {"古代天空之书（残缺）", ITEMTYPE_EQUIP_e}},
    {dItemNo_ANCIENT_DOCUMENT2_e, {"古代天空之书（完整）", ITEMTYPE_EQUIP_e}},
    {dItemNo_LV7_DUNGEON_EXIT_e, {"乌可爷爷（天空之城）"}},
    {dItemNo_LINKS_SAVINGS_e, {"紫卢比（林克的积蓄）"}},
    {dItemNo_SMALL_KEY2_e, {"小钥匙（法隆北门）"}},
    {dItemNo_POU_FIRE1_e, {"波伊之火 1"}},
    {dItemNo_POU_FIRE2_e, {"波伊之火 2"}},
    {dItemNo_POU_FIRE3_e, {"波伊之火 3"}},
    {dItemNo_POU_FIRE4_e, {"波伊之火 4"}},
    {dItemNo_BOSSRIDER_KEY_e, {"海拉尔平原钥匙"}},
    {dItemNo_TOMATO_PUREE_e, {"奥登南瓜", ITEMTYPE_EQUIP_e}},
    {dItemNo_TASTE_e, {"奥登山羊奶酪", ITEMTYPE_EQUIP_e}},
    {dItemNo_LV5_BOSS_KEY_e, {"卧室钥匙"}},
    {dItemNo_SURFBOARD_e, {"冲浪叶"}},
    {dItemNo_KANTERA2_e, {"提灯（取回）"}},
    {dItemNo_L2_KEY_PIECES1_e, {"钥匙碎片（1）"}},
    {dItemNo_L2_KEY_PIECES2_e, {"钥匙碎片（2）"}},
    {dItemNo_L2_KEY_PIECES3_e, {"钥匙碎片（3）"}},
    {dItemNo_KEY_OF_CARAVAN_e, {"布尔布林营地钥匙"}},
    {dItemNo_LV2_BOSS_KEY_e, {"戈隆矿山头目钥匙"}},
    {dItemNo_KEY_OF_FILONE_e, {"法隆南门钥匙"}},
    {dItemNo_NONE_e, {"无"}},
};

Rml::String get_item_name(u8 id) {
    const auto it = itemMap.find(id);
    if (it == itemMap.end()) {
        return fmt::format("物品 {}", id);
    }
    return it->second.m_name;
}

Rml::String item_label_for_slot(u8 slot) {
    if (slot == 0xFF) {
        return "无";
    }
    const auto id = dComIfGs_getSaveData()->getPlayer().getItem().mItems[slot];
    return fmt::format("栏位 {0} ({1})", slot, get_item_name(id));
}

struct NamedIndexEntry {
    const char* name;
    u8 index;
};

struct NamedFlagEntry {
    const char* name;
    u16 flag;
};

struct BugSpeciesEntry {
    const char* name;
    u8 maleItem;
    u8 femaleItem;
    u16 maleTurnInFlag;
    u16 femaleTurnInFlag;
};

struct FishSpeciesEntry {
    const char* name;
    u8 index;
};

constexpr std::array<u8, 4> swordEntries = {
    dItemNo_SWORD_e,
    dItemNo_MASTER_SWORD_e,
    dItemNo_WOOD_STICK_e,
    dItemNo_LIGHT_SWORD_e,
};

constexpr std::array<u8, 3> shieldEntries = {
    dItemNo_SHIELD_e,
    dItemNo_WOOD_SHIELD_e,
    dItemNo_HYLIA_SHIELD_e,
};

constexpr std::array<u8, 5> smellEntries = {
    dItemNo_SMELL_CHILDREN_e,
    dItemNo_SMELL_YELIA_POUCH_e,
    dItemNo_SMELL_POH_e,
    dItemNo_SMELL_FISH_e,
    dItemNo_SMELL_MEDICINE_e,
};

constexpr std::array fusedShadowEntries = {
    NamedIndexEntry{"森林神殿", 0},
    NamedIndexEntry{"戈隆矿山", 1},
    NamedIndexEntry{"湖底神殿", 2},
};

constexpr std::array mirrorShardEntries = {
    NamedIndexEntry{"雪峰遗迹", 1},
    NamedIndexEntry{"时之神殿", 2},
    NamedIndexEntry{"天空之城", 3},
};

constexpr std::array bugSpeciesEntries = {
    BugSpeciesEntry{"蚂蚁", dItemNo_M_ANT_e, dItemNo_F_ANT_e, dSv_event_flag_c::F_0421,
        dSv_event_flag_c::F_0422},
    BugSpeciesEntry{"蜉蝣", dItemNo_M_MAYFLY_e, dItemNo_F_MAYFLY_e, dSv_event_flag_c::F_0423,
        dSv_event_flag_c::F_0424},
    BugSpeciesEntry{"甲虫", dItemNo_M_BEETLE_e, dItemNo_F_BEETLE_e, dSv_event_flag_c::F_0401,
        dSv_event_flag_c::F_0402},
    BugSpeciesEntry{"螳螂", dItemNo_M_MANTIS_e, dItemNo_F_MANTIS_e, dSv_event_flag_c::F_0413,
        dSv_event_flag_c::F_0414},
    BugSpeciesEntry{"锹形虫", dItemNo_M_STAG_BEETLE_e, dItemNo_F_STAG_BEETLE_e,
        dSv_event_flag_c::F_0405, dSv_event_flag_c::F_0406},
    BugSpeciesEntry{"鼠妇", dItemNo_M_DANGOMUSHI_e, dItemNo_F_DANGOMUSHI_e,
        dSv_event_flag_c::F_0411, dSv_event_flag_c::F_0412},
    BugSpeciesEntry{"蝴蝶", dItemNo_M_BUTTERFLY_e, dItemNo_F_BUTTERFLY_e,
        dSv_event_flag_c::F_0403, dSv_event_flag_c::F_0404},
    BugSpeciesEntry{"瓢虫", dItemNo_M_LADYBUG_e, dItemNo_F_LADYBUG_e, dSv_event_flag_c::F_0415,
        dSv_event_flag_c::F_0416},
    BugSpeciesEntry{"蜗牛", dItemNo_M_SNAIL_e, dItemNo_F_SNAIL_e, dSv_event_flag_c::F_0417,
        dSv_event_flag_c::F_0418},
    BugSpeciesEntry{"竹节虫", dItemNo_M_NANAFUSHI_e, dItemNo_F_NANAFUSHI_e,
        dSv_event_flag_c::F_0409, dSv_event_flag_c::F_0410},
    BugSpeciesEntry{"蚱蜢", dItemNo_M_GRASSHOPPER_e, dItemNo_F_GRASSHOPPER_e,
        dSv_event_flag_c::F_0407, dSv_event_flag_c::F_0408},
    BugSpeciesEntry{"蜻蜓", dItemNo_M_DRAGONFLY_e, dItemNo_F_DRAGONFLY_e,
        dSv_event_flag_c::F_0419, dSv_event_flag_c::F_0420},
};

constexpr std::array<NamedFlagEntry, 7> hiddenSkillEntries = {
    NamedFlagEntry{"终结一击", dSv_event_flag_c::F_0339},
    NamedFlagEntry{"盾击", dSv_event_flag_c::F_0338},
    NamedFlagEntry{"背后斩", dSv_event_flag_c::F_0340},
    NamedFlagEntry{"破盔斩", dSv_event_flag_c::F_0341},
    NamedFlagEntry{"居合斩", dSv_event_flag_c::F_0342},
    NamedFlagEntry{"跳跃斩", dSv_event_flag_c::F_0343},
    NamedFlagEntry{"大回旋斩", dSv_event_flag_c::F_0344},
};

constexpr std::array<const char*, 16> letterSenders = {
    "雷纳多",
    "乌可 1",
    "乌可 2",
    "邮递员",
    "卡卡利科杂货",
    "巴恩斯 1",
    "巴恩斯 2",
    "巴恩斯炸弹",
    "玛洛商店",
    "泰尔玛",
    "普鲁",
    "来自 Jr.",
    "阿吉莎公主",
    "拉聂耳观光",
    "沙德",
    "耶塔",
};

constexpr std::array<FishSpeciesEntry, 6> fishSpeciesEntries = {
    FishSpeciesEntry{"奥登鲶鱼", 3},
    FishSpeciesEntry{"绿鳃鱼", 5},
    FishSpeciesEntry{"臭鱼", 4},
    FishSpeciesEntry{"海拉尔鲈鱼", 0},
    FishSpeciesEntry{"海利亚狗鱼", 2},
    FishSpeciesEntry{"海利亚泥鳅", 1},
};

constexpr std::array<const char*, 2> targetTypeNames = {
    "按住",
    "切换",
};

constexpr std::array<const char*, 3> soundModeNames = {
    "单声道",
    "立体声",
    "环绕声",
};

struct DefaultInventoryEntry {
    u8 slot;
    u8 item;
};

constexpr std::array<DefaultInventoryEntry, 22> defaultInventory = {
    DefaultInventoryEntry{SLOT_0, dItemNo_BOOMERANG_e},
    DefaultInventoryEntry{SLOT_1, dItemNo_KANTERA_e},
    DefaultInventoryEntry{SLOT_2, dItemNo_SPINNER_e},
    DefaultInventoryEntry{SLOT_3, dItemNo_HVY_BOOTS_e},
    DefaultInventoryEntry{SLOT_4, dItemNo_BOW_e},
    DefaultInventoryEntry{SLOT_5, dItemNo_HAWK_EYE_e},
    DefaultInventoryEntry{SLOT_6, dItemNo_IRONBALL_e},
    DefaultInventoryEntry{SLOT_8, dItemNo_COPY_ROD_e},
    DefaultInventoryEntry{SLOT_9, dItemNo_HOOKSHOT_e},
    DefaultInventoryEntry{SLOT_10, dItemNo_W_HOOKSHOT_e},
    DefaultInventoryEntry{SLOT_11, dItemNo_EMPTY_BOTTLE_e},
    DefaultInventoryEntry{SLOT_12, dItemNo_EMPTY_BOTTLE_e},
    DefaultInventoryEntry{SLOT_13, dItemNo_EMPTY_BOTTLE_e},
    DefaultInventoryEntry{SLOT_14, dItemNo_EMPTY_BOTTLE_e},
    DefaultInventoryEntry{SLOT_15, dItemNo_NORMAL_BOMB_e},
    DefaultInventoryEntry{SLOT_16, dItemNo_WATER_BOMB_e},
    DefaultInventoryEntry{SLOT_17, dItemNo_POKE_BOMB_e},
    DefaultInventoryEntry{SLOT_18, dItemNo_DUNGEON_EXIT_e},
    DefaultInventoryEntry{SLOT_20, dItemNo_FISHING_ROD_1_e},
    DefaultInventoryEntry{SLOT_21, dItemNo_HORSE_FLUTE_e},
    DefaultInventoryEntry{SLOT_22, dItemNo_ANCIENT_DOCUMENT_e},
    DefaultInventoryEntry{SLOT_23, dItemNo_PACHINKO_e},
};

u8 get_slot_default(int slot) {
    for (const auto& entry : defaultInventory) {
        if (entry.slot == slot) {
            return entry.item;
        }
    }
    return dItemNo_NONE_e;
}

void set_item_first_bit(u8 itemNo, bool owned) {
    if (owned) {
        dComIfGs_onItemFirstBit(itemNo);
    } else {
        dComIfGs_offItemFirstBit(itemNo);
    }
}

void toggle_item_first_bit(u8 itemNo) {
    set_item_first_bit(itemNo, !dComIfGs_isItemFirstBit(itemNo));
}

void set_event_bit(u16 flag, bool enabled) {
    if (enabled) {
        dComIfGs_onEventBit(flag);
    } else {
        dComIfGs_offEventBit(flag);
    }
}

void set_letter_get_flag(int index, bool received) {
    if (received) {
        if (dComIfGs_isLetterGetFlag(index)) {
            return;
        }
        dComIfGs_onLetterGetFlag(index);
        const u8 slot = dMeter2Info_getRecieveLetterNum() - 1;
        if (slot < 64) {
            dComIfGs_setGetNumber(slot, static_cast<u8>(index + 1));
        }
    } else {
        if (!dComIfGs_isLetterGetFlag(index)) {
            return;
        }
        auto& info = dComIfGs_getSaveData()->getPlayer().getLetterInfo();
        info.mLetterGetFlags[index >> 5] &= ~(1u << (index & 0x1F));
        for (int slot = 0; slot < 64; ++slot) {
            if (dComIfGs_getGetNumber(slot) != index + 1) {
                continue;
            }
            for (int nextSlot = slot; nextSlot < 63; ++nextSlot) {
                dComIfGs_setGetNumber(nextSlot, dComIfGs_getGetNumber(nextSlot + 1));
            }
            dComIfGs_setGetNumber(63, 0);
            break;
        }
    }
}

void set_max_life(int maxLife) {
    maxLife = std::clamp(maxLife, 15, 100);
    dComIfGs_setMaxLife(static_cast<u8>(maxLife));
    const u16 maxHealth = (dComIfGs_getMaxLife() / 5) * 4;
    if (dComIfGs_getLife() > maxHealth) {
        dComIfGs_setLife(maxHealth);
    }
}

Rml::String max_life_label() {
    const int maxLife = dComIfGs_getMaxLife();
    return fmt::format("{} hearts + {} pieces", maxLife / 5, maxLife % 5);
}

struct ToggleEntry {
    Rml::String text;
    std::function<bool()> isSelected;
    std::function<void(bool)> setSelected;
};

void populate_toggle_group(Pane& pane, const std::vector<ToggleEntry>& entries) {
    pane.clear();
    pane.add_section("操作");
    pane.add_button("全选").on_pressed([entries] {
        mDoAud_seStartMenu(kSoundItemChange);
        for (const auto& entry : entries) {
            entry.setSelected(true);
        }
    });
    pane.add_button("全不选").on_pressed([entries] {
        mDoAud_seStartMenu(kSoundItemChange);
        for (const auto& entry : entries) {
            entry.setSelected(false);
        }
    });

    pane.add_section("物品");
    for (const auto& entry : entries) {
        pane.add_button({
                            .text = entry.text,
                            .isSelected = entry.isSelected,
                        })
            .on_pressed([isSelected = entry.isSelected, setSelected = entry.setSelected] {
                mDoAud_seStartMenu(kSoundItemChange);
                setSelected(!isSelected());
            });
    }
}

template <size_t Size>
int count_item_first_bits(const std::array<u8, Size>& entries) {
    int count = 0;
    for (const auto item : entries) {
        if (dComIfGs_isItemFirstBit(item)) {
            ++count;
        }
    }
    return count;
}

template <size_t Size>
int count_event_bits(const std::array<NamedFlagEntry, Size>& entries) {
    int count = 0;
    for (const auto& entry : entries) {
        if (dComIfGs_isEventBit(entry.flag)) {
            ++count;
        }
    }
    return count;
}

template <size_t Size>
int count_collect_crystals(const std::array<NamedIndexEntry, Size>& entries) {
    int count = 0;
    for (const auto& entry : entries) {
        if (dComIfGs_isCollectCrystal(entry.index)) {
            ++count;
        }
    }
    return count;
}

template <size_t Size>
int count_collect_mirrors(const std::array<NamedIndexEntry, Size>& entries) {
    int count = 0;
    for (const auto& entry : entries) {
        if (dComIfGs_isCollectMirror(entry.index)) {
            ++count;
        }
    }
    return count;
}

Rml::String count_label(int count, int total) {
    return fmt::format("{} / {}", count, total);
}

int count_clothing() {
    int count = 0;
    if (dComIfGs_isItemFirstBit(dItemNo_WEAR_CASUAL_e)) {
        ++count;
    }
    if (dComIfGs_isCollectClothes(KOKIRI_CLOTHES_FLAG)) {
        ++count;
    }
    if (dComIfGs_isItemFirstBit(dItemNo_WEAR_ZORA_e)) {
        ++count;
    }
    if (dComIfGs_isItemFirstBit(dItemNo_ARMOR_e)) {
        ++count;
    }
    return count;
}

int count_letters() {
    int count = 0;
    for (int index = 0; index < letterSenders.size(); ++index) {
        if (dComIfGs_isLetterGetFlag(index)) {
            ++count;
        }
    }
    return count;
}

Rml::String bug_species_label(const BugSpeciesEntry& bug) {
    int owned = 0;
    int given = 0;
    if (dComIfGs_isItemFirstBit(bug.maleItem)) {
        ++owned;
    }
    if (dComIfGs_isItemFirstBit(bug.femaleItem)) {
        ++owned;
    }
    if (dComIfGs_isEventBit(bug.maleTurnInFlag)) {
        ++given;
    }
    if (dComIfGs_isEventBit(bug.femaleTurnInFlag)) {
        ++given;
    }
    return fmt::format("{} / 2 owned, {} / 2 given", owned, given);
}

Rml::String fish_species_label(const FishSpeciesEntry& fish) {
    return fmt::format(
        "{} caught, {} cm", dComIfGs_getFishNum(fish.index), dComIfGs_getFishSize(fish.index));
}

bool can_edit_item_first_bit(int itemId, const itemInfo& item) {
    return itemId < 254 && item.m_name != "保留";
}

void set_all_item_first_bits(bool owned) {
    for (const auto& [itemId, item] : itemMap) {
        if (!can_edit_item_first_bit(itemId, item)) {
            continue;
        }
        set_item_first_bit(static_cast<u8>(itemId), owned);
    }
}

void populate_item_slot_picker(Pane& pane, int slot) {
    pane.clear();
    pane.add_section("操作");
    pane.add_button(fmt::format("默认（{}）", get_item_name(get_slot_default(slot))))
        .on_pressed([slot] {
            mDoAud_seStartMenu(kSoundItemChange);
            dComIfGs_setItem(slot, get_slot_default(slot));
        });

    pane.add_section("物品");
    pane.add_button(
            {
                .text = "无",
                .isSelected = [slot] { return get_player_item()->mItems[slot] == dItemNo_NONE_e; },
            })
        .on_pressed([slot] {
            mDoAud_seStartMenu(kSoundItemChange);
            dComIfGs_setItem(slot, dItemNo_NONE_e);
        });
    for (const auto& [itemId, item] : itemMap) {
        if (item.m_type != ITEMTYPE_EQUIP_e) {
            continue;
        }
        pane
            .add_button({
                .text = item.m_name,
                .isSelected = [slot, itemId] { return get_player_item()->mItems[slot] == itemId; },
            })
            .on_pressed([slot, itemId] {
                mDoAud_seStartMenu(kSoundItemChange);
                dComIfGs_setItem(slot, static_cast<u8>(itemId));
            });
    }
}

void populate_item_flag_picker(Pane& pane) {
    pane.clear();
    pane.add_section("操作");
    pane.add_button("全选").on_pressed([] {
        mDoAud_seStartMenu(kSoundItemChange);
        set_all_item_first_bits(true);
    });
    pane.add_button("全部清除").on_pressed([] {
        mDoAud_seStartMenu(kSoundItemChange);
        set_all_item_first_bits(false);
    });

    pane.add_section("物品");
    for (const auto& [itemId, item] : itemMap) {
        if (!can_edit_item_first_bit(itemId, item)) {
            continue;
        }
        pane
            .add_button({
                .text = item.m_name,
                .isSelected = [itemId] { return dComIfGs_isItemFirstBit(static_cast<u8>(itemId)); },
            })
            .on_pressed([itemId] {
                mDoAud_seStartMenu(kSoundItemChange);
                toggle_item_first_bit(static_cast<u8>(itemId));
            });
    }
}

void populate_select_item_picker(Pane& pane, u8& selectItemData) {
    pane.clear();
    pane.add_button(
            {
                .text = "无",
                .isSelected = [&selectItemData] { return selectItemData == dItemNo_NONE_e; },
            })
        .on_pressed([&selectItemData] {
            mDoAud_seStartMenu(kSoundItemChange);
            selectItemData = dItemNo_NONE_e;
        });
    for (int i = 0; i < 24; i++) {
        pane.add_button({
                            .text = item_label_for_slot(i),
                            .isSelected = [i, &selectItemData] { return selectItemData == i; },
                        })
            .on_pressed([i, &selectItemData] {
                mDoAud_seStartMenu(kSoundItemChange);
                selectItemData = i;
            });
    }
}

void populate_select_clothes_picker(Pane& pane) {
    pane.clear();
    const auto addOption = [&pane](u8 id) {
        pane.add_button(
                {
                    .text = get_item_name(id),
                    .isSelected = [id] { return get_player_status()->mSelectEquip[0] == id; },
                })
            .on_pressed([id] {
                mDoAud_seStartMenu(kSoundItemChange);
                dMeter2Info_setCloth(id, false);
                daPy_getPlayerActorClass()->setClothesChange(0);
            });
    };
    addOption(dItemNo_WEAR_CASUAL_e);
    addOption(dItemNo_WEAR_KOKIRI_e);
    addOption(dItemNo_WEAR_ZORA_e);
    addOption(dItemNo_ARMOR_e);
}

template <size_t Size>
void populate_select_equip_picker(Pane& pane, u8& equip, const std::array<u8, Size>& entries) {
    pane.clear();
    const auto addOption = [&pane, &equip](u8 id) {
        pane.add_button({
                            .text = get_item_name(id),
                            .isSelected = [id, &equip] { return equip == id; },
                        })
            .on_pressed([id, &equip] {
                mDoAud_seStartMenu(kSoundItemChange);
                equip = id;
            });
    };
    addOption(dItemNo_NONE_e);
    for (const auto item : entries) {
        addOption(item);
    }
}

static const std::array<Rml::String, 3> walletSizeNames = {
    "普通",
    "大",
    "特大",
};

void populate_wallet_picker(Pane& pane) {
    pane.clear();
    for (int i = 0; i < walletSizeNames.size(); ++i) {
        pane.add_button({
                            .text = walletSizeNames[i],
                            .isSelected = [i] { return get_player_status()->getWalletSize() == i; },
                        })
            .on_pressed([i] {
                mDoAud_seStartMenu(kSoundItemChange);
                get_player_status()->setWalletSize(i);
            });
    }
}

static const std::array<Rml::String, 2> formNames = {
    "人形",
    "狼形",
};

void populate_form_picker(Pane& pane) {
    pane.clear();
    for (int i = 0; i < formNames.size(); ++i) {
        pane.add_button(
                {
                    .text = formNames[i],
                    .isSelected = [i] { return get_player_status()->getTransformStatus() == i; },
                })
            .on_pressed([i] {
                mDoAud_seStartMenu(kSoundItemChange);
                get_player_status()->setTransformStatus(i);
            });
    }
}

void add_toggle_button(Pane& pane, ToggleEntry entry) {
    auto isSelected = std::move(entry.isSelected);
    auto setSelected = std::move(entry.setSelected);
    pane.add_button({
                        .text = entry.text,
                        .isSelected = isSelected,
                    })
        .on_pressed([isSelected, setSelected] {
            mDoAud_seStartMenu(kSoundItemChange);
            setSelected(!isSelected());
        });
}

template <size_t Size>
std::vector<ToggleEntry> item_toggle_entries(const std::array<u8, Size>& entries) {
    std::vector<ToggleEntry> toggles;
    toggles.reserve(entries.size());
    for (const auto item : entries) {
        toggles.push_back({
            .text = get_item_name(item),
            .isSelected = [item] { return dComIfGs_isItemFirstBit(item); },
            .setSelected = [item](bool selected) { set_item_first_bit(item, selected); },
        });
    }
    return toggles;
}

template <size_t Size>
std::vector<ToggleEntry> event_toggle_entries(const std::array<NamedFlagEntry, Size>& entries) {
    std::vector<ToggleEntry> toggles;
    toggles.reserve(entries.size());
    for (const auto& [name, flag] : entries) {
        toggles.push_back({
            .text = name,
            .isSelected = [flag] { return dComIfGs_isEventBit(flag); },
            .setSelected = [flag](bool selected) { set_event_bit(flag, selected); },
        });
    }
    return toggles;
}

template <size_t Size>
std::vector<ToggleEntry> collect_crystal_toggle_entries(
    const std::array<NamedIndexEntry, Size>& entries) {
    std::vector<ToggleEntry> toggles;
    toggles.reserve(entries.size());
    for (const auto& [name, index] : entries) {
        toggles.push_back({
            .text = name,
            .isSelected = [index] { return dComIfGs_isCollectCrystal(index); },
            .setSelected =
                [index](bool selected) {
                    if (selected) {
                        dComIfGs_onCollectCrystal(index);
                    } else {
                        dComIfGs_offCollectCrystal(index);
                    }
                },
        });
    }
    return toggles;
}

template <size_t Size>
std::vector<ToggleEntry> collect_mirror_toggle_entries(
    const std::array<NamedIndexEntry, Size>& entries) {
    std::vector<ToggleEntry> toggles;
    toggles.reserve(entries.size());
    for (const auto& [name, index] : entries) {
        toggles.push_back({
            .text = name,
            .isSelected = [index] { return dComIfGs_isCollectMirror(index); },
            .setSelected =
                [index](bool selected) {
                    if (selected) {
                        dComIfGs_onCollectMirror(index);
                    } else {
                        dComIfGs_offCollectMirror(index);
                    }
                },
        });
    }
    return toggles;
}

void populate_collect_clothes_picker(Pane& pane) {
    populate_toggle_group(pane,
        {
            ToggleEntry{
                .text = "奥登之服",
                .isSelected = [] { return dComIfGs_isItemFirstBit(dItemNo_WEAR_CASUAL_e); },
                .setSelected =
                    [](bool selected) { set_item_first_bit(dItemNo_WEAR_CASUAL_e, selected); },
            },
            ToggleEntry{
                .text = "勇者之服",
                .isSelected = [] { return dComIfGs_isCollectClothes(KOKIRI_CLOTHES_FLAG); },
                .setSelected =
                    [](bool selected) {
                        if (selected) {
                            dComIfGs_setCollectClothes(KOKIRI_CLOTHES_FLAG);
                        } else {
                            dComIfGs_offCollectClothes(KOKIRI_CLOTHES_FLAG);
                        }
                    },
            },
            ToggleEntry{
                .text = "佐拉之铠",
                .isSelected = [] { return dComIfGs_isItemFirstBit(dItemNo_WEAR_ZORA_e); },
                .setSelected =
                    [](bool selected) { set_item_first_bit(dItemNo_WEAR_ZORA_e, selected); },
            },
            ToggleEntry{
                .text = "魔法铠甲",
                .isSelected = [] { return dComIfGs_isItemFirstBit(dItemNo_ARMOR_e); },
                .setSelected = [](bool selected) { set_item_first_bit(dItemNo_ARMOR_e, selected); },
            },
        });
}

void populate_poe_souls_picker(Pane& pane) {
    pane.clear();
    pane.add_section("操作");
    pane.add_button("全部 60").on_pressed([] {
        mDoAud_seStartMenu(kSoundItemChange);
        dComIfGs_setPohSpiritNum(60);
    });
    pane.add_button("清除").on_pressed([] {
        mDoAud_seStartMenu(kSoundItemChange);
        dComIfGs_setPohSpiritNum(0);
    });

    pane.add_section("数值");
    pane.add_child<NumberButton>(NumberButton::Props{
        .key = "已收集",
        .getValue = [] { return dComIfGs_getPohSpiritNum(); },
        .setValue =
            [](int value) { dComIfGs_setPohSpiritNum(static_cast<u8>(std::clamp(value, 0, 60))); },
        .max = 60,
    });
}

void populate_max_life_picker(Pane& pane) {
    pane.clear();
    pane.add_section("操作");
    pane.add_button("3 颗心").on_pressed([] {
        mDoAud_seStartMenu(kSoundItemChange);
        dComIfGs_setMaxLife(15);
        dComIfGs_setLife(12);
    });
    pane.add_button("20 颗心").on_pressed([] {
        mDoAud_seStartMenu(kSoundItemChange);
        dComIfGs_setMaxLife(100);
        dComIfGs_setLife(80);
    });

    pane.add_section("数值");
    pane.add_child<NumberButton>(NumberButton::Props{
        .key = "最大生命",
        .getValue = [] { return dComIfGs_getMaxLife(); },
        .setValue = [](int value) { set_max_life(value); },
        .min = 15,
        .max = 100,
    });
}

void populate_bug_species_picker(Pane& pane, const BugSpeciesEntry& bug) {
    pane.clear();
    pane.add_section("已拥有");
    add_toggle_button(
        pane, {
                  .text = fmt::format("雄 {}", bug.name),
                  .isSelected = [item = bug.maleItem] { return dComIfGs_isItemFirstBit(item); },
                  .setSelected = [item = bug.maleItem](
                                     bool selected) { set_item_first_bit(item, selected); },
              });
    add_toggle_button(
        pane, {
                  .text = fmt::format("雌性 {}", bug.name),
                  .isSelected = [item = bug.femaleItem] { return dComIfGs_isItemFirstBit(item); },
                  .setSelected = [item = bug.femaleItem](
                                     bool selected) { set_item_first_bit(item, selected); },
              });

    pane.add_section("已交给阿吉莎");
    add_toggle_button(
        pane, {
                  .text = fmt::format("雄 {}", bug.name),
                  .isSelected = [flag = bug.maleTurnInFlag] { return dComIfGs_isEventBit(flag); },
                  .setSelected = [flag = bug.maleTurnInFlag](
                                     bool selected) { set_event_bit(flag, selected); },
              });
    add_toggle_button(
        pane, {
                  .text = fmt::format("雌性 {}", bug.name),
                  .isSelected = [flag = bug.femaleTurnInFlag] { return dComIfGs_isEventBit(flag); },
                  .setSelected = [flag = bug.femaleTurnInFlag](
                                     bool selected) { set_event_bit(flag, selected); },
              });
}

void populate_letters_picker(Pane& pane) {
    std::vector<ToggleEntry> toggles;
    toggles.reserve(letterSenders.size());
    for (int index = 0; index < letterSenders.size(); ++index) {
        toggles.push_back({
            .text = letterSenders[index],
            .isSelected = [index] { return dComIfGs_isLetterGetFlag(index); },
            .setSelected = [index](bool selected) { set_letter_get_flag(index, selected); },
        });
    }
    populate_toggle_group(pane, toggles);
}

void populate_fish_species_picker(Pane& pane, const FishSpeciesEntry& fish) {
    pane.clear();
    pane.add_section(fish.name);
    pane.add_child<NumberButton>(NumberButton::Props{
        .key = "已捕获",
        .getValue = [index = fish.index] { return dComIfGs_getFishNum(index); },
        .setValue =
            [index = fish.index](int value) {
                get_player_fishing_info()->mFishCount[index] =
                    static_cast<u16>(std::clamp(value, 0, 999));
            },
        .max = 999,
    });
    pane.add_child<NumberButton>(NumberButton::Props{
        .key = "最大",
        .getValue = [index = fish.index] { return dComIfGs_getFishSize(index); },
        .setValue =
            [index = fish.index](int value) {
                dComIfGs_setFishSize(index, static_cast<u8>(std::clamp(value, 0, 255)));
            },
        .max = 255,
    });
}

Rml::String target_type_label() {
    const auto type = get_player_config()->getAttentionType();
    if (type >= targetTypeNames.size()) {
        return fmt::format("未知（{}）", type);
    }
    return targetTypeNames[type];
}

Rml::String sound_mode_label() {
    const auto mode = get_player_config()->getSound();
    if (mode >= soundModeNames.size()) {
        return fmt::format("未知 ({})", mode);
    }
    return soundModeNames[mode];
}

void populate_target_type_picker(Pane& pane) {
    pane.clear();
    for (u8 type = 0; type < targetTypeNames.size(); ++type) {
        pane
            .add_button({
                .text = targetTypeNames[type],
                .isSelected = [type] { return get_player_config()->getAttentionType() == type; },
            })
            .on_pressed([type] {
                mDoAud_seStartMenu(kSoundItemChange);
                get_player_config()->setAttentionType(type);
            });
    }
}

void populate_sound_mode_picker(Pane& pane) {
    pane.clear();
    for (u8 mode = 0; mode < soundModeNames.size(); ++mode) {
        pane.add_button(
                {
                    .text = soundModeNames[mode],
                    .isSelected = [mode] { return get_player_config()->getSound() == mode; },
                })
            .on_pressed([mode] {
                mDoAud_seStartMenu(kSoundItemChange);
                get_player_config()->setSound(mode);
            });
    }
}

constexpr float kDaytimeUnitsPerHour = 15.0f;

float daytime_from_clock(int hour, int minute) {
    hour = std::clamp(hour, 0, 23);
    minute = std::clamp(minute, 0, 59);
    return (hour * kDaytimeUnitsPerHour) + (minute / 60.0f * kDaytimeUnitsPerHour);
}

void set_clock_time(int hour, int minute) {
    if (auto* statusB = get_player_status_b()) {
        statusB->setTime(daytime_from_clock(hour, minute));
    }
}

}  // namespace

EditorWindow::EditorWindow() {
    add_tab("玩家状态", [this](Rml::Element* content) {
        auto& leftPane = add_child<Pane>(content, Pane::Type::Controlled);
        auto& rightPane = add_child<Pane>(content, Pane::Type::Uncontrolled);

        leftPane.add_section("玩家");
        leftPane.register_control(leftPane.add_child<StringButton>(StringButton::Props{
                                      .key = "玩家姓名",
                                      .getValue = get_player_name,
                                      .setValue = set_player_name,
                                      .maxLength = 16,
                                  }),
            rightPane, {});
        leftPane.register_control(leftPane.add_child<StringButton>(StringButton::Props{
                                      .key = "马匹姓名",
                                      .getValue = get_horse_name,
                                      .setValue = set_horse_name,
                                      .maxLength = 16,
                                  }),
            rightPane, {});
        leftPane.register_control(
            leftPane.add_child<NumberButton>(NumberButton::Props{
                .key = "最大生命",
                .getValue = [] { return get_player_status()->getMaxLife(); },
                .setValue = [](int value) { return get_player_status()->setMaxLife(value); },
                .max = UINT16_MAX,  // TODO: actual max
            }),
            rightPane, {});
        leftPane.register_control(
            leftPane.add_child<NumberButton>(NumberButton::Props{
                .key = "生命",
                .getValue = [] { return get_player_status()->getLife(); },
                .setValue = [](int value) { return get_player_status()->setLife(value); },
                .max = UINT16_MAX,  // TODO: actual max
            }),
            rightPane, {});
        leftPane.register_control(
            leftPane.add_child<NumberButton>(NumberButton::Props{
                .key = "卢比",
                .getValue = [] { return get_player_status()->getRupee(); },
                .setValue = [](int value) { return get_player_status()->setRupee(value); },
                .max = get_player_status()->getRupeeMax(),
            }),
            rightPane, {});
        leftPane.register_control(
            leftPane.add_child<NumberButton>(NumberButton::Props{
                .key = "最大灯油",
                .getValue = [] { return get_player_status()->getMaxOil(); },
                .setValue = [](int value) { return get_player_status()->setMaxOil(value); },
                .max = UINT16_MAX,  // TODO: actual max
            }),
            rightPane, {});
        leftPane.register_control(
            leftPane.add_child<NumberButton>(NumberButton::Props{
                .key = "灯油",
                .getValue = [] { return get_player_status()->getOil(); },
                .setValue = [](int value) { return get_player_status()->setOil(value); },
                .max = UINT16_MAX,  // TODO: actual max
            }),
            rightPane, {});

        leftPane.add_section("装备");
        const auto genSelectItemComboBox = [&leftPane, &rightPane](
                                               const Rml::String& label, u8& selectItemData) {
            leftPane.register_control(
                leftPane.add_select_button({
                    .key = label,
                    .getValue = [&selectItemData] { return item_label_for_slot(selectItemData); },
                }),
                rightPane, [&selectItemData](Pane& pane) {
                    populate_select_item_picker(pane, selectItemData);
                });
        };
        genSelectItemComboBox("装备 X", get_player_status()->mSelectItem[0]);
        genSelectItemComboBox("装备 Y", get_player_status()->mSelectItem[1]);
        genSelectItemComboBox("组合装备 X", get_player_status()->mMixItem[0]);
        genSelectItemComboBox("组合装备 Y", get_player_status()->mMixItem[1]);

        leftPane.register_control(
            leftPane.add_select_button({
                .key = "服装",
                .getValue = [] { return get_item_name(get_player_status()->mSelectEquip[0]); },
            }),
            rightPane, [](Pane& pane) { populate_select_clothes_picker(pane); });
        leftPane.register_control(
            leftPane.add_select_button({
                .key = "剑",
                .getValue = [] { return get_item_name(get_player_status()->mSelectEquip[1]); },
            }),
            rightPane, [](Pane& pane) {
                populate_select_equip_picker(
                    pane, get_player_status()->mSelectEquip[1], swordEntries);
            });
        leftPane.register_control(
            leftPane.add_select_button({
                .key = "盾",
                .getValue = [] { return get_item_name(get_player_status()->mSelectEquip[2]); },
            }),
            rightPane, [](Pane& pane) {
                populate_select_equip_picker(
                    pane, get_player_status()->mSelectEquip[2], shieldEntries);
            });
        leftPane.register_control(
            leftPane.add_select_button({
                .key = "气味",
                .getValue = [] { return get_item_name(get_player_status()->mSelectEquip[3]); },
            }),
            rightPane, [](Pane& pane) {
                populate_select_equip_picker(
                    pane, get_player_status()->mSelectEquip[3], smellEntries);
            });
        leftPane.register_control(
            leftPane.add_select_button({
                .key = "钱包容量",
                .getValue = [] { return walletSizeNames[get_player_status()->getWalletSize()]; },
            }),
            rightPane, [](Pane& pane) { populate_wallet_picker(pane); });
        leftPane.register_control(
            leftPane.add_select_button({
                .key = "形态",
                .getValue = [] { return formNames[get_player_status()->getTransformStatus()]; },
            }),
            rightPane, [](Pane& pane) { populate_form_picker(pane); });

        leftPane.add_section("世界");
        leftPane.register_control(
            leftPane.add_child<NumberButton>(NumberButton::Props{
                .key = "天",
                .getValue = [] { return get_player_status_b()->getDate(); },
                .setValue =
                    [](int value) { get_player_status_b()->setDate(static_cast<u16>(value)); },
                .max = UINT16_MAX,
            }),
            rightPane, {});
        leftPane.register_control(
            leftPane.add_child<NumberButton>(NumberButton::Props{
                .key = "小时",
                .getValue = [] { return dKy_getdaytime_hour(); },
                .setValue = [](int value) { set_clock_time(value, dKy_getdaytime_minute()); },
                .max = 23,
            }),
            rightPane, {});
        leftPane.register_control(
            leftPane.add_child<NumberButton>(NumberButton::Props{
                .key = "分钟",
                .getValue = [] { return dKy_getdaytime_minute(); },
                .setValue = [](int value) { set_clock_time(dKy_getdaytime_hour(), value); },
                .max = 59,
            }),
            rightPane, {});
        leftPane.register_control(
            leftPane.add_child<NumberButton>(NumberButton::Props{
                .key = "变身等级",
                .getValue =
                    [] {
                        return std::popcount(static_cast<unsigned>(
                            get_player_status_b()->mTransformLevelFlag & 0xF));
                    },
                .setValue =
                    [](int value) {
                        get_player_status_b()->mTransformLevelFlag =
                            static_cast<u8>((1u << value) - 1u);
                    },
                .max = 4,
            }),
            rightPane, {});
        leftPane.register_control(
            leftPane.add_child<NumberButton>(NumberButton::Props{
                .key = "黄昏清除等级",
                .getValue =
                    [] {
                        return std::popcount(static_cast<unsigned>(
                            get_player_status_b()->mDarkClearLevelFlag & 0x7));
                    },
                .setValue =
                    [](int value) {
                        get_player_status_b()->mDarkClearLevelFlag =
                            static_cast<u8>((1u << value) - 1u);
                    },
                .max = 3,
            }),
            rightPane, {});
    });

    add_tab("地点", [this](Rml::Element* content) {
        auto& leftPane = add_child<Pane>(content, Pane::Type::Controlled);
        auto& rightPane = add_child<Pane>(content, Pane::Type::Uncontrolled);

        leftPane.add_section("存档地点");
        leftPane
            .register_control(leftPane.add_select_button({
                                  .key = "场景",
                                  .getValue =
                                      [] {
                                          return stage_label_for_file(
                                              fixed_string(get_player_return_place()->mName));
                                      },
                              }),
                rightPane,
                [](Pane& pane) {
                    populate_stage_picker(
                        pane, [] { return fixed_string(get_player_return_place()->mName); },
                        [](const char* stageFile) {
                            set_fixed_string(
                                get_player_return_place()->mName, Rml::String(stageFile));
                        });
                })
            .set_disabled(true);
        leftPane.register_control(
            leftPane.add_child<NumberButton>(NumberButton::Props{
                .key = "房间",
                .getValue = [] { return get_player_return_place()->mRoomNo; },
                .setValue =
                    [](int value) { get_player_return_place()->mRoomNo = static_cast<s8>(value); },
                .min = std::numeric_limits<s8>::min(),
                .max = std::numeric_limits<s8>::max(),
            }),
            rightPane, {});
        leftPane.register_control(
            leftPane.add_child<NumberButton>(NumberButton::Props{
                .key = "出生点 ID",
                .getValue = [] { return get_player_return_place()->mPlayerStatus; },
                .setValue =
                    [](int value) {
                        get_player_return_place()->mPlayerStatus = static_cast<u8>(value);
                    },
                .max = std::numeric_limits<u8>::max(),
            }),
            rightPane, {});

        leftPane.add_section("马匹位置");
        leftPane.register_control(leftPane.add_child<StringButton>(StringButton::Props{
                                      .key = "马匹坐标",
                                      .getValue =
                                          [] {
                                              const auto* horsePlace = get_horse_place();
                                              return fmt::format("{}, {}, {}",
                                                  static_cast<float>(horsePlace->mPos.x),
                                                  static_cast<float>(horsePlace->mPos.y),
                                                  static_cast<float>(horsePlace->mPos.z));
                                          },
                                      .setValue =
                                          [](Rml::String value) {
                                              float x = 0.0f;
                                              float y = 0.0f;
                                              float z = 0.0f;
                                              if (parse_vec3(value, x, y, z)) {
                                                  auto* horsePlace = get_horse_place();
                                                  horsePlace->mPos.x = x;
                                                  horsePlace->mPos.y = y;
                                                  horsePlace->mPos.z = z;
                                              }
                                          },
                                  }),
            rightPane, {});
        leftPane.register_control(
            leftPane.add_child<NumberButton>(NumberButton::Props{
                .key = "马匹角度",
                .getValue = [] { return get_horse_place()->mAngleY; },
                .setValue = [](int value) { get_horse_place()->mAngleY = static_cast<s16>(value); },
                .min = std::numeric_limits<s16>::min(),
                .max = std::numeric_limits<s16>::max(),
            }),
            rightPane, {});
        leftPane
            .register_control(
                leftPane.add_select_button({
                    .key = "马匹场景",
                    .getValue =
                        [] { return stage_label_for_file(fixed_string(get_horse_place()->mName)); },
                }),
                rightPane,
                [](Pane& pane) {
                    populate_stage_picker(
                        pane, [] { return fixed_string(get_horse_place()->mName); },
                        [](const char* stageFile) {
                            set_fixed_string(get_horse_place()->mName, Rml::String(stageFile));
                        });
                })
            .set_disabled(true);
        leftPane.register_control(
            leftPane.add_child<NumberButton>(NumberButton::Props{
                .key = "马匹房间",
                .getValue = [] { return get_horse_place()->mRoomNo; },
                .setValue = [](int value) { get_horse_place()->mRoomNo = static_cast<s8>(value); },
                .min = std::numeric_limits<s8>::min(),
                .max = std::numeric_limits<s8>::max(),
            }),
            rightPane, {});
        leftPane.register_control(
            leftPane.add_child<NumberButton>(NumberButton::Props{
                .key = "马匹出生点 ID",
                .getValue = [] { return get_horse_place()->mSpawnId; },
                .setValue = [](int value) { get_horse_place()->mSpawnId = static_cast<u8>(value); },
                .max = std::numeric_limits<u8>::max(),
            }),
            rightPane, {});
    });

    add_tab("物品栏", [this](Rml::Element* content) {
        auto& leftPane = add_child<Pane>(content, Pane::Type::Controlled);
        auto& rightPane = add_child<Pane>(content, Pane::Type::Uncontrolled);

        leftPane.add_section("道具轮盘");
        leftPane.register_control(leftPane.add_button("全部默认").on_pressed([&rightPane] {
            mDoAud_seStartMenu(kSoundItemChange);
            for (int slot = 0; slot < 24; ++slot) {
                dComIfGs_setItem(slot, get_slot_default(slot));
            }
            rightPane.clear();
        }),
            rightPane, {});
        leftPane.register_control(leftPane.add_button("全部清除").on_pressed([&rightPane] {
            mDoAud_seStartMenu(kSoundItemChange);
            for (int slot = 0; slot < 24; ++slot) {
                dComIfGs_setItem(slot, dItemNo_NONE_e);
            }
            rightPane.clear();
        }),
            rightPane, {});
        for (int slot = 0; slot < 24; ++slot) {
            leftPane.register_control(
                leftPane.add_select_button({
                    .key = fmt::format("栏位 {0:02d}", slot),
                    .getValue = [slot] { return get_item_name(get_player_item()->mItems[slot]); },
                }),
                rightPane, [slot](Pane& pane) { populate_item_slot_picker(pane, slot); });
        }

        leftPane.add_section("数量");
        leftPane.register_control(
            leftPane.add_child<NumberButton>(NumberButton::Props{
                .key = "箭矢数量",
                .getValue = [] { return get_player_item_record()->mArrowNum; },
                .setValue =
                    [](int value) { get_player_item_record()->mArrowNum = static_cast<u8>(value); },
                .max = std::numeric_limits<u8>::max(),
            }),
            rightPane, {});
        leftPane.register_control(
            leftPane.add_child<NumberButton>(NumberButton::Props{
                .key = "弹弓数量",
                .getValue = [] { return get_player_item_record()->mPachinkoNum; },
                .setValue =
                    [](int value) {
                        get_player_item_record()->mPachinkoNum = static_cast<u8>(value);
                    },
                .max = std::numeric_limits<u8>::max(),
            }),
            rightPane, {});
        for (int bag = 0; bag < 3; ++bag) {
            leftPane.register_control(
                leftPane.add_child<NumberButton>(NumberButton::Props{
                    .key = fmt::format("炸弹袋 {} 数量", bag + 1),
                    .getValue = [bag] { return get_player_item_record()->mBombNum[bag]; },
                    .setValue =
                        [bag](int value) {
                            get_player_item_record()->mBombNum[bag] = static_cast<u8>(value);
                        },
                    .max = std::numeric_limits<u8>::max(),
                }),
                rightPane, {});
        }
        for (int bottle = 0; bottle < 4; ++bottle) {
            leftPane.register_control(
                leftPane.add_child<NumberButton>(NumberButton::Props{
                    .key = fmt::format("瓶子 {} 数量", bottle + 1),
                    .getValue = [bottle] { return get_player_item_record()->mBottleNum[bottle]; },
                    .setValue =
                        [bottle](int value) {
                            get_player_item_record()->mBottleNum[bottle] = static_cast<u8>(value);
                        },
                    .max = std::numeric_limits<u8>::max(),
                }),
                rightPane, {});
        }

        leftPane.add_section("容量");
        leftPane.register_control(
            leftPane.add_child<NumberButton>(NumberButton::Props{
                .key = "箭矢上限",
                .getValue = [] { return get_player_item_max()->mItemMax[0]; },
                .setValue =
                    [](int value) { get_player_item_max()->mItemMax[0] = static_cast<u8>(value); },
                .max = std::numeric_limits<u8>::max(),
            }),
            rightPane, {});
        leftPane.register_control(
            leftPane.add_child<NumberButton>(NumberButton::Props{
                .key = "普通炸弹上限",
                .getValue = [] { return get_player_item_max()->mItemMax[1]; },
                .setValue =
                    [](int value) { get_player_item_max()->mItemMax[1] = static_cast<u8>(value); },
                .max = std::numeric_limits<u8>::max(),
            }),
            rightPane, {});
        leftPane.register_control(
            leftPane.add_child<NumberButton>(NumberButton::Props{
                .key = "水炸弹上限",
                .getValue = [] { return get_player_item_max()->mItemMax[2]; },
                .setValue =
                    [](int value) { get_player_item_max()->mItemMax[2] = static_cast<u8>(value); },
                .max = std::numeric_limits<u8>::max(),
            }),
            rightPane, {});
        leftPane.register_control(
            leftPane.add_child<NumberButton>(NumberButton::Props{
                .key = "炸弹虫上限",
                .getValue = [] { return get_player_item_max()->mItemMax[3]; },
                .setValue =
                    [](int value) { get_player_item_max()->mItemMax[3] = static_cast<u8>(value); },
                .max = std::numeric_limits<u8>::max(),
            }),
            rightPane, {});

        leftPane.add_section("标记");
        leftPane.register_control(leftPane.add_select_button({
                                      .key = "已获得物品",
                                      .getValue = [] { return "编辑"; },
                                  }),
            rightPane, [](Pane& pane) { populate_item_flag_picker(pane); });
    });
    add_tab("收藏", [this](Rml::Element* content) {
        auto& leftPane = add_child<Pane>(content, Pane::Type::Controlled);
        auto& rightPane = add_child<Pane>(content, Pane::Type::Uncontrolled);

        leftPane.add_section("装备");
        leftPane.register_control(
            leftPane.add_select_button({
                .key = "剑",
                .getValue =
                    [] {
                        return count_label(
                            count_item_first_bits(swordEntries), swordEntries.size());
                    },
            }),
            rightPane,
            [](Pane& pane) { populate_toggle_group(pane, item_toggle_entries(swordEntries)); });
        leftPane.register_control(
            leftPane.add_select_button({
                .key = "盾",
                .getValue =
                    [] {
                        return count_label(
                            count_item_first_bits(shieldEntries), shieldEntries.size());
                    },
            }),
            rightPane,
            [](Pane& pane) { populate_toggle_group(pane, item_toggle_entries(shieldEntries)); });
        leftPane.register_control(leftPane.add_select_button({
                                      .key = "服装",
                                      .getValue = [] { return count_label(count_clothing(), 4); },
                                  }),
            rightPane, [](Pane& pane) { populate_collect_clothes_picker(pane); });

        leftPane.add_section("关键道具");
        leftPane.register_control(
            leftPane.add_select_button({
                .key = "融合暗影",
                .getValue =
                    [] {
                        return count_label(
                            count_collect_crystals(fusedShadowEntries), fusedShadowEntries.size());
                    },
            }),
            rightPane, [](Pane& pane) {
                populate_toggle_group(pane, collect_crystal_toggle_entries(fusedShadowEntries));
            });
        leftPane.register_control(
            leftPane.add_select_button({
                .key = "镜之碎片",
                .getValue =
                    [] {
                        return count_label(
                            count_collect_mirrors(mirrorShardEntries), mirrorShardEntries.size());
                    },
            }),
            rightPane, [](Pane& pane) {
                populate_toggle_group(pane, collect_mirror_toggle_entries(mirrorShardEntries));
            });

        leftPane.add_section("生命与灵魂");
        leftPane.register_control(
            leftPane.add_select_button({
                .key = "波伊之魂",
                .getValue = [] { return fmt::format("{} / 60", dComIfGs_getPohSpiritNum()); },
            }),
            rightPane, [](Pane& pane) { populate_poe_souls_picker(pane); });
        leftPane.register_control(leftPane.add_select_button({
                                      .key = "最大生命",
                                      .getValue = [] { return max_life_label(); },
                                  }),
            rightPane, [](Pane& pane) { populate_max_life_picker(pane); });

        leftPane.add_section("黄金虫");
        for (const auto& bug : bugSpeciesEntries) {
            leftPane.register_control(leftPane.add_select_button({
                                          .key = bug.name,
                                          .getValue = [bug] { return bug_species_label(bug); },
                                      }),
                rightPane, [bug](Pane& pane) { populate_bug_species_picker(pane, bug); });
        }

        leftPane.add_section("技能");
        leftPane.register_control(
            leftPane.add_select_button({
                .key = "隐藏技能",
                .getValue =
                    [] {
                        return count_label(
                            count_event_bits(hiddenSkillEntries), hiddenSkillEntries.size());
                    },
            }),
            rightPane, [](Pane& pane) {
                populate_toggle_group(pane, event_toggle_entries(hiddenSkillEntries));
            });

        leftPane.add_section("日志");
        leftPane.register_control(
            leftPane.add_select_button({
                .key = "邮差信件",
                .getValue = [] { return count_label(count_letters(), letterSenders.size()); },
            }),
            rightPane, [](Pane& pane) { populate_letters_picker(pane); });

        leftPane.add_section("钓鱼记录");
        for (const auto& fish : fishSpeciesEntries) {
            leftPane.register_control(leftPane.add_select_button({
                                          .key = fish.name,
                                          .getValue = [fish] { return fish_species_label(fish); },
                                      }),
                rightPane, [fish](Pane& pane) { populate_fish_species_picker(pane, fish); });
        }
    });

    //add_tab("Flags", [this](Rml::Element* content) {
    //    // TODO
    //});

    add_tab("小游戏", [this](Rml::Element* content) {
        auto& leftPane = add_child<Pane>(content, Pane::Type::Controlled);
        auto& rightPane = add_child<Pane>(content, Pane::Type::Uncontrolled);

        leftPane.add_section("记录");
        leftPane.register_control(
            leftPane.add_child<NumberButton>(NumberButton::Props{
                .key = "STAR 游戏时间（毫秒）",
                .getValue =
                    [] {
                        return static_cast<int>(std::min<u32>(
                            get_minigame()->getHookGameTime(), std::numeric_limits<int>::max()));
                    },
                .setValue =
                    [](int value) {
                        get_minigame()->setHookGameTime(static_cast<u32>(std::max(0, value)));
                    },
                .max = std::numeric_limits<int>::max(),
            }),
            rightPane, {});
        leftPane.register_control(
            leftPane.add_child<NumberButton>(NumberButton::Props{
                .key = "滑雪竞速时间（毫秒）",
                .getValue =
                    [] {
                        return static_cast<int>(std::min<u32>(
                            get_minigame()->getRaceGameTime(), std::numeric_limits<int>::max()));
                    },
                .setValue =
                    [](int value) {
                        get_minigame()->setRaceGameTime(static_cast<u32>(std::max(0, value)));
                    },
                .max = std::numeric_limits<int>::max(),
            }),
            rightPane, {});
        leftPane.register_control(
            leftPane.add_child<NumberButton>(NumberButton::Props{
                .key = "弹果飞行得分",
                .getValue =
                    [] {
                        return static_cast<int>(std::min<u32>(
                            get_minigame()->getBalloonScore(), std::numeric_limits<int>::max()));
                    },
                .setValue =
                    [](int value) {
                        get_minigame()->setBalloonScore(static_cast<u32>(std::max(0, value)));
                    },
                .max = std::numeric_limits<int>::max(),
            }),
            rightPane, {});
    });

    add_tab("配置", [this](Rml::Element* content) {
        auto& leftPane = add_child<Pane>(content, Pane::Type::Controlled);
        auto& rightPane = add_child<Pane>(content, Pane::Type::Uncontrolled);

        leftPane.add_section("选项");
        leftPane.register_control(
            leftPane.add_child<BoolButton>(BoolButton::Props{
                .key = "启用震动",
                .getValue = [] { return get_player_config()->getVibration() != 0; },
                .setValue = [](bool value) { get_player_config()->setVibration(value); },
            }),
            rightPane, {});
        leftPane.register_control(leftPane.add_select_button({
                                      .key = "目标类型",
                                      .getValue = [] { return target_type_label(); },
                                  }),
            rightPane, [](Pane& pane) { populate_target_type_picker(pane); });
        leftPane.register_control(leftPane.add_select_button({
                                      .key = "声音",
                                      .getValue = [] { return sound_mode_label(); },
                                  }),
            rightPane, [](Pane& pane) { populate_sound_mode_picker(pane); });
    });
}

}  // namespace dusk::ui
