#ifndef __MSG_STR__
#define __MSG_STR__

// ***************************************************************************************
// ***  SYMBOLS  *************************************************************************
// ***************************************************************************************

#define TEXT_SYMBOL_DOT                    ( 149 )
#define TEXT_SYMBOL_CROSS                  ( 134 )
#define TEXT_SYMBOL_TM                     ( 153 )
#define TEXT_SYMBOL_EURO                   ( 163 )
#define TEXT_SYMBOL_COPY_C                 ( 169 )
#define TEXT_SYMBOL_COPY_R                 ( 174 )
#define TEXT_SYMBOL_POW2                   ( 178 )
#define TEXT_SYMBOL_POW3                   ( 179 )
#define TEXT_SYMBOL_PI                     ( 182 )
#define TEXT_SYMBOL_POW0                   ( 186 )
#define TEXT_SYMBOL_1ON4                   ( 188 )
#define TEXT_SYMBOL_1ON2                   ( 189 )
#define TEXT_SYMBOL_3ON4                   ( 190 )
#define TEXT_SYMBOL_UP                     ( 24 )
#define TEXT_SYMBOL_DOWN                   ( 25 )

// ***************************************************************************************
// ***  FOGAME.MSG  **********************************************************************
// ***************************************************************************************

#define STR_VERSION_INFO                   ( 1 )

// Months, days
#define STR_MONTH( m )            ( 20 + ( m ) )
#define STR_DAY_OF_WEEK( dow )    ( 32 + ( dow ) )

// Misc
#define STR_DOT                            ( 50 )
#define STR_COMMA                          ( 52 )
#define STR_SLASH                          ( 53 )
#define STR_AND                            ( 54 )
#define STR_OR                             ( 55 )

// Music/Video
#define STR_MUSIC_MAIN_THEME               ( 80 )
#define STR_VIDEO_INTRO_BEGIN              ( 90 )
#define STR_VIDEO_INTRO_END                ( 99 )

// Chat
#define STR_CRNORM                         ( 100 )
#define STR_CRSHOUT                        ( 102 )
#define STR_CREMOTE                        ( 104 )
#define STR_CRWHISP                        ( 106 )
#define STR_CRSOCIAL                       ( 108 )
#define STR_MBNORM                         ( 120 )
#define STR_MBSHOUT                        ( 122 )
#define STR_MBEMOTE                        ( 124 )
#define STR_MBWHISP                        ( 126 )
#define STR_MBSOCIAL                       ( 128 )
#define STR_MBRADIO                        ( 130 )
#define STR_MBNET                          ( 132 )

// FixBoy
#define STR_FIX_PARAMS                     ( 200 )
#define STR_FIX_TOOLS                      ( 201 )
#define STR_FIX_ITEMS                      ( 202 )
#define STR_FIX_PIECES                     ( 205 )
#define STR_FIX_YOUHAVE                    ( 206 )
#define STR_FIX_SUCCESS                    ( 210 )
#define STR_FIX_FAIL                       ( 211 )
#define STR_FIX_TIMEOUT                    ( 212 )

// Interface
#define STR_OPTIONS_NOT_AVIABLE            ( 250 )
#define STR_SCREENSHOT_SAVED               ( 251 )
#define STR_SCREENSHOT_NOT_SAVED           ( 252 )
#define STR_LOG_SAVED                      ( 253 )
#define STR_LOG_NOT_SAVED                  ( 254 )
#define STR_ZOOM                           ( 260 )
#define STR_GAME_PAUSED                    ( 265 )
#define STR_DIALOG_BARTER                  ( 275 )
#define STR_DIALOG_SAY                     ( 276 )
#define STR_DIALOG_LOG                     ( 277 )
#define STR_BARTER_OFFER                   ( 280 )
#define STR_BARTER_TALK                    ( 281 )
#define STR_BARTER_END                     ( 282 )
#define STR_MENUOPT_SAVEGAME               ( 290 )
#define STR_MENUOPT_LOADGAME               ( 291 )
#define STR_MENUOPT_OPTIONS                ( 292 )
#define STR_MENUOPT_EXIT                   ( 293 )
#define STR_MENUOPT_RESUME                 ( 294 )
#define STR_LOGIN_NEWGAME                  ( 300 )
#define STR_LOGIN_LOADGAME                 ( 301 )
#define STR_LOGIN_PLAY                     ( 302 )
#define STR_LOGIN_REGISTRATION             ( 303 )
#define STR_LOGIN_OPTIONS                  ( 304 )
#define STR_LOGIN_CREDITS                  ( 305 )
#define STR_LOGIN_EXIT                     ( 306 )
#define STR_DIALOGBOX_CANCEL               ( 310 )
#define STR_DIALOGBOX_OK                   ( 311 )
#define STR_DIALOGBOX_FOLLOW               ( 312 )
#define STR_DIALOGBOX_BARTER_OPEN          ( 313 )
#define STR_DIALOGBOX_BARTER_HIDE          ( 314 )
#define STR_DIALOGBOX_ENCOUNTER_RT         ( 315 )
#define STR_DIALOGBOX_ENCOUNTER_TB         ( 316 )
#define STR_CHA_NAME_NAME                  ( 320 )
#define STR_CHA_NAME_PASS                  ( 321 )
#define STR_CHA_OK                         ( 325 )
#define STR_CHA_CANCEL                     ( 326 )
#define STR_CHA_PRINT                      ( 327 )
#define STR_CHA_SKILLS                     ( 328 )
#define STR_CHA_UNSPENT_SP                 ( 329 )
#define STR_REG_UNSPENT_TAGS               ( 335 )
#define STR_REG_SPECIAL_SUM                ( 336 )
#define STR_PERK_TAKE                      ( 340 )
#define STR_PERK_CANCEL                    ( 341 )
#define STR_GMAP_LOCKED                    ( 350 )
#define STR_GMAP_CUR_INFO                  ( 351 )
#define STR_GMAP_CUR_LOC_INFO              ( 352 )
#define STR_GMAP_LOC_INFO                  ( 353 )
#define STR_FINDPATH_AIMBLOCK              ( 360 )
#define STR_SPLIT_TITLE                    ( 365 )
#define STR_SPLIT_ALL                      ( 366 )
#define STR_TIMER_TITLE                    ( 370 )
#define STR_SAY_OK                         ( 375 )
#define STR_SAY_CANCEL                     ( 376 )
#define STR_SAY_TITLE                      ( 377 )
#define STR_INPUT_BOX_WRITE                ( 380 )
#define STR_INPUT_BOX_BACK                 ( 381 )
#define STR_TOWN_VIEW_BACK                 ( 385 )
#define STR_TOWN_VIEW_ENTER                ( 386 )
#define STR_TOWN_VIEW_CONTOURS             ( 387 )
#define STR_SAVE_LOAD_SAVE                 ( 390 )
#define STR_SAVE_LOAD_LOAD                 ( 391 )
#define STR_SAVE_LOAD_DONE                 ( 392 )
#define STR_SAVE_LOAD_BACK                 ( 393 )
#define STR_SAVE_LOAD_NEW_RECORD           ( 394 )
#define STR_SAVE_LOAD_TYPE_RECORD_NAME     ( 395 )

// Inventory
#define STR_INV_SHORT_SPECIAL_( num )    ( 400 + ( num ) )
#define STR_INV_SHORT_SPECIAL              # (num)    ( 400 + ( num ) )
#define STR_INV_HP                         ( 407 )
#define STR_INV_AC                         ( 408 )
#define STR_INV_NORMAL                     ( 409 )
#define STR_INV_LASER                      ( 410 )
#define STR_INV_FIRE                       ( 411 )
#define STR_INV_PLASMA                     ( 412 )
#define STR_INV_EXPLODE                    ( 413 )
#define STR_INV_NO_ITEM                    ( 414 )
#define STR_INV_DMG                        ( 415 )
#define STR_INV_DIST                       ( 416 )
#define STR_INV_AMMO                       ( 417 )
#define STR_INV_TOTAL_WEIGHT               ( 420 )
#define STR_INV_UNARMED_DMG                ( 424 )
#define STR_OVERWEIGHT                     ( 425 )
#define STR_OVERVOLUME                     ( 426 )
#define STR_INV_HAS_SHOTS                  ( 434 )
#define STR_ITEM_WEIGHT_GRAMM              ( 435 )
#define STR_ITEM_WEIGHT_FUNT               ( 436 )
#define STR_ITEM_COST                      ( 437 )
#define STR_ITEM_TRADER_COST               ( 438 )

#define STR_OVERWEIGHT_TITLE               ( 440 )
#define STR_OVERVOLUME_TITLE               ( 448 )

// Radio
#define STR_RADIO_CANT_SEND                ( 479 )

// Barter
#define STR_BARTER_GOOD_OFFER              ( 480 )
#define STR_BARTER_BAD_OFFER               ( 481 )
#define STR_BARTER_OVERWEIGHT              ( 482 )
#define STR_BARTER_OVERSIZE                ( 483 )
#define STR_BARTER_SALE_ITEM_NOT_FOUND     ( 484 )
#define STR_BARTER_BUY_ITEM_NOT_FOUND      ( 485 )
#define STR_BARTER_NO_BARTER_NOW           ( 486 )
#define STR_BARTER_NO_BARTER_MODE          ( 487 )
#define STR_BARTER_DIALOGBOX               ( 490 )
#define STR_BARTER_OPEN_MODE               ( 491 )
#define STR_BARTER_HIDE_MODE               ( 492 )
#define STR_BARTER_BEGIN_FAIL              ( 493 )
#define STR_BARTER_BEGIN                   ( 495 )
#define STR_BARTER_READY_OFFER             ( 496 )

// Items info
// Wear
#define STR_INV_WEAR_PROC                  ( 500 )
#define STR_INV_WEAR_SERVICE               ( 501 )
#define STR_INV_WEAR_SERVICE_EXT           ( 502 )
#define STR_INV_WEAR_BROKEN_LOW            ( 503 )
#define STR_INV_WEAR_BROKEN_NORM           ( 504 )
#define STR_INV_WEAR_BROKEN_HIGH           ( 505 )
#define STR_INV_WEAR_NO_RESC               ( 506 )
#define STR_INV_WEAR_BROKEN_COUNT          ( 507 )
#define STR_INV_WEAR_SERVICE_ALREADY       ( 510 )
#define STR_INV_WEAR_SERVICE_SUCC          ( 511 )
#define STR_INV_WEAR_SERVICE_FAIL          ( 512 )
#define STR_INV_WEAR_REPAIR_SUCC           ( 515 )
#define STR_INV_WEAR_REPAIR_FAIL           ( 516 )
#define STR_INV_WEAR_ARMOR_BROKEN          ( 520 )
#define STR_INV_WEAR_WEAPON_BROKEN         ( 521 )
// Key
#define STR_INV_KEY_NUMBER                 ( 550 )
// Car
#define STR_INV_CAR_NUMBER                 ( 560 )
#define STR_INV_CAR_FUEL                   ( 561 )
#define STR_INV_CAR_WEAR                   ( 562 )

// PipBoy
#define STR_PIP_STATUS                     ( 700 )
#define STR_PIP_REPLICATION_MONEY          ( 701 )
#define STR_PIP_REPLICATION_MONEY_VAL      ( 702 )
#define STR_PIP_REPLICATION_COST           ( 703 )
#define STR_PIP_REPLICATION_COST_VAL       ( 704 )
#define STR_PIP_REPLICATION_COUNT          ( 705 )
#define STR_PIP_REPLICATION_COUNT_VAL      ( 706 )
#define STR_PIP_QUESTS                     ( 720 )
#define STR_PIP_SCORES                     ( 723 )
#define STR_PIP_MAPS                       ( 725 )
#define STR_PIP_INFO                       ( 730 )
// Timeouts
#define STR_PIP_TIMEOUTS                   ( 740 )
#define STR_TIMEOUT_SECONDS                ( 741 )
#define STR_TIMEOUT_MINUTES                ( 742 )
#define STR_TIMEOUT_TRANSFER_WAIT          ( 790 )
#define STR_TIMEOUT_BATTLE_WAIT            ( 791 )
#define STR_TIMEOUT_SNEAK_WAIT             ( 792 )
// Automaps
#define STR_AUTOMAP_LOADING                ( 795 )
#define STR_AUTOMAP_LOADING_ERROR          ( 796 )

// Dialog
#define STR_DIALOG_CANT_TALK_WITH_MOB      ( 800 )
#define STR_DIALOG_NPC_NOT_LIFE            ( 801 )
#define STR_DIALOG_NPC_BUSY                ( 802 )
#define STR_DIALOG_DIST_TOO_LONG           ( 803 )
#define STR_DIALOG_NO_DIALOGS              ( 804 )
#define STR_DIALOG_MANY_TALKERS            ( 805 )
#define STR_DIALOG_PRE_INST_FAIL           ( 806 )
#define STR_DIALOG_FROM_LINK_NOT_FOUND     ( 807 )
#define STR_DIALOG_COMPILE_FAIL            ( 808 )
#define STR_DIALOG_NPC_NOT_FOUND           ( 809 )
#define STR_DIALOG_ANSWER_NULLPTR          ( 810 )

// Global
#define STR_GLOBAL_LOCATION_NOT_FOUND      ( 900 )
#define STR_GLOBAL_LOCATION_REMOVED        ( 901 )
#define STR_GLOBAL_PLACE_NOT_FOUND         ( 902 )
#define STR_GLOBAL_CAR_PLACE_NOT_FOUND     ( 903 )
#define STR_GLOBAL_PLAYERS_OVERFLOW        ( 904 )
#define STR_FOLLOW_PREP                    ( 950 )
#define STR_FOLLOW_FORCE                   ( 952 )
#define STR_FOLLOW_UNKNOWN_CRNAME          ( 970 )
#define STR_FOLLOW_GMNAME                  ( 972 )

// Net
#define STR_NET_WRONG_LOGIN                ( 1001 )
#define STR_NET_WRONG_PASS                 ( 1002 )
#define STR_NET_ACCOUNT_ALREADY            ( 1003 )
#define STR_NET_PLAYER_IN_GAME             ( 1004 )
#define STR_NET_WRONG_SPECIAL              ( 1005 )
#define STR_NET_REG_SUCCESS                ( 1006 )
#define STR_NET_CONNECTION                 ( 1007 )
#define STR_NET_CONN_ERROR                 ( 1008 )
#define STR_NET_LOGINPASS_WRONG            ( 1009 )
#define STR_NET_CONN_SUCCESS               ( 1010 )
#define STR_NET_HEXES_BUSY                 ( 1012 )
#define STR_NET_DISCONN_BY_DEMAND          ( 1013 )
#define STR_NET_WRONG_NAME                 ( 1014 )
#define STR_NET_WRONG_CASES                ( 1015 )
#define STR_NET_WRONG_GENDER               ( 1016 )
#define STR_NET_WRONG_AGE                  ( 1017 )
#define STR_NET_CONN_FAIL                  ( 1018 )
#define STR_NET_WRONG_DATA                 ( 1019 )
#define STR_NET_STARTLOC_FAIL              ( 1020 )
#define STR_NET_STARTMAP_FAIL              ( 1021 )
#define STR_NET_STARTCOORD_FAIL            ( 1022 )
#define STR_NET_BD_ERROR                   ( 1023 )
#define STR_NET_WRONG_NETPROTO             ( 1024 )
#define STR_NET_DATATRANS_ERR              ( 1025 )
#define STR_NET_NETMSG_ERR                 ( 1026 )
#define STR_NET_SETPROTO_ERR               ( 1027 )
#define STR_NET_LOGINOK                    ( 1028 )
#define STR_NET_WRONG_TAGSKILL             ( 1029 )
#define STR_NET_DIFFERENT_LANG             ( 1030 )
#define STR_NET_MANY_SYMBOLS               ( 1031 )
#define STR_NET_BEGIN_END_SPACES           ( 1032 )
#define STR_NET_TWO_SPACE                  ( 1033 )
#define STR_NET_BANNED                     ( 1034 )
#define STR_NET_NAME_WRONG_CHARS           ( 1035 )
#define STR_NET_PASS_WRONG_CHARS           ( 1036 )
#define STR_NET_FAIL_TO_LOAD_IFACE         ( 1037 )
#define STR_NET_FAIL_RUN_START_SCRIPT      ( 1038 )
#define STR_NET_LANGUAGE_NOT_SUPPORTED     ( 1039 )
#define STR_INVALID_RESOLUTION             ( 1040 )
#define STR_NET_KNOCK_KNOCK                ( 1041 )
#define STR_NET_REGISTRATION_IP_WAIT       ( 1042 )
#define STR_NET_BANNED_IP                  ( 1043 )
#define STR_NET_UID_FAIL                   ( 1044 )
#define STR_NET_TIME_LEFT                  ( 1045 )
#define STR_NET_BAN                        ( 1046 )
#define STR_NET_BAN_REASON                 ( 1047 )
#define STR_NET_LOGIN_SCRIPT_FAIL          ( 1048 )
#define STR_NET_PERMANENT_DEATH            ( 1049 )

#define STR_SP_SAVE_SUCCESS                ( 1070 )
#define STR_SP_SAVE_FAIL                   ( 1071 )
#define STR_SP_LOAD_SUCCESS                ( 1072 )
#define STR_SP_LOAD_FAIL                   ( 1073 )
#define STR_SP_NEW_GAME_SUCCESS            ( 1074 )
#define STR_SP_NEW_GAME_FAIL               ( 1075 )

// Parameters
#define STR_PARAM_NAME_( index )            ( 100000 + ( index ) * 10 + 1 )
#define STR_PARAM_NAME                     # (index)         ( 100000 + ( index ) * 10 + 1 )
#define STR_PARAM_DESC_( index )            ( 100000 + ( index ) * 10 + 2 )
#define STR_PARAM_DESC                     # (index)         ( 100000 + ( index ) * 10 + 2 )
#define STR_PARAM_NAME_SHORT_( index )      ( 100000 + ( index ) * 10 + 3 )
#define STR_PARAM_NAME_SHORT               # (index)   ( 100000 + ( index ) * 10 + 3 )

#define STR_STAT_LEVEL_( val )              ( ( val ) > 10 ? ( 2310 ) : ( ( val ) < 1 ? ( 2301 ) : 2300 + ( val ) ) )
#define STR_STAT_LEVEL                     # (val)           ( ( val ) > 10 ? ( 2310 ) : ( ( val ) < 1 ? ( 2301 ) : 2300 + ( val ) ) )
#define STR_STAT_LEVEL_ABB_( val )          ( ( val ) > 10 ? ( 2320 ) : ( ( val ) < 1 ? ( 2311 ) : 2310 + ( val ) ) )
#define STR_STAT_LEVEL_ABB                 # (val)       ( ( val ) > 10 ? ( 2320 ) : ( ( val ) < 1 ? ( 2311 ) : 2310 + ( val ) ) )

#define STR_NEXT_LEVEL_NAME                ( 4001 )
#define STR_UNSPENT_SKILL_POINTS_NAME      ( 4002 )
#define STR_LEVEL_NAME                     ( 4003 )
#define STR_EXPERIENCE_NAME                ( 4004 )
#define STR_REPUTATION_NAME                ( 4005 )
#define STR_KARMA_NAME                     ( 4006 )
#define STR_PERKS_NAME                     ( 4007 )
#define STR_KILLS_NAME                     ( 4008 )
#define STR_TRAITS_NAME                    ( 4011 )
#define STR_MALE_NAME                      ( 4107 )
#define STR_FEMALE_NAME                    ( 4108 )
#define STR_NEXT_LEVEL_DESC                ( 4051 )
#define STR_UNSPENT_SKILL_POINTS_DESC      ( 4052 )
#define STR_LEVEL_DESC                     ( 4053 )
#define STR_EXPERIENCE_DESC                ( 4054 )
#define STR_REPUTATION_DESC                ( 4055 )
#define STR_KARMA_DESC                     ( 4056 )
#define STR_PERKS_DESC                     ( 4057 )
#define STR_KILLS_DESC                     ( 4058 )
#define STR_TRAITS_DESC                    ( 4061 )

#define STR_SWITCH_PERKS_NAME              ( 4109 )
#define STR_SWITCH_KARMA_NAME              ( 4110 )
#define STR_SWITCH_KILLS_NAME              ( 4111 )

// Skills
#define STR_SKILL_NORESSURECT              ( 3400 )
#define STR_SKILL_WEARINESS                ( 3401 )
#define STR_SKILL_NONEED_FIRSTAID          ( 3402 )
#define STR_SKILL_NONEED_DOCTOR            ( 3403 )
#define STR_SKILL_NOFIRSTAID_NEEDDOCTOR    ( 3404 )
#define STR_SKILL_NODOCTOR_NEEDFIRSTAID    ( 3405 )
#define STR_SKILL_HEAL_DMG_( dmg )          ( 3410 + ( dmg ) )
#define STR_SKILL_HEAL_DMG                 # (dmg)       ( 3410 + ( dmg ) )
#define STR_SKILL_NOHEAL_DMG_( dmg )        ( 3420 + ( dmg ) )
#define STR_SKILL_NOHEAL_DMG               # (dmg)     ( 3420 + ( dmg ) )
#define STR_SKILL_STEAL_TRIED_GET          ( 3430 )
#define STR_SKILL_STEAL_TRIED_PUT          ( 3431 )
#define STR_SKILL_LOCKPICK_FAIL            ( 3440 )
#define STR_SKILL_TRAPS_SET_FAIL           ( 3441 )
#define STR_SKILL_TRAPS_SET_SUCC           ( 3442 )
#define STR_SKILL_TRAPS_FAIL               ( 3443 )
#define STR_SKILL_TRAPS_SUCC               ( 3444 )
#define STR_SKILL_SNEAK_VISIBLE            ( 3445 )

// Skilldex
#define STR_SKILLDEX_NAME                  ( 3340 )
#define STR_SKILLDEX_CANCEL                ( 3341 )

// Character
#define STR_CHA_LEVEL                      ( 4113 )
#define STR_CHA_EXPERIENCE                 ( 4114 )
#define STR_CHA_NEXT_LEVEL                 ( 4115 )

// Damage
#define STR_DMG_LIFE                       ( 4250 )

// Karma, reputation
#define STR_KARMA_GEN_GEN_NAME             ( 6000 )
#define STR_KARMA_GEN_GEN_NAME2            ( 6001 )
#define STR_KARMA_GEN_GEN_DESC             ( 6002 )
#define STR_KARMA_GEN_COUNT                ( 6099 )
#define STR_KARMA_GEN_VAL_( num )           ( 6100 + ( num ) * 3 )
#define STR_KARMA_GEN_NAME_( num )          ( 6100 + ( num ) * 3 + 1 )
#define STR_KARMA_GEN_SKILLDEX_( num )      ( 6100 + ( num ) * 3 + 2 )

#define STR_TOWNREP_TITLE_NAME             ( 6405 )
#define STR_TOWNREP_TITLE_DESC             ( 6406 )
#define STR_TOWNREP_RATIO_NAME_( val )      ( 6500 + ( ( val ) >= GameOpt.ReputationLoved ? 0 : ( ( val ) >= GameOpt.ReputationLiked ? 1 : ( ( val ) >= GameOpt.ReputationAccepted ? 2 : ( ( val ) >= GameOpt.ReputationNeutral ? 3 : ( ( val ) >= GameOpt.ReputationAntipathy ? 4 : ( ( val ) >= GameOpt.ReputationHated ? 5 : 6 ) ) ) ) ) ) )
#define STR_TOWNREP_RATIO_DESC_( val )      ( 6550 + ( ( val ) >= GameOpt.ReputationLoved ? 0 : ( ( val ) >= GameOpt.ReputationLiked ? 1 : ( ( val ) >= GameOpt.ReputationAccepted ? 2 : ( ( val ) >= GameOpt.ReputationNeutral ? 3 : ( ( val ) >= GameOpt.ReputationAntipathy ? 4 : ( ( val ) >= GameOpt.ReputationHated ? 5 : 6 ) ) ) ) ) ) )

#define STR_ADDICT_TITLE_NAME              ( 6900 )
#define STR_ADDICT_TITLE_DESC              ( 6901 )

// Kills
#define STR_KILL_NAME_( num )               ( 8000 + ( num ) )
#define STR_KILL_DESC_( num )               ( 8100 + ( num ) )

// Items
#define STR_ITEM_LOOK                      ( 10000 )
#define STR_ITEM_LOOK_INFO                 ( 10001 )
#define STR_ITEM_LOOK_NOTHING              ( 10010 )
#define STR_ITEM_NOT_FOUND                 ( 10020 )
#define STR_ITEM_LOCKER_NEED_KEY           ( 10100 )
#define STR_USE_NOTHING                    ( 10202 )
#define STR_SOLAR_SCORCHER_NO_LIGHT        ( 10240 )
#define STR_CAR_FUEL_TANK_FILL             ( 10250 )
#define STR_CAR_FUEL_TANK_FULL             ( 10251 )
#define STR_CAR_REPAIR_SUCCESS             ( 10252 )
#define STR_CAR_REPAIR_NO_NEED             ( 10253 )
#define STR_CAR_FUEL_NOT_SUPPORTED         ( 10254 )
#define STR_CAR_FUEL_EMPTY                 ( 10255 )
#define STR_CAR_BROKEN                     ( 10256 )
#define STR_CAR_CRIT_OVERLOAD              ( 10257 )
#define STR_CRIT_BAD_CHARISMA              ( 10258 )
// Drugs
#define STR_DRUG_STAT_GAIN                 ( 10300 )
#define STR_DRUG_STAT_LOSE                 ( 10301 )
#define STR_DRUG_ADDICTION_END             ( 10302 )
#define STR_DRUG_DEAD                      ( 10303 )
#define STR_DRUG_NOTHING_HAPPENS           ( 10304 )
#define STR_DRUG_PARISH                    ( 10305 )
#define STR_DRUG_USE_ON_SUCC               ( 10306 )
#define STR_DRUG_USE_ON_FAIL               ( 10307 )
// Books
#define STR_BOOK_READ_SUCCESS              ( 10450 )
#define STR_BOOK_READ_FAIL                 ( 10451 )
#define STR_BOOK_READ_SCIENCE              ( 10452 )
#define STR_BOOK_READ_REPAIR               ( 10453 )
#define STR_BOOK_READ_FISRT_AID            ( 10454 )
#define STR_BOOK_READ_SMALL_GUNS           ( 10455 )
#define STR_BOOK_READ_ENERGY_WEAPONS       ( 10456 )
#define STR_BOOK_READ_OUTDOORSMAN          ( 10457 )
// Grave
#define STR_GRAVE_UNCOVERED                ( 10550 )
#define STR_GRAVE_COVERED                  ( 10551 )

// Caliber
#define STR_CALIBER_( num )                 ( 10900 + ( num ) )
#define STR_CALIBER                        # (num)              ( 10900 + ( num ) )

// Critters
#define STR_CRIT_LOOK1                     # (gen)                   ( 11000 + ( gen ) * 1000 )
#define STR_CRIT_LOOK1_SELF                # (gen)              ( 11001 + ( gen ) * 1000 )
#define STR_CRIT_LOOK2                     # (gen)                   ( 11002 + ( gen ) * 1000 )
#define STR_CRIT_LOOK3                     # (gen)                   ( 11003 + ( gen ) * 1000 )
#define STR_CRIT_LOOK_SELF                 # (gen)               ( 11005 + ( gen ) * 1000 )
#define STR_CRIT_LOOK_LIFE                 ( 11010 )
#define STR_CRIT_LOOK_KO                   ( 11011 )
#define STR_CRIT_LOOK_DEAD                 ( 11012 )
#define STR_CRIT_LOOK_CRITICAL_DEAD        ( 11013 )
#define STR_CRIT_LOOK_NOTHING              ( 11014 )
#define STR_CRIT_LOOK_AGE                  # ( gen, age )( 11100 + ( ( age ) > 99 ? 99 : ( age ) ) + ( gen ) * 1000 )
#define STR_CRIT_LOOK_COND                 # ( gen, main, ext )( 11200 + ( main ) * 10 + ( ext ) + ( gen ) * 1000 )
#define STR_CRIT_LOOK_SPEC                 # ( gen, num, val )( 11300 + ( num ) * 10 + ( ( val ) > 10 ? 10 : ( val ) ) + ( gen ) * 1000 )
#define STR_CRIT_LOOK_DMG                  # ( gen, dmg )( 11400 + ( dmg ) + ( gen ) * 1000 )
#define STR_CRIT_LOOK_PERK                 # ( gen, perk )( 11500 + ( perk ) + ( gen ) * 1000 )
// Fallout system
#define STR_CRIT_LOOK_WHO                  # (gender)             ( 12500 + ( gender ) )
#define STR_CRIT_LOOK_STATE                # (num)              ( 12510 + ( num ) )
#define STR_CRIT_LOOK_LIMBS                # ( gender, unhurt )( 12520 + ( gender ) * 2 + ( unhurt ? 1 : 0 ) )
#define STR_CRIT_LOOK_HP                   # (gender)              ( 12530 + ( gender ) )
#define STR_CRIT_LOOK_WEAP                 ( 12540 )
#define STR_CRIT_LOOK_WEAP_AMMO            ( 12541 )
#define STR_CRIT_LOOK_MISC                 ( 12542 )

#define STR_CRITTER_CANT_MOVE              ( 12600 )

// Scores
#define STR_SCORES_TITLE_( score )          ( 13000 + ( score ) * 10 )
#define STR_SCORES_NAME_( score )           ( 13000 + ( score ) * 10 + 1 )

// Intellect words
#define STR_INTELLECT_WORDS                ( 1999000000 )
#define STR_INTELLECT_SYMBOLS              ( 1999000001 )

// Help
#define STR_GAME_HELP                      ( 2000000000 )

// Credits
#define STR_GAME_CREDITS_SPEED             ( 2000001000 )
#define STR_GAME_CREDITS                   ( 2000001010 )
#define STR_GAME_CREDITS_EXT               ( 2000001020 )

// ***************************************************************************************
// ***  FOOBJ.MSG  ***********************************************************************
// ***************************************************************************************

#define STR_ITEM_INFO                      # (item)           ( item.GetProtoId() * 100 + item.Info )

// ***************************************************************************************
// ***  FOGM.MSG  ************************************************************************
// ***************************************************************************************

// Local map info
#define STR_MAP_NAME_( pid )                ( ( pid + 1 ) * 10 + 0 )
#define STR_MAP_MUSIC_( pid )               ( ( pid + 1 ) * 10 + 5 )
#define STR_MAP_AMBIENT_( pid )             ( ( pid + 1 ) * 10 + 6 )

// Global map info
#define STR_GM_NAME_( pid )                 ( ( pid + 100 ) * 1000 + 0 )
#define STR_GM_INFO_( pid )                 ( ( pid + 100 ) * 1000 + 5 )
#define STR_GM_PIC_( pid )                  ( ( pid + 100 ) * 1000 + 20 )
#define STR_GM_LABELPIC_( pid )             ( ( pid + 100 ) * 1000 + 30 )

#define STR_GM_ENTRANCE_COUNT_( pid )       ( ( pid + 100 ) * 1000 + 90 )
#define STR_GM_ENTRANCE_NAME_( pid, en )    ( ( pid + 100 ) * 1000 + 100 + ( ( en ) * 10 + 1 ) )
#define STR_GM_ENTRANCE_PICX_( pid, en )    ( ( pid + 100 ) * 1000 + 100 + ( ( en ) * 10 + 2 ) )
#define STR_GM_ENTRANCE_PICY_( pid, en )    ( ( pid + 100 ) * 1000 + 100 + ( ( en ) * 10 + 3 ) )

// ***************************************************************************************
// ***  FODLG.MSG  ***********************************************************************
// ***************************************************************************************

#define STR_NPC_PROTO_NAME_( pid )          ( ( pid ) * 10 )
#define STR_NPC_PROTO_NAME                 # (pid)                          ( ( pid ) * 10 )
#define STR_NPC_PROTO_DESC                 # (pid)                          ( ( pid ) * 10 + 1 )
#define STR_NPC_NAME_( num, pid )           ( ( num ) != 0 ? 100000 + ( num ) * 1000 + 100 : STR_NPC_PROTO_NAME_( pid ) )
#define STR_NPC_NAME                       # ( num, pid )( ( num ) != 0 ? 100000 + ( num ) * 1000 + 100 : STR_NPC_PROTO_NAME( pid ) )
#define STR_NPC_AVATAR_( num )              ( ( num ) != 0 ? 100000 + ( num ) * 1000 + 110 : 0 )
#define STR_NPC_AVATAR                     # (num)                              ( ( num ) != 0 ? 100000 + ( num ) * 1000 + 110 : 0 )
#define STR_NPC_INFO_LIFE                  # ( num, pid )( ( num ) != 0 ? 100000 + ( num ) * 1000 + 200 : STR_NPC_PROTO_DESC( pid ) )
#define STR_NPC_FULL_INFO_LIFE             # ( num, pid )( ( num ) != 0 ? 100000 + ( num ) * 1000 + 210 : STR_NPC_PROTO_DESC( pid ) )
#define STR_NPC_INFO_KO                    # ( num, pid )( ( num ) != 0 ? 100000 + ( num ) * 1000 + 220 : STR_NPC_PROTO_DESC( pid ) )
#define STR_NPC_FULL_INFO_KO               # ( num, pid )( ( num ) != 0 ? 100000 + ( num ) * 1000 + 230 : STR_NPC_PROTO_DESC( pid ) )
#define STR_NPC_INFO_DEAD                  # ( num, pid )( ( num ) != 0 ? 100000 + ( num ) * 1000 + 240 : STR_NPC_PROTO_DESC( pid ) )
#define STR_NPC_FULL_INFO_DEAD             # ( num, pid )( ( num ) != 0 ? 100000 + ( num ) * 1000 + 250 : STR_NPC_PROTO_DESC( pid ) )
#define STR_NPC_INFO_CRITICAL_DEAD         # ( num, pid )( ( num ) != 0 ? 100000 + ( num ) * 1000 + 260 : STR_NPC_PROTO_DESC( pid ) )
#define STR_NPC_FULL_INFO_CRITICAL_DEAD    # ( num, pid )( ( num ) != 0 ? 100000 + ( num ) * 1000 + 270 : STR_NPC_PROTO_DESC( pid ) )
// #define STR_DLGSTR(dialog_id,str_num)   ()

// ***************************************************************************************
// ***  FOQUEST.MSG  *********************************************************************
// ***************************************************************************************

#define STR_QUEST_NUMBER                   ( 4 )
#define STR_QUEST_PROCESS                  ( 5 )
#define STR_QUEST_MAP_( num )               ( ( num ) * 1000 + 101 )
#define STR_QUEST_INFO_( num )              ( ( num ) * 1000 + 102 )

// ***************************************************************************************
// ***  FOHOLO.MSG, FOUSERHOLO.MSG  ******************************************************
// ***************************************************************************************

#define STR_HOLO_READ_SUCC                 ( 1 )
#define STR_HOLO_READ_FAIL                 ( 2 )
#define STR_HOLO_READ_ALREADY              ( 3 )
#define STR_HOLO_READ_MEMORY_FULL          ( 4 )
#define STR_HOLO_ERASE_SUCC                ( 5 )
#define STR_HOLO_ERASE_FAIL                ( 6 )
#define STR_HOLO_WRITE_SUCC                ( 7 )
#define STR_HOLO_WRITE_FAIL                ( 8 )
#define STR_HOLO_INFO_NAME_( num )          ( ( num ) * 10 )
#define STR_HOLO_INFO_NAME                 # (num)       ( ( num ) * 10 )
#define STR_HOLO_INFO_DESC_( num )          ( ( num ) * 10 + 1 )
#define STR_HOLO_INFO_DESC                 # (num)       ( ( num ) * 10 + 1 )

// ***************************************************************************************
// ***  FOCOMBAT.MSG  ********************************************************************
// ***************************************************************************************

#define STR_COMBAT_NEED_AP                 ( 100 )
#define STR_COMBAT_NEED_OUT_OF_AMMO        ( 101 )
#define STR_COMBAT_NEED_OUT_OF_RANGE       ( 102 )
#define STR_COMBAT_NEED_CANNOT_END         ( 103 )
#define STR_COMBAT_NEED_AIM_BLOCKED        ( 104 )
#define STR_COMBAT_NEED_DMG_TWO_ARMS       ( 105 )
#define STR_COMBAT_NEED_DMG_ARM            ( 106 )
#define STR_COMBAT_NEED_LOW_STRENGTH       ( 107 )

// ***************************************************************************************
// ***  FOINTERNAL.MSG  ******************************************************************
// ***************************************************************************************

#define STR_INTERNAL_SCRIPT_MODULES        ( 100 )
#define STR_INTERNAL_SCRIPT_DLLS           ( 50000 )
#define STR_INTERNAL_SCRIPT_PRAGMAS        ( 90000 )
#define STR_INTERNAL_SCRIPT_VERSION        ( 99998 )
#define STR_INTERNAL_SCRIPT_CONFIG         ( 99999 )
#define STR_INTERNAL_CRTYPE( num )          ( 100000 + ( num ) )

// ***************************************************************************************
// ***************************************************************************************
// ***************************************************************************************

#endif // __MSG_STR__
