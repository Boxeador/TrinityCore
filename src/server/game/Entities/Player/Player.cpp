/*
 * Copyright (C) 2008-2012 TrinityCore <http://www.trinitycore.org/>
 * Copyright (C) 2005-2009 MaNGOS <http://getmangos.com/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "Common.h"
#include "Language.h"
#include "DatabaseEnv.h"
#include "Log.h"
#include "Opcodes.h"
#include "SpellMgr.h"
#include "World.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "UpdateMask.h"
#include "Player.h"
#include "Vehicle.h"
#include "SkillDiscovery.h"
#include "QuestDef.h"
#include "GossipDef.h"
#include "UpdateData.h"
#include "Channel.h"
#include "ChannelMgr.h"
#include "MapManager.h"
#include "MapInstanced.h"
#include "InstanceSaveMgr.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "CellImpl.h"
#include "ObjectMgr.h"
#include "ArenaTeamMgr.h"
#include "GuildMgr.h"
#include "GroupMgr.h"
#include "ObjectAccessor.h"
#include "CreatureAI.h"
#include "Formulas.h"
#include "Group.h"
#include "Guild.h"
#include "Pet.h"
#include "Util.h"
#include "Transport.h"
#include "Weather.h"
#include "Battleground.h"
#include "BattlegroundAV.h"
#include "BattlegroundMgr.h"
#include "OutdoorPvP.h"
#include "OutdoorPvPMgr.h"
#include "ArenaTeam.h"
#include "Chat.h"
#include "Spell.h"
#include "SocialMgr.h"
#include "GameEventMgr.h"
#include "AchievementMgr.h"
#include "SpellAuras.h"
#include "SpellAuraEffects.h"
#include "ConditionMgr.h"
#include "DisableMgr.h"
#include "WeatherMgr.h"
#include "LFGMgr.h"
#include "CharacterDatabaseCleaner.h"
#include "InstanceScript.h"
#include <cmath>
#include "AccountMgr.h"
#include "Battlefield.h"
#include "BattlefieldMgr.h"
#include "BattlefieldWG.h"

#define ZONE_UPDATE_INTERVAL (1*IN_MILLISECONDS)

#define PLAYER_SKILL_INDEX(x)       (PLAYER_SKILL_INFO_1_1 + ((x)*3))
#define PLAYER_SKILL_VALUE_INDEX(x) (PLAYER_SKILL_INDEX(x)+1)
#define PLAYER_SKILL_BONUS_INDEX(x) (PLAYER_SKILL_INDEX(x)+2)

#define SKILL_VALUE(x)         PAIR32_LOPART(x)
#define SKILL_MAX(x)           PAIR32_HIPART(x)
#define MAKE_SKILL_VALUE(v, m) MAKE_PAIR32(v, m)

#define SKILL_TEMP_BONUS(x)    int16(PAIR32_LOPART(x))
#define SKILL_PERM_BONUS(x)    int16(PAIR32_HIPART(x))
#define MAKE_SKILL_BONUS(t, p) MAKE_PAIR32(t, p)

enum CharacterFlags
{
    CHARACTER_FLAG_NONE                 = 0x00000000,
    CHARACTER_FLAG_UNK1                 = 0x00000001,
    CHARACTER_FLAG_UNK2                 = 0x00000002,
    CHARACTER_LOCKED_FOR_TRANSFER       = 0x00000004,
    CHARACTER_FLAG_UNK4                 = 0x00000008,
    CHARACTER_FLAG_UNK5                 = 0x00000010,
    CHARACTER_FLAG_UNK6                 = 0x00000020,
    CHARACTER_FLAG_UNK7                 = 0x00000040,
    CHARACTER_FLAG_UNK8                 = 0x00000080,
    CHARACTER_FLAG_UNK9                 = 0x00000100,
    CHARACTER_FLAG_UNK10                = 0x00000200,
    CHARACTER_FLAG_HIDE_HELM            = 0x00000400,
    CHARACTER_FLAG_HIDE_CLOAK           = 0x00000800,
    CHARACTER_FLAG_UNK13                = 0x00001000,
    CHARACTER_FLAG_GHOST                = 0x00002000,
    CHARACTER_FLAG_RENAME               = 0x00004000,
    CHARACTER_FLAG_UNK16                = 0x00008000,
    CHARACTER_FLAG_UNK17                = 0x00010000,
    CHARACTER_FLAG_UNK18                = 0x00020000,
    CHARACTER_FLAG_UNK19                = 0x00040000,
    CHARACTER_FLAG_UNK20                = 0x00080000,
    CHARACTER_FLAG_UNK21                = 0x00100000,
    CHARACTER_FLAG_UNK22                = 0x00200000,
    CHARACTER_FLAG_UNK23                = 0x00400000,
    CHARACTER_FLAG_UNK24                = 0x00800000,
    CHARACTER_FLAG_LOCKED_BY_BILLING    = 0x01000000,
    CHARACTER_FLAG_DECLINED             = 0x02000000,
    CHARACTER_FLAG_UNK27                = 0x04000000,
    CHARACTER_FLAG_UNK28                = 0x08000000,
    CHARACTER_FLAG_UNK29                = 0x10000000,
    CHARACTER_FLAG_UNK30                = 0x20000000,
    CHARACTER_FLAG_UNK31                = 0x40000000,
    CHARACTER_FLAG_UNK32                = 0x80000000
};

enum CharacterCustomizeFlags
{
    CHAR_CUSTOMIZE_FLAG_NONE            = 0x00000000,
    CHAR_CUSTOMIZE_FLAG_CUSTOMIZE       = 0x00000001,       // name, gender, etc...
    CHAR_CUSTOMIZE_FLAG_FACTION         = 0x00010000,       // name, gender, faction, etc...
    CHAR_CUSTOMIZE_FLAG_RACE            = 0x00100000        // name, gender, race, etc...
};

// corpse reclaim times
#define DEATH_EXPIRE_STEP (5*MINUTE)
#define MAX_DEATH_COUNT 3

static uint32 copseReclaimDelay[MAX_DEATH_COUNT] = { 30, 60, 120 };

// == PlayerTaxi ================================================

PlayerTaxi::PlayerTaxi()
{
    memset(m_taximask, 0, sizeof(m_taximask));
}

void PlayerTaxi::InitTaxiNodesForLevel(uint32 race, uint32 chrClass, uint8 level)
{
    // class specific initial known nodes
    switch (chrClass)
    {
        case CLASS_DEATH_KNIGHT:
        {
            for (uint8 i = 0; i < TaxiMaskSize; ++i)
                m_taximask[i] |= sOldContinentsNodesMask[i];
            break;
        }
    }

    // race specific initial known nodes: capital and taxi hub masks
    switch (race)
    {
        case RACE_HUMAN:    SetTaximaskNode(2);  break;     // Human
        case RACE_ORC:      SetTaximaskNode(23); break;     // Orc
        case RACE_DWARF:    SetTaximaskNode(6);  break;     // Dwarf
        case RACE_NIGHTELF: SetTaximaskNode(26);
                            SetTaximaskNode(27); break;     // Night Elf
        case RACE_UNDEAD_PLAYER: SetTaximaskNode(11); break;// Undead
        case RACE_TAUREN:   SetTaximaskNode(22); break;     // Tauren
        case RACE_GNOME:    SetTaximaskNode(6);  break;     // Gnome
        case RACE_TROLL:    SetTaximaskNode(23); break;     // Troll
        case RACE_BLOODELF: SetTaximaskNode(82); break;     // Blood Elf
        case RACE_DRAENEI:  SetTaximaskNode(94); break;     // Draenei
    }

    // new continent starting masks (It will be accessible only at new map)
    switch (Player::TeamForRace(race))
    {
        case ALLIANCE: SetTaximaskNode(100); break;
        case HORDE:    SetTaximaskNode(99);  break;
    }
    // level dependent taxi hubs
    if (level >= 68)
        SetTaximaskNode(213);                               //Shattered Sun Staging Area
}

void PlayerTaxi::LoadTaxiMask(const char* data)
{
    Tokenizer tokens(data, ' ');

    uint8 index;
    Tokenizer::const_iterator iter;
    for (iter = tokens.begin(), index = 0;
        (index < TaxiMaskSize) && (iter != tokens.end()); ++iter, ++index)
    {
        // load and set bits only for existed taxi nodes
        m_taximask[index] = sTaxiNodesMask[index] & uint32(atol(*iter));
    }
}

void PlayerTaxi::AppendTaximaskTo(ByteBuffer& data, bool all)
{
    if (all)
    {
        for (uint8 i=0; i<TaxiMaskSize; i++)
            data << uint32(sTaxiNodesMask[i]);              // all existed nodes
    }
    else
    {
        for (uint8 i=0; i<TaxiMaskSize; i++)
            data << uint32(m_taximask[i]);                  // known nodes
    }
}

bool PlayerTaxi::LoadTaxiDestinationsFromString(const std::string& values, uint32 team)
{
    ClearTaxiDestinations();

    Tokenizer tokens(values, ' ');

    for (Tokenizer::const_iterator iter = tokens.begin(); iter != tokens.end(); ++iter)
    {
        uint32 node = uint32(atol(*iter));
        AddTaxiDestination(node);
    }

    if (m_TaxiDestinations.empty())
        return true;

    // Check integrity
    if (m_TaxiDestinations.size() < 2)
        return false;

    for (size_t i = 1; i < m_TaxiDestinations.size(); ++i)
    {
        uint32 cost;
        uint32 path;
        sObjectMgr->GetTaxiPath(m_TaxiDestinations[i-1], m_TaxiDestinations[i], path, cost);
        if (!path)
            return false;
    }

    // can't load taxi path without mount set (quest taxi path?)
    if (!sObjectMgr->GetTaxiMountDisplayId(GetTaxiSource(), team, true))
        return false;

    return true;
}

std::string PlayerTaxi::SaveTaxiDestinationsToString()
{
    if (m_TaxiDestinations.empty())
        return "";

    std::ostringstream ss;

    for (size_t i=0; i < m_TaxiDestinations.size(); ++i)
        ss << m_TaxiDestinations[i] << ' ';

    return ss.str();
}

uint32 PlayerTaxi::GetCurrentTaxiPath() const
{
    if (m_TaxiDestinations.size() < 2)
        return 0;

    uint32 path;
    uint32 cost;

    sObjectMgr->GetTaxiPath(m_TaxiDestinations[0], m_TaxiDestinations[1], path, cost);

    return path;
}

std::ostringstream& operator<< (std::ostringstream& ss, PlayerTaxi const& taxi)
{
    for (uint8 i = 0; i < TaxiMaskSize; ++i)
        ss << taxi.m_taximask[i] << ' ';
    return ss;
}

//== TradeData =================================================

TradeData* TradeData::GetTraderData() const
{
    return m_trader->GetTradeData();
}

Item* TradeData::GetItem(TradeSlots slot) const
{
    return m_items[slot] ? m_player->GetItemByGuid(m_items[slot]) : NULL;
}

bool TradeData::HasItem(uint64 itemGuid) const
{
    for (uint8 i = 0; i < TRADE_SLOT_COUNT; ++i)
        if (m_items[i] == itemGuid)
            return true;

    return false;
}

TradeSlots TradeData::GetTradeSlotForItem(uint64 itemGuid) const
{
    for (uint8 i = 0; i < TRADE_SLOT_COUNT; ++i)
        if (m_items[i] == itemGuid)
            return TradeSlots(i);

    return TRADE_SLOT_INVALID;
}

Item* TradeData::GetSpellCastItem() const
{
    return m_spellCastItem ? m_player->GetItemByGuid(m_spellCastItem) : NULL;
}

void TradeData::SetItem(TradeSlots slot, Item* item)
{
    uint64 itemGuid = item ? item->GetGUID() : 0;

    if (m_items[slot] == itemGuid)
        return;

    m_items[slot] = itemGuid;

    SetAccepted(false);
    GetTraderData()->SetAccepted(false);

    Update();

    // need remove possible trader spell applied to changed item
    if (slot == TRADE_SLOT_NONTRADED)
        GetTraderData()->SetSpell(0);

    // need remove possible player spell applied (possible move reagent)
    SetSpell(0);
}

void TradeData::SetSpell(uint32 spell_id, Item* castItem /*= NULL*/)
{
    uint64 itemGuid = castItem ? castItem->GetGUID() : 0;

    if (m_spell == spell_id && m_spellCastItem == itemGuid)
        return;

    m_spell = spell_id;
    m_spellCastItem = itemGuid;

    SetAccepted(false);
    GetTraderData()->SetAccepted(false);

    Update(true);                                           // send spell info to item owner
    Update(false);                                          // send spell info to caster self
}

void TradeData::SetMoney(uint32 money)
{
    if (m_money == money)
        return;

    m_money = money;

    SetAccepted(false);
    GetTraderData()->SetAccepted(false);

    Update(true);
}

void TradeData::Update(bool forTarget /*= true*/)
{
    if (forTarget)
        m_trader->GetSession()->SendUpdateTrade(true);      // player state for trader
    else
        m_player->GetSession()->SendUpdateTrade(false);     // player state for player
}

void TradeData::SetAccepted(bool state, bool crosssend /*= false*/)
{
    m_accepted = state;

    if (!state)
    {
        if (crosssend)
            m_trader->GetSession()->SendTradeStatus(TRADE_STATUS_BACK_TO_TRADE);
        else
            m_player->GetSession()->SendTradeStatus(TRADE_STATUS_BACK_TO_TRADE);
    }
}

// == KillRewarder ====================================================
// KillRewarder incapsulates logic of rewarding player upon kill with:
// * XP;
// * honor;
// * reputation;
// * kill credit (for quest objectives).
// Rewarding is initiated in two cases: when player kills unit in Unit::Kill()
// and on battlegrounds in Battleground::RewardXPAtKill().
//
// Rewarding algorithm is:
// 1. Initialize internal variables to default values.
// 2. In case when player is in group, initialize variables necessary for group calculations:
// 2.1. _count - number of alive group members within reward distance;
// 2.2. _sumLevel - sum of levels of alive group members within reward distance;
// 2.3. _maxLevel - maximum level of alive group member within reward distance;
// 2.4. _maxNotGrayMember - maximum level of alive group member within reward distance,
//      for whom victim is not gray;
// 2.5. _isFullXP - flag identifying that for all group members victim is not gray,
//      so 100% XP will be rewarded (50% otherwise).
// 3. Reward killer (and group, if necessary).
// 3.1. If killer is in group, reward group.
// 3.1.1. Initialize initial XP amount based on maximum level of group member,
//        for whom victim is not gray.
// 3.1.2. Alter group rate if group is in raid (not for battlegrounds).
// 3.1.3. Reward each group member (even dead) within reward distance (see 4. for more details).
// 3.2. Reward single killer (not group case).
// 3.2.1. Initialize initial XP amount based on killer's level.
// 3.2.2. Reward killer (see 4. for more details).
// 4. Reward player.
// 4.1. Give honor (player must be alive and not on BG).
// 4.2. Give XP.
// 4.2.1. If player is in group, adjust XP:
//        * set to 0 if player's level is more than maximum level of not gray member;
//        * cut XP in half if _isFullXP is false.
// 4.2.2. Apply auras modifying rewarded XP.
// 4.2.3. Give XP to player.
// 4.2.4. If player has pet, reward pet with XP (100% for single player, 50% for group case).
// 4.3. Give reputation (player must not be on BG).
// 4.4. Give kill credit (player must not be in group, or he must be alive or without corpse).
// 5. Credit instance encounter.
KillRewarder::KillRewarder(Player* killer, Unit* victim, bool isBattleGround) :
    // 1. Initialize internal variables to default values.
    _killer(killer), _victim(victim), _group(killer->GetGroup()),
    _groupRate(1.0f), _maxNotGrayMember(NULL), _count(0), _sumLevel(0), _xp(0),
    _isFullXP(false), _maxLevel(0), _isBattleGround(isBattleGround), _isPvP(false)
{
    // mark the credit as pvp if victim is player
    if (victim->GetTypeId() == TYPEID_PLAYER)
        _isPvP = true;
    // or if its owned by player and its not a vehicle
    else if (IS_PLAYER_GUID(victim->GetCharmerOrOwnerGUID()))
        _isPvP = !victim->IsVehicle();

    _InitGroupData();
}

inline void KillRewarder::_InitGroupData()
{
    if (_group)
    {
        // 2. In case when player is in group, initialize variables necessary for group calculations:
        for (GroupReference* itr = _group->GetFirstMember(); itr != NULL; itr = itr->next())
            if (Player* member = itr->getSource())
                if (member->isAlive() && member->IsAtGroupRewardDistance(_victim))
                {
                    const uint8 lvl = member->getLevel();
                    // 2.1. _count - number of alive group members within reward distance;
                    ++_count;
                    // 2.2. _sumLevel - sum of levels of alive group members within reward distance;
                    _sumLevel += lvl;
                    // 2.3. _maxLevel - maximum level of alive group member within reward distance;
                    if (_maxLevel < lvl)
                        _maxLevel = lvl;
                    // 2.4. _maxNotGrayMember - maximum level of alive group member within reward distance,
                    //      for whom victim is not gray;
                    uint32 grayLevel = Trinity::XP::GetGrayLevel(lvl);
                    if (_victim->getLevel() > grayLevel && (!_maxNotGrayMember || _maxNotGrayMember->getLevel() < lvl))
                        _maxNotGrayMember = member;
                }
        // 2.5. _isFullXP - flag identifying that for all group members victim is not gray,
        //      so 100% XP will be rewarded (50% otherwise).
        _isFullXP = _maxNotGrayMember && (_maxLevel == _maxNotGrayMember->getLevel());
    }
    else
        _count = 1;
}

inline void KillRewarder::_InitXP(Player* player)
{
    // Get initial value of XP for kill.
    // XP is given:
    // * on battlegrounds;
    // * otherwise, not in PvP;
    // * not if killer is on vehicle.
    if (_isBattleGround || (!_isPvP && !_killer->GetVehicle()))
        _xp = Trinity::XP::Gain(player, _victim);
}

inline void KillRewarder::_RewardHonor(Player* player)
{
    // Rewarded player must be alive.
    if (player->isAlive())
        player->RewardHonor(_victim, _count, -1, true);
}

inline void KillRewarder::_RewardXP(Player* player, float rate)
{
    uint32 xp(_xp);
    if (_group)
    {
        // 4.2.1. If player is in group, adjust XP:
        //        * set to 0 if player's level is more than maximum level of not gray member;
        //        * cut XP in half if _isFullXP is false.
        if (_maxNotGrayMember && player->isAlive() &&
            _maxNotGrayMember->getLevel() >= player->getLevel())
            xp = _isFullXP ?
                uint32(xp * rate) :             // Reward FULL XP if all group members are not gray.
                uint32(xp * rate / 2) + 1;      // Reward only HALF of XP if some of group members are gray.
        else
            xp = 0;
    }
    if (xp)
    {
        // 4.2.2. Apply auras modifying rewarded XP (SPELL_AURA_MOD_XP_PCT).
        Unit::AuraEffectList const& auras = player->GetAuraEffectsByType(SPELL_AURA_MOD_XP_PCT);
        for (Unit::AuraEffectList::const_iterator i = auras.begin(); i != auras.end(); ++i)
            AddPct(xp, (*i)->GetAmount());

        // 4.2.3. Give XP to player.
        player->GiveXP(xp, _victim, _groupRate);
        if (Pet* pet = player->GetPet())
            // 4.2.4. If player has pet, reward pet with XP (100% for single player, 50% for group case).
            pet->GivePetXP(_group ? xp / 2 : xp);
    }
}

inline void KillRewarder::_RewardReputation(Player* player, float rate)
{
    // 4.3. Give reputation (player must not be on BG).
    // Even dead players and corpses are rewarded.
    player->RewardReputation(_victim, rate);
}

inline void KillRewarder::_RewardKillCredit(Player* player)
{
    // 4.4. Give kill credit (player must not be in group, or he must be alive or without corpse).
    if (!_group || player->isAlive() || !player->GetCorpse())
        if (_victim->GetTypeId() == TYPEID_UNIT)
            player->KilledMonster(_victim->ToCreature()->GetCreatureTemplate(), _victim->GetGUID());
}

void KillRewarder::_RewardPlayer(Player* player, bool isDungeon)
{
    // 4. Reward player.
    if (!_isBattleGround)
    {
        // 4.1. Give honor (player must be alive and not on BG).
        _RewardHonor(player);
        // 4.1.1 Send player killcredit for quests with PlayerSlain
        if (_victim->GetTypeId() == TYPEID_PLAYER)
            player->KilledPlayerCredit();
    }
    // Give XP only in PvE or in battlegrounds.
    // Give reputation and kill credit only in PvE.
    if (!_isPvP || _isBattleGround)
    {
        const float rate = _group ?
            _groupRate * float(player->getLevel()) / _sumLevel : // Group rate depends on summary level.
            1.0f;                                                // Personal rate is 100%.
        if (_xp)
            // 4.2. Give XP.
            _RewardXP(player, rate);
        if (!_isBattleGround)
        {
            // If killer is in dungeon then all members receive full reputation at kill.
            _RewardReputation(player, isDungeon ? 1.0f : rate);
            _RewardKillCredit(player);
        }
    }
}

void KillRewarder::_RewardGroup()
{
    if (_maxLevel)
    {
        if (_maxNotGrayMember)
            // 3.1.1. Initialize initial XP amount based on maximum level of group member,
            //        for whom victim is not gray.
            _InitXP(_maxNotGrayMember);
        // To avoid unnecessary calculations and calls,
        // proceed only if XP is not ZERO or player is not on battleground
        // (battleground rewards only XP, that's why).
        if (!_isBattleGround || _xp)
        {
            const bool isDungeon = !_isPvP && sMapStore.LookupEntry(_killer->GetMapId())->IsDungeon();
            if (!_isBattleGround)
            {
                // 3.1.2. Alter group rate if group is in raid (not for battlegrounds).
                const bool isRaid = !_isPvP && sMapStore.LookupEntry(_killer->GetMapId())->IsRaid() && _group->isRaidGroup();
                _groupRate = Trinity::XP::xp_in_group_rate(_count, isRaid);
            }

            // 3.1.3. Reward each group member (even dead or corpse) within reward distance.
            for (GroupReference* itr = _group->GetFirstMember(); itr != NULL; itr = itr->next())
            {
                if (Player* member = itr->getSource())
                {
                    if (member->IsAtGroupRewardDistance(_victim))
                    {
                        _RewardPlayer(member, isDungeon);
                        member->UpdateAchievementCriteria(ACHIEVEMENT_CRITERIA_TYPE_SPECIAL_PVP_KILL, 1, 0, _victim);
                    }
                }
            }
        }
    }
}

void KillRewarder::Reward()
{
    // 3. Reward killer (and group, if necessary).
    if (_group)
        // 3.1. If killer is in group, reward group.
        _RewardGroup();
    else
    {
        // 3.2. Reward single killer (not group case).
        // 3.2.1. Initialize initial XP amount based on killer's level.
        _InitXP(_killer);
        // To avoid unnecessary calculations and calls,
        // proceed only if XP is not ZERO or player is not on battleground
        // (battleground rewards only XP, that's why).
        if (!_isBattleGround || _xp)
            // 3.2.2. Reward killer.
            _RewardPlayer(_killer, false);
    }

    // 5. Credit instance encounter.
    if (Creature* victim = _victim->ToCreature())
        if (victim->IsDungeonBoss())
            if (InstanceScript* instance = _victim->GetInstanceScript())
                instance->UpdateEncounterState(ENCOUNTER_CREDIT_KILL_CREATURE, _victim->GetEntry(), _victim);
}

// == Player ====================================================

// we can disable this warning for this since it only
// causes undefined behavior when passed to the base class constructor
#ifdef _MSC_VER
#pragma warning(disable:4355)
#endif
Player::Player(WorldSession* session): Unit(true), m_achievementMgr(this), m_reputationMgr(this)
{
#ifdef _MSC_VER
#pragma warning(default:4355)
#endif

    m_speakTime = 0;
    m_speakCount = 0;

    m_objectType |= TYPEMASK_PLAYER;
    m_objectTypeId = TYPEID_PLAYER;

    m_valuesCount = PLAYER_END;

    m_session = session;

    m_divider = 0;

    m_ExtraFlags = 0;

    m_spellModTakingSpell = NULL;
    //m_pad = 0;

    // players always accept
    if (AccountMgr::IsPlayerAccount(GetSession()->GetSecurity()))
        SetAcceptWhispers(true);

    m_curSelection = 0;
    m_lootGuid = 0;

    m_comboTarget = 0;
    m_comboPoints = 0;

    m_usedTalentCount = 0;
    m_questRewardTalentCount = 0;

    m_regenTimer = 0;
    m_regenTimerCount = 0;
    m_weaponChangeTimer = 0;

    m_zoneUpdateId = 0;
    m_zoneUpdateTimer = 0;

    m_areaUpdateId = 0;

    m_nextSave = sWorld->getIntConfig(CONFIG_INTERVAL_SAVE);

    clearResurrectRequestData();

    memset(m_items, 0, sizeof(Item*)*PLAYER_SLOTS_COUNT);

    m_social = NULL;

    // group is initialized in the reference constructor
    SetGroupInvite(NULL);
    m_groupUpdateMask = 0;
    m_auraRaidUpdateMask = 0;
    m_bPassOnGroupLoot = false;

    duel = NULL;

    m_GuildIdInvited = 0;
    m_ArenaTeamIdInvited = 0;

    m_atLoginFlags = AT_LOGIN_NONE;

    mSemaphoreTeleport_Near = false;
    mSemaphoreTeleport_Far = false;

    m_DelayedOperations = 0;
    m_bCanDelayTeleport = false;
    m_bHasDelayedTeleport = false;
    m_teleport_options = 0;

    m_trade = NULL;

    m_cinematic = 0;

    PlayerTalkClass = new PlayerMenu(GetSession());
    m_currentBuybackSlot = BUYBACK_SLOT_START;

    m_DailyQuestChanged = false;
    m_lastDailyQuestTime = 0;

    for (uint8 i=0; i<MAX_TIMERS; i++)
        m_MirrorTimer[i] = DISABLED_MIRROR_TIMER;

    m_MirrorTimerFlags = UNDERWATER_NONE;
    m_MirrorTimerFlagsLast = UNDERWATER_NONE;
    m_isInWater = false;
    m_drunkTimer = 0;
    m_restTime = 0;
    m_deathTimer = 0;
    m_deathExpireTime = 0;

    m_swingErrorMsg = 0;

    for (uint8 j = 0; j < PLAYER_MAX_BATTLEGROUND_QUEUES; ++j)
    {
        m_bgBattlegroundQueueID[j].bgQueueTypeId  = BATTLEGROUND_QUEUE_NONE;
        m_bgBattlegroundQueueID[j].invitedToInstance = 0;
    }

    m_logintime = time(NULL);
    m_Last_tick = m_logintime;
    m_WeaponProficiency = 0;
    m_ArmorProficiency = 0;
    m_canParry = false;
    m_canBlock = false;
    m_canDualWield = false;
    m_canTitanGrip = false;
    m_ammoDPS = 0.0f;

    m_temporaryUnsummonedPetNumber = 0;
    //cache for UNIT_CREATED_BY_SPELL to allow
    //returning reagents for temporarily removed pets
    //when dying/logging out
    m_oldpetspell = 0;
    m_lastpetnumber = 0;

    ////////////////////Rest System/////////////////////
    time_inn_enter=0;
    inn_pos_mapid=0;
    inn_pos_x=0;
    inn_pos_y=0;
    inn_pos_z=0;
    m_rest_bonus=0;
    rest_type=REST_TYPE_NO;
    ////////////////////Rest System/////////////////////

    m_mailsLoaded = false;
    m_mailsUpdated = false;
    unReadMails = 0;
    m_nextMailDelivereTime = 0;

    m_resetTalentsCost = 0;
    m_resetTalentsTime = 0;
    m_itemUpdateQueueBlocked = false;

    for (uint8 i = 0; i < MAX_MOVE_TYPE; ++i)
        m_forced_speed_changes[i] = 0;

    m_stableSlots = 0;

    /////////////////// Instance System /////////////////////

    m_HomebindTimer = 0;
    m_InstanceValid = true;
    m_dungeonDifficulty = DUNGEON_DIFFICULTY_NORMAL;
    m_raidDifficulty = RAID_DIFFICULTY_10MAN_NORMAL;

    m_lastPotionId = 0;

    m_activeSpec = 0;
    m_specsCount = 1;

    for (uint8 i = 0; i < MAX_TALENT_SPECS; ++i)
    {
        for (uint8 g = 0; g < MAX_GLYPH_SLOT_INDEX; ++g)
            m_Glyphs[i][g] = 0;

        m_talents[i] = new PlayerTalentMap();
    }

    for (uint8 i = 0; i < BASEMOD_END; ++i)
    {
        m_auraBaseMod[i][FLAT_MOD] = 0.0f;
        m_auraBaseMod[i][PCT_MOD] = 1.0f;
    }

    for (uint8 i = 0; i < MAX_COMBAT_RATING; i++)
        m_baseRatingValue[i] = 0;

    m_baseSpellPower = 0;
    m_baseFeralAP = 0;
    m_baseManaRegen = 0;
    m_baseHealthRegen = 0;
    m_spellPenetrationItemMod = 0;

    // Honor System
    m_lastHonorUpdateTime = time(NULL);

    m_IsBGRandomWinner = false;

    // Player summoning
    m_summon_expire = 0;
    m_summon_mapid = 0;
    m_summon_x = 0.0f;
    m_summon_y = 0.0f;
    m_summon_z = 0.0f;

    m_mover = this;
    m_movedPlayer = this;
    m_seer = this;

    m_contestedPvPTimer = 0;

    m_declinedname = NULL;

    m_isActive = true;

    m_runes = NULL;

    m_lastFallTime = 0;
    m_lastFallZ = 0;

    m_grantableLevels = 0;

    m_ControlledByPlayer = true;

    sWorld->IncreasePlayerCount();

    m_ChampioningFaction = 0;

    for (uint8 i = 0; i < MAX_POWERS; ++i)
        m_powerFraction[i] = 0;

    isDebugAreaTriggers = false;

    m_WeeklyQuestChanged = false;

    m_SeasonalQuestChanged = false;

    spectatorFlag = false;
    spectateCanceled = false;
    spectateFrom = NULL;


    SetPendingBind(0, 0);

    _activeCheats = CHEAT_NONE;
}

Player::~Player()
{
    // it must be unloaded already in PlayerLogout and accessed only for loggined player
    //m_social = NULL;

    // Note: buy back item already deleted from DB when player was saved
    for (uint8 i = 0; i < PLAYER_SLOTS_COUNT; ++i)
        delete m_items[i];

    for (PlayerSpellMap::const_iterator itr = m_spells.begin(); itr != m_spells.end(); ++itr)
        delete itr->second;

    for (uint8 i = 0; i < MAX_TALENT_SPECS; ++i)
    {
        for (PlayerTalentMap::const_iterator itr = m_talents[i]->begin(); itr != m_talents[i]->end(); ++itr)
            delete itr->second;
        delete m_talents[i];
    }

    //all mailed items should be deleted, also all mail should be deallocated
    for (PlayerMails::iterator itr = m_mail.begin(); itr != m_mail.end(); ++itr)
        delete *itr;

    for (ItemMap::iterator iter = mMitems.begin(); iter != mMitems.end(); ++iter)
        delete iter->second;                                //if item is duplicated... then server may crash ... but that item should be deallocated

    delete PlayerTalkClass;

    for (size_t x = 0; x < ItemSetEff.size(); x++)
        delete ItemSetEff[x];

    delete m_declinedname;
    delete m_runes;

    sWorld->DecreasePlayerCount();
}

void Player::CleanupsBeforeDelete(bool finalCleanup)
{
    TradeCancel(false);
    DuelComplete(DUEL_INTERRUPTED);

    Unit::CleanupsBeforeDelete(finalCleanup);

    if (m_transport)
        m_transport->RemovePassenger(this);

    // clean up player-instance binds, may unload some instance saves
    for (uint8 i = 0; i < MAX_DIFFICULTY; ++i)
        for (BoundInstancesMap::iterator itr = m_boundInstances[i].begin(); itr != m_boundInstances[i].end(); ++itr)
            itr->second.save->RemovePlayer(this);
}

bool Player::Create(uint32 guidlow, CharacterCreateInfo* createInfo)
{
    //FIXME: outfitId not used in player creating
    // TODO: need more checks against packet modifications
    // should check that skin, face, hair* are valid via DBC per race/class
    // also do it in Player::BuildEnumData, Player::LoadFromDB

    Object::_Create(guidlow, 0, HIGHGUID_PLAYER);

    m_name = createInfo->Name;

    PlayerInfo const* info = sObjectMgr->GetPlayerInfo(createInfo->Race, createInfo->Class);
    if (!info)
    {
        sLog->outError(LOG_FILTER_PLAYER, "Player::Create: Possible hacking-attempt: Account %u tried creating a character named '%s' with an invalid race/class pair (%u/%u) - refusing to do so.",
                GetSession()->GetAccountId(), m_name.c_str(), createInfo->Race, createInfo->Class);
        return false;
    }

    for (uint8 i = 0; i < PLAYER_SLOTS_COUNT; i++)
        m_items[i] = NULL;

    Relocate(info->positionX, info->positionY, info->positionZ, info->orientation);

    ChrClassesEntry const* cEntry = sChrClassesStore.LookupEntry(createInfo->Class);
    if (!cEntry)
    {
        sLog->outError(LOG_FILTER_PLAYER, "Player::Create: Possible hacking-attempt: Account %u tried creating a character named '%s' with an invalid character class (%u) - refusing to do so (wrong DBC-files?)",
                GetSession()->GetAccountId(), m_name.c_str(), createInfo->Class);
        return false;
    }

    SetMap(sMapMgr->CreateMap(info->mapId, this));

    uint8 powertype = cEntry->powerType;

    SetFloatValue(UNIT_FIELD_BOUNDINGRADIUS, DEFAULT_WORLD_OBJECT_SIZE);
    SetFloatValue(UNIT_FIELD_COMBATREACH, 1.5f);

    setFactionForRace(createInfo->Race);

    if (!IsValidGender(createInfo->Gender))
    {
        sLog->outError(LOG_FILTER_PLAYER, "Player::Create: Possible hacking-attempt: Account %u tried creating a character named '%s' with an invalid gender (%hu) - refusing to do so",
                GetSession()->GetAccountId(), m_name.c_str(), createInfo->Gender);
        return false;
    }

    uint32 RaceClassGender = (createInfo->Race) | (createInfo->Class << 8) | (createInfo->Gender << 16);

    SetUInt32Value(UNIT_FIELD_BYTES_0, (RaceClassGender | (powertype << 24)));
    InitDisplayIds();
    if (sWorld->getIntConfig(CONFIG_GAME_TYPE) == REALM_TYPE_PVP || sWorld->getIntConfig(CONFIG_GAME_TYPE) == REALM_TYPE_RPPVP)
    {
        SetByteFlag(UNIT_FIELD_BYTES_2, 1, UNIT_BYTE2_FLAG_PVP);
        SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_PVP_ATTACKABLE);
    }
    SetFlag(UNIT_FIELD_FLAGS_2, UNIT_FLAG2_REGENERATE_POWER);
    SetFloatValue(UNIT_MOD_CAST_SPEED, 1.0f);               // fix cast time showed in spell tooltip on client
    SetFloatValue(UNIT_FIELD_HOVERHEIGHT, 1.0f);            // default for players in 3.0.3

                                                            // -1 is default value
    SetInt32Value(PLAYER_FIELD_WATCHED_FACTION_INDEX, uint32(-1));

    SetUInt32Value(PLAYER_BYTES, (createInfo->Skin | (createInfo->Face << 8) | (createInfo->HairStyle << 16) | (createInfo->HairColor << 24)));
    SetUInt32Value(PLAYER_BYTES_2, (createInfo->FacialHair |
                                   (0x00 << 8) |
                                   (0x00 << 16) |
                                   (((GetSession()->IsARecruiter() || GetSession()->GetRecruiterId() != 0) ? REST_STATE_RAF_LINKED : REST_STATE_NOT_RAF_LINKED) << 24)));
    SetByteValue(PLAYER_BYTES_3, 0, createInfo->Gender);
    SetByteValue(PLAYER_BYTES_3, 3, 0);                     // BattlefieldArenaFaction (0 or 1)

    SetUInt32Value(PLAYER_GUILDID, 0);
    SetUInt32Value(PLAYER_GUILDRANK, 0);
    SetUInt32Value(PLAYER_GUILD_TIMESTAMP, 0);

    for (int i = 0; i < KNOWN_TITLES_SIZE; ++i)
        SetUInt64Value(PLAYER__FIELD_KNOWN_TITLES + i, 0);  // 0=disabled
    SetUInt32Value(PLAYER_CHOSEN_TITLE, 0);

    SetUInt32Value(PLAYER_FIELD_KILLS, 0);
    SetUInt32Value(PLAYER_FIELD_LIFETIME_HONORABLE_KILLS, 0);
    SetUInt32Value(PLAYER_FIELD_TODAY_CONTRIBUTION, 0);
    SetUInt32Value(PLAYER_FIELD_YESTERDAY_CONTRIBUTION, 0);

    // set starting level
    uint32 start_level = getClass() != CLASS_DEATH_KNIGHT
        ? sWorld->getIntConfig(CONFIG_START_PLAYER_LEVEL)
        : sWorld->getIntConfig(CONFIG_START_HEROIC_PLAYER_LEVEL);

    if (!AccountMgr::IsPlayerAccount(GetSession()->GetSecurity()))
    {
        uint32 gm_level = sWorld->getIntConfig(CONFIG_START_GM_LEVEL);
        if (gm_level > start_level)
            start_level = gm_level;
    }

    SetUInt32Value(UNIT_FIELD_LEVEL, start_level);

    InitRunes();

    SetUInt32Value(PLAYER_FIELD_COINAGE, sWorld->getIntConfig(CONFIG_START_PLAYER_MONEY));
    SetHonorPoints(sWorld->getIntConfig(CONFIG_START_HONOR_POINTS));
    SetArenaPoints(sWorld->getIntConfig(CONFIG_START_ARENA_POINTS));

    // start with every map explored
    if (sWorld->getBoolConfig(CONFIG_START_ALL_EXPLORED))
    {
        for (uint8 i=0; i<PLAYER_EXPLORED_ZONES_SIZE; i++)
            SetFlag(PLAYER_EXPLORED_ZONES_1+i, 0xFFFFFFFF);
    }

    //Reputations if "StartAllReputation" is enabled, -- TODO: Fix this in a better way
    if (sWorld->getBoolConfig(CONFIG_START_ALL_REP))
    {
        GetReputationMgr().SetReputation(sFactionStore.LookupEntry(942), 42999);
        GetReputationMgr().SetReputation(sFactionStore.LookupEntry(935), 42999);
        GetReputationMgr().SetReputation(sFactionStore.LookupEntry(936), 42999);
        GetReputationMgr().SetReputation(sFactionStore.LookupEntry(1011), 42999);
        GetReputationMgr().SetReputation(sFactionStore.LookupEntry(970), 42999);
        GetReputationMgr().SetReputation(sFactionStore.LookupEntry(967), 42999);
        GetReputationMgr().SetReputation(sFactionStore.LookupEntry(989), 42999);
        GetReputationMgr().SetReputation(sFactionStore.LookupEntry(932), 42999);
        GetReputationMgr().SetReputation(sFactionStore.LookupEntry(934), 42999);
        GetReputationMgr().SetReputation(sFactionStore.LookupEntry(1038), 42999);
        GetReputationMgr().SetReputation(sFactionStore.LookupEntry(1077), 42999);

        // Factions depending on team, like cities and some more stuff
        switch (GetTeam())
        {
        case ALLIANCE:
            GetReputationMgr().SetReputation(sFactionStore.LookupEntry(72), 42999);
            GetReputationMgr().SetReputation(sFactionStore.LookupEntry(47), 42999);
            GetReputationMgr().SetReputation(sFactionStore.LookupEntry(69), 42999);
            GetReputationMgr().SetReputation(sFactionStore.LookupEntry(930), 42999);
            GetReputationMgr().SetReputation(sFactionStore.LookupEntry(730), 42999);
            GetReputationMgr().SetReputation(sFactionStore.LookupEntry(978), 42999);
            GetReputationMgr().SetReputation(sFactionStore.LookupEntry(54), 42999);
            GetReputationMgr().SetReputation(sFactionStore.LookupEntry(946), 42999);
            break;
        case HORDE:
            GetReputationMgr().SetReputation(sFactionStore.LookupEntry(76), 42999);
            GetReputationMgr().SetReputation(sFactionStore.LookupEntry(68), 42999);
            GetReputationMgr().SetReputation(sFactionStore.LookupEntry(81), 42999);
            GetReputationMgr().SetReputation(sFactionStore.LookupEntry(911), 42999);
            GetReputationMgr().SetReputation(sFactionStore.LookupEntry(729), 42999);
            GetReputationMgr().SetReputation(sFactionStore.LookupEntry(941), 42999);
            GetReputationMgr().SetReputation(sFactionStore.LookupEntry(530), 42999);
            GetReputationMgr().SetReputation(sFactionStore.LookupEntry(947), 42999);
            break;
        default:
            break;
        }
    }

    // Played time
    m_Last_tick = time(NULL);
    m_Played_time[PLAYED_TIME_TOTAL] = 0;
    m_Played_time[PLAYED_TIME_LEVEL] = 0;

    // base stats and related field values
    InitStatsForLevel();
    InitTaxiNodesForLevel();
    InitGlyphsForLevel();
    InitTalentForLevel();
    InitPrimaryProfessions();                               // to max set before any spell added

    // apply original stats mods before spell loading or item equipment that call before equip _RemoveStatsMods()
    UpdateMaxHealth();                                      // Update max Health (for add bonus from stamina)
    SetFullHealth();
    if (getPowerType() == POWER_MANA)
    {
        UpdateMaxPower(POWER_MANA);                         // Update max Mana (for add bonus from intellect)
        SetPower(POWER_MANA, GetMaxPower(POWER_MANA));
    }

    if (getPowerType() == POWER_RUNIC_POWER)
    {
        SetPower(POWER_RUNE, 8);
        SetMaxPower(POWER_RUNE, 8);
        SetPower(POWER_RUNIC_POWER, 0);
        SetMaxPower(POWER_RUNIC_POWER, 1000);
    }

    // original spells
    learnDefaultSpells();

    // original action bar
    for (PlayerCreateInfoActions::const_iterator action_itr = info->action.begin(); action_itr != info->action.end(); ++action_itr)
        addActionButton(action_itr->button, action_itr->action, action_itr->type);

    // original items
    CharStartOutfitEntry const* oEntry = NULL;
    for (uint32 i = 1; i < sCharStartOutfitStore.GetNumRows(); ++i)
    {
        if (CharStartOutfitEntry const* entry = sCharStartOutfitStore.LookupEntry(i))
        {
            if (entry->RaceClassGender == RaceClassGender)
            {
                oEntry = entry;
                break;
            }
        }
    }

    if (oEntry)
    {
        for (int j = 0; j < MAX_OUTFIT_ITEMS; ++j)
        {
            if (oEntry->ItemId[j] <= 0)
                continue;

            uint32 itemId = oEntry->ItemId[j];

            // just skip, reported in ObjectMgr::LoadItemTemplates
            ItemTemplate const* iProto = sObjectMgr->GetItemTemplate(itemId);
            if (!iProto)
                continue;

            // BuyCount by default
            uint32 count = iProto->BuyCount;

            // special amount for food/drink
            if (iProto->Class == ITEM_CLASS_CONSUMABLE && iProto->SubClass == ITEM_SUBCLASS_FOOD)
            {
                switch (iProto->Spells[0].SpellCategory)
                {
                    case SPELL_CATEGORY_FOOD:                                // food
                        count = getClass() == CLASS_DEATH_KNIGHT ? 10 : 4;
                        break;
                    case SPELL_CATEGORY_DRINK:                                // drink
                        count = 2;
                        break;
                }
                if (iProto->GetMaxStackSize() < count)
                    count = iProto->GetMaxStackSize();
            }
            StoreNewItemInBestSlots(itemId, count);
        }
    }

    for (PlayerCreateInfoItems::const_iterator item_id_itr = info->item.begin(); item_id_itr != info->item.end(); ++item_id_itr)
        StoreNewItemInBestSlots(item_id_itr->item_id, item_id_itr->item_amount);

    // bags and main-hand weapon must equipped at this moment
    // now second pass for not equipped (offhand weapon/shield if it attempt equipped before main-hand weapon)
    // or ammo not equipped in special bag
    for (uint8 i = INVENTORY_SLOT_ITEM_START; i < INVENTORY_SLOT_ITEM_END; i++)
    {
        if (Item* pItem = GetItemByPos(INVENTORY_SLOT_BAG_0, i))
        {
            uint16 eDest;
            // equip offhand weapon/shield if it attempt equipped before main-hand weapon
            InventoryResult msg = CanEquipItem(NULL_SLOT, eDest, pItem, false);
            if (msg == EQUIP_ERR_OK)
            {
                RemoveItem(INVENTORY_SLOT_BAG_0, i, true);
                EquipItem(eDest, pItem, true);
            }
            // move other items to more appropriate slots (ammo not equipped in special bag)
            else
            {
                ItemPosCountVec sDest;
                msg = CanStoreItem(NULL_BAG, NULL_SLOT, sDest, pItem, false);
                if (msg == EQUIP_ERR_OK)
                {
                    RemoveItem(INVENTORY_SLOT_BAG_0, i, true);
                    pItem = StoreItem(sDest, pItem, true);
                }

                // if  this is ammo then use it
                msg = CanUseAmmo(pItem->GetEntry());
                if (msg == EQUIP_ERR_OK)
                    SetAmmo(pItem->GetEntry());
            }
        }
    }
    // all item positions resolved

    return true;
}

bool Player::StoreNewItemInBestSlots(uint32 titem_id, uint32 titem_amount)
{
    sLog->outDebug(LOG_FILTER_PLAYER_ITEMS, "STORAGE: Creating initial item, itemId = %u, count = %u", titem_id, titem_amount);

    // attempt equip by one
    while (titem_amount > 0)
    {
        uint16 eDest;
        InventoryResult msg = CanEquipNewItem(NULL_SLOT, eDest, titem_id, false);
        if (msg != EQUIP_ERR_OK)
            break;

        EquipNewItem(eDest, titem_id, true);
        AutoUnequipOffhandIfNeed();
        --titem_amount;
    }

    if (titem_amount == 0)
        return true;                                        // equipped

    // attempt store
    ItemPosCountVec sDest;
    // store in main bag to simplify second pass (special bags can be not equipped yet at this moment)
    InventoryResult msg = CanStoreNewItem(INVENTORY_SLOT_BAG_0, NULL_SLOT, sDest, titem_id, titem_amount);
    if (msg == EQUIP_ERR_OK)
    {
        StoreNewItem(sDest, titem_id, true, Item::GenerateItemRandomPropertyId(titem_id));
        return true;                                        // stored
    }

    // item can't be added
    sLog->outError(LOG_FILTER_PLAYER_ITEMS, "STORAGE: Can't equip or store initial item %u for race %u class %u, error msg = %u", titem_id, getRace(), getClass(), msg);
    return false;
}

void Player::SendMirrorTimer(MirrorTimerType Type, uint32 MaxValue, uint32 CurrentValue, int32 Regen)
{
    if (int(MaxValue) == DISABLED_MIRROR_TIMER)
    {
        if (int(CurrentValue) != DISABLED_MIRROR_TIMER)
            StopMirrorTimer(Type);
        return;
    }
    WorldPacket data(SMSG_START_MIRROR_TIMER, (21));
    data << (uint32)Type;
    data << CurrentValue;
    data << MaxValue;
    data << Regen;
    data << (uint8)0;
    data << (uint32)0;                                      // spell id
    GetSession()->SendPacket(&data);
}

void Player::StopMirrorTimer(MirrorTimerType Type)
{
    m_MirrorTimer[Type] = DISABLED_MIRROR_TIMER;
    WorldPacket data(SMSG_STOP_MIRROR_TIMER, 4);
    data << (uint32)Type;
    GetSession()->SendPacket(&data);
}

bool Player::IsImmuneToEnvironmentalDamage()
{
    // check for GM and death state included in isAttackableByAOE
    return (!isTargetableForAttack(false));
}

uint32 Player::EnvironmentalDamage(EnviromentalDamage type, uint32 damage)
{
    if (IsImmuneToEnvironmentalDamage())
        return 0;

    // Absorb, resist some environmental damage type
    uint32 absorb = 0;
    uint32 resist = 0;
    if (type == DAMAGE_LAVA)
        CalcAbsorbResist(this, SPELL_SCHOOL_MASK_FIRE, DIRECT_DAMAGE, damage, &absorb, &resist);
    else if (type == DAMAGE_SLIME)
        CalcAbsorbResist(this, SPELL_SCHOOL_MASK_NATURE, DIRECT_DAMAGE, damage, &absorb, &resist);

    damage -= absorb + resist;

    DealDamageMods(this, damage, &absorb);

    WorldPacket data(SMSG_ENVIRONMENTALDAMAGELOG, (21));
    data << uint64(GetGUID());
    data << uint8(type != DAMAGE_FALL_TO_VOID ? type : DAMAGE_FALL);
    data << uint32(damage);
    data << uint32(absorb);
    data << uint32(resist);
    SendMessageToSet(&data, true);

    uint32 final_damage = DealDamage(this, damage, NULL, SELF_DAMAGE, SPELL_SCHOOL_MASK_NORMAL, NULL, false);

    if (!isAlive())
    {
        if (type == DAMAGE_FALL)                               // DealDamage not apply item durability loss at self damage
        {
            sLog->outDebug(LOG_FILTER_PLAYER, "We are fall to death, loosing 10 percents durability");
            DurabilityLossAll(0.10f, false);
            // durability lost message
            WorldPacket data2(SMSG_DURABILITY_DAMAGE_DEATH, 0);
            GetSession()->SendPacket(&data2);
        }

        UpdateAchievementCriteria(ACHIEVEMENT_CRITERIA_TYPE_DEATHS_FROM, 1, type);
    }

    return final_damage;
}

int32 Player::getMaxTimer(MirrorTimerType timer)
{
    switch (timer)
    {
        case FATIGUE_TIMER:
            return MINUTE * IN_MILLISECONDS;
        case BREATH_TIMER:
        {
            if (!isAlive() || HasAuraType(SPELL_AURA_WATER_BREATHING) || GetSession()->GetSecurity() >= AccountTypes(sWorld->getIntConfig(CONFIG_DISABLE_BREATHING)))
                return DISABLED_MIRROR_TIMER;
            int32 UnderWaterTime = 3 * MINUTE * IN_MILLISECONDS;
            AuraEffectList const& mModWaterBreathing = GetAuraEffectsByType(SPELL_AURA_MOD_WATER_BREATHING);
            for (AuraEffectList::const_iterator i = mModWaterBreathing.begin(); i != mModWaterBreathing.end(); ++i)
                AddPct(UnderWaterTime, (*i)->GetAmount());
            return UnderWaterTime;
        }
        case FIRE_TIMER:
        {
            if (!isAlive())
                return DISABLED_MIRROR_TIMER;
            return 1 * IN_MILLISECONDS;
        }
        default:
            return 0;
    }
}

void Player::UpdateMirrorTimers()
{
    // Desync flags for update on next HandleDrowning
    if (m_MirrorTimerFlags)
        m_MirrorTimerFlagsLast = ~m_MirrorTimerFlags;
}

void Player::HandleDrowning(uint32 time_diff)
{
    if (!m_MirrorTimerFlags)
        return;

    // In water
    if (m_MirrorTimerFlags & UNDERWATER_INWATER)
    {
        // Breath timer not activated - activate it
        if (m_MirrorTimer[BREATH_TIMER] == DISABLED_MIRROR_TIMER)
        {
            m_MirrorTimer[BREATH_TIMER] = getMaxTimer(BREATH_TIMER);
            SendMirrorTimer(BREATH_TIMER, m_MirrorTimer[BREATH_TIMER], m_MirrorTimer[BREATH_TIMER], -1);
        }
        else                                                              // If activated - do tick
        {
            m_MirrorTimer[BREATH_TIMER]-=time_diff;
            // Timer limit - need deal damage
            if (m_MirrorTimer[BREATH_TIMER] < 0)
            {
                m_MirrorTimer[BREATH_TIMER]+= 1*IN_MILLISECONDS;
                // Calculate and deal damage
                // TODO: Check this formula
                uint32 damage = GetMaxHealth() / 5 + urand(0, getLevel()-1);
                EnvironmentalDamage(DAMAGE_DROWNING, damage);
            }
            else if (!(m_MirrorTimerFlagsLast & UNDERWATER_INWATER))      // Update time in client if need
                SendMirrorTimer(BREATH_TIMER, getMaxTimer(BREATH_TIMER), m_MirrorTimer[BREATH_TIMER], -1);
        }
    }
    else if (m_MirrorTimer[BREATH_TIMER] != DISABLED_MIRROR_TIMER)        // Regen timer
    {
        int32 UnderWaterTime = getMaxTimer(BREATH_TIMER);
        // Need breath regen
        m_MirrorTimer[BREATH_TIMER]+=10*time_diff;
        if (m_MirrorTimer[BREATH_TIMER] >= UnderWaterTime || !isAlive())
            StopMirrorTimer(BREATH_TIMER);
        else if (m_MirrorTimerFlagsLast & UNDERWATER_INWATER)
            SendMirrorTimer(BREATH_TIMER, UnderWaterTime, m_MirrorTimer[BREATH_TIMER], 10);
    }

    // In dark water
    if (m_MirrorTimerFlags & UNDERWARER_INDARKWATER)
    {
        // Fatigue timer not activated - activate it
        if (m_MirrorTimer[FATIGUE_TIMER] == DISABLED_MIRROR_TIMER)
        {
            m_MirrorTimer[FATIGUE_TIMER] = getMaxTimer(FATIGUE_TIMER);
            SendMirrorTimer(FATIGUE_TIMER, m_MirrorTimer[FATIGUE_TIMER], m_MirrorTimer[FATIGUE_TIMER], -1);
        }
        else
        {
            m_MirrorTimer[FATIGUE_TIMER]-=time_diff;
            // Timer limit - need deal damage or teleport ghost to graveyard
            if (m_MirrorTimer[FATIGUE_TIMER] < 0)
            {
                m_MirrorTimer[FATIGUE_TIMER]+= 1*IN_MILLISECONDS;
                if (isAlive())                                            // Calculate and deal damage
                {
                    uint32 damage = GetMaxHealth() / 5 + urand(0, getLevel()-1);
                    EnvironmentalDamage(DAMAGE_EXHAUSTED, damage);
                }
                else if (HasFlag(PLAYER_FLAGS, PLAYER_FLAGS_GHOST))       // Teleport ghost to graveyard
                    RepopAtGraveyard();
            }
            else if (!(m_MirrorTimerFlagsLast & UNDERWARER_INDARKWATER))
                SendMirrorTimer(FATIGUE_TIMER, getMaxTimer(FATIGUE_TIMER), m_MirrorTimer[FATIGUE_TIMER], -1);
        }
    }
    else if (m_MirrorTimer[FATIGUE_TIMER] != DISABLED_MIRROR_TIMER)       // Regen timer
    {
        int32 DarkWaterTime = getMaxTimer(FATIGUE_TIMER);
        m_MirrorTimer[FATIGUE_TIMER]+=10*time_diff;
        if (m_MirrorTimer[FATIGUE_TIMER] >= DarkWaterTime || !isAlive())
            StopMirrorTimer(FATIGUE_TIMER);
        else if (m_MirrorTimerFlagsLast & UNDERWARER_INDARKWATER)
            SendMirrorTimer(FATIGUE_TIMER, DarkWaterTime, m_MirrorTimer[FATIGUE_TIMER], 10);
    }

    if (m_MirrorTimerFlags & (UNDERWATER_INLAVA /*| UNDERWATER_INSLIME*/) && !(_lastLiquid && _lastLiquid->SpellId))
    {
        // Breath timer not activated - activate it
        if (m_MirrorTimer[FIRE_TIMER] == DISABLED_MIRROR_TIMER)
            m_MirrorTimer[FIRE_TIMER] = getMaxTimer(FIRE_TIMER);
        else
        {
            m_MirrorTimer[FIRE_TIMER] -= time_diff;
            if (m_MirrorTimer[FIRE_TIMER] < 0)
            {
                m_MirrorTimer[FIRE_TIMER]+= 1*IN_MILLISECONDS;
                // Calculate and deal damage
                // TODO: Check this formula
                uint32 damage = urand(600, 700);
                if (m_MirrorTimerFlags & UNDERWATER_INLAVA)
                    EnvironmentalDamage(DAMAGE_LAVA, damage);
                // need to skip Slime damage in Undercity,
                // maybe someone can find better way to handle environmental damage
                //else if (m_zoneUpdateId != 1497)
                //    EnvironmentalDamage(DAMAGE_SLIME, damage);
            }
        }
    }
    else
        m_MirrorTimer[FIRE_TIMER] = DISABLED_MIRROR_TIMER;

    // Recheck timers flag
    m_MirrorTimerFlags&=~UNDERWATER_EXIST_TIMERS;
    for (uint8 i = 0; i< MAX_TIMERS; ++i)
        if (m_MirrorTimer[i] != DISABLED_MIRROR_TIMER)
        {
            m_MirrorTimerFlags|=UNDERWATER_EXIST_TIMERS;
            break;
        }
    m_MirrorTimerFlagsLast = m_MirrorTimerFlags;
}

///The player sobers by 1% every 9 seconds
void Player::HandleSobering()
{
    m_drunkTimer = 0;

    uint8 currentDrunkValue = GetDrunkValue();
    uint8 drunk = currentDrunkValue ? --currentDrunkValue : 0;
    SetDrunkValue(drunk);
}

DrunkenState Player::GetDrunkenstateByValue(uint8 value)
{
    if (value >= 90)
        return DRUNKEN_SMASHED;
    if (value >= 50)
        return DRUNKEN_DRUNK;
    if (value)
        return DRUNKEN_TIPSY;
    return DRUNKEN_SOBER;
}

void Player::SetDrunkValue(uint8 newDrunkValue, uint32 itemId /*= 0*/)
{
    bool isSobering = newDrunkValue < GetDrunkValue();
    uint32 oldDrunkenState = Player::GetDrunkenstateByValue(GetDrunkValue());
    if (newDrunkValue > 100)
        newDrunkValue = 100;

    // select drunk percent or total SPELL_AURA_MOD_FAKE_INEBRIATE amount, whichever is higher for visibility updates
    int32 drunkPercent = std::max<int32>(newDrunkValue, GetTotalAuraModifier(SPELL_AURA_MOD_FAKE_INEBRIATE));
    if (drunkPercent)
    {
        m_invisibilityDetect.AddFlag(INVISIBILITY_DRUNK);
        m_invisibilityDetect.SetValue(INVISIBILITY_DRUNK, drunkPercent);
    }
    else if (!HasAuraType(SPELL_AURA_MOD_FAKE_INEBRIATE) && !newDrunkValue)
        m_invisibilityDetect.DelFlag(INVISIBILITY_DRUNK);

    uint32 newDrunkenState = Player::GetDrunkenstateByValue(newDrunkValue);
    SetByteValue(PLAYER_BYTES_3, 1, newDrunkValue);
    UpdateObjectVisibility();

    if (!isSobering)
        m_drunkTimer = 0;   // reset sobering timer

    if (newDrunkenState == oldDrunkenState)
        return;

    WorldPacket data(SMSG_CROSSED_INEBRIATION_THRESHOLD, (8+4+4));
    data << uint64(GetGUID());
    data << uint32(newDrunkenState);
    data << uint32(itemId);
    SendMessageToSet(&data, true);
}

void Player::Update(uint32 p_time)
{
    if (!IsInWorld())
        return;

    // undelivered mail
    if (m_nextMailDelivereTime && m_nextMailDelivereTime <= time(NULL))
    {
        SendNewMail();
        ++unReadMails;

        // It will be recalculate at mailbox open (for unReadMails important non-0 until mailbox open, it also will be recalculated)
        m_nextMailDelivereTime = 0;
    }

    // If this is set during update SetSpellModTakingSpell call is missing somewhere in the code
    // Having this would prevent more aura charges to be dropped, so let's crash
    //ASSERT (!m_spellModTakingSpell);
    if (m_spellModTakingSpell)
    {
        //sLog->outFatal(LOG_FILTER_PLAYER, "Player has m_pad %u during update!", m_pad);
        //if (m_spellModTakingSpell)
        sLog->outFatal(LOG_FILTER_SPELLS_AURAS, "Player has m_spellModTakingSpell %u during update!", m_spellModTakingSpell->m_spellInfo->Id);
        m_spellModTakingSpell = NULL;
    }

    //used to implement delayed far teleports
    SetCanDelayTeleport(true);
    Unit::Update(p_time);
    SetCanDelayTeleport(false);

    time_t now = time(NULL);

    UpdatePvPFlag(now);

    UpdateContestedPvP(p_time);

    UpdateDuelFlag(now);

    CheckDuelDistance(now);

    UpdateAfkReport(now);

    if (isCharmed())
        if (Unit* charmer = GetCharmer())
            if (charmer->GetTypeId() == TYPEID_UNIT && charmer->isAlive())
                UpdateCharmedAI();

    // Update items that have just a limited lifetime
    if (now > m_Last_tick)
        UpdateItemDuration(uint32(now - m_Last_tick));

    // check every second
    if (now > m_Last_tick + 1)
        UpdateSoulboundTradeItems();
        
    // If mute expired, remove it from the DB
    if (GetSession()->m_muteTime && GetSession()->m_muteTime < now)
    {
        GetSession()->m_muteTime = 0;
        PreparedStatement* stmt = LoginDatabase.GetPreparedStatement(LOGIN_UPD_MUTE_TIME);
        stmt->setInt64(0, 0); // Set the mute time to 0
        stmt->setUInt32(1, GetSession()->GetAccountId());
        LoginDatabase.Execute(stmt);
    }

    if (!m_timedquests.empty())
    {
        QuestSet::iterator iter = m_timedquests.begin();
        while (iter != m_timedquests.end())
        {
            QuestStatusData& q_status = m_QuestStatus[*iter];
            if (q_status.Timer <= p_time)
            {
                uint32 quest_id  = *iter;
                ++iter;                                     // current iter will be removed in FailQuest
                FailQuest(quest_id);
            }
            else
            {
                q_status.Timer -= p_time;
                m_QuestStatusSave[*iter] = true;
                ++iter;
            }
        }
    }

    m_achievementMgr.UpdateTimedAchievements(p_time);

    if (HasUnitState(UNIT_STATE_MELEE_ATTACKING) && !HasUnitState(UNIT_STATE_CASTING))
    {
        if (Unit* victim = getVictim())
        {
            // default combat reach 10
            // TODO add weapon, skill check

            if (isAttackReady(BASE_ATTACK))
            {
                if (!IsWithinMeleeRange(victim))
                {
                    setAttackTimer(BASE_ATTACK, 100);
                    if (m_swingErrorMsg != 1)               // send single time (client auto repeat)
                    {
                        SendAttackSwingNotInRange();
                        m_swingErrorMsg = 1;
                    }
                }
                //120 degrees of radiant range
                else if (!HasInArc(2*M_PI/3, victim))
                {
                    setAttackTimer(BASE_ATTACK, 100);
                    if (m_swingErrorMsg != 2)               // send single time (client auto repeat)
                    {
                        SendAttackSwingBadFacingAttack();
                        m_swingErrorMsg = 2;
                    }
                }
                else
                {
                    m_swingErrorMsg = 0;                    // reset swing error state

                    // prevent base and off attack in same time, delay attack at 0.2 sec
                    if (haveOffhandWeapon())
                        if (getAttackTimer(OFF_ATTACK) < ATTACK_DISPLAY_DELAY)
                            setAttackTimer(OFF_ATTACK, ATTACK_DISPLAY_DELAY);

                    // do attack
                    AttackerStateUpdate(victim, BASE_ATTACK);
                    resetAttackTimer(BASE_ATTACK);
                }
            }

            if (haveOffhandWeapon() && isAttackReady(OFF_ATTACK))
            {
                if (!IsWithinMeleeRange(victim))
                    setAttackTimer(OFF_ATTACK, 100);
                else if (!HasInArc(2*M_PI/3, victim))
                    setAttackTimer(OFF_ATTACK, 100);
                else
                {
                    // prevent base and off attack in same time, delay attack at 0.2 sec
                    if (getAttackTimer(BASE_ATTACK) < ATTACK_DISPLAY_DELAY)
                        setAttackTimer(BASE_ATTACK, ATTACK_DISPLAY_DELAY);

                    // do attack
                    AttackerStateUpdate(victim, OFF_ATTACK);
                    resetAttackTimer(OFF_ATTACK);
                }
            }

            /*Unit* owner = victim->GetOwner();
            Unit* u = owner ? owner : victim;
            if (u->IsPvP() && (!duel || duel->opponent != u))
            {
                UpdatePvP(true);
                RemoveAurasWithInterruptFlags(AURA_INTERRUPT_FLAG_ENTER_PVP_COMBAT);
            }*/
        }
    }

    if (HasFlag(PLAYER_FLAGS, PLAYER_FLAGS_RESTING))
    {
        if (roll_chance_i(3) && GetTimeInnEnter() > 0)      // freeze update
        {
            time_t time_inn = time(NULL)-GetTimeInnEnter();
            if (time_inn >= 10)                             // freeze update
            {
                float bubble = 0.125f*sWorld->getRate(RATE_REST_INGAME);
                                                            // speed collect rest bonus (section/in hour)
                SetRestBonus(GetRestBonus()+ time_inn*((float)GetUInt32Value(PLAYER_NEXT_LEVEL_XP)/72000)*bubble);
                UpdateInnerTime(time(NULL));
            }
        }
    }

    if (m_weaponChangeTimer > 0)
    {
        if (p_time >= m_weaponChangeTimer)
            m_weaponChangeTimer = 0;
        else
            m_weaponChangeTimer -= p_time;
    }

    if (m_zoneUpdateTimer > 0)
    {
        if (p_time >= m_zoneUpdateTimer)
        {
            uint32 newzone, newarea;
            GetZoneAndAreaId(newzone, newarea);

            if (m_zoneUpdateId != newzone)
                UpdateZone(newzone, newarea);                // also update area
            else
            {
                // use area updates as well
                // needed for free far all arenas for example
                if (m_areaUpdateId != newarea)
                    UpdateArea(newarea);

                m_zoneUpdateTimer = ZONE_UPDATE_INTERVAL;
            }
        }
        else
            m_zoneUpdateTimer -= p_time;
    }

    if (m_timeSyncTimer > 0)
    {
        if (p_time >= m_timeSyncTimer)
            SendTimeSync();
        else
            m_timeSyncTimer -= p_time;
    }

    if (isAlive())
    {
        m_regenTimer += p_time;
        RegenerateAll();
    }

    if (m_deathState == JUST_DIED)
        KillPlayer();

    if (m_nextSave > 0)
    {
        if (p_time >= m_nextSave)
        {
            // m_nextSave reseted in SaveToDB call
            SaveToDB();
            sLog->outDebug(LOG_FILTER_PLAYER, "Player '%s' (GUID: %u) saved", GetName(), GetGUIDLow());
        }
        else
            m_nextSave -= p_time;
    }

    //Handle Water/drowning
    HandleDrowning(p_time);

    // Played time
    if (now > m_Last_tick)
    {
        uint32 elapsed = uint32(now - m_Last_tick);
        m_Played_time[PLAYED_TIME_TOTAL] += elapsed;        // Total played time
        m_Played_time[PLAYED_TIME_LEVEL] += elapsed;        // Level played time
        m_Last_tick = now;
    }

    if (GetDrunkValue())
    {
        m_drunkTimer += p_time;
        if (m_drunkTimer > 9 * IN_MILLISECONDS)
            HandleSobering();
    }

    if (HasPendingBind())
    {
        if (_pendingBindTimer <= p_time)
        {
            // Player left the instance
            if (_pendingBindId == GetInstanceId())
                BindToInstance();
            SetPendingBind(0, 0);
        }
        else
            _pendingBindTimer -= p_time;
    }

    // not auto-free ghost from body in instances
    if (m_deathTimer > 0 && !GetBaseMap()->Instanceable() && !HasAuraType(SPELL_AURA_PREVENT_RESURRECTION))
    {
        if (p_time >= m_deathTimer)
        {
            m_deathTimer = 0;
            BuildPlayerRepop();
            RepopAtGraveyard();
        }
        else
            m_deathTimer -= p_time;
    }

    UpdateEnchantTime(p_time);
    UpdateHomebindTime(p_time);

    if (!_instanceResetTimes.empty())
    {
        for (InstanceTimeMap::iterator itr = _instanceResetTimes.begin(); itr != _instanceResetTimes.end();)
        {
            if (itr->second < now)
                _instanceResetTimes.erase(itr++);
            else
                ++itr;
        }
    }

    // group update
    SendUpdateToOutOfRangeGroupMembers();

    Pet* pet = GetPet();
    if (pet && !pet->IsWithinDistInMap(this, GetMap()->GetVisibilityRange()) && !pet->isPossessed())
    //if (pet && !pet->IsWithinDistInMap(this, GetMap()->GetVisibilityDistance()) && (GetCharmGUID() && (pet->GetGUID() != GetCharmGUID())))
        RemovePet(pet, PET_SAVE_NOT_IN_SLOT, true);

    //we should execute delayed teleports only for alive(!) players
    //because we don't want player's ghost teleported from graveyard
    if (IsHasDelayedTeleport() && isAlive())
        TeleportTo(m_teleport_dest, m_teleport_options);
}

void Player::setDeathState(DeathState s)
{
    uint32 ressSpellId = 0;

    bool cur = isAlive();

    if (s == JUST_DIED)
    {
        if (!cur)
        {
            sLog->outError(LOG_FILTER_PLAYER, "setDeathState: attempt to kill a dead player %s(%d)", GetName(), GetGUIDLow());
            return;
        }

        // send spectate addon message
        if (HaveSpectators())
        {
            SpectatorAddonMsg msg;
            msg.SetPlayer(GetName());
            msg.SetStatus(false);
            SendSpectatorAddonMsgToBG(msg);
        }


        // drunken state is cleared on death
        SetDrunkValue(0);
        // lost combo points at any target (targeted combo points clear in Unit::setDeathState)
        ClearComboPoints();

        clearResurrectRequestData();

        //FIXME: is pet dismissed at dying or releasing spirit? if second, add setDeathState(DEAD) to HandleRepopRequestOpcode and define pet unsummon here with (s == DEAD)
        RemovePet(NULL, PET_SAVE_NOT_IN_SLOT, true);

        // save value before aura remove in Unit::setDeathState
        ressSpellId = GetUInt32Value(PLAYER_SELF_RES_SPELL);

        // passive spell
        if (!ressSpellId)
            ressSpellId = GetResurrectionSpellId();
        UpdateAchievementCriteria(ACHIEVEMENT_CRITERIA_TYPE_DEATH_AT_MAP, 1);
        UpdateAchievementCriteria(ACHIEVEMENT_CRITERIA_TYPE_DEATH, 1);
        UpdateAchievementCriteria(ACHIEVEMENT_CRITERIA_TYPE_DEATH_IN_DUNGEON, 1);
        ResetAchievementCriteria(ACHIEVEMENT_CRITERIA_TYPE_BG_OBJECTIVE_CAPTURE, ACHIEVEMENT_CRITERIA_CONDITION_NO_DEATH);
        ResetAchievementCriteria(ACHIEVEMENT_CRITERIA_TYPE_HONORABLE_KILL, ACHIEVEMENT_CRITERIA_CONDITION_NO_DEATH);
        ResetAchievementCriteria(ACHIEVEMENT_CRITERIA_TYPE_GET_KILLING_BLOWS, ACHIEVEMENT_CRITERIA_CONDITION_NO_DEATH);
    }

    Unit::setDeathState(s);

    // restore resurrection spell id for player after aura remove
    if (s == JUST_DIED && cur && ressSpellId)
        SetUInt32Value(PLAYER_SELF_RES_SPELL, ressSpellId);


    if (isAlive() && !cur)
        //clear aura case after resurrection by another way (spells will be applied before next death)
        SetUInt32Value(PLAYER_SELF_RES_SPELL, 0);
}

void Player::SetSelection(uint64 guid)
{
    m_curSelection = guid;
    SetUInt64Value(UNIT_FIELD_TARGET, guid);
    if (Player *target = ObjectAccessor::FindPlayer(guid))
        if (HaveSpectators())
        {
            SpectatorAddonMsg msg;
            msg.SetPlayer(GetName());
            msg.SetTarget(target->GetName());
            SendSpectatorAddonMsgToBG(msg);
        }
}

bool Player::BuildEnumData(PreparedQueryResult result, WorldPacket* data)
{
    //             0               1                2                3                 4                  5                       6                        7
    //    "SELECT characters.guid, characters.name, characters.race, characters.class, characters.gender, characters.playerBytes, characters.playerBytes2, characters.level, "
    //     8                9               10                     11                     12                     13                    14
    //    "characters.zone, characters.map, characters.position_x, characters.position_y, characters.position_z, guild_member.guildid, characters.playerFlags, "
    //    15                    16                   17                     18                   19               20                     21
    //    "characters.at_login, character_pet.entry, character_pet.modelid, character_pet.level, characters.data, character_banned.guid, character_declinedname.genitive "

    Field* fields = result->Fetch();

    uint32 guid = fields[0].GetUInt32();
    uint8 plrRace = fields[2].GetUInt8();
    uint8 plrClass = fields[3].GetUInt8();
    uint8 gender = fields[4].GetUInt8();

    PlayerInfo const* info = sObjectMgr->GetPlayerInfo(plrRace, plrClass);
    if (!info)
    {
        sLog->outError(LOG_FILTER_PLAYER_LOADING, "Player %u has incorrect race/class pair. Don't build enum.", guid);
        return false;
    }
    else if (!IsValidGender(gender))
    {
        sLog->outError(LOG_FILTER_PLAYER_LOADING, "Player (%u) has incorrect gender (%hu), don't build enum.", guid, gender);
        return false;
    }

    *data << uint64(MAKE_NEW_GUID(guid, 0, HIGHGUID_PLAYER));
    *data << fields[1].GetString();                         // name
    *data << uint8(plrRace);                                // race
    *data << uint8(plrClass);                               // class
    *data << uint8(gender);                                 // gender

    uint32 playerBytes = fields[5].GetUInt32();
    *data << uint8(playerBytes);                            // skin
    *data << uint8(playerBytes >> 8);                       // face
    *data << uint8(playerBytes >> 16);                      // hair style
    *data << uint8(playerBytes >> 24);                      // hair color

    uint32 playerBytes2 = fields[6].GetUInt32();
    *data << uint8(playerBytes2 & 0xFF);                    // facial hair

    *data << uint8(fields[7].GetUInt8());                   // level
    *data << uint32(fields[8].GetUInt16());                 // zone
    *data << uint32(fields[9].GetUInt16());                 // map

    *data << fields[10].GetFloat();                         // x
    *data << fields[11].GetFloat();                         // y
    *data << fields[12].GetFloat();                         // z

    *data << uint32(fields[13].GetUInt32());                // guild id

    uint32 charFlags = 0;
    uint32 playerFlags = fields[14].GetUInt32();
    uint16 atLoginFlags = fields[15].GetUInt16();
    if (playerFlags & PLAYER_FLAGS_HIDE_HELM)
        charFlags |= CHARACTER_FLAG_HIDE_HELM;
    if (playerFlags & PLAYER_FLAGS_HIDE_CLOAK)
        charFlags |= CHARACTER_FLAG_HIDE_CLOAK;
    if (playerFlags & PLAYER_FLAGS_GHOST)
        charFlags |= CHARACTER_FLAG_GHOST;
    if (atLoginFlags & AT_LOGIN_RENAME)
        charFlags |= CHARACTER_FLAG_RENAME;
    if (fields[20].GetUInt32())
        charFlags |= CHARACTER_FLAG_LOCKED_BY_BILLING;
    if (sWorld->getBoolConfig(CONFIG_DECLINED_NAMES_USED))
    {
        if (!fields[21].GetString().empty())
            charFlags |= CHARACTER_FLAG_DECLINED;
    }
    else
        charFlags |= CHARACTER_FLAG_DECLINED;

    *data << uint32(charFlags);                             // character flags

    // character customize flags
    if (atLoginFlags & AT_LOGIN_CUSTOMIZE)
        *data << uint32(CHAR_CUSTOMIZE_FLAG_CUSTOMIZE);
    else if (atLoginFlags & AT_LOGIN_CHANGE_FACTION)
        *data << uint32(CHAR_CUSTOMIZE_FLAG_FACTION);
    else if (atLoginFlags & AT_LOGIN_CHANGE_RACE)
        *data << uint32(CHAR_CUSTOMIZE_FLAG_RACE);
    else
        *data << uint32(CHAR_CUSTOMIZE_FLAG_NONE);

    // First login
    *data << uint8(atLoginFlags & AT_LOGIN_FIRST ? 1 : 0);

    // Pets info
    uint32 petDisplayId = 0;
    uint32 petLevel = 0;
    uint32 petFamily = 0;

    // show pet at selection character in character list only for non-ghost character
    if (result && !(playerFlags & PLAYER_FLAGS_GHOST) && (plrClass == CLASS_WARLOCK || plrClass == CLASS_HUNTER || plrClass == CLASS_DEATH_KNIGHT))
    {
        uint32 entry = fields[16].GetUInt32();
        CreatureTemplate const* creatureInfo = sObjectMgr->GetCreatureTemplate(entry);
        if (creatureInfo)
        {
            petDisplayId = fields[17].GetUInt32();
            petLevel = fields[18].GetUInt16();
            petFamily = creatureInfo->family;
        }
    }

    *data << uint32(petDisplayId);
    *data << uint32(petLevel);
    *data << uint32(petFamily);

    Tokenizer equipment(fields[19].GetString(), ' ');
    for (uint8 slot = 0; slot < INVENTORY_SLOT_BAG_END; ++slot)
    {
        uint32 visualBase = slot * 2;
        uint32 itemId = GetUInt32ValueFromArray(equipment, visualBase);
        ItemTemplate const* proto = sObjectMgr->GetItemTemplate(itemId);
        if (!proto)
        {
            *data << uint32(0);
            *data << uint8(0);
            *data << uint32(0);
            continue;
        }

        SpellItemEnchantmentEntry const* enchant = NULL;

        uint32 enchants = GetUInt32ValueFromArray(equipment, visualBase + 1);
        for (uint8 enchantSlot = PERM_ENCHANTMENT_SLOT; enchantSlot <= TEMP_ENCHANTMENT_SLOT; ++enchantSlot)
        {
            // values stored in 2 uint16
            uint32 enchantId = 0x0000FFFF & (enchants >> enchantSlot*16);
            if (!enchantId)
                continue;

            enchant = sSpellItemEnchantmentStore.LookupEntry(enchantId);
            if (enchant)
                break;
        }

        *data << uint32(proto->DisplayInfoID);
        *data << uint8(proto->InventoryType);
        *data << uint32(enchant ? enchant->aura_id : 0);
    }

    return true;
}

bool Player::ToggleAFK()
{
    ToggleFlag(PLAYER_FLAGS, PLAYER_FLAGS_AFK);

    bool state = HasFlag(PLAYER_FLAGS, PLAYER_FLAGS_AFK);

    // afk player not allowed in battleground
    if (state && InBattleground() && !InArena())
        LeaveBattleground();

    return state;
}

bool Player::ToggleDND()
{
    ToggleFlag(PLAYER_FLAGS, PLAYER_FLAGS_DND);

    return HasFlag(PLAYER_FLAGS, PLAYER_FLAGS_DND);
}

uint8 Player::GetChatTag() const
{
    uint8 tag = CHAT_TAG_NONE;

    if (isGMChat())
        tag |= CHAT_TAG_GM;
    if (isDND())
        tag |= CHAT_TAG_DND;
    if (isAFK())
        tag |= CHAT_TAG_AFK;
    if (HasFlag(PLAYER_FLAGS, PLAYER_FLAGS_DEVELOPER))
        tag |= CHAT_TAG_DEV;

    return tag;
}

void Player::SendTeleportPacket(Position &oldPos)
{
    WorldPacket data2(MSG_MOVE_TELEPORT, 38);
    data2.append(GetPackGUID());
    BuildMovementPacket(&data2);
    Relocate(&oldPos);
    SendMessageToSet(&data2, false);
}

void Player::SendTeleportAckPacket()
{
    WorldPacket data(MSG_MOVE_TELEPORT_ACK, 41);
    data.append(GetPackGUID());
    data << uint32(0);                                     // this value increments every time
    BuildMovementPacket(&data);
    GetSession()->SendPacket(&data);
}

bool Player::TeleportTo(uint32 mapid, float x, float y, float z, float orientation, uint32 options)
{
    if (!MapManager::IsValidMapCoord(mapid, x, y, z, orientation))
    {
        sLog->outError(LOG_FILTER_MAPS, "TeleportTo: invalid map (%d) or invalid coordinates (X: %f, Y: %f, Z: %f, O: %f) given when teleporting player (GUID: %u, name: %s, map: %d, X: %f, Y: %f, Z: %f, O: %f).",
            mapid, x, y, z, orientation, GetGUIDLow(), GetName(), GetMapId(), GetPositionX(), GetPositionY(), GetPositionZ(), GetOrientation());
        return false;
    }

    if (AccountMgr::IsPlayerAccount(GetSession()->GetSecurity()) && DisableMgr::IsDisabledFor(DISABLE_TYPE_MAP, mapid, this))
    {
        sLog->outError(LOG_FILTER_MAPS, "Player (GUID: %u, name: %s) tried to enter a forbidden map %u", GetGUIDLow(), GetName(), mapid);
        SendTransferAborted(mapid, TRANSFER_ABORT_MAP_NOT_ALLOWED);
        return false;
    }

    // preparing unsummon pet if lost (we must get pet before teleportation or will not find it later)
    Pet* pet = GetPet();

    MapEntry const* mEntry = sMapStore.LookupEntry(mapid);

    // don't let enter battlegrounds without assigned battleground id (for example through areatrigger)...
    // don't let gm level > 1 either
    if (!InBattleground() && mEntry->IsBattlegroundOrArena())
        return false;

    // client without expansion support
    if (GetSession()->Expansion() < mEntry->Expansion())
    {
        sLog->outDebug(LOG_FILTER_MAPS, "Player %s using client without required expansion tried teleport to non accessible map %u", GetName(), mapid);

        if (GetTransport())
        {
            m_transport->RemovePassenger(this);
            m_transport = NULL;
            m_movementInfo.t_pos.Relocate(0.0f, 0.0f, 0.0f, 0.0f);
            m_movementInfo.t_time = 0;
            m_movementInfo.t_seat = -1;
            RepopAtGraveyard();                             // teleport to near graveyard if on transport, looks blizz like :)
        }

        SendTransferAborted(mapid, TRANSFER_ABORT_INSUF_EXPAN_LVL, mEntry->Expansion());

        return false;                                       // normal client can't teleport to this map...
    }
    else
        sLog->outDebug(LOG_FILTER_MAPS, "Player %s is being teleported to map %u", GetName(), mapid);

    if (m_vehicle)
        ExitVehicle();

    // reset movement flags at teleport, because player will continue move with these flags after teleport
    SetUnitMovementFlags(0);
    DisableSpline();

    if (m_transport)
    {
        if (options & TELE_TO_NOT_LEAVE_TRANSPORT)
            AddUnitMovementFlag(MOVEMENTFLAG_ONTRANSPORT);
        else
        {
            m_transport->RemovePassenger(this);
            m_transport = NULL;
            m_movementInfo.t_pos.Relocate(0.0f, 0.0f, 0.0f, 0.0f);
            m_movementInfo.t_time = 0;
            m_movementInfo.t_seat = -1;
        }
    }

    // The player was ported to another map and loses the duel immediately.
    // We have to perform this check before the teleport, otherwise the
    // ObjectAccessor won't find the flag.
    if (duel && GetMapId() != mapid && GetMap()->GetGameObject(GetUInt64Value(PLAYER_DUEL_ARBITER)))
        DuelComplete(DUEL_FLED);

    if (GetMapId() == mapid)
    {
        //lets reset far teleport flag if it wasn't reset during chained teleports
        SetSemaphoreTeleportFar(false);
        //setup delayed teleport flag
        SetDelayedTeleportFlag(IsCanDelayTeleport());
        //if teleport spell is casted in Unit::Update() func
        //then we need to delay it until update process will be finished
        if (IsHasDelayedTeleport())
        {
            SetSemaphoreTeleportNear(true);
            //lets save teleport destination for player
            m_teleport_dest = WorldLocation(mapid, x, y, z, orientation);
            m_teleport_options = options;
            return true;
        }

        if (!(options & TELE_TO_NOT_UNSUMMON_PET))
        {
            //same map, only remove pet if out of range for new position
            if (pet && !pet->IsWithinDist3d(x, y, z, GetMap()->GetVisibilityRange()))
                UnsummonPetTemporaryIfAny();
        }

        if (!(options & TELE_TO_NOT_LEAVE_COMBAT))
            CombatStop();

        // this will be used instead of the current location in SaveToDB
        m_teleport_dest = WorldLocation(mapid, x, y, z, orientation);
        SetFallInformation(0, z);

        // code for finish transfer called in WorldSession::HandleMovementOpcodes()
        // at client packet MSG_MOVE_TELEPORT_ACK
        SetSemaphoreTeleportNear(true);
        // near teleport, triggering send MSG_MOVE_TELEPORT_ACK from client at landing
        if (!GetSession()->PlayerLogout())
        {
            Position oldPos;
            GetPosition(&oldPos);
            Relocate(x, y, z, orientation);
            SendTeleportAckPacket();
            SendTeleportPacket(oldPos); // this automatically relocates to oldPos in order to broadcast the packet in the right place
        }
    }
    else
    {
        if (getClass() == CLASS_DEATH_KNIGHT && GetMapId() == 609 && !isGameMaster() && !HasSpell(50977))
            return false;

        // far teleport to another map
        Map* oldmap = IsInWorld() ? GetMap() : NULL;
        // check if we can enter before stopping combat / removing pet / totems / interrupting spells

        // Check enter rights before map getting to avoid creating instance copy for player
        // this check not dependent from map instance copy and same for all instance copies of selected map
        if (!sMapMgr->CanPlayerEnter(mapid, this, false))
            return false;

        //I think this always returns true. Correct me if I am wrong.
        // If the map is not created, assume it is possible to enter it.
        // It will be created in the WorldPortAck.
        //Map* map = sMapMgr->FindBaseNonInstanceMap(mapid);
        //if (!map || map->CanEnter(this))
        {
            //lets reset near teleport flag if it wasn't reset during chained teleports
            SetSemaphoreTeleportNear(false);
            //setup delayed teleport flag
            SetDelayedTeleportFlag(IsCanDelayTeleport());
            //if teleport spell is casted in Unit::Update() func
            //then we need to delay it until update process will be finished
            if (IsHasDelayedTeleport())
            {
                SetSemaphoreTeleportFar(true);
                //lets save teleport destination for player
                m_teleport_dest = WorldLocation(mapid, x, y, z, orientation);
                m_teleport_options = options;
                return true;
            }

            SetSelection(0);

            CombatStop();

            ResetContestedPvP();

            // remove player from battleground on far teleport (when changing maps)
            if (Battleground const* bg = GetBattleground())
            {
                // Note: at battleground join battleground id set before teleport
                // and we already will found "current" battleground
                // just need check that this is targeted map or leave
                if (bg->GetMapId() != mapid)
                    LeaveBattleground(false);                   // don't teleport to entry point
            }

            // remove arena spell coldowns/buffs now to also remove pet's cooldowns before it's temporarily unsummoned
            if (mEntry->IsBattleArena())
            {
                RemoveArenaSpellCooldowns(true);
                RemoveArenaAuras();
                if (pet)
                    pet->RemoveArenaAuras();
            }

            // remove pet on map change
            if (pet)
                UnsummonPetTemporaryIfAny();

            // remove all dyn objects
            RemoveAllDynObjects();

            // stop spellcasting
            // not attempt interrupt teleportation spell at caster teleport
            if (!(options & TELE_TO_SPELL))
                if (IsNonMeleeSpellCasted(true))
                    InterruptNonMeleeSpells(true);

            //remove auras before removing from map...
            RemoveAurasWithInterruptFlags(AURA_INTERRUPT_FLAG_CHANGE_MAP | AURA_INTERRUPT_FLAG_MOVE | AURA_INTERRUPT_FLAG_TURNING);

            if (!GetSession()->PlayerLogout())
            {
                // send transfer packets
                WorldPacket data(SMSG_TRANSFER_PENDING, 4 + 4 + 4);
                data << uint32(mapid);
                if (m_transport)
                    data << m_transport->GetEntry() << GetMapId();

                GetSession()->SendPacket(&data);
            }

            // remove from old map now
            if (oldmap)
                oldmap->RemovePlayerFromMap(this, false);

            // new final coordinates
            float final_x = x;
            float final_y = y;
            float final_z = z;
            float final_o = orientation;

            if (m_transport)
            {
                final_x += m_movementInfo.t_pos.GetPositionX();
                final_y += m_movementInfo.t_pos.GetPositionY();
                final_z += m_movementInfo.t_pos.GetPositionZ();
                final_o += m_movementInfo.t_pos.GetOrientation();
            }

            m_teleport_dest = WorldLocation(mapid, final_x, final_y, final_z, final_o);
            SetFallInformation(0, final_z);
            // if the player is saved before worldportack (at logout for example)
            // this will be used instead of the current location in SaveToDB

            if (!GetSession()->PlayerLogout())
            {
                WorldPacket data(SMSG_NEW_WORLD, 4 + 4 + 4 + 4 + 4);
                data << uint32(mapid);
                if (m_transport)
                    data << m_movementInfo.t_pos.PositionXYZOStream();
                else
                    data << m_teleport_dest.PositionXYZOStream();

                GetSession()->SendPacket(&data);
                SendSavedInstances();
            }

            // move packet sent by client always after far teleport
            // code for finish transfer to new map called in WorldSession::HandleMoveWorldportAckOpcode at client packet
            SetSemaphoreTeleportFar(true);
        }
        //else
        //    return false;
    }
    return true;
}

bool Player::TeleportToBGEntryPoint()
{
    if (m_bgData.joinPos.m_mapId == MAPID_INVALID)
        return false;

    ScheduleDelayedOperation(DELAYED_BG_MOUNT_RESTORE);
    ScheduleDelayedOperation(DELAYED_BG_TAXI_RESTORE);
    ScheduleDelayedOperation(DELAYED_BG_GROUP_RESTORE);
    Battleground *oldBg = GetBattleground();
    bool result = TeleportTo(m_bgData.joinPos);

    if (isSpectator() && result)
    {
        SetSpectate(false);
        if (oldBg)
            oldBg->RemoveSpectator(GetGUID());
    }

    return result;
}

void Player::ProcessDelayedOperations()
{
    if (m_DelayedOperations == 0)
        return;

    if (m_DelayedOperations & DELAYED_RESURRECT_PLAYER)
    {
        ResurrectPlayer(0.0f, false);

        if (GetMaxHealth() > m_resurrectHealth)
            SetHealth(m_resurrectHealth);
        else
            SetFullHealth();

        if (GetMaxPower(POWER_MANA) > m_resurrectMana)
            SetPower(POWER_MANA, m_resurrectMana);
        else
            SetPower(POWER_MANA, GetMaxPower(POWER_MANA));

        SetPower(POWER_RAGE, 0);
        SetPower(POWER_ENERGY, GetMaxPower(POWER_ENERGY));

        SpawnCorpseBones();
    }

    if (m_DelayedOperations & DELAYED_SAVE_PLAYER)
        SaveToDB();

    if (m_DelayedOperations & DELAYED_SPELL_CAST_DESERTER)
        CastSpell(this, 26013, true);               // Deserter

    if (m_DelayedOperations & DELAYED_BG_MOUNT_RESTORE)
    {
        if (m_bgData.mountSpell)
        {
            CastSpell(this, m_bgData.mountSpell, true);
            m_bgData.mountSpell = 0;
        }
    }

    if (m_DelayedOperations & DELAYED_BG_TAXI_RESTORE)
    {
        if (m_bgData.HasTaxiPath())
        {
            m_taxi.AddTaxiDestination(m_bgData.taxiPath[0]);
            m_taxi.AddTaxiDestination(m_bgData.taxiPath[1]);
            m_bgData.ClearTaxiPath();

            ContinueTaxiFlight();
        }
    }

    if (m_DelayedOperations & DELAYED_BG_GROUP_RESTORE)
    {
        if (Group *g = GetGroup())
            g->SendUpdateToPlayer(GetGUID());
    }

    //we have executed ALL delayed ops, so clear the flag
    m_DelayedOperations = 0;
}

void Player::AddToWorld()
{
    ///- Do not add/remove the player from the object storage
    ///- It will crash when updating the ObjectAccessor
    ///- The player should only be added when logging in
    Unit::AddToWorld();

    for (uint8 i = PLAYER_SLOT_START; i < PLAYER_SLOT_END; ++i)
        if (m_items[i])
            m_items[i]->AddToWorld();
}

void Player::RemoveFromWorld()
{
    // cleanup
    if (IsInWorld())
    {
        ///- Release charmed creatures, unsummon totems and remove pets/guardians
        StopCastingCharm();
        StopCastingBindSight();
        UnsummonPetTemporaryIfAny();
        sOutdoorPvPMgr->HandlePlayerLeaveZone(this, m_zoneUpdateId);
        sBattlefieldMgr->HandlePlayerLeaveZone(this, m_zoneUpdateId);
    }

    ///- Do not add/remove the player from the object storage
    ///- It will crash when updating the ObjectAccessor
    ///- The player should only be removed when logging out
    Unit::RemoveFromWorld();

    for (uint8 i = PLAYER_SLOT_START; i < PLAYER_SLOT_END; ++i)
    {
        if (m_items[i])
            m_items[i]->RemoveFromWorld();
    }

    for (ItemMap::iterator iter = mMitems.begin(); iter != mMitems.end(); ++iter)
        iter->second->RemoveFromWorld();

    if (m_uint32Values)
    {
        if (WorldObject* viewpoint = GetViewpoint())
        {
            sLog->outError(LOG_FILTER_PLAYER, "Player %s has viewpoint %u %u when removed from world", GetName(), viewpoint->GetEntry(), viewpoint->GetTypeId());
            SetViewpoint(viewpoint, false);
        }
    }
}

void Player::RegenerateAll()
{
    //if (m_regenTimer <= 500)
    //    return;

    m_regenTimerCount += m_regenTimer;

    Regenerate(POWER_ENERGY);

    Regenerate(POWER_MANA);

    // Runes act as cooldowns, and they don't need to send any data
    if (getClass() == CLASS_DEATH_KNIGHT)
        for (uint8 i = 0; i < MAX_RUNES; ++i)
            if (uint32 cd = GetRuneCooldown(i))
                SetRuneCooldown(i, (cd > m_regenTimer) ? cd - m_regenTimer : 0);

    if (m_regenTimerCount >= 2000)
    {
        // Not in combat or they have regeneration
        if (!isInCombat() || IsPolymorphed() || m_baseHealthRegen ||
            HasAuraType(SPELL_AURA_MOD_REGEN_DURING_COMBAT) ||
            HasAuraType(SPELL_AURA_MOD_HEALTH_REGEN_IN_COMBAT))
        {
            RegenerateHealth();
        }

        Regenerate(POWER_RAGE);
        if (getClass() == CLASS_DEATH_KNIGHT)
            Regenerate(POWER_RUNIC_POWER);

        m_regenTimerCount -= 2000;
    }

    m_regenTimer = 0;
}

void Player::Regenerate(Powers power)
{
    uint32 maxValue = GetMaxPower(power);
    if (!maxValue)
        return;

    uint32 curValue = GetPower(power);

    // TODO: possible use of miscvalueb instead of amount
    if (HasAuraTypeWithValue(SPELL_AURA_PREVENT_REGENERATE_POWER, power))
        return;

    float addvalue = 0.0f;

    switch (power)
    {
        case POWER_MANA:
        {
            bool recentCast = IsUnderLastManaUseEffect();
            float ManaIncreaseRate = sWorld->getRate(RATE_POWER_MANA);

            if (getLevel() < 15)
                ManaIncreaseRate = sWorld->getRate(RATE_POWER_MANA) * (2.066f - (getLevel() * 0.066f));

            if (recentCast) // Trinity Updates Mana in intervals of 2s, which is correct
                addvalue += GetFloatValue(UNIT_FIELD_POWER_REGEN_INTERRUPTED_FLAT_MODIFIER) *  ManaIncreaseRate * 0.001f * m_regenTimer;
            else
                addvalue += GetFloatValue(UNIT_FIELD_POWER_REGEN_FLAT_MODIFIER) * ManaIncreaseRate * 0.001f * m_regenTimer;
        }   break;
        case POWER_RAGE:                                    // Regenerate rage
        {
            if (!isInCombat() && !HasAuraType(SPELL_AURA_INTERRUPT_REGEN))
            {
                float RageDecreaseRate = sWorld->getRate(RATE_POWER_RAGE_LOSS);
                addvalue += -20 * RageDecreaseRate;               // 2 rage by tick (= 2 seconds => 1 rage/sec)
            }
        }   break;
        case POWER_ENERGY:                                  // Regenerate energy (rogue)
            addvalue += 0.01f * m_regenTimer * sWorld->getRate(RATE_POWER_ENERGY);
            break;
        case POWER_RUNIC_POWER:
        {
            if (!isInCombat() && !HasAuraType(SPELL_AURA_INTERRUPT_REGEN))
            {
                float RunicPowerDecreaseRate = sWorld->getRate(RATE_POWER_RUNICPOWER_LOSS);
                addvalue += -30 * RunicPowerDecreaseRate;         // 3 RunicPower by tick
            }
        }   break;
        case POWER_RUNE:
        case POWER_FOCUS:
        case POWER_HAPPINESS:
        case POWER_HEALTH:
            break;
        default:
            break;
    }

    // Mana regen calculated in Player::UpdateManaRegen()
    if (power != POWER_MANA)
    {
        AuraEffectList const& ModPowerRegenPCTAuras = GetAuraEffectsByType(SPELL_AURA_MOD_POWER_REGEN_PERCENT);
        for (AuraEffectList::const_iterator i = ModPowerRegenPCTAuras.begin(); i != ModPowerRegenPCTAuras.end(); ++i)
            if (Powers((*i)->GetMiscValue()) == power)
                AddPct(addvalue, (*i)->GetAmount());

        // Butchery requires combat for this effect
        if (power != POWER_RUNIC_POWER || isInCombat())
            addvalue += GetTotalAuraModifierByMiscValue(SPELL_AURA_MOD_POWER_REGEN, power) * ((power != POWER_ENERGY) ? m_regenTimerCount : m_regenTimer) / (5 * IN_MILLISECONDS);
    }

    if (addvalue < 0.0f)
    {
        if (curValue == 0)
            return;
    }
    else if (addvalue > 0.0f)
    {
        if (curValue == maxValue)
            return;
    }
    else
        return;

    addvalue += m_powerFraction[power];
    uint32 integerValue = uint32(fabs(addvalue));

    if (addvalue < 0.0f)
    {
        if (curValue > integerValue)
        {
            curValue -= integerValue;
            m_powerFraction[power] = addvalue + integerValue;
        }
        else
        {
            curValue = 0;
            m_powerFraction[power] = 0;
        }
    }
    else
    {
        curValue += integerValue;

        if (curValue > maxValue)
        {
            curValue = maxValue;
            m_powerFraction[power] = 0;
        }
        else
            m_powerFraction[power] = addvalue - integerValue;
    }
    if (m_regenTimerCount >= 2000)
        SetPower(power, curValue);
    else
        UpdateUInt32Value(UNIT_FIELD_POWER1 + power, curValue);
}

void Player::RegenerateHealth()
{
    uint32 curValue = GetHealth();
    uint32 maxValue = GetMaxHealth();

    if (curValue >= maxValue)
        return;

    float HealthIncreaseRate = sWorld->getRate(RATE_HEALTH);

    if (getLevel() < 15)
        HealthIncreaseRate = sWorld->getRate(RATE_HEALTH) * (2.066f - (getLevel() * 0.066f));

    float addvalue = 0.0f;

    // polymorphed case
    if (IsPolymorphed())
        addvalue = (float)GetMaxHealth()/3;
    // normal regen case (maybe partly in combat case)
    else if (!isInCombat() || HasAuraType(SPELL_AURA_MOD_REGEN_DURING_COMBAT))
    {
        addvalue = OCTRegenHPPerSpirit() * HealthIncreaseRate;
        if (!isInCombat())
        {
            AuraEffectList const& mModHealthRegenPct = GetAuraEffectsByType(SPELL_AURA_MOD_HEALTH_REGEN_PERCENT);
            for (AuraEffectList::const_iterator i = mModHealthRegenPct.begin(); i != mModHealthRegenPct.end(); ++i)
                AddPct(addvalue, (*i)->GetAmount());

            addvalue += GetTotalAuraModifier(SPELL_AURA_MOD_REGEN) * 2 * IN_MILLISECONDS / (5 * IN_MILLISECONDS);
        }
        else if (HasAuraType(SPELL_AURA_MOD_REGEN_DURING_COMBAT))
            ApplyPct(addvalue, GetTotalAuraModifier(SPELL_AURA_MOD_REGEN_DURING_COMBAT));

        if (!IsStandState())
            addvalue *= 1.5f;
    }

    // always regeneration bonus (including combat)
    addvalue += GetTotalAuraModifier(SPELL_AURA_MOD_HEALTH_REGEN_IN_COMBAT);
    addvalue += m_baseHealthRegen / 2.5f;

    if (addvalue < 0)
        addvalue = 0;

    ModifyHealth(int32(addvalue));
}

void Player::ResetAllPowers()
{
    SetHealth(GetMaxHealth());
    switch (getPowerType())
    {
        case POWER_MANA:
            SetPower(POWER_MANA, GetMaxPower(POWER_MANA));
            break;
        case POWER_RAGE:
            SetPower(POWER_RAGE, 0);
            break;
        case POWER_ENERGY:
            SetPower(POWER_ENERGY, GetMaxPower(POWER_ENERGY));
            break;
        case POWER_RUNIC_POWER:
            SetPower(POWER_RUNIC_POWER, 0);
            break;
        default:
            break;
    }
}

bool Player::CanInteractWithQuestGiver(Object* questGiver)
{
    switch (questGiver->GetTypeId())
    {
        case TYPEID_UNIT:
            return GetNPCIfCanInteractWith(questGiver->GetGUID(), UNIT_NPC_FLAG_QUESTGIVER) != NULL;
        case TYPEID_GAMEOBJECT:
            return GetGameObjectIfCanInteractWith(questGiver->GetGUID(), GAMEOBJECT_TYPE_QUESTGIVER) != NULL;
        case TYPEID_PLAYER:
            return isAlive() && questGiver->ToPlayer()->isAlive();
        case TYPEID_ITEM:
            return isAlive();
        default:
            break;
    }
    return false;
}

Creature* Player::GetNPCIfCanInteractWith(uint64 guid, uint32 npcflagmask)
{
    // unit checks
    if (!guid)
        return NULL;

    if (!IsInWorld())
        return NULL;

    if (isInFlight())
        return NULL;

    // exist (we need look pets also for some interaction (quest/etc)
    Creature* creature = ObjectAccessor::GetCreatureOrPetOrVehicle(*this, guid);
    if (!creature)
        return NULL;

    // Deathstate checks
    if (!isAlive() && !(creature->GetCreatureTemplate()->type_flags & CREATURE_TYPEFLAGS_GHOST))
        return NULL;

    // alive or spirit healer
    if (!creature->isAlive() && !(creature->GetCreatureTemplate()->type_flags & CREATURE_TYPEFLAGS_DEAD_INTERACT))
        return NULL;

    // appropriate npc type
    if (npcflagmask && !creature->HasFlag(UNIT_NPC_FLAGS, npcflagmask))
        return NULL;

    // not allow interaction under control, but allow with own pets
    if (creature->GetCharmerGUID())
        return NULL;

    // not enemy
    if (creature->IsHostileTo(this))
        return NULL;

    // not unfriendly
    if (FactionTemplateEntry const* factionTemplate = sFactionTemplateStore.LookupEntry(creature->getFaction()))
        if (factionTemplate->faction)
            if (FactionEntry const* faction = sFactionStore.LookupEntry(factionTemplate->faction))
                if (faction->reputationListID >= 0 && GetReputationMgr().GetRank(faction) <= REP_UNFRIENDLY)
                    return NULL;

    // not too far
    if (!creature->IsWithinDistInMap(this, INTERACTION_DISTANCE))
        return NULL;

    return creature;
}

GameObject* Player::GetGameObjectIfCanInteractWith(uint64 guid, GameobjectTypes type) const
{
    if (GameObject* go = GetMap()->GetGameObject(guid))
    {
        if (go->GetGoType() == type)
        {
            float maxdist;
            switch (type)
            {
                // TODO: find out how the client calculates the maximal usage distance to spellless working
                // gameobjects like guildbanks and mailboxes - 10.0 is a just an abitrary choosen number
                case GAMEOBJECT_TYPE_GUILD_BANK:
                case GAMEOBJECT_TYPE_MAILBOX:
                    maxdist = 10.0f;
                    break;
                case GAMEOBJECT_TYPE_FISHINGHOLE:
                    maxdist = 20.0f+CONTACT_DISTANCE;       // max spell range
                    break;
                default:
                    maxdist = INTERACTION_DISTANCE;
                    break;
            }

            if (go->IsWithinDistInMap(this, maxdist))
                return go;

            sLog->outDebug(LOG_FILTER_MAPS, "IsGameObjectOfTypeInRange: GameObject '%s' [GUID: %u] is too far away from player %s [GUID: %u] to be used by him (distance=%f, maximal 10 is allowed)", go->GetGOInfo()->name.c_str(),
                go->GetGUIDLow(), GetName(), GetGUIDLow(), go->GetDistance(this));
        }
    }
    return NULL;
}

bool Player::IsUnderWater() const
{
    return IsInWater() &&
        GetPositionZ() < (GetBaseMap()->GetWaterLevel(GetPositionX(), GetPositionY())-2);
}

void Player::SetInWater(bool apply)
{
    if (m_isInWater == apply)
        return;

    //define player in water by opcodes
    //move player's guid into HateOfflineList of those mobs
    //which can't swim and move guid back into ThreatList when
    //on surface.
    //TODO: exist also swimming mobs, and function must be symmetric to enter/leave water
    m_isInWater = apply;

    // remove auras that need water/land
    RemoveAurasWithInterruptFlags(apply ? AURA_INTERRUPT_FLAG_NOT_ABOVEWATER : AURA_INTERRUPT_FLAG_NOT_UNDERWATER);

    getHostileRefManager().updateThreatTables();
}
void Player::SetSpectate(bool on)
{
    if (on)
    {
        SetSpeed(MOVE_RUN, 2.5);
        spectatorFlag = true;

        m_ExtraFlags |= PLAYER_EXTRA_GM_ON;
        setFaction(35);

        if (Pet* pet = GetPet())
        {
            RemovePet(pet, PET_SAVE_AS_CURRENT);
        }
        UnsummonPetTemporaryIfAny();

        RemoveByteFlag(UNIT_FIELD_BYTES_2, 1, UNIT_BYTE2_FLAG_FFA_PVP);
        ResetContestedPvP();

        getHostileRefManager().setOnlineOfflineState(false);
        CombatStopWithPets();

        // random dispay id`s
        uint32 morphs[8] = {25900};
        SetDisplayId(morphs[urand(0, 1)]);

        m_serverSideVisibility.SetValue(SERVERSIDE_VISIBILITY_GM, SEC_ADMINISTRATOR);
    }
    else
    {
        uint32 newPhase = 0;
        AuraEffectList const& phases = GetAuraEffectsByType(SPELL_AURA_PHASE);
        if (!phases.empty())
            for (AuraEffectList::const_iterator itr = phases.begin(); itr != phases.end(); ++itr)
                newPhase |= (*itr)->GetMiscValue();

        if (!newPhase)
            newPhase = PHASEMASK_NORMAL;

        SetPhaseMask(newPhase, false);

        m_ExtraFlags &= ~ PLAYER_EXTRA_GM_ON;
        setFactionForRace(getRace());
        RemoveFlag(PLAYER_FLAGS, PLAYER_FLAGS_GM);
        RemoveFlag(UNIT_FIELD_FLAGS_2, UNIT_FLAG2_ALLOW_CHEAT_SPELLS);

        if (spectateFrom)
            SetViewpoint(spectateFrom, false);

        // restore FFA PvP Server state
        if (sWorld->IsFFAPvPRealm())
            SetByteFlag(UNIT_FIELD_BYTES_2, 1, UNIT_BYTE2_FLAG_FFA_PVP);

        // restore FFA PvP area state, remove not allowed for GM mounts
        UpdateArea(m_areaUpdateId);

        getHostileRefManager().setOnlineOfflineState(true);
        m_serverSideVisibility.SetValue(SERVERSIDE_VISIBILITY_GM, SEC_PLAYER);
        spectateCanceled = false;
        spectatorFlag = false;
        RestoreDisplayId();
        UpdateSpeed(MOVE_RUN, true);
    }
    UpdateObjectVisibility();
}

bool Player::HaveSpectators()
{
    if (isSpectator())
        return false;

    if (Battleground *bg = GetBattleground())
        if (bg->isArena())
        {
            if (bg->GetStatus() != STATUS_IN_PROGRESS)
                return false;

            return bg->HaveSpectators();
        }

    return false;
}

void Player::SendSpectatorAddonMsgToBG(SpectatorAddonMsg msg)
{
    if (!HaveSpectators())
        return;

    GetBattleground()->SendSpectateAddonsMsg(msg);
}

void Player::SetGameMaster(bool on)
{
    if (on)
    {
        m_ExtraFlags |= PLAYER_EXTRA_GM_ON;
        setFaction(35);
        SetFlag(PLAYER_FLAGS, PLAYER_FLAGS_GM);
        SetFlag(UNIT_FIELD_FLAGS_2, UNIT_FLAG2_ALLOW_CHEAT_SPELLS);

        if (Pet* pet = GetPet())
        {
            pet->setFaction(35);
            pet->getHostileRefManager().setOnlineOfflineState(false);
        }

        RemoveByteFlag(UNIT_FIELD_BYTES_2, 1, UNIT_BYTE2_FLAG_FFA_PVP);
        ResetContestedPvP();

        getHostileRefManager().setOnlineOfflineState(false);
        CombatStopWithPets();

        SetPhaseMask(uint32(PHASEMASK_ANYWHERE), false);    // see and visible in all phases
        m_serverSideVisibilityDetect.SetValue(SERVERSIDE_VISIBILITY_GM, GetSession()->GetSecurity());
    }
    else
    {
        // restore phase
        uint32 newPhase = 0;
        AuraEffectList const& phases = GetAuraEffectsByType(SPELL_AURA_PHASE);
        if (!phases.empty())
            for (AuraEffectList::const_iterator itr = phases.begin(); itr != phases.end(); ++itr)
                newPhase |= (*itr)->GetMiscValue();

        if (!newPhase)
            newPhase = PHASEMASK_NORMAL;

        SetPhaseMask(newPhase, false);

        m_ExtraFlags &= ~ PLAYER_EXTRA_GM_ON;
        setFactionForRace(getRace());
        RemoveFlag(PLAYER_FLAGS, PLAYER_FLAGS_GM);
        RemoveFlag(UNIT_FIELD_FLAGS_2, UNIT_FLAG2_ALLOW_CHEAT_SPELLS);

        if (Pet* pet = GetPet())
        {
            pet->setFaction(getFaction());
            pet->getHostileRefManager().setOnlineOfflineState(true);
        }

        // restore FFA PvP Server state
        if (sWorld->IsFFAPvPRealm())
            SetByteFlag(UNIT_FIELD_BYTES_2, 1, UNIT_BYTE2_FLAG_FFA_PVP);

        // restore FFA PvP area state, remove not allowed for GM mounts
        UpdateArea(m_areaUpdateId);

        getHostileRefManager().setOnlineOfflineState(true);
        m_serverSideVisibilityDetect.SetValue(SERVERSIDE_VISIBILITY_GM, SEC_PLAYER);
    }

    UpdateObjectVisibility();
}

void Player::SetGMVisible(bool on)
{
    if (on)
    {
        m_ExtraFlags &= ~PLAYER_EXTRA_GM_INVISIBLE;         //remove flag
        m_serverSideVisibility.SetValue(SERVERSIDE_VISIBILITY_GM, SEC_PLAYER);
    }
    else
    {
        m_ExtraFlags |= PLAYER_EXTRA_GM_INVISIBLE;          //add flag

        SetAcceptWhispers(false);
        SetGameMaster(true);

        m_serverSideVisibility.SetValue(SERVERSIDE_VISIBILITY_GM, GetSession()->GetSecurity());
    }
}

bool Player::IsGroupVisibleFor(Player const* p) const
{
    switch (sWorld->getIntConfig(CONFIG_GROUP_VISIBILITY))
    {
        default: return IsInSameGroupWith(p);
        case 1:  return IsInSameRaidWith(p);
        case 2:  return GetTeam() == p->GetTeam();
    }
}

bool Player::IsInSameGroupWith(Player const* p) const
{
    return p == this || (GetGroup() != NULL &&
        GetGroup() == p->GetGroup() &&
        GetGroup()->SameSubGroup(this, p));
}

///- If the player is invited, remove him. If the group if then only 1 person, disband the group.
/// \todo Shouldn't we also check if there is no other invitees before disbanding the group?
void Player::UninviteFromGroup()
{
    Group* group = GetGroupInvite();
    if (!group)
        return;

    group->RemoveInvite(this);

    if (group->GetMembersCount() <= 1)                       // group has just 1 member => disband
    {
        if (group->IsCreated())
        {
            group->Disband(true);
        }
        else
        {
            group->RemoveAllInvites();
            delete group;
        }
    }
}

void Player::RemoveFromGroup(Group* group, uint64 guid, RemoveMethod method /* = GROUP_REMOVEMETHOD_DEFAULT*/, uint64 kicker /* = 0 */, const char* reason /* = NULL */)
{
    if (group)
    {
        group->RemoveMember(guid, method, kicker, reason);
        group = NULL;
    }
}

void Player::SendLogXPGain(uint32 GivenXP, Unit* victim, uint32 BonusXP, bool recruitAFriend, float /*group_rate*/)
{
    WorldPacket data(SMSG_LOG_XPGAIN, 21); // guess size?
    data << uint64(victim ? victim->GetGUID() : 0);         // guid
    data << uint32(GivenXP + BonusXP);                      // given experience
    data << uint8(victim ? 0 : 1);                          // 00-kill_xp type, 01-non_kill_xp type

    if (victim)
    {
        data << uint32(GivenXP);                            // experience without bonus

        // should use group_rate here but can't figure out how
        data << float(1);                                   // 1 - none 0 - 100% group bonus output
    }

    data << uint8(recruitAFriend ? 1 : 0);                  // does the GivenXP include a RaF bonus?
    GetSession()->SendPacket(&data);
}

void Player::GiveXP(uint32 xp, Unit* victim, float group_rate)
{
    if (xp < 1)
        return;

    if (!isAlive() && !GetBattlegroundId())
        return;

    if (HasFlag(PLAYER_FLAGS, PLAYER_FLAGS_NO_XP_GAIN))
        return;

    if (victim && victim->GetTypeId() == TYPEID_UNIT && !victim->ToCreature()->hasLootRecipient())
        return;

    uint8 level = getLevel();

    sScriptMgr->OnGivePlayerXP(this, xp, victim);

    // Favored experience increase START
    uint32 zone = GetZoneId();
    float favored_exp_mult = 0;
    if ((HasAura(32096) || HasAura(32098)) && (zone == 3483 || zone == 3562 || zone == 3836 || zone == 3713 || zone == 3714))
        favored_exp_mult = 0.05f; // Thrallmar's Favor and Honor Hold's Favor
    xp = uint32(xp * (1 + favored_exp_mult));
    // Favored experience increase END

    // XP to money conversion processed in Player::RewardQuest
    if (level >= sWorld->getIntConfig(CONFIG_MAX_PLAYER_LEVEL))
        return;

    uint32 bonus_xp = 0;
    bool recruitAFriend = GetsRecruitAFriendBonus(true);

    // RaF does NOT stack with rested experience
    if (recruitAFriend)
        bonus_xp = 2 * xp; // xp + bonus_xp must add up to 3 * xp for RaF; calculation for quests done client-side
    else
        bonus_xp = victim ? GetXPRestBonus(xp) : 0; // XP resting bonus

    SendLogXPGain(xp, victim, bonus_xp, recruitAFriend, group_rate);

    uint32 curXP = GetUInt32Value(PLAYER_XP);
    uint32 nextLvlXP = GetUInt32Value(PLAYER_NEXT_LEVEL_XP);
    uint32 newXP = curXP + xp + bonus_xp;

    while (newXP >= nextLvlXP && level < sWorld->getIntConfig(CONFIG_MAX_PLAYER_LEVEL))
    {
        newXP -= nextLvlXP;

        if (level < sWorld->getIntConfig(CONFIG_MAX_PLAYER_LEVEL))
            GiveLevel(level + 1);

        level = getLevel();
        nextLvlXP = GetUInt32Value(PLAYER_NEXT_LEVEL_XP);
    }

    SetUInt32Value(PLAYER_XP, newXP);
}

// Update player to next level
// Current player experience not update (must be update by caller)
void Player::GiveLevel(uint8 level)
{
    uint8 oldLevel = getLevel();
    if (level == oldLevel)
        return;

    PlayerLevelInfo info;
    sObjectMgr->GetPlayerLevelInfo(getRace(), getClass(), level, &info);

    PlayerClassLevelInfo classInfo;
    sObjectMgr->GetPlayerClassLevelInfo(getClass(), level, &classInfo);

    // send levelup info to client
    WorldPacket data(SMSG_LEVELUP_INFO, (4+4+MAX_POWERS*4+MAX_STATS*4));
    data << uint32(level);
    data << uint32(int32(classInfo.basehealth) - int32(GetCreateHealth()));
    // for (int i = 0; i < MAX_POWERS; ++i)                  // Powers loop (0-6)
    data << uint32(int32(classInfo.basemana)   - int32(GetCreateMana()));
    data << uint32(0);
    data << uint32(0);
    data << uint32(0);
    data << uint32(0);
    data << uint32(0);
    data << uint32(0);
    // end for
    for (uint8 i = STAT_STRENGTH; i < MAX_STATS; ++i)          // Stats loop (0-4)
        data << uint32(int32(info.stats[i]) - GetCreateStat(Stats(i)));

    GetSession()->SendPacket(&data);

    SetUInt32Value(PLAYER_NEXT_LEVEL_XP, sObjectMgr->GetXPForLevel(level));

    //update level, max level of skills
    m_Played_time[PLAYED_TIME_LEVEL] = 0;                   // Level Played Time reset

    _ApplyAllLevelScaleItemMods(false);

    SetLevel(level);

    UpdateSkillsForLevel();

    // save base values (bonuses already included in stored stats
    for (uint8 i = STAT_STRENGTH; i < MAX_STATS; ++i)
        SetCreateStat(Stats(i), info.stats[i]);

    SetCreateHealth(classInfo.basehealth);
    SetCreateMana(classInfo.basemana);

    InitTalentForLevel();
    InitTaxiNodesForLevel();
    InitGlyphsForLevel();

    UpdateAllStats();

    if (sWorld->getBoolConfig(CONFIG_ALWAYS_MAXSKILL)) // Max weapon skill when leveling up
        UpdateSkillsToMaxSkillsForLevel();

    // set current level health and mana/energy to maximum after applying all mods.
    SetFullHealth();
    SetPower(POWER_MANA, GetMaxPower(POWER_MANA));
    SetPower(POWER_ENERGY, GetMaxPower(POWER_ENERGY));
    if (GetPower(POWER_RAGE) > GetMaxPower(POWER_RAGE))
        SetPower(POWER_RAGE, GetMaxPower(POWER_RAGE));
    SetPower(POWER_FOCUS, 0);
    SetPower(POWER_HAPPINESS, 0);

    _ApplyAllLevelScaleItemMods(true);

    // update level to hunter/summon pet
    if (Pet* pet = GetPet())
        pet->SynchronizeLevelWithOwner();

    if (MailLevelReward const* mailReward = sObjectMgr->GetMailLevelReward(level, getRaceMask()))
    {
        //- TODO: Poor design of mail system
        SQLTransaction trans = CharacterDatabase.BeginTransaction();
        MailDraft(mailReward->mailTemplateId).SendMailTo(trans, this, MailSender(MAIL_CREATURE, mailReward->senderEntry));
        CharacterDatabase.CommitTransaction(trans);
    }

    UpdateAchievementCriteria(ACHIEVEMENT_CRITERIA_TYPE_REACH_LEVEL);

    // Refer-A-Friend
    if (GetSession()->GetRecruiterId())
        if (level < sWorld->getIntConfig(CONFIG_MAX_RECRUIT_A_FRIEND_BONUS_PLAYER_LEVEL))
            if (level % 2 == 0)
            {
                ++m_grantableLevels;

                if (!HasByteFlag(PLAYER_FIELD_BYTES, 1, 0x01))
                    SetByteFlag(PLAYER_FIELD_BYTES, 1, 0x01);
            }

    sScriptMgr->OnPlayerLevelChanged(this, oldLevel);
}

void Player::InitTalentForLevel()
{
    uint8 level = getLevel();
    // talents base at level diff (talents = level - 9 but some can be used already)
    if (level < 10)
    {
        // Remove all talent points
        if (m_usedTalentCount > 0)                           // Free any used talents
        {
            resetTalents(true);
            SetFreeTalentPoints(0);
        }
    }
    else
    {
        if (level < sWorld->getIntConfig(CONFIG_MIN_DUALSPEC_LEVEL) || m_specsCount == 0)
        {
            m_specsCount = 1;
            m_activeSpec = 0;
        }

        uint32 talentPointsForLevel = CalculateTalentsPoints();

        // if used more that have then reset
        if (m_usedTalentCount > talentPointsForLevel)
        {
            if (!AccountMgr::IsAdminAccount(GetSession()->GetSecurity()))
                resetTalents(true);
            else
                SetFreeTalentPoints(0);
        }
        // else update amount of free points
        else
            SetFreeTalentPoints(talentPointsForLevel - m_usedTalentCount);
    }

    if (!GetSession()->PlayerLoading())
        SendTalentsInfoData(false);                         // update at client
}

void Player::InitStatsForLevel(bool reapplyMods)
{
    if (reapplyMods)                                        //reapply stats values only on .reset stats (level) command
        _RemoveAllStatBonuses();

    PlayerClassLevelInfo classInfo;
    sObjectMgr->GetPlayerClassLevelInfo(getClass(), getLevel(), &classInfo);

    PlayerLevelInfo info;
    sObjectMgr->GetPlayerLevelInfo(getRace(), getClass(), getLevel(), &info);

    SetUInt32Value(PLAYER_FIELD_MAX_LEVEL, sWorld->getIntConfig(CONFIG_MAX_PLAYER_LEVEL));
    SetUInt32Value(PLAYER_NEXT_LEVEL_XP, sObjectMgr->GetXPForLevel(getLevel()));

    // reset before any aura state sources (health set/aura apply)
    SetUInt32Value(UNIT_FIELD_AURASTATE, 0);

    UpdateSkillsForLevel();

    // set default cast time multiplier
    SetFloatValue(UNIT_MOD_CAST_SPEED, 1.0f);

    // reset size before reapply auras
    SetObjectScale(1.0f);

    // save base values (bonuses already included in stored stats
    for (uint8 i = STAT_STRENGTH; i < MAX_STATS; ++i)
        SetCreateStat(Stats(i), info.stats[i]);

    for (uint8 i = STAT_STRENGTH; i < MAX_STATS; ++i)
        SetStat(Stats(i), info.stats[i]);

    SetCreateHealth(classInfo.basehealth);

    //set create powers
    SetCreateMana(classInfo.basemana);

    SetArmor(int32(m_createStats[STAT_AGILITY]*2));

    InitStatBuffMods();

    //reset rating fields values
    for (uint16 index = PLAYER_FIELD_COMBAT_RATING_1; index < PLAYER_FIELD_COMBAT_RATING_1 + MAX_COMBAT_RATING; ++index)
        SetUInt32Value(index, 0);

    SetUInt32Value(PLAYER_FIELD_MOD_HEALING_DONE_POS, 0);
    for (uint8 i = 0; i < 7; ++i)
    {
        SetUInt32Value(PLAYER_FIELD_MOD_DAMAGE_DONE_NEG+i, 0);
        SetUInt32Value(PLAYER_FIELD_MOD_DAMAGE_DONE_POS+i, 0);
        SetFloatValue(PLAYER_FIELD_MOD_DAMAGE_DONE_PCT+i, 1.00f);
    }

    //reset attack power, damage and attack speed fields
    SetFloatValue(UNIT_FIELD_BASEATTACKTIME, 2000.0f);
    SetFloatValue(UNIT_FIELD_BASEATTACKTIME + 1, 2000.0f); // offhand attack time
    SetFloatValue(UNIT_FIELD_RANGEDATTACKTIME, 2000.0f);

    SetFloatValue(UNIT_FIELD_MINDAMAGE, 0.0f);
    SetFloatValue(UNIT_FIELD_MAXDAMAGE, 0.0f);
    SetFloatValue(UNIT_FIELD_MINOFFHANDDAMAGE, 0.0f);
    SetFloatValue(UNIT_FIELD_MAXOFFHANDDAMAGE, 0.0f);
    SetFloatValue(UNIT_FIELD_MINRANGEDDAMAGE, 0.0f);
    SetFloatValue(UNIT_FIELD_MAXRANGEDDAMAGE, 0.0f);

    SetInt32Value(UNIT_FIELD_ATTACK_POWER,            0);
    SetInt32Value(UNIT_FIELD_ATTACK_POWER_MODS,       0);
    SetFloatValue(UNIT_FIELD_ATTACK_POWER_MULTIPLIER, 0.0f);
    SetInt32Value(UNIT_FIELD_RANGED_ATTACK_POWER,     0);
    SetInt32Value(UNIT_FIELD_RANGED_ATTACK_POWER_MODS, 0);
    SetFloatValue(UNIT_FIELD_RANGED_ATTACK_POWER_MULTIPLIER, 0.0f);

    // Base crit values (will be recalculated in UpdateAllStats() at loading and in _ApplyAllStatBonuses() at reset
    SetFloatValue(PLAYER_CRIT_PERCENTAGE, 0.0f);
    SetFloatValue(PLAYER_OFFHAND_CRIT_PERCENTAGE, 0.0f);
    SetFloatValue(PLAYER_RANGED_CRIT_PERCENTAGE, 0.0f);

    // Init spell schools (will be recalculated in UpdateAllStats() at loading and in _ApplyAllStatBonuses() at reset
    for (uint8 i = 0; i < 7; ++i)
        SetFloatValue(PLAYER_SPELL_CRIT_PERCENTAGE1+i, 0.0f);

    SetFloatValue(PLAYER_PARRY_PERCENTAGE, 0.0f);
    SetFloatValue(PLAYER_BLOCK_PERCENTAGE, 0.0f);
    SetUInt32Value(PLAYER_SHIELD_BLOCK, 0);

    // Dodge percentage
    SetFloatValue(PLAYER_DODGE_PERCENTAGE, 0.0f);

    // set armor (resistance 0) to original value (create_agility*2)
    SetArmor(int32(m_createStats[STAT_AGILITY]*2));
    SetResistanceBuffMods(SpellSchools(0), true, 0.0f);
    SetResistanceBuffMods(SpellSchools(0), false, 0.0f);
    // set other resistance to original value (0)
    for (uint8 i = 1; i < MAX_SPELL_SCHOOL; ++i)
    {
        SetResistance(SpellSchools(i), 0);
        SetResistanceBuffMods(SpellSchools(i), true, 0.0f);
        SetResistanceBuffMods(SpellSchools(i), false, 0.0f);
    }

    SetUInt32Value(PLAYER_FIELD_MOD_TARGET_RESISTANCE, 0);
    SetUInt32Value(PLAYER_FIELD_MOD_TARGET_PHYSICAL_RESISTANCE, 0);
    for (uint8 i = 0; i < MAX_SPELL_SCHOOL; ++i)
    {
        SetUInt32Value(UNIT_FIELD_POWER_COST_MODIFIER+i, 0);
        SetFloatValue(UNIT_FIELD_POWER_COST_MULTIPLIER+i, 0.0f);
    }
    // Reset no reagent cost field
    for (uint8 i = 0; i < 3; ++i)
        SetUInt32Value(PLAYER_NO_REAGENT_COST_1 + i, 0);
    // Init data for form but skip reapply item mods for form
    InitDataForForm(reapplyMods);

    // save new stats
    for (uint8 i = POWER_MANA; i < MAX_POWERS; ++i)
        SetMaxPower(Powers(i),  uint32(GetCreatePowers(Powers(i))));

    SetMaxHealth(classInfo.basehealth);                     // stamina bonus will applied later

    // cleanup mounted state (it will set correctly at aura loading if player saved at mount.
    SetUInt32Value(UNIT_FIELD_MOUNTDISPLAYID, 0);

    // cleanup unit flags (will be re-applied if need at aura load).
    RemoveFlag(UNIT_FIELD_FLAGS,
        UNIT_FLAG_NON_ATTACKABLE | UNIT_FLAG_DISABLE_MOVE | UNIT_FLAG_NOT_ATTACKABLE_1 |
        UNIT_FLAG_IMMUNE_TO_PC | UNIT_FLAG_IMMUNE_TO_NPC  | UNIT_FLAG_LOOTING          |
        UNIT_FLAG_PET_IN_COMBAT  | UNIT_FLAG_SILENCED     | UNIT_FLAG_PACIFIED         |
        UNIT_FLAG_STUNNED        | UNIT_FLAG_IN_COMBAT    | UNIT_FLAG_DISARMED         |
        UNIT_FLAG_CONFUSED       | UNIT_FLAG_FLEEING      | UNIT_FLAG_NOT_SELECTABLE   |
        UNIT_FLAG_SKINNABLE      | UNIT_FLAG_MOUNT        | UNIT_FLAG_TAXI_FLIGHT      );
    SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_PVP_ATTACKABLE);   // must be set

    SetFlag(UNIT_FIELD_FLAGS_2, UNIT_FLAG2_REGENERATE_POWER);// must be set

    // cleanup player flags (will be re-applied if need at aura load), to avoid have ghost flag without ghost aura, for example.
    RemoveFlag(PLAYER_FLAGS, PLAYER_FLAGS_AFK | PLAYER_FLAGS_DND | PLAYER_FLAGS_GM | PLAYER_FLAGS_GHOST | PLAYER_ALLOW_ONLY_ABILITY);

    RemoveStandFlags(UNIT_STAND_FLAGS_ALL);                 // one form stealth modified bytes
    RemoveByteFlag(UNIT_FIELD_BYTES_2, 1, UNIT_BYTE2_FLAG_FFA_PVP | UNIT_BYTE2_FLAG_SANCTUARY);

    // restore if need some important flags
    SetUInt32Value(PLAYER_FIELD_BYTES2, 0);                 // flags empty by default

    if (reapplyMods)                                        // reapply stats values only on .reset stats (level) command
        _ApplyAllStatBonuses();

    // set current level health and mana/energy to maximum after applying all mods.
    SetFullHealth();
    SetPower(POWER_MANA, GetMaxPower(POWER_MANA));
    SetPower(POWER_ENERGY, GetMaxPower(POWER_ENERGY));
    if (GetPower(POWER_RAGE) > GetMaxPower(POWER_RAGE))
        SetPower(POWER_RAGE, GetMaxPower(POWER_RAGE));
    SetPower(POWER_FOCUS, 0);
    SetPower(POWER_HAPPINESS, 0);
    SetPower(POWER_RUNIC_POWER, 0);

    // update level to hunter/summon pet
    if (Pet* pet = GetPet())
        pet->SynchronizeLevelWithOwner();
}

void Player::SendInitialSpells()
{
    time_t curTime = time(NULL);
    time_t infTime = curTime + infinityCooldownDelayCheck;

    uint16 spellCount = 0;

    WorldPacket data(SMSG_INITIAL_SPELLS, (1+2+4*m_spells.size()+2+m_spellCooldowns.size()*(2+2+2+4+4)));
    data << uint8(0);

    size_t countPos = data.wpos();
    data << uint16(spellCount);                             // spell count placeholder

    for (PlayerSpellMap::const_iterator itr = m_spells.begin(); itr != m_spells.end(); ++itr)
    {
        if (itr->second->state == PLAYERSPELL_REMOVED)
            continue;

        if (!itr->second->active || itr->second->disabled)
            continue;

        data << uint32(itr->first);
        data << uint16(0);                                  // it's not slot id

        spellCount +=1;
    }

    data.put<uint16>(countPos, spellCount);                  // write real count value

    uint16 spellCooldowns = m_spellCooldowns.size();
    data << uint16(spellCooldowns);
    for (SpellCooldowns::const_iterator itr = m_spellCooldowns.begin(); itr != m_spellCooldowns.end(); ++itr)
    {
        SpellInfo const* sEntry = sSpellMgr->GetSpellInfo(itr->first);
        if (!sEntry)
            continue;

        data << uint32(itr->first);

        data << uint16(itr->second.itemid);                 // cast item id
        data << uint16(sEntry->Category);                   // spell category

        // send infinity cooldown in special format
        if (itr->second.end >= infTime)
        {
            data << uint32(1);                              // cooldown
            data << uint32(0x80000000);                     // category cooldown
            continue;
        }

        time_t cooldown = itr->second.end > curTime ? (itr->second.end-curTime)*IN_MILLISECONDS : 0;

        if (sEntry->Category)                                // may be wrong, but anyway better than nothing...
        {
            data << uint32(0);                              // cooldown
            data << uint32(cooldown);                       // category cooldown
        }
        else
        {
            data << uint32(cooldown);                       // cooldown
            data << uint32(0);                              // category cooldown
        }
    }

    GetSession()->SendPacket(&data);

    sLog->outDebug(LOG_FILTER_NETWORKIO, "CHARACTER: Sent Initial Spells");
}

void Player::RemoveMail(uint32 id)
{
    for (PlayerMails::iterator itr = m_mail.begin(); itr != m_mail.end(); ++itr)
    {
        if ((*itr)->messageID == id)
        {
            //do not delete item, because Player::removeMail() is called when returning mail to sender.
            m_mail.erase(itr);
            return;
        }
    }
}

void Player::SendMailResult(uint32 mailId, MailResponseType mailAction, MailResponseResult mailError, uint32 equipError, uint32 item_guid, uint32 item_count)
{
    WorldPacket data(SMSG_SEND_MAIL_RESULT, (4+4+4+(mailError == MAIL_ERR_EQUIP_ERROR?4:(mailAction == MAIL_ITEM_TAKEN?4+4:0))));
    data << (uint32) mailId;
    data << (uint32) mailAction;
    data << (uint32) mailError;
    if (mailError == MAIL_ERR_EQUIP_ERROR)
        data << (uint32) equipError;
    else if (mailAction == MAIL_ITEM_TAKEN)
    {
        data << (uint32) item_guid;                         // item guid low?
        data << (uint32) item_count;                        // item count?
    }
    GetSession()->SendPacket(&data);
}

void Player::SendNewMail()
{
    // deliver undelivered mail
    WorldPacket data(SMSG_RECEIVED_MAIL, 4);
    data << (uint32) 0;
    GetSession()->SendPacket(&data);
}

void Player::UpdateNextMailTimeAndUnreads()
{
    // calculate next delivery time (min. from non-delivered mails
    // and recalculate unReadMail
    time_t cTime = time(NULL);
    m_nextMailDelivereTime = 0;
    unReadMails = 0;
    for (PlayerMails::iterator itr = m_mail.begin(); itr != m_mail.end(); ++itr)
    {
        if ((*itr)->deliver_time > cTime)
        {
            if (!m_nextMailDelivereTime || m_nextMailDelivereTime > (*itr)->deliver_time)
                m_nextMailDelivereTime = (*itr)->deliver_time;
        }
        else if (((*itr)->checked & MAIL_CHECK_MASK_READ) == 0)
            ++unReadMails;
    }
}

void Player::AddNewMailDeliverTime(time_t deliver_time)
{
    if (deliver_time <= time(NULL))                          // ready now
    {
        ++unReadMails;
        SendNewMail();
    }
    else                                                    // not ready and no have ready mails
    {
        if (!m_nextMailDelivereTime || m_nextMailDelivereTime > deliver_time)
            m_nextMailDelivereTime = deliver_time;
    }
}

bool Player::AddTalent(uint32 spellId, uint8 spec, bool learning)
{
    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId);
    if (!spellInfo)
    {
        // do character spell book cleanup (all characters)
        if (!IsInWorld() && !learning)                       // spell load case
        {
            sLog->outError(LOG_FILTER_SPELLS_AURAS, "Player::addSpell: Non-existed in SpellStore spell #%u request, deleting for all characters in `character_spell`.", spellId);

            PreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_INVALID_SPELL);

            stmt->setUInt32(0, spellId);

            CharacterDatabase.Execute(stmt);
        }
        else
            sLog->outError(LOG_FILTER_SPELLS_AURAS, "Player::addSpell: Non-existed in SpellStore spell #%u request.", spellId);

        return false;
    }

    if (!SpellMgr::IsSpellValid(spellInfo, this, false))
    {
        // do character spell book cleanup (all characters)
        if (!IsInWorld() && !learning)                       // spell load case
        {
            sLog->outError(LOG_FILTER_SPELLS_AURAS, "Player::addTalent: Broken spell #%u learning not allowed, deleting for all characters in `character_talent`.", spellId);

            PreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_INVALID_SPELL);

            stmt->setUInt32(0, spellId);

            CharacterDatabase.Execute(stmt);
        }
        else
            sLog->outError(LOG_FILTER_SPELLS_AURAS, "Player::addTalent: Broken spell #%u learning not allowed.", spellId);

        return false;
    }

    PlayerTalentMap::iterator itr = m_talents[spec]->find(spellId);
    if (itr != m_talents[spec]->end())
        itr->second->state = PLAYERSPELL_UNCHANGED;
    else if (TalentSpellPos const* talentPos = GetTalentSpellPos(spellId))
    {
        if (TalentEntry const* talentInfo = sTalentStore.LookupEntry(talentPos->talent_id))
        {
            for (uint8 rank = 0; rank < MAX_TALENT_RANK; ++rank)
            {
                // skip learning spell and no rank spell case
                uint32 rankSpellId = talentInfo->RankID[rank];
                if (!rankSpellId || rankSpellId == spellId)
                    continue;

                itr = m_talents[spec]->find(rankSpellId);
                if (itr != m_talents[spec]->end())
                    itr->second->state = PLAYERSPELL_REMOVED;
            }
        }

        PlayerSpellState state = learning ? PLAYERSPELL_NEW : PLAYERSPELL_UNCHANGED;
        PlayerTalent* newtalent = new PlayerTalent();

        newtalent->state = state;
        newtalent->spec = spec;

        (*m_talents[spec])[spellId] = newtalent;
        return true;
    }
    return false;
}

bool Player::addSpell(uint32 spellId, bool active, bool learning, bool dependent, bool disabled, bool loading /*= false*/)
{
    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId);
    if (!spellInfo)
    {
        // do character spell book cleanup (all characters)
        if (!IsInWorld() && !learning)                       // spell load case
        {
            sLog->outError(LOG_FILTER_SPELLS_AURAS, "Player::addSpell: Non-existed in SpellStore spell #%u request, deleting for all characters in `character_spell`.", spellId);

            PreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_INVALID_SPELL);

            stmt->setUInt32(0, spellId);

            CharacterDatabase.Execute(stmt);
        }
        else
            sLog->outError(LOG_FILTER_SPELLS_AURAS, "Player::addSpell: Non-existed in SpellStore spell #%u request.", spellId);

        return false;
    }

    if (!SpellMgr::IsSpellValid(spellInfo, this, false))
    {
        // do character spell book cleanup (all characters)
        if (!IsInWorld() && !learning)                       // spell load case
        {
            sLog->outError(LOG_FILTER_SPELLS_AURAS, "Player::addSpell: Broken spell #%u learning not allowed, deleting for all characters in `character_spell`.", spellId);

            PreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_INVALID_SPELL);

            stmt->setUInt32(0, spellId);

            CharacterDatabase.Execute(stmt);
        }
        else
            sLog->outError(LOG_FILTER_SPELLS_AURAS, "Player::addSpell: Broken spell #%u learning not allowed.", spellId);

        return false;
    }

    PlayerSpellState state = learning ? PLAYERSPELL_NEW : PLAYERSPELL_UNCHANGED;

    bool dependent_set = false;
    bool disabled_case = false;
    bool superceded_old = false;

    PlayerSpellMap::iterator itr = m_spells.find(spellId);

    // Remove temporary spell if found to prevent conflicts
    if (itr != m_spells.end() && itr->second->state == PLAYERSPELL_TEMPORARY)
        RemoveTemporarySpell(spellId);
    else if (itr != m_spells.end())
    {
        uint32 next_active_spell_id = 0;
        // fix activate state for non-stackable low rank (and find next spell for !active case)
        if (!spellInfo->IsStackableWithRanks() && spellInfo->IsRanked())
        {
            if (uint32 next = sSpellMgr->GetNextSpellInChain(spellId))
            {
                if (HasSpell(next))
                {
                    // high rank already known so this must !active
                    active = false;
                    next_active_spell_id = next;
                }
            }
        }

        // not do anything if already known in expected state
        if (itr->second->state != PLAYERSPELL_REMOVED && itr->second->active == active &&
            itr->second->dependent == dependent && itr->second->disabled == disabled)
        {
            if (!IsInWorld() && !learning)                   // explicitly load from DB and then exist in it already and set correctly
                itr->second->state = PLAYERSPELL_UNCHANGED;

            return false;
        }

        // dependent spell known as not dependent, overwrite state
        if (itr->second->state != PLAYERSPELL_REMOVED && !itr->second->dependent && dependent)
        {
            itr->second->dependent = dependent;
            if (itr->second->state != PLAYERSPELL_NEW)
                itr->second->state = PLAYERSPELL_CHANGED;
            dependent_set = true;
        }

        // update active state for known spell
        if (itr->second->active != active && itr->second->state != PLAYERSPELL_REMOVED && !itr->second->disabled)
        {
            itr->second->active = active;

            if (!IsInWorld() && !learning && !dependent_set) // explicitly load from DB and then exist in it already and set correctly
                itr->second->state = PLAYERSPELL_UNCHANGED;
            else if (itr->second->state != PLAYERSPELL_NEW)
                itr->second->state = PLAYERSPELL_CHANGED;

            if (active)
            {
                if (spellInfo->IsPassive() && IsNeedCastPassiveSpellAtLearn(spellInfo))
                    CastSpell (this, spellId, true);
            }
            else if (IsInWorld())
            {
                if (next_active_spell_id)
                {
                    // update spell ranks in spellbook and action bar
                    WorldPacket data(SMSG_SUPERCEDED_SPELL, 4 + 4);
                    data << uint32(spellId);
                    data << uint32(next_active_spell_id);
                    GetSession()->SendPacket(&data);
                }
                else
                {
                    WorldPacket data(SMSG_REMOVED_SPELL, 4);
                    data << uint32(spellId);
                    GetSession()->SendPacket(&data);
                }
            }

            return active;                                  // learn (show in spell book if active now)
        }

        if (itr->second->disabled != disabled && itr->second->state != PLAYERSPELL_REMOVED)
        {
            if (itr->second->state != PLAYERSPELL_NEW)
                itr->second->state = PLAYERSPELL_CHANGED;
            itr->second->disabled = disabled;

            if (disabled)
                return false;

            disabled_case = true;
        }
        else switch (itr->second->state)
        {
            case PLAYERSPELL_UNCHANGED:                     // known saved spell
                return false;
            case PLAYERSPELL_REMOVED:                       // re-learning removed not saved spell
            {
                delete itr->second;
                m_spells.erase(itr);
                state = PLAYERSPELL_CHANGED;
                break;                                      // need re-add
            }
            default:                                        // known not saved yet spell (new or modified)
            {
                // can be in case spell loading but learned at some previous spell loading
                if (!IsInWorld() && !learning && !dependent_set)
                    itr->second->state = PLAYERSPELL_UNCHANGED;

                return false;
            }
        }
    }

    if (!disabled_case) // skip new spell adding if spell already known (disabled spells case)
    {
        // talent: unlearn all other talent ranks (high and low)
        if (TalentSpellPos const* talentPos = GetTalentSpellPos(spellId))
        {
            if (TalentEntry const* talentInfo = sTalentStore.LookupEntry(talentPos->talent_id))
            {
                for (uint8 rank = 0; rank < MAX_TALENT_RANK; ++rank)
                {
                    // skip learning spell and no rank spell case
                    uint32 rankSpellId = talentInfo->RankID[rank];
                    if (!rankSpellId || rankSpellId == spellId)
                        continue;

                    removeSpell(rankSpellId, false, false);
                }
            }
        }
        // non talent spell: learn low ranks (recursive call)
        else if (uint32 prev_spell = sSpellMgr->GetPrevSpellInChain(spellId))
        {
            if (!IsInWorld() || disabled)                    // at spells loading, no output, but allow save
                addSpell(prev_spell, active, true, true, disabled);
            else                                            // at normal learning
                learnSpell(prev_spell, true);
        }

        PlayerSpell* newspell = new PlayerSpell;
        newspell->state     = state;
        newspell->active    = active;
        newspell->dependent = dependent;
        newspell->disabled  = disabled;

        // replace spells in action bars and spellbook to bigger rank if only one spell rank must be accessible
        if (newspell->active && !newspell->disabled && !spellInfo->IsStackableWithRanks() && spellInfo->IsRanked() != 0)
        {
            for (PlayerSpellMap::iterator itr2 = m_spells.begin(); itr2 != m_spells.end(); ++itr2)
            {
                if (itr2->second->state == PLAYERSPELL_REMOVED)
                    continue;

                SpellInfo const* i_spellInfo = sSpellMgr->GetSpellInfo(itr2->first);
                if (!i_spellInfo)
                    continue;

                if (spellInfo->IsDifferentRankOf(i_spellInfo))
                {
                    if (itr2->second->active)
                    {
                        if (spellInfo->IsHighRankOf(i_spellInfo))
                        {
                            if (IsInWorld())                 // not send spell (re-/over-)learn packets at loading
                            {
                                WorldPacket data(SMSG_SUPERCEDED_SPELL, 4 + 4);
                                data << uint32(itr2->first);
                                data << uint32(spellId);
                                GetSession()->SendPacket(&data);
                            }

                            // mark old spell as disable (SMSG_SUPERCEDED_SPELL replace it in client by new)
                            itr2->second->active = false;
                            if (itr2->second->state != PLAYERSPELL_NEW)
                                itr2->second->state = PLAYERSPELL_CHANGED;
                            superceded_old = true;          // new spell replace old in action bars and spell book.
                        }
                        else
                        {
                            if (IsInWorld())                 // not send spell (re-/over-)learn packets at loading
                            {
                                WorldPacket data(SMSG_SUPERCEDED_SPELL, 4 + 4);
                                data << uint32(spellId);
                                data << uint32(itr2->first);
                                GetSession()->SendPacket(&data);
                            }

                            // mark new spell as disable (not learned yet for client and will not learned)
                            newspell->active = false;
                            if (newspell->state != PLAYERSPELL_NEW)
                                newspell->state = PLAYERSPELL_CHANGED;
                        }
                    }
                }
            }
        }

        m_spells[spellId] = newspell;

        // return false if spell disabled
        if (newspell->disabled)
            return false;
    }

    uint32 talentCost = GetTalentSpellCost(spellId);

    // cast talents with SPELL_EFFECT_LEARN_SPELL (other dependent spells will learned later as not auto-learned)
    // note: all spells with SPELL_EFFECT_LEARN_SPELL isn't passive
    if (!loading && talentCost > 0 && spellInfo->HasEffect(SPELL_EFFECT_LEARN_SPELL))
    {
        // ignore stance requirement for talent learn spell (stance set for spell only for client spell description show)
        CastSpell(this, spellId, true);
    }
    // also cast passive spells (including all talents without SPELL_EFFECT_LEARN_SPELL) with additional checks
    else if (spellInfo->IsPassive())
    {
        if (IsNeedCastPassiveSpellAtLearn(spellInfo))
            CastSpell(this, spellId, true);
    }
    else if (spellInfo->HasEffect(SPELL_EFFECT_SKILL_STEP))
    {
        CastSpell(this, spellId, true);
        return false;
    }

    // update used talent points count
    m_usedTalentCount += talentCost;

    // update free primary prof.points (if any, can be none in case GM .learn prof. learning)
    if (uint32 freeProfs = GetFreePrimaryProfessionPoints())
    {
        if (spellInfo->IsPrimaryProfessionFirstRank())
            SetFreePrimaryProfessions(freeProfs-1);
    }

    // add dependent skills
    uint16 maxskill     = GetMaxSkillValueForLevel();

    SpellLearnSkillNode const* spellLearnSkill = sSpellMgr->GetSpellLearnSkill(spellId);

    SkillLineAbilityMapBounds skill_bounds = sSpellMgr->GetSkillLineAbilityMapBounds(spellId);

    if (spellLearnSkill)
    {
        uint32 skill_value = GetPureSkillValue(spellLearnSkill->skill);
        uint32 skill_max_value = GetPureMaxSkillValue(spellLearnSkill->skill);

        if (skill_value < spellLearnSkill->value)
            skill_value = spellLearnSkill->value;

        uint32 new_skill_max_value = spellLearnSkill->maxvalue == 0 ? maxskill : spellLearnSkill->maxvalue;

        if (skill_max_value < new_skill_max_value)
            skill_max_value = new_skill_max_value;

        SetSkill(spellLearnSkill->skill, spellLearnSkill->step, skill_value, skill_max_value);
    }
    else
    {
        // not ranked skills
        for (SkillLineAbilityMap::const_iterator _spell_idx = skill_bounds.first; _spell_idx != skill_bounds.second; ++_spell_idx)
        {
            SkillLineEntry const* pSkill = sSkillLineStore.LookupEntry(_spell_idx->second->skillId);
            if (!pSkill)
                continue;

            if (!Has310Flyer(false) && pSkill->id == SKILL_MOUNTS)
                for (uint8 i = 0; i < MAX_SPELL_EFFECTS; ++i)
                    if (spellInfo->Effects[i].ApplyAuraName == SPELL_AURA_MOD_INCREASE_MOUNTED_FLIGHT_SPEED &&
                        spellInfo->Effects[i].CalcValue() == 310)
                        SetHas310Flyer(true);

            if (HasSkill(pSkill->id))
                continue;

            if (_spell_idx->second->learnOnGetSkill == ABILITY_LEARNED_ON_GET_RACE_OR_CLASS_SKILL ||
                // lockpicking/runeforging special case, not have ABILITY_LEARNED_ON_GET_RACE_OR_CLASS_SKILL
                ((pSkill->id == SKILL_LOCKPICKING || pSkill->id == SKILL_RUNEFORGING) && _spell_idx->second->max_value == 0))
            {
                switch (GetSkillRangeType(pSkill, _spell_idx->second->racemask != 0))
                {
                    case SKILL_RANGE_LANGUAGE:
                        SetSkill(pSkill->id, GetSkillStep(pSkill->id), 300, 300);
                        break;
                    case SKILL_RANGE_LEVEL:
                        SetSkill(pSkill->id, GetSkillStep(pSkill->id), 1, GetMaxSkillValueForLevel());
                        break;
                    case SKILL_RANGE_MONO:
                        SetSkill(pSkill->id, GetSkillStep(pSkill->id), 1, 1);
                        break;
                    default:
                        break;
                }
            }
        }
    }

    // learn dependent spells
    SpellLearnSpellMapBounds spell_bounds = sSpellMgr->GetSpellLearnSpellMapBounds(spellId);

    for (SpellLearnSpellMap::const_iterator itr2 = spell_bounds.first; itr2 != spell_bounds.second; ++itr2)
    {
        if (!itr2->second.autoLearned)
        {
            if (!IsInWorld() || !itr2->second.active)       // at spells loading, no output, but allow save
                addSpell(itr2->second.spell, itr2->second.active, true, true, false);
            else                                            // at normal learning
                learnSpell(itr2->second.spell, true);
        }
    }

    if (!GetSession()->PlayerLoading())
    {
        // not ranked skills
        for (SkillLineAbilityMap::const_iterator _spell_idx = skill_bounds.first; _spell_idx != skill_bounds.second; ++_spell_idx)
        {
            UpdateAchievementCriteria(ACHIEVEMENT_CRITERIA_TYPE_LEARN_SKILL_LINE, _spell_idx->second->skillId);
            UpdateAchievementCriteria(ACHIEVEMENT_CRITERIA_TYPE_LEARN_SKILLLINE_SPELLS, _spell_idx->second->skillId);
        }

        UpdateAchievementCriteria(ACHIEVEMENT_CRITERIA_TYPE_LEARN_SPELL, spellId);
    }

    // return true (for send learn packet) only if spell active (in case ranked spells) and not replace old spell
    return active && !disabled && !superceded_old;
}

void Player::AddTemporarySpell(uint32 spellId)
{
    PlayerSpellMap::iterator itr = m_spells.find(spellId);
    // spell already added - do not do anything
    if (itr != m_spells.end())
        return;
    PlayerSpell* newspell = new PlayerSpell;
    newspell->state     = PLAYERSPELL_TEMPORARY;
    newspell->active    = true;
    newspell->dependent = false;
    newspell->disabled  = false;
    m_spells[spellId]   = newspell;
}

void Player::RemoveTemporarySpell(uint32 spellId)
{
    PlayerSpellMap::iterator itr = m_spells.find(spellId);
    // spell already not in list - do not do anything
    if (itr == m_spells.end())
        return;
    // spell has other state than temporary - do not change it
    if (itr->second->state != PLAYERSPELL_TEMPORARY)
        return;
    delete itr->second;
    m_spells.erase(itr);
}

bool Player::IsNeedCastPassiveSpellAtLearn(SpellInfo const* spellInfo) const
{
    // note: form passives activated with shapeshift spells be implemented by HandleShapeshiftBoosts instead of spell_learn_spell
    // talent dependent passives activated at form apply have proper stance data
    ShapeshiftForm form = GetShapeshiftForm();
    bool need_cast = (!spellInfo->Stances || (form && (spellInfo->Stances & (1 << (form - 1)))) ||
        (!form && (spellInfo->AttributesEx2 & SPELL_ATTR2_NOT_NEED_SHAPESHIFT)));

    //Check CasterAuraStates
    return need_cast && (!spellInfo->CasterAuraState || HasAuraState(AuraStateType(spellInfo->CasterAuraState)));
}

void Player::learnSpell(uint32 spell_id, bool dependent)
{
    PlayerSpellMap::iterator itr = m_spells.find(spell_id);

    bool disabled = (itr != m_spells.end()) ? itr->second->disabled : false;
    bool active = disabled ? itr->second->active : true;

    bool learning = addSpell(spell_id, active, true, dependent, false);

    // prevent duplicated entires in spell book, also not send if not in world (loading)
    if (learning && IsInWorld())
    {
        WorldPacket data(SMSG_LEARNED_SPELL, 6);
        data << uint32(spell_id);
        data << uint16(0);
        GetSession()->SendPacket(&data);
    }

    // learn all disabled higher ranks and required spells (recursive)
    if (disabled)
    {
        if (uint32 nextSpell = sSpellMgr->GetNextSpellInChain(spell_id))
        {
            PlayerSpellMap::iterator iter = m_spells.find(nextSpell);
            if (iter != m_spells.end() && iter->second->disabled)
                learnSpell(nextSpell, false);
        }

        SpellsRequiringSpellMapBounds spellsRequiringSpell = sSpellMgr->GetSpellsRequiringSpellBounds(spell_id);
        for (SpellsRequiringSpellMap::const_iterator itr2 = spellsRequiringSpell.first; itr2 != spellsRequiringSpell.second; ++itr2)
        {
            PlayerSpellMap::iterator iter2 = m_spells.find(itr2->second);
            if (iter2 != m_spells.end() && iter2->second->disabled)
                learnSpell(itr2->second, false);
        }
    }
}

void Player::removeSpell(uint32 spell_id, bool disabled, bool learn_low_rank)
{
    PlayerSpellMap::iterator itr = m_spells.find(spell_id);
    if (itr == m_spells.end())
        return;

    if (itr->second->state == PLAYERSPELL_REMOVED || (disabled && itr->second->disabled) || itr->second->state == PLAYERSPELL_TEMPORARY)
        return;

    // unlearn non talent higher ranks (recursive)
    if (uint32 nextSpell = sSpellMgr->GetNextSpellInChain(spell_id))
    {
        if (HasSpell(nextSpell) && !GetTalentSpellPos(nextSpell))
            removeSpell(nextSpell, disabled, false);
    }
    //unlearn spells dependent from recently removed spells
    SpellsRequiringSpellMapBounds spellsRequiringSpell = sSpellMgr->GetSpellsRequiringSpellBounds(spell_id);
    for (SpellsRequiringSpellMap::const_iterator itr2 = spellsRequiringSpell.first; itr2 != spellsRequiringSpell.second; ++itr2)
        removeSpell(itr2->second, disabled);

    // re-search, it can be corrupted in prev loop
    itr = m_spells.find(spell_id);
    if (itr == m_spells.end())
        return;                                             // already unleared

    bool giveTalentPoints = disabled || !itr->second->disabled;

    bool cur_active    = itr->second->active;
    bool cur_dependent = itr->second->dependent;

    if (disabled)
    {
        itr->second->disabled = disabled;
        if (itr->second->state != PLAYERSPELL_NEW)
            itr->second->state = PLAYERSPELL_CHANGED;
    }
    else
    {
        if (itr->second->state == PLAYERSPELL_NEW)
        {
            delete itr->second;
            m_spells.erase(itr);
        }
        else
            itr->second->state = PLAYERSPELL_REMOVED;
    }

    RemoveAurasDueToSpell(spell_id);

    // remove pet auras
    for (uint8 i = 0; i < MAX_SPELL_EFFECTS; ++i)
        if (PetAura const* petSpell = sSpellMgr->GetPetAura(spell_id, i))
            RemovePetAura(petSpell);

    // free talent points
    uint32 talentCosts = GetTalentSpellCost(spell_id);
    if (talentCosts > 0 && giveTalentPoints)
    {
        if (talentCosts < m_usedTalentCount)
            m_usedTalentCount -= talentCosts;
        else
            m_usedTalentCount = 0;
    }

    // update free primary prof.points (if not overflow setting, can be in case GM use before .learn prof. learning)
    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spell_id);
    if (spellInfo && spellInfo->IsPrimaryProfessionFirstRank())
    {
        uint32 freeProfs = GetFreePrimaryProfessionPoints()+1;
        if (freeProfs <= sWorld->getIntConfig(CONFIG_MAX_PRIMARY_TRADE_SKILL))
            SetFreePrimaryProfessions(freeProfs);
    }

    // remove dependent skill
    SpellLearnSkillNode const* spellLearnSkill = sSpellMgr->GetSpellLearnSkill(spell_id);
    if (spellLearnSkill)
    {
        uint32 prev_spell = sSpellMgr->GetPrevSpellInChain(spell_id);
        if (!prev_spell)                                    // first rank, remove skill
            SetSkill(spellLearnSkill->skill, 0, 0, 0);
        else
        {
            // search prev. skill setting by spell ranks chain
            SpellLearnSkillNode const* prevSkill = sSpellMgr->GetSpellLearnSkill(prev_spell);
            while (!prevSkill && prev_spell)
            {
                prev_spell = sSpellMgr->GetPrevSpellInChain(prev_spell);
                prevSkill = sSpellMgr->GetSpellLearnSkill(sSpellMgr->GetFirstSpellInChain(prev_spell));
            }

            if (!prevSkill)                                 // not found prev skill setting, remove skill
                SetSkill(spellLearnSkill->skill, 0, 0, 0);
            else                                            // set to prev. skill setting values
            {
                uint32 skill_value = GetPureSkillValue(prevSkill->skill);
                uint32 skill_max_value = GetPureMaxSkillValue(prevSkill->skill);

                if (skill_value > prevSkill->value)
                    skill_value = prevSkill->value;

                uint32 new_skill_max_value = prevSkill->maxvalue == 0 ? GetMaxSkillValueForLevel() : prevSkill->maxvalue;

                if (skill_max_value > new_skill_max_value)
                    skill_max_value = new_skill_max_value;

                SetSkill(prevSkill->skill, prevSkill->step, skill_value, skill_max_value);
            }
        }
    }
    else
    {
        // not ranked skills
        SkillLineAbilityMapBounds bounds = sSpellMgr->GetSkillLineAbilityMapBounds(spell_id);

        for (SkillLineAbilityMap::const_iterator _spell_idx = bounds.first; _spell_idx != bounds.second; ++_spell_idx)
        {
            SkillLineEntry const* pSkill = sSkillLineStore.LookupEntry(_spell_idx->second->skillId);
            if (!pSkill)
                continue;

            if ((_spell_idx->second->learnOnGetSkill == ABILITY_LEARNED_ON_GET_RACE_OR_CLASS_SKILL &&
                pSkill->categoryId != SKILL_CATEGORY_CLASS) ||// not unlearn class skills (spellbook/talent pages)
                // lockpicking/runeforging special case, not have ABILITY_LEARNED_ON_GET_RACE_OR_CLASS_SKILL
                ((pSkill->id == SKILL_LOCKPICKING || pSkill->id == SKILL_RUNEFORGING) && _spell_idx->second->max_value == 0))
            {
                // not reset skills for professions and racial abilities
                if ((pSkill->categoryId == SKILL_CATEGORY_SECONDARY || pSkill->categoryId == SKILL_CATEGORY_PROFESSION) &&
                    (IsProfessionSkill(pSkill->id) || _spell_idx->second->racemask != 0))
                    continue;

                SetSkill(pSkill->id, GetSkillStep(pSkill->id), 0, 0);
            }

            // most likely will never be used, haven't heard of cases where players unlearn a mount
            if (Has310Flyer(false) && _spell_idx->second->skillId == SKILL_MOUNTS)
            {
                if (spellInfo)
                    for (uint8 i = 0; i < MAX_SPELL_EFFECTS; ++i)
                        if (spellInfo->Effects[i].ApplyAuraName == SPELL_AURA_MOD_INCREASE_MOUNTED_FLIGHT_SPEED &&
                            spellInfo->Effects[i].CalcValue() == 310)
                            Has310Flyer(true, spell_id);    // with true as first argument its also used to set/remove the flag
            }
        }
    }

    // remove dependent spells
    SpellLearnSpellMapBounds spell_bounds = sSpellMgr->GetSpellLearnSpellMapBounds(spell_id);

    for (SpellLearnSpellMap::const_iterator itr2 = spell_bounds.first; itr2 != spell_bounds.second; ++itr2)
        removeSpell(itr2->second.spell, disabled);

    // activate lesser rank in spellbook/action bar, and cast it if need
    bool prev_activate = false;

    if (uint32 prev_id = sSpellMgr->GetPrevSpellInChain(spell_id))
    {
        // if talent then lesser rank also talent and need learn
        if (talentCosts)
        {
            // I cannot see why mangos has these lines.
            //if (learn_low_rank)
            //    learnSpell(prev_id, false);
        }
        // if ranked non-stackable spell: need activate lesser rank and update dendence state
        else if (cur_active && !spellInfo->IsStackableWithRanks() && spellInfo->IsRanked())
        {
            // need manually update dependence state (learn spell ignore like attempts)
            PlayerSpellMap::iterator prev_itr = m_spells.find(prev_id);
            if (prev_itr != m_spells.end())
            {
                if (prev_itr->second->dependent != cur_dependent)
                {
                    prev_itr->second->dependent = cur_dependent;
                    if (prev_itr->second->state != PLAYERSPELL_NEW)
                        prev_itr->second->state = PLAYERSPELL_CHANGED;
                }

                // now re-learn if need re-activate
                if (cur_active && !prev_itr->second->active && learn_low_rank)
                {
                    if (addSpell(prev_id, true, false, prev_itr->second->dependent, prev_itr->second->disabled))
                    {
                        // downgrade spell ranks in spellbook and action bar
                        WorldPacket data(SMSG_SUPERCEDED_SPELL, 4 + 4);
                        data << uint32(spell_id);
                        data << uint32(prev_id);
                        GetSession()->SendPacket(&data);
                        prev_activate = true;
                    }
                }
            }
        }
    }

    if (spell_id == 46917 && m_canTitanGrip)
        SetCanTitanGrip(false);
    if (spell_id == 674 && m_canDualWield)
        SetCanDualWield(false);

    if (sWorld->getBoolConfig(CONFIG_OFFHAND_CHECK_AT_SPELL_UNLEARN))
        AutoUnequipOffhandIfNeed();

    // remove from spell book if not replaced by lesser rank
    if (!prev_activate)
    {
        WorldPacket data(SMSG_REMOVED_SPELL, 4);
        data << uint32(spell_id);
        GetSession()->SendPacket(&data);
    }
}

bool Player::Has310Flyer(bool checkAllSpells, uint32 excludeSpellId)
{
    if (!checkAllSpells)
        return m_ExtraFlags & PLAYER_EXTRA_HAS_310_FLYER;
    else
    {
        SetHas310Flyer(false);
        SpellInfo const* spellInfo;
        for (PlayerSpellMap::iterator itr = m_spells.begin(); itr != m_spells.end(); ++itr)
        {
            if (itr->first == excludeSpellId)
                continue;

            SkillLineAbilityMapBounds bounds = sSpellMgr->GetSkillLineAbilityMapBounds(itr->first);
            for (SkillLineAbilityMap::const_iterator _spell_idx = bounds.first; _spell_idx != bounds.second; ++_spell_idx)
            {
                if (_spell_idx->second->skillId != SKILL_MOUNTS)
                    break;  // We can break because mount spells belong only to one skillline (at least 310 flyers do)

                spellInfo = sSpellMgr->GetSpellInfo(itr->first);
                for (uint8 i = 0; i < MAX_SPELL_EFFECTS; ++i)
                    if (spellInfo->Effects[i].ApplyAuraName == SPELL_AURA_MOD_INCREASE_MOUNTED_FLIGHT_SPEED &&
                        spellInfo->Effects[i].CalcValue() == 310)
                    {
                        SetHas310Flyer(true);
                        return true;
                    }
            }
        }
    }

    return false;
}

void Player::RemoveSpellCooldown(uint32 spell_id, bool update /* = false */)
{
    m_spellCooldowns.erase(spell_id);

    if (update)
        SendClearCooldown(spell_id, this);
}

// I am not sure which one is more efficient
void Player::RemoveCategoryCooldown(uint32 cat)
{
    SpellCategoryStore::const_iterator i_scstore = sSpellCategoryStore.find(cat);
    if (i_scstore != sSpellCategoryStore.end())
        for (SpellCategorySet::const_iterator i_scset = i_scstore->second.begin(); i_scset != i_scstore->second.end(); ++i_scset)
            RemoveSpellCooldown(*i_scset, true);
}

void Player::RemoveSpellCategoryCooldown(uint32 cat, bool update /* = false */)
{
    SpellCategoryStore::const_iterator ct = sSpellCategoryStore.find(cat);
    if (ct == sSpellCategoryStore.end())
        return;

    const SpellCategorySet& ct_set = ct->second;
    for (SpellCooldowns::const_iterator i = m_spellCooldowns.begin(); i != m_spellCooldowns.end();)
    {
        if (ct_set.find(i->first) != ct_set.end())
            RemoveSpellCooldown((i++)->first, update);
        else
            ++i;
    }
}

void Player::RemoveArenaSpellCooldowns(bool removeActivePetCooldowns)
{
    // remove cooldowns on spells that have <= 10 min CD

    SpellCooldowns::iterator itr, next;
    for (itr = m_spellCooldowns.begin(); itr != m_spellCooldowns.end(); itr = next)
    {
        next = itr;
        ++next;
        SpellInfo const* entry = sSpellMgr->GetSpellInfo(itr->first);
        // check if spellentry is present and if the cooldown is less or equal to 10 min
        if (entry &&
            entry->RecoveryTime <= 10 * MINUTE * IN_MILLISECONDS &&
            entry->CategoryRecoveryTime <= 10 * MINUTE * IN_MILLISECONDS)
        {
            // remove & notify
            RemoveSpellCooldown(itr->first, true);
        }
    }

    // pet cooldowns
    if (removeActivePetCooldowns)
        if (Pet* pet = GetPet())
        {
            // notify player
            for (CreatureSpellCooldowns::const_iterator itr2 = pet->m_CreatureSpellCooldowns.begin(); itr2 != pet->m_CreatureSpellCooldowns.end(); ++itr2)
                SendClearCooldown(itr2->first, pet);

            // actually clear cooldowns
            pet->m_CreatureSpellCooldowns.clear();
        }
}

void Player::RemoveAllSpellCooldown()
{
    if (!m_spellCooldowns.empty())
    {
        for (SpellCooldowns::const_iterator itr = m_spellCooldowns.begin(); itr != m_spellCooldowns.end(); ++itr)
            SendClearCooldown(itr->first, this);

        m_spellCooldowns.clear();
    }
}

void Player::_LoadSpellCooldowns(PreparedQueryResult result)
{
    // some cooldowns can be already set at aura loading...

    //QueryResult* result = CharacterDatabase.PQuery("SELECT spell, item, time FROM character_spell_cooldown WHERE guid = '%u'", GetGUIDLow());

    if (result)
    {
        time_t curTime = time(NULL);

        do
        {
            Field* fields = result->Fetch();
            uint32 spell_id = fields[0].GetUInt32();
            uint32 item_id  = fields[1].GetUInt32();
            time_t db_time  = time_t(fields[2].GetUInt32());

            if (!sSpellMgr->GetSpellInfo(spell_id))
            {
                sLog->outError(LOG_FILTER_PLAYER_LOADING, "Player %u has unknown spell %u in `character_spell_cooldown`, skipping.", GetGUIDLow(), spell_id);
                continue;
            }

            // skip outdated cooldown
            if (db_time <= curTime)
                continue;

            AddSpellCooldown(spell_id, item_id, db_time);

            sLog->outDebug(LOG_FILTER_PLAYER_LOADING, "Player (GUID: %u) spell %u, item %u cooldown loaded (%u secs).", GetGUIDLow(), spell_id, item_id, uint32(db_time-curTime));
        }
        while (result->NextRow());
    }
}

void Player::_SaveSpellCooldowns(SQLTransaction& trans)
{
    PreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_CHAR_SPELL_COOLDOWN);
    stmt->setUInt32(0, GetGUIDLow());
    trans->Append(stmt);

    time_t curTime = time(NULL);
    time_t infTime = curTime + infinityCooldownDelayCheck;

    bool first_round = true;
    std::ostringstream ss;

    // remove outdated and save active
    for (SpellCooldowns::iterator itr = m_spellCooldowns.begin(); itr != m_spellCooldowns.end();)
    {
        if (itr->second.end <= curTime)
            m_spellCooldowns.erase(itr++);
        else if (itr->second.end <= infTime)                 // not save locked cooldowns, it will be reset or set at reload
        {
            if (first_round)
            {
                ss << "INSERT INTO character_spell_cooldown (guid, spell, item, time) VALUES ";
                first_round = false;
            }
            // next new/changed record prefix
            else
                ss << ',';
            ss << '(' << GetGUIDLow() << ',' << itr->first << ',' << itr->second.itemid << ',' << uint64(itr->second.end) << ')';
            ++itr;
        }
        else
            ++itr;
    }
    // if something changed execute
    if (!first_round)
        trans->Append(ss.str().c_str());
}

uint32 Player::resetTalentsCost() const
{
    // The first time reset costs 1 gold
    if (m_resetTalentsCost < 1*GOLD)
        return 1*GOLD;
    // then 5 gold
    else if (m_resetTalentsCost < 5*GOLD)
        return 5*GOLD;
    // After that it increases in increments of 5 gold
    else if (m_resetTalentsCost < 10*GOLD)
        return 10*GOLD;
    else
    {
        uint64 months = (sWorld->GetGameTime() - m_resetTalentsTime)/MONTH;
        if (months > 0)
        {
            // This cost will be reduced by a rate of 5 gold per month
            int32 new_cost = int32(m_resetTalentsCost - 5*GOLD*months);
            // to a minimum of 10 gold.
            return (new_cost < 10*GOLD ? 10*GOLD : new_cost);
        }
        else
        {
            // After that it increases in increments of 5 gold
            int32 new_cost = m_resetTalentsCost + 5*GOLD;
            // until it hits a cap of 50 gold.
            if (new_cost > 50*GOLD)
                new_cost = 50*GOLD;
            return new_cost;
        }
    }
}

bool Player::resetTalents(bool no_cost)
{
    sScriptMgr->OnPlayerTalentsReset(this, no_cost);

    // not need after this call
    if (HasAtLoginFlag(AT_LOGIN_RESET_TALENTS))
        RemoveAtLoginFlag(AT_LOGIN_RESET_TALENTS, true);

    uint32 talentPointsForLevel = CalculateTalentsPoints();

    if (m_usedTalentCount == 0)
    {
        SetFreeTalentPoints(talentPointsForLevel);
        return false;
    }

    uint32 cost = 0;

    if (!no_cost && !sWorld->getBoolConfig(CONFIG_NO_RESET_TALENT_COST))
    {
        cost = resetTalentsCost();

        if (!HasEnoughMoney(cost))
        {
            SendBuyError(BUY_ERR_NOT_ENOUGHT_MONEY, 0, 0, 0);
            return false;
        }
    }

    RemovePet(NULL, PET_SAVE_NOT_IN_SLOT, true);

    for (uint32 talentId = 0; talentId < sTalentStore.GetNumRows(); ++talentId)
    {
        TalentEntry const* talentInfo = sTalentStore.LookupEntry(talentId);

        if (!talentInfo)
            continue;

        TalentTabEntry const* talentTabInfo = sTalentTabStore.LookupEntry(talentInfo->TalentTab);

        if (!talentTabInfo)
            continue;

        // unlearn only talents for character class
        // some spell learned by one class as normal spells or know at creation but another class learn it as talent,
        // to prevent unexpected lost normal learned spell skip another class talents
        if ((getClassMask() & talentTabInfo->ClassMask) == 0)
            continue;

        for (int8 rank = MAX_TALENT_RANK-1; rank >= 0; --rank)
        {
            // skip non-existant talent ranks
            if (talentInfo->RankID[rank] == 0)
                continue;
            const SpellInfo* _spellEntry = sSpellMgr->GetSpellInfo(talentInfo->RankID[rank]);
            if (!_spellEntry)
                continue;
            removeSpell(talentInfo->RankID[rank], true);
            // search for spells that the talent teaches and unlearn them
            for (uint8 i = 0; i < MAX_SPELL_EFFECTS; ++i)
                if (_spellEntry->Effects[i].TriggerSpell > 0 && _spellEntry->Effects[i].Effect == SPELL_EFFECT_LEARN_SPELL)
                    removeSpell(_spellEntry->Effects[i].TriggerSpell, true);
            // if this talent rank can be found in the PlayerTalentMap, mark the talent as removed so it gets deleted
            PlayerTalentMap::iterator plrTalent = m_talents[m_activeSpec]->find(talentInfo->RankID[rank]);
            if (plrTalent != m_talents[m_activeSpec]->end())
                plrTalent->second->state = PLAYERSPELL_REMOVED;
        }
    }

    SQLTransaction trans = CharacterDatabase.BeginTransaction();
    _SaveTalents(trans);
    _SaveSpells(trans);
    CharacterDatabase.CommitTransaction(trans);

    SetFreeTalentPoints(talentPointsForLevel);

    if (!no_cost)
    {
        ModifyMoney(-(int32)cost);
        UpdateAchievementCriteria(ACHIEVEMENT_CRITERIA_TYPE_GOLD_SPENT_FOR_TALENTS, cost);
        UpdateAchievementCriteria(ACHIEVEMENT_CRITERIA_TYPE_NUMBER_OF_TALENT_RESETS, 1);

        m_resetTalentsCost = cost;
        m_resetTalentsTime = time(NULL);
    }

    /* when prev line will dropped use next line
    if (Pet* pet = GetPet())
    {
        if (pet->getPetType() == HUNTER_PET && !pet->GetCreatureTemplate()->isTameable(CanTameExoticPets()))
            RemovePet(NULL, PET_SAVE_NOT_IN_SLOT, true);
    }
    */

    return true;
}

void Player::SetFreeTalentPoints(uint32 points)
{
    sScriptMgr->OnPlayerFreeTalentPointsChanged(this, points);
    SetUInt32Value(PLAYER_CHARACTER_POINTS1, points);
}

Mail* Player::GetMail(uint32 id)
{
    for (PlayerMails::iterator itr = m_mail.begin(); itr != m_mail.end(); ++itr)
        if ((*itr)->messageID == id)
            return (*itr);

    return NULL;
}

void Player::BuildCreateUpdateBlockForPlayer(UpdateData* data, Player* target) const
{
    for (uint8 i = 0; i < EQUIPMENT_SLOT_END; ++i)
    {
        if (m_items[i] == NULL)
            continue;

        m_items[i]->BuildCreateUpdateBlockForPlayer(data, target);
    }

    if (target == this)
    {
        for (uint8 i = INVENTORY_SLOT_BAG_START; i < BANK_SLOT_BAG_END; ++i)
        {
            if (m_items[i] == NULL)
                continue;

            m_items[i]->BuildCreateUpdateBlockForPlayer(data, target);
        }
        for (uint8 i = KEYRING_SLOT_START; i < CURRENCYTOKEN_SLOT_END; ++i)
        {
            if (m_items[i] == NULL)
                continue;

            m_items[i]->BuildCreateUpdateBlockForPlayer(data, target);
        }
    }

    Unit::BuildCreateUpdateBlockForPlayer(data, target);
}

void Player::DestroyForPlayer(Player* target, bool onDeath) const
{
    Unit::DestroyForPlayer(target, onDeath);

    for (uint8 i = 0; i < INVENTORY_SLOT_BAG_END; ++i)
    {
        if (m_items[i] == NULL)
            continue;

        m_items[i]->DestroyForPlayer(target);
    }

    if (target == this)
    {
        for (uint8 i = INVENTORY_SLOT_BAG_START; i < BANK_SLOT_BAG_END; ++i)
        {
            if (m_items[i] == NULL)
                continue;

            m_items[i]->DestroyForPlayer(target);
        }
        for (uint8 i = KEYRING_SLOT_START; i < CURRENCYTOKEN_SLOT_END; ++i)
        {
            if (m_items[i] == NULL)
                continue;

            m_items[i]->DestroyForPlayer(target);
        }
    }
}

bool Player::HasSpell(uint32 spell) const
{
    PlayerSpellMap::const_iterator itr = m_spells.find(spell);
    return (itr != m_spells.end() && itr->second->state != PLAYERSPELL_REMOVED &&
        !itr->second->disabled);
}

bool Player::HasTalent(uint32 spell, uint8 spec) const
{
    PlayerTalentMap::const_iterator itr = m_talents[spec]->find(spell);
    return (itr != m_talents[spec]->end() && itr->second->state != PLAYERSPELL_REMOVED);
}

bool Player::HasActiveSpell(uint32 spell) const
{
    PlayerSpellMap::const_iterator itr = m_spells.find(spell);
    return (itr != m_spells.end() && itr->second->state != PLAYERSPELL_REMOVED &&
        itr->second->active && !itr->second->disabled);
}

TrainerSpellState Player::GetTrainerSpellState(TrainerSpell const* trainer_spell) const
{
    if (!trainer_spell)
        return TRAINER_SPELL_RED;

    bool hasSpell = true;
    for (uint8 i = 0; i < MAX_SPELL_EFFECTS; ++i)
    {
        if (!trainer_spell->learnedSpell[i])
            continue;

        if (!HasSpell(trainer_spell->learnedSpell[i]))
        {
            hasSpell = false;
            break;
        }
    }
    // known spell
    if (hasSpell)
        return TRAINER_SPELL_GRAY;

    // check skill requirement
    if (trainer_spell->reqSkill && GetBaseSkillValue(trainer_spell->reqSkill) < trainer_spell->reqSkillValue)
        return TRAINER_SPELL_RED;

    // check level requirement
    if (getLevel() < trainer_spell->reqLevel)
        return TRAINER_SPELL_RED;

    for (uint8 i = 0; i < MAX_SPELL_EFFECTS; ++i)
    {
        if (!trainer_spell->learnedSpell[i])
            continue;

        // check race/class requirement
        if (!IsSpellFitByClassAndRace(trainer_spell->learnedSpell[i]))
            return TRAINER_SPELL_RED;

        if (uint32 prevSpell = sSpellMgr->GetPrevSpellInChain(trainer_spell->learnedSpell[i]))
        {
            // check prev.rank requirement
            if (prevSpell && !HasSpell(prevSpell))
                return TRAINER_SPELL_RED;
        }

        SpellsRequiringSpellMapBounds spellsRequired = sSpellMgr->GetSpellsRequiredForSpellBounds(trainer_spell->learnedSpell[i]);
        for (SpellsRequiringSpellMap::const_iterator itr = spellsRequired.first; itr != spellsRequired.second; ++itr)
        {
            // check additional spell requirement
            if (!HasSpell(itr->second))
                return TRAINER_SPELL_RED;
        }
    }

    // check primary prof. limit
    // first rank of primary profession spell when there are no proffesions avalible is disabled
    for (uint8 i = 0; i < MAX_SPELL_EFFECTS; ++i)
    {
        if (!trainer_spell->learnedSpell[i])
            continue;
        SpellInfo const* learnedSpellInfo = sSpellMgr->GetSpellInfo(trainer_spell->learnedSpell[i]);
        if (learnedSpellInfo && learnedSpellInfo->IsPrimaryProfessionFirstRank() && (GetFreePrimaryProfessionPoints() == 0))
            return TRAINER_SPELL_GREEN_DISABLED;
    }

    return TRAINER_SPELL_GREEN;
}

/**
 * Deletes a character from the database
 *
 * The way, how the characters will be deleted is decided based on the config option.
 *
 * @see Player::DeleteOldCharacters
 *
 * @param playerguid       the low-GUID from the player which should be deleted
 * @param accountId        the account id from the player
 * @param updateRealmChars when this flag is set, the amount of characters on that realm will be updated in the realmlist
 * @param deleteFinally    if this flag is set, the config option will be ignored and the character will be permanently removed from the database
 */
void Player::DeleteFromDB(uint64 playerguid, uint32 accountId, bool updateRealmChars, bool deleteFinally)
{
    // for not existed account avoid update realm
    if (accountId == 0)
        updateRealmChars = false;

    uint32 charDelete_method = sWorld->getIntConfig(CONFIG_CHARDELETE_METHOD);
    uint32 charDelete_minLvl = sWorld->getIntConfig(CONFIG_CHARDELETE_MIN_LEVEL);

    // if we want to finally delete the character or the character does not meet the level requirement,
    // we set it to mode CHAR_DELETE_REMOVE
    if (deleteFinally || Player::GetLevelFromDB(playerguid) < charDelete_minLvl)
        charDelete_method = CHAR_DELETE_REMOVE;

    uint32 guid = GUID_LOPART(playerguid);

    // convert corpse to bones if exist (to prevent exiting Corpse in World without DB entry)
    // bones will be deleted by corpse/bones deleting thread shortly
    sObjectAccessor->ConvertCorpseForPlayer(playerguid);

    if (uint32 guildId = GetGuildIdFromDB(playerguid))
        if (Guild* guild = sGuildMgr->GetGuildById(guildId))
            guild->DeleteMember(guid);

    // remove from arena teams
    LeaveAllArenaTeams(playerguid);

    // the player was uninvited already on logout so just remove from group
    PreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_GROUP_MEMBER);
    stmt->setUInt32(0, guid);
    PreparedQueryResult resultGroup = CharacterDatabase.Query(stmt);

    if (resultGroup)
        if (Group* group = sGroupMgr->GetGroupByDbStoreId((*resultGroup)[0].GetUInt32()))
            RemoveFromGroup(group, playerguid);

    // Remove signs from petitions (also remove petitions if owner);
    RemovePetitionsAndSigns(playerguid, 10);

    switch (charDelete_method)
    {
        // Completely remove from the database
        case CHAR_DELETE_REMOVE:
        {
            SQLTransaction trans = CharacterDatabase.BeginTransaction();

            stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHAR_COD_ITEM_MAIL);
            stmt->setUInt32(0, guid);
            PreparedQueryResult resultMail = CharacterDatabase.Query(stmt);

            if (resultMail)
            {
                do
                {
                    Field* mailFields = resultMail->Fetch();

                    uint32 mail_id       = mailFields[0].GetUInt32();
                    uint8 mailType       = mailFields[1].GetUInt8();
                    uint16 mailTemplateId= mailFields[2].GetUInt16();
                    uint32 sender        = mailFields[3].GetUInt32();
                    std::string subject  = mailFields[4].GetString();
                    std::string body     = mailFields[5].GetString();
                    uint32 money         = mailFields[6].GetUInt32();
                    bool has_items       = mailFields[7].GetBool();

                    // We can return mail now
                    // So firstly delete the old one
                    PreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_MAIL_BY_ID);
                    stmt->setUInt32(0, mail_id);
                    trans->Append(stmt);

                    // Mail is not from player
                    if (mailType != MAIL_NORMAL)
                    {
                        if (has_items)
                        {
                            PreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_MAIL_ITEM_BY_ID);
                            stmt->setUInt32(0, mail_id);
                            trans->Append(stmt);
                        }
                        continue;
                    }

                    MailDraft draft(subject, body);
                    if (mailTemplateId)
                        draft = MailDraft(mailTemplateId, false);    // items are already included

                    if (has_items)
                    {
                        // Data needs to be at first place for Item::LoadFromDB
                        PreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_MAILITEMS);
                        stmt->setUInt32(0, mail_id);
                        PreparedQueryResult resultItems = CharacterDatabase.Query(stmt);
                        if (resultItems)
                        {
                            do
                            {
                                Field* itemFields = resultItems->Fetch();
                                uint32 item_guidlow = itemFields[11].GetUInt32();
                                uint32 item_template = itemFields[12].GetUInt32();

                                ItemTemplate const* itemProto = sObjectMgr->GetItemTemplate(item_template);
                                if (!itemProto)
                                {
                                    stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_ITEM_INSTANCE);
                                    stmt->setUInt32(0, item_guidlow);
                                    trans->Append(stmt);
                                    continue;
                                }

                                Item* pItem = NewItemOrBag(itemProto);
                                if (!pItem->LoadFromDB(item_guidlow, MAKE_NEW_GUID(guid, 0, HIGHGUID_PLAYER), itemFields, item_template))
                                {
                                    pItem->FSetState(ITEM_REMOVED);
                                    pItem->SaveToDB(trans);              // it also deletes item object!
                                    continue;
                                }

                                draft.AddItem(pItem);
                            }
                            while (resultItems->NextRow());
                        }
                    }

                    stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_MAIL_ITEM_BY_ID);
                    stmt->setUInt32(0, mail_id);
                    trans->Append(stmt);

                    uint32 pl_account = sObjectMgr->GetPlayerAccountIdByGUID(MAKE_NEW_GUID(guid, 0, HIGHGUID_PLAYER));

                    draft.AddMoney(money).SendReturnToSender(pl_account, guid, sender, trans);
                }
                while (resultMail->NextRow());
            }

            // Unsummon and delete for pets in world is not required: player deleted from CLI or character list with not loaded pet.
            // NOW we can finally clear other DB data related to character
            stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHAR_PETS);
            stmt->setUInt32(0, guid);
            PreparedQueryResult resultPets = CharacterDatabase.Query(stmt);

            if (resultPets)
            {
                do
                {
                    uint32 petguidlow = (*resultPets)[0].GetUInt32();
                    Pet::DeleteFromDB(petguidlow);
                } while (resultPets->NextRow());
            }

            // Delete char from social list of online chars
            stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHAR_SOCIAL);
            stmt->setUInt32(0, guid);
            PreparedQueryResult resultFriends = CharacterDatabase.Query(stmt);

            if (resultFriends)
            {
                do
                {
                    if (Player* pFriend = ObjectAccessor::FindPlayer(MAKE_NEW_GUID((*resultFriends)[0].GetUInt32(), 0, HIGHGUID_PLAYER)))
                    {
                        if (pFriend->IsInWorld())
                        {
                            pFriend->GetSocial()->RemoveFromSocialList(guid, false);
                            sSocialMgr->SendFriendStatus(pFriend, FRIEND_REMOVED, guid, false);
                        }
                    }
                } while (resultFriends->NextRow());
            }

            stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_CHARACTER);
            stmt->setUInt32(0, guid);
            trans->Append(stmt);

            stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_PLAYER_ACCOUNT_DATA);
            stmt->setUInt32(0, guid);
            trans->Append(stmt);

            stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_CHAR_DECLINED_NAME);
            stmt->setUInt32(0, guid);
            trans->Append(stmt);

            stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_CHAR_ACTION);
            stmt->setUInt32(0, guid);
            trans->Append(stmt);

            stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_CHAR_AURA);
            stmt->setUInt32(0, guid);
            trans->Append(stmt);

            stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_CHAR_GIFT);
            stmt->setUInt32(0, guid);
            trans->Append(stmt);

            stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_PLAYER_HOMEBIND);
            stmt->setUInt32(0, guid);
            trans->Append(stmt);

            stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_CHAR_INSTANCE);
            stmt->setUInt32(0, guid);
            trans->Append(stmt);

            stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_CHAR_INVENTORY);
            stmt->setUInt32(0, guid);
            trans->Append(stmt);

            stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_CHAR_QUESTSTATUS);
            stmt->setUInt32(0, guid);
            trans->Append(stmt);

            stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_CHAR_QUESTSTATUS_REWARDED);
            stmt->setUInt32(0, guid);
            trans->Append(stmt);

            stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_CHAR_REPUTATION);
            stmt->setUInt32(0, guid);
            trans->Append(stmt);

            stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_CHAR_SPELL);
            stmt->setUInt32(0, guid);
            trans->Append(stmt);

            stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_CHAR_SPELL_COOLDOWN);
            stmt->setUInt32(0, guid);
            trans->Append(stmt);

            stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_PLAYER_GM_TICKETS);
            stmt->setUInt32(0, guid);
            trans->Append(stmt);

            stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_ITEM_INSTANCE);
            stmt->setUInt32(0, guid);
            trans->Append(stmt);

            stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_CHAR_SOCIAL_BY_FRIEND);
            stmt->setUInt32(0, guid);
            trans->Append(stmt);

            stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_CHAR_SOCIAL_BY_GUID);
            stmt->setUInt32(0, guid);
            trans->Append(stmt);

            stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_MAIL);
            stmt->setUInt32(0, guid);
            trans->Append(stmt);

            stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_MAIL_ITEMS);
            stmt->setUInt32(0, guid);
            trans->Append(stmt);

            stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_CHAR_PET_BY_OWNER);
            stmt->setUInt32(0, guid);
            trans->Append(stmt);

            stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_CHAR_PET_DECLINEDNAME_BY_OWNER);
            stmt->setUInt32(0, guid);
            trans->Append(stmt);

            stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_CHAR_ACHIEVEMENTS);
            stmt->setUInt32(0, guid);
            trans->Append(stmt);

            stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_CHAR_ACHIEVEMENT_PROGRESS);
            stmt->setUInt32(0, guid);
            trans->Append(stmt);

            stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_CHAR_EQUIPMENTSETS);
            stmt->setUInt32(0, guid);
            trans->Append(stmt);

            stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_GUILD_EVENTLOG_BY_PLAYER);
            stmt->setUInt32(0, guid);
            stmt->setUInt32(1, guid);
            trans->Append(stmt);

            stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_GUILD_BANK_EVENTLOG_BY_PLAYER);
            stmt->setUInt32(0, guid);
            trans->Append(stmt);

            stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_PLAYER_BGDATA);
            stmt->setUInt32(0, guid);
            trans->Append(stmt);

            stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_CHAR_GLYPHS);
            stmt->setUInt32(0, guid);
            trans->Append(stmt);

            stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_CHAR_QUESTSTATUS_DAILY);
            stmt->setUInt32(0, guid);
            trans->Append(stmt);

            stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_CHAR_TALENT);
            stmt->setUInt32(0, guid);
            trans->Append(stmt);

            stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_CHAR_SKILLS);
            stmt->setUInt32(0, guid);
            trans->Append(stmt);

            CharacterDatabase.CommitTransaction(trans);
            break;
        }
        // The character gets unlinked from the account, the name gets freed up and appears as deleted ingame
        case CHAR_DELETE_UNLINK:
        {
            PreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_UPD_DELETE_INFO);

            stmt->setUInt32(0, guid);

            CharacterDatabase.Execute(stmt);
            break;
        }
        default:
            sLog->outError(LOG_FILTER_PLAYER, "Player::DeleteFromDB: Unsupported delete method: %u.", charDelete_method);
    }

    if (updateRealmChars)
        sWorld->UpdateRealmCharCount(accountId);
}

/**
 * Characters which were kept back in the database after being deleted and are now too old (see config option "CharDelete.KeepDays"), will be completely deleted.
 *
 * @see Player::DeleteFromDB
 */
void Player::DeleteOldCharacters()
{
    uint32 keepDays = sWorld->getIntConfig(CONFIG_CHARDELETE_KEEP_DAYS);
    if (!keepDays)
        return;

    Player::DeleteOldCharacters(keepDays);
}

/**
 * Characters which were kept back in the database after being deleted and are older than the specified amount of days, will be completely deleted.
 *
 * @see Player::DeleteFromDB
 *
 * @param keepDays overrite the config option by another amount of days
 */
void Player::DeleteOldCharacters(uint32 keepDays)
{
    sLog->outInfo(LOG_FILTER_PLAYER, "Player::DeleteOldChars: Deleting all characters which have been deleted %u days before...", keepDays);

    PreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHAR_OLD_CHARS);
    stmt->setUInt32(0, uint32(time(NULL) - time_t(keepDays * DAY)));
    PreparedQueryResult result = CharacterDatabase.Query(stmt);

    if (result)
    {
         sLog->outDebug(LOG_FILTER_PLAYER, "Player::DeleteOldChars: Found " UI64FMTD " character(s) to delete", result->GetRowCount());
         do
         {
            Field* fields = result->Fetch();
            Player::DeleteFromDB(fields[0].GetUInt32(), fields[1].GetUInt32(), true, true);
         }
         while (result->NextRow());
    }
}

void Player::SetMovement(PlayerMovementType pType)
{
    WorldPacket data;
    switch (pType)
    {
        case MOVE_ROOT:       data.Initialize(SMSG_FORCE_MOVE_ROOT,   GetPackGUID().size()+4); break;
        case MOVE_UNROOT:     data.Initialize(SMSG_FORCE_MOVE_UNROOT, GetPackGUID().size()+4); break;
        case MOVE_WATER_WALK: data.Initialize(SMSG_MOVE_WATER_WALK,   GetPackGUID().size()+4); break;
        case MOVE_LAND_WALK:  data.Initialize(SMSG_MOVE_LAND_WALK,    GetPackGUID().size()+4); break;
        default:
            sLog->outError(LOG_FILTER_PLAYER, "Player::SetMovement: Unsupported move type (%d), data not sent to client.", pType);
            return;
    }
    data.append(GetPackGUID());
    data << uint32(0);
    GetSession()->SendPacket(&data);
}

/* Preconditions:
  - a resurrectable corpse must not be loaded for the player (only bones)
  - the player must be in world
*/
void Player::BuildPlayerRepop()
{
    WorldPacket data(SMSG_PRE_RESURRECT, GetPackGUID().size());
    data.append(GetPackGUID());
    GetSession()->SendPacket(&data);

    if (getRace() == RACE_NIGHTELF)
        CastSpell(this, 20584, true);
    CastSpell(this, 8326, true);

    // there must be SMSG.FORCE_RUN_SPEED_CHANGE, SMSG.FORCE_SWIM_SPEED_CHANGE, SMSG.MOVE_WATER_WALK
    // there must be SMSG.STOP_MIRROR_TIMER
    // there we must send 888 opcode

    // the player cannot have a corpse already, only bones which are not returned by GetCorpse
    if (GetCorpse())
    {
        sLog->outError(LOG_FILTER_PLAYER, "BuildPlayerRepop: player %s(%d) already has a corpse", GetName(), GetGUIDLow());
        return;
    }

    // create a corpse and place it at the player's location
    CreateCorpse();
    Corpse* corpse = GetCorpse();
    if (!corpse)
    {
        sLog->outError(LOG_FILTER_PLAYER, "Error creating corpse for Player %s [%u]", GetName(), GetGUIDLow());
        return;
    }
    GetMap()->AddToMap(corpse);

    // convert player body to ghost
    SetHealth(1);

    SetMovement(MOVE_WATER_WALK);
    if (!GetSession()->isLogingOut())
        SetMovement(MOVE_UNROOT);

    // BG - remove insignia related
    RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_SKINNABLE);

//    SendCorpseReclaimDelay();

    // to prevent cheating
    corpse->ResetGhostTime();

    StopMirrorTimers();                                     //disable timers(bars)

    SetFloatValue(UNIT_FIELD_BOUNDINGRADIUS, float(1.0f));   //see radius of death player?

    // set and clear other
    SetByteValue(UNIT_FIELD_BYTES_1, 3, UNIT_BYTE1_FLAG_ALWAYS_STAND);
}

void Player::ResurrectPlayer(float restore_percent, bool applySickness)
{
    WorldPacket data(SMSG_DEATH_RELEASE_LOC, 4*4);          // remove spirit healer position
    data << uint32(-1);
    data << float(0);
    data << float(0);
    data << float(0);
    GetSession()->SendPacket(&data);

    // speed change, land walk

    // remove death flag + set aura
    SetByteValue(UNIT_FIELD_BYTES_1, 3, 0x00);
    if (getRace() == RACE_NIGHTELF)
        RemoveAurasDueToSpell(20584);                       // speed bonuses
    RemoveAurasDueToSpell(8326);                            // SPELL_AURA_GHOST

    if (GetSession()->IsARecruiter() || (GetSession()->GetRecruiterId() != 0))
        SetFlag(UNIT_DYNAMIC_FLAGS, UNIT_DYNFLAG_REFER_A_FRIEND);

    setDeathState(ALIVE);

    SetMovement(MOVE_LAND_WALK);
    SetMovement(MOVE_UNROOT);

    m_deathTimer = 0;

    // set health/powers (0- will be set in caller)
    if (restore_percent > 0.0f)
    {
        SetHealth(uint32(GetMaxHealth()*restore_percent));
        SetPower(POWER_MANA, uint32(GetMaxPower(POWER_MANA)*restore_percent));
        SetPower(POWER_RAGE, 0);
        SetPower(POWER_ENERGY, uint32(GetMaxPower(POWER_ENERGY)*restore_percent));
    }

    // trigger update zone for alive state zone updates
    uint32 newzone, newarea;
    GetZoneAndAreaId(newzone, newarea);
    UpdateZone(newzone, newarea);
    sOutdoorPvPMgr->HandlePlayerResurrects(this, newzone);

    if (InBattleground())
    {
        if (Battleground* bg = GetBattleground())
            bg->HandlePlayerResurrect(this);
    }

    // update visibility
    UpdateObjectVisibility();

    if (!applySickness)
        return;

    //Characters from level 1-10 are not affected by resurrection sickness.
    //Characters from level 11-19 will suffer from one minute of sickness
    //for each level they are above 10.
    //Characters level 20 and up suffer from ten minutes of sickness.
    int32 startLevel = sWorld->getIntConfig(CONFIG_DEATH_SICKNESS_LEVEL);

    if (int32(getLevel()) >= startLevel)
    {
        // set resurrection sickness
        CastSpell(this, 15007, true);

        // not full duration
        if (int32(getLevel()) < startLevel+9)
        {
            int32 delta = (int32(getLevel()) - startLevel + 1)*MINUTE;

            if (Aura* aur = GetAura(15007, GetGUID()))
            {
                aur->SetDuration(delta*IN_MILLISECONDS);
            }
        }
    }
}

void Player::KillPlayer()
{
    if (IsFlying() && !GetTransport())
        i_motionMaster.MoveFall();

    SetMovement(MOVE_ROOT);

    StopMirrorTimers();                                     //disable timers(bars)

    setDeathState(CORPSE);
    //SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NOT_IN_PVP);

    SetUInt32Value(UNIT_DYNAMIC_FLAGS, UNIT_DYNFLAG_NONE);
    ApplyModFlag(PLAYER_FIELD_BYTES, PLAYER_FIELD_BYTE_RELEASE_TIMER, !sMapStore.LookupEntry(GetMapId())->Instanceable() && !HasAuraType(SPELL_AURA_PREVENT_RESURRECTION));

    // 6 minutes until repop at graveyard
    m_deathTimer = 6 * MINUTE * IN_MILLISECONDS;

    UpdateCorpseReclaimDelay();                             // dependent at use SetDeathPvP() call before kill
    SendCorpseReclaimDelay();

    // don't create corpse at this moment, player might be falling

    // update visibility
    UpdateObjectVisibility();
}

void Player::CreateCorpse()
{
    // prevent existence 2 corpse for player
    SpawnCorpseBones();

    uint32 _uf, _pb, _pb2, _cfb1, _cfb2;

    Corpse* corpse = new Corpse((m_ExtraFlags & PLAYER_EXTRA_PVP_DEATH) ? CORPSE_RESURRECTABLE_PVP : CORPSE_RESURRECTABLE_PVE);
    SetPvPDeath(false);

    if (!corpse->Create(sObjectMgr->GenerateLowGuid(HIGHGUID_CORPSE), this))
    {
        delete corpse;
        return;
    }

    _uf = GetUInt32Value(UNIT_FIELD_BYTES_0);
    _pb = GetUInt32Value(PLAYER_BYTES);
    _pb2 = GetUInt32Value(PLAYER_BYTES_2);

    uint8 race       = (uint8)(_uf);
    uint8 skin       = (uint8)(_pb);
    uint8 face       = (uint8)(_pb >> 8);
    uint8 hairstyle  = (uint8)(_pb >> 16);
    uint8 haircolor  = (uint8)(_pb >> 24);
    uint8 facialhair = (uint8)(_pb2);

    _cfb1 = ((0x00) | (race << 8) | (getGender() << 16) | (skin << 24));
    _cfb2 = ((face) | (hairstyle << 8) | (haircolor << 16) | (facialhair << 24));

    corpse->SetUInt32Value(CORPSE_FIELD_BYTES_1, _cfb1);
    corpse->SetUInt32Value(CORPSE_FIELD_BYTES_2, _cfb2);

    uint32 flags = CORPSE_FLAG_UNK2;
    if (HasFlag(PLAYER_FLAGS, PLAYER_FLAGS_HIDE_HELM))
        flags |= CORPSE_FLAG_HIDE_HELM;
    if (HasFlag(PLAYER_FLAGS, PLAYER_FLAGS_HIDE_CLOAK))
        flags |= CORPSE_FLAG_HIDE_CLOAK;
    if (InBattleground() && !InArena())
        flags |= CORPSE_FLAG_LOOTABLE;                      // to be able to remove insignia
    corpse->SetUInt32Value(CORPSE_FIELD_FLAGS, flags);

    corpse->SetUInt32Value(CORPSE_FIELD_DISPLAY_ID, GetNativeDisplayId());

    corpse->SetUInt32Value(CORPSE_FIELD_GUILD, GetGuildId());

    uint32 iDisplayID;
    uint32 iIventoryType;
    uint32 _cfi;
    for (uint8 i = 0; i < EQUIPMENT_SLOT_END; i++)
    {
        if (m_items[i])
        {
            iDisplayID = m_items[i]->GetTemplate()->DisplayInfoID;
            iIventoryType = m_items[i]->GetTemplate()->InventoryType;

            _cfi = iDisplayID | (iIventoryType << 24);
            corpse->SetUInt32Value(CORPSE_FIELD_ITEM + i, _cfi);
        }
    }

    // we do not need to save corpses for BG/arenas
    if (!GetMap()->IsBattlegroundOrArena())
        corpse->SaveToDB();

    // register for player, but not show
    sObjectAccessor->AddCorpse(corpse);
}

void Player::SpawnCorpseBones()
{
    if (sObjectAccessor->ConvertCorpseForPlayer(GetGUID()))
        if (!GetSession()->PlayerLogoutWithSave())          // at logout we will already store the player
            SaveToDB();                                     // prevent loading as ghost without corpse
}

Corpse* Player::GetCorpse() const
{
    return sObjectAccessor->GetCorpseForPlayerGUID(GetGUID());
}

void Player::DurabilityLossAll(double percent, bool inventory)
{
    for (uint8 i = EQUIPMENT_SLOT_START; i < EQUIPMENT_SLOT_END; i++)
        if (Item* pItem = GetItemByPos(INVENTORY_SLOT_BAG_0, i))
            DurabilityLoss(pItem, percent);

    if (inventory)
    {
        // bags not have durability
        // for (int i = INVENTORY_SLOT_BAG_START; i < INVENTORY_SLOT_BAG_END; i++)

        for (uint8 i = INVENTORY_SLOT_ITEM_START; i < INVENTORY_SLOT_ITEM_END; i++)
            if (Item* pItem = GetItemByPos(INVENTORY_SLOT_BAG_0, i))
                DurabilityLoss(pItem, percent);

        // keys not have durability
        //for (int i = KEYRING_SLOT_START; i < KEYRING_SLOT_END; i++)

        for (uint8 i = INVENTORY_SLOT_BAG_START; i < INVENTORY_SLOT_BAG_END; i++)
            if (Bag* pBag = GetBagByPos(i))
                for (uint32 j = 0; j < pBag->GetBagSize(); j++)
                    if (Item* pItem = GetItemByPos(i, j))
                        DurabilityLoss(pItem, percent);
    }
}

void Player::DurabilityLoss(Item* item, double percent)
{
    if (!item)
        return;

    uint32 pMaxDurability = item ->GetUInt32Value(ITEM_FIELD_MAXDURABILITY);

    if (!pMaxDurability)
        return;

    uint32 pDurabilityLoss = uint32(pMaxDurability*percent);

    if (pDurabilityLoss < 1)
        pDurabilityLoss = 1;

    DurabilityPointsLoss(item, pDurabilityLoss);
}

void Player::DurabilityPointsLossAll(int32 points, bool inventory)
{
    for (uint8 i = EQUIPMENT_SLOT_START; i < EQUIPMENT_SLOT_END; i++)
        if (Item* pItem = GetItemByPos(INVENTORY_SLOT_BAG_0, i))
            DurabilityPointsLoss(pItem, points);

    if (inventory)
    {
        // bags not have durability
        // for (int i = INVENTORY_SLOT_BAG_START; i < INVENTORY_SLOT_BAG_END; i++)

        for (uint8 i = INVENTORY_SLOT_ITEM_START; i < INVENTORY_SLOT_ITEM_END; i++)
            if (Item* pItem = GetItemByPos(INVENTORY_SLOT_BAG_0, i))
                DurabilityPointsLoss(pItem, points);

        // keys not have durability
        //for (int i = KEYRING_SLOT_START; i < KEYRING_SLOT_END; i++)

        for (uint8 i = INVENTORY_SLOT_BAG_START; i < INVENTORY_SLOT_BAG_END; i++)
            if (Bag* pBag = (Bag*)GetItemByPos(INVENTORY_SLOT_BAG_0, i))
                for (uint32 j = 0; j < pBag->GetBagSize(); j++)
                    if (Item* pItem = GetItemByPos(i, j))
                        DurabilityPointsLoss(pItem, points);
    }
}

void Player::DurabilityPointsLoss(Item* item, int32 points)
{
    int32 pMaxDurability = item->GetUInt32Value(ITEM_FIELD_MAXDURABILITY);
    int32 pOldDurability = item->GetUInt32Value(ITEM_FIELD_DURABILITY);
    int32 pNewDurability = pOldDurability - points;

    if (pNewDurability < 0)
        pNewDurability = 0;
    else if (pNewDurability > pMaxDurability)
        pNewDurability = pMaxDurability;

    if (pOldDurability != pNewDurability)
    {
        // modify item stats _before_ Durability set to 0 to pass _ApplyItemMods internal check
        if (pNewDurability == 0 && pOldDurability > 0 && item->IsEquipped())
            _ApplyItemMods(item, item->GetSlot(), false);

        item->SetUInt32Value(ITEM_FIELD_DURABILITY, pNewDurability);

        // modify item stats _after_ restore durability to pass _ApplyItemMods internal check
        if (pNewDurability > 0 && pOldDurability == 0 && item->IsEquipped())
            _ApplyItemMods(item, item->GetSlot(), true);

        item->SetState(ITEM_CHANGED, this);
    }
}

void Player::DurabilityPointLossForEquipSlot(EquipmentSlots slot)
{
    if (Item* pItem = GetItemByPos(INVENTORY_SLOT_BAG_0, slot))
        DurabilityPointsLoss(pItem, 1);
}

uint32 Player::DurabilityRepairAll(bool cost, float discountMod, bool guildBank)
{
    uint32 TotalCost = 0;
    // equipped, backpack, bags itself
    for (uint8 i = EQUIPMENT_SLOT_START; i < INVENTORY_SLOT_ITEM_END; i++)
        TotalCost += DurabilityRepair(((INVENTORY_SLOT_BAG_0 << 8) | i), cost, discountMod, guildBank);

    // bank, buyback and keys not repaired

    // items in inventory bags
    for (uint8 j = INVENTORY_SLOT_BAG_START; j < INVENTORY_SLOT_BAG_END; j++)
        for (uint8 i = 0; i < MAX_BAG_SIZE; i++)
            TotalCost += DurabilityRepair(((j << 8) | i), cost, discountMod, guildBank);
    return TotalCost;
}

uint32 Player::DurabilityRepair(uint16 pos, bool cost, float discountMod, bool guildBank)
{
    Item* item = GetItemByPos(pos);

    uint32 TotalCost = 0;
    if (!item)
        return TotalCost;

    uint32 maxDurability = item->GetUInt32Value(ITEM_FIELD_MAXDURABILITY);
    if (!maxDurability)
        return TotalCost;

    uint32 curDurability = item->GetUInt32Value(ITEM_FIELD_DURABILITY);

    if (cost)
    {
        uint32 LostDurability = maxDurability - curDurability;
        if (LostDurability>0)
        {
            ItemTemplate const* ditemProto = item->GetTemplate();

            DurabilityCostsEntry const* dcost = sDurabilityCostsStore.LookupEntry(ditemProto->ItemLevel);
            if (!dcost)
            {
                sLog->outError(LOG_FILTER_PLAYER_ITEMS, "RepairDurability: Wrong item lvl %u", ditemProto->ItemLevel);
                return TotalCost;
            }

            uint32 dQualitymodEntryId = (ditemProto->Quality+1)*2;
            DurabilityQualityEntry const* dQualitymodEntry = sDurabilityQualityStore.LookupEntry(dQualitymodEntryId);
            if (!dQualitymodEntry)
            {
                sLog->outError(LOG_FILTER_PLAYER_ITEMS, "RepairDurability: Wrong dQualityModEntry %u", dQualitymodEntryId);
                return TotalCost;
            }

            uint32 dmultiplier = dcost->multiplier[ItemSubClassToDurabilityMultiplierId(ditemProto->Class, ditemProto->SubClass)];
            uint32 costs = uint32(LostDurability*dmultiplier*double(dQualitymodEntry->quality_mod));

            costs = uint32(costs * discountMod * sWorld->getRate(RATE_REPAIRCOST));

            if (costs == 0)                                   //fix for ITEM_QUALITY_ARTIFACT
                costs = 1;

            if (guildBank)
            {
                if (GetGuildId() == 0)
                {
                    sLog->outDebug(LOG_FILTER_PLAYER_ITEMS, "You are not member of a guild");
                    return TotalCost;
                }

                Guild* guild = sGuildMgr->GetGuildById(GetGuildId());
                if (!guild)
                    return TotalCost;

                if (!guild->HandleMemberWithdrawMoney(GetSession(), costs, true))
                    return TotalCost;

                TotalCost = costs;
            }
            else if (!HasEnoughMoney(costs))
            {
                sLog->outDebug(LOG_FILTER_PLAYER_ITEMS, "You do not have enough money");
                return TotalCost;
            }
            else
                ModifyMoney(-int32(costs));
        }
    }

    item->SetUInt32Value(ITEM_FIELD_DURABILITY, maxDurability);
    item->SetState(ITEM_CHANGED, this);

    // reapply mods for total broken and repaired item if equipped
    if (IsEquipmentPos(pos) && !curDurability)
        _ApplyItemMods(item, pos & 255, true);
    return TotalCost;
}

void Player::RepopAtGraveyard()
{
    // note: this can be called also when the player is alive
    // for example from WorldSession::HandleMovementOpcodes

    AreaTableEntry const* zone = GetAreaEntryByAreaID(GetAreaId());

    // Such zones are considered unreachable as a ghost and the player must be automatically revived
    if ((!isAlive() && zone && zone->flags & AREA_FLAG_NEED_FLY) || GetTransport() || GetPositionZ() < -500.0f)
    {
        ResurrectPlayer(0.5f);
        SpawnCorpseBones();
    }

    WorldSafeLocsEntry const* ClosestGrave = NULL;

    // Special handle for battleground maps
    if (Battleground* bg = GetBattleground())
        ClosestGrave = bg->GetClosestGraveYard(this);
    else
    {
        if (Battlefield* bf = sBattlefieldMgr->GetBattlefieldToZoneId(GetZoneId()))
            ClosestGrave = bf->GetClosestGraveYard(this);
        else
            ClosestGrave = sObjectMgr->GetClosestGraveYard(GetPositionX(), GetPositionY(), GetPositionZ(), GetMapId(), GetTeam());
    }

    // stop countdown until repop
    m_deathTimer = 0;

    // if no grave found, stay at the current location
    // and don't show spirit healer location
    if (ClosestGrave)
    {
        TeleportTo(ClosestGrave->map_id, ClosestGrave->x, ClosestGrave->y, ClosestGrave->z, GetOrientation());
        if (isDead())                                        // not send if alive, because it used in TeleportTo()
        {
            WorldPacket data(SMSG_DEATH_RELEASE_LOC, 4*4);  // show spirit healer position on minimap
            data << ClosestGrave->map_id;
            data << ClosestGrave->x;
            data << ClosestGrave->y;
            data << ClosestGrave->z;
            GetSession()->SendPacket(&data);
        }
    }
    else if (GetPositionZ() < -500.0f)
        TeleportTo(m_homebindMapId, m_homebindX, m_homebindY, m_homebindZ, GetOrientation());
}

bool Player::CanJoinConstantChannelInZone(ChatChannelsEntry const* channel, AreaTableEntry const* zone)
{
    if (channel->flags & CHANNEL_DBC_FLAG_ZONE_DEP && zone->flags & AREA_FLAG_ARENA_INSTANCE)
        return false;

    if ((channel->flags & CHANNEL_DBC_FLAG_CITY_ONLY) && (!(zone->flags & AREA_FLAG_SLAVE_CAPITAL)))
        return false;

    if ((channel->flags & CHANNEL_DBC_FLAG_GUILD_REQ) && GetGuildId())
        return false;

    return true;
}

void Player::JoinedChannel(Channel* c)
{
    m_channels.push_back(c);
}

void Player::LeftChannel(Channel* c)
{
    m_channels.remove(c);
}

void Player::CleanupChannels()
{
    while (!m_channels.empty())
    {
        Channel* ch = *m_channels.begin();
        m_channels.erase(m_channels.begin());               // remove from player's channel list
        ch->Leave(GetGUID(), false);                        // not send to client, not remove from player's channel list
        if (ChannelMgr* cMgr = channelMgr(GetTeam()))
            cMgr->LeftChannel(ch->GetName());               // deleted channel if empty
    }
    sLog->outDebug(LOG_FILTER_CHATSYS, "Player: channels cleaned up!");
}

void Player::UpdateLocalChannels(uint32 newZone)
{
    if (GetSession()->PlayerLoading() && !IsBeingTeleportedFar())
        return;                                              // The client handles it automatically after loading, but not after teleporting

    AreaTableEntry const* current_zone = GetAreaEntryByAreaID(newZone);
    if (!current_zone)
        return;

    ChannelMgr* cMgr = channelMgr(GetTeam());
    if (!cMgr)
        return;

    std::string current_zone_name = current_zone->area_name[GetSession()->GetSessionDbcLocale()];

    for (uint32 i = 0; i < sChatChannelsStore.GetNumRows(); ++i)
    {
        if (ChatChannelsEntry const* channel = sChatChannelsStore.LookupEntry(i))
        {
            Channel* usedChannel = NULL;

            for (JoinedChannelsList::iterator itr = m_channels.begin(); itr != m_channels.end(); ++itr)
            {
                if ((*itr)->GetChannelId() == i)
                {
                    usedChannel = *itr;
                    break;
                }
            }

            Channel* removeChannel = NULL;
            Channel* joinChannel = NULL;
            bool sendRemove = true;

            if (CanJoinConstantChannelInZone(channel, current_zone))
            {
                if (!(channel->flags & CHANNEL_DBC_FLAG_GLOBAL))
                {
                    if (channel->flags & CHANNEL_DBC_FLAG_CITY_ONLY && usedChannel)
                        continue;                            // Already on the channel, as city channel names are not changing

                    char new_channel_name_buf[100];
                    char const* currentNameExt;

                    if (channel->flags & CHANNEL_DBC_FLAG_CITY_ONLY)
                        currentNameExt = sObjectMgr->GetTrinityStringForDBCLocale(LANG_CHANNEL_CITY);
                    else
                        currentNameExt = current_zone_name.c_str();

                    snprintf(new_channel_name_buf, 100, channel->pattern[m_session->GetSessionDbcLocale()], currentNameExt);

                    joinChannel = cMgr->GetJoinChannel(new_channel_name_buf, channel->ChannelID);
                    if (usedChannel)
                    {
                        if (joinChannel != usedChannel)
                        {
                            removeChannel = usedChannel;
                            sendRemove = false;              // Do not send leave channel, it already replaced at client
                        }
                        else
                            joinChannel = NULL;
                    }
                }
                else
                    joinChannel = cMgr->GetJoinChannel(channel->pattern[m_session->GetSessionDbcLocale()], channel->ChannelID);
            }
            else
                removeChannel = usedChannel;

            if (joinChannel)
                joinChannel->Join(GetGUID(), "");            // Changed Channel: ... or Joined Channel: ...

            if (removeChannel)
            {
                removeChannel->Leave(GetGUID(), sendRemove); // Leave old channel
                std::string name = removeChannel->GetName(); // Store name, (*i)erase in LeftChannel
                LeftChannel(removeChannel);                  // Remove from player's channel list
                cMgr->LeftChannel(name);                     // Delete if empty
            }
        }
    }
}

void Player::LeaveLFGChannel()
{
    for (JoinedChannelsList::iterator i = m_channels.begin(); i != m_channels.end(); ++i)
    {
        if ((*i)->IsLFG())
        {
            (*i)->Leave(GetGUID());
            break;
        }
    }
}

void Player::UpdateDefense()
{
    uint32 defense_skill_gain = sWorld->getIntConfig(CONFIG_SKILL_GAIN_DEFENSE);

    if (UpdateSkill(SKILL_DEFENSE, defense_skill_gain))
    {
        // update dependent from defense skill part
        UpdateDefenseBonusesMod();
    }
}

void Player::HandleBaseModValue(BaseModGroup modGroup, BaseModType modType, float amount, bool apply)
{
    if (modGroup >= BASEMOD_END || modType >= MOD_END)
    {
        sLog->outError(LOG_FILTER_SPELLS_AURAS, "ERROR in HandleBaseModValue(): non existed BaseModGroup of wrong BaseModType!");
        return;
    }

    switch (modType)
    {
        case FLAT_MOD:
            m_auraBaseMod[modGroup][modType] += apply ? amount : -amount;
            break;
        case PCT_MOD:
            ApplyPercentModFloatVar(m_auraBaseMod[modGroup][modType], amount, apply);
            break;
    }

    if (!CanModifyStats())
        return;

    switch (modGroup)
    {
        case CRIT_PERCENTAGE:              UpdateCritPercentage(BASE_ATTACK);                          break;
        case RANGED_CRIT_PERCENTAGE:       UpdateCritPercentage(RANGED_ATTACK);                        break;
        case OFFHAND_CRIT_PERCENTAGE:      UpdateCritPercentage(OFF_ATTACK);                           break;
        case SHIELD_BLOCK_VALUE:           UpdateShieldBlockValue();                                   break;
        default: break;
    }
}

float Player::GetBaseModValue(BaseModGroup modGroup, BaseModType modType) const
{
    if (modGroup >= BASEMOD_END || modType > MOD_END)
    {
        sLog->outError(LOG_FILTER_SPELLS_AURAS, "trial to access non existed BaseModGroup or wrong BaseModType!");
        return 0.0f;
    }

    if (modType == PCT_MOD && m_auraBaseMod[modGroup][PCT_MOD] <= 0.0f)
        return 0.0f;

    return m_auraBaseMod[modGroup][modType];
}

float Player::GetTotalBaseModValue(BaseModGroup modGroup) const
{
    if (modGroup >= BASEMOD_END)
    {
        sLog->outError(LOG_FILTER_SPELLS_AURAS, "wrong BaseModGroup in GetTotalBaseModValue()!");
        return 0.0f;
    }

    if (m_auraBaseMod[modGroup][PCT_MOD] <= 0.0f)
        return 0.0f;

    return m_auraBaseMod[modGroup][FLAT_MOD] * m_auraBaseMod[modGroup][PCT_MOD];
}

uint32 Player::GetShieldBlockValue() const
{
    float value = (m_auraBaseMod[SHIELD_BLOCK_VALUE][FLAT_MOD] + GetStat(STAT_STRENGTH) * 0.5f - 10)*m_auraBaseMod[SHIELD_BLOCK_VALUE][PCT_MOD];

    value = (value < 0) ? 0 : value;

    return uint32(value);
}

float Player::GetMeleeCritFromAgility()
{
    uint8 level = getLevel();
    uint32 pclass = getClass();

    if (level > GT_MAX_LEVEL)
        level = GT_MAX_LEVEL;

    GtChanceToMeleeCritBaseEntry const* critBase  = sGtChanceToMeleeCritBaseStore.LookupEntry(pclass-1);
    GtChanceToMeleeCritEntry     const* critRatio = sGtChanceToMeleeCritStore.LookupEntry((pclass-1)*GT_MAX_LEVEL + level-1);
    if (critBase == NULL || critRatio == NULL)
        return 0.0f;

    float crit = critBase->base + GetStat(STAT_AGILITY)*critRatio->ratio;
    return crit*100.0f;
}

void Player::GetDodgeFromAgility(float &diminishing, float &nondiminishing)
{
    // Table for base dodge values
    const float dodge_base[MAX_CLASSES] =
    {
         0.036640f, // Warrior
         0.034943f, // Paladi
        -0.040873f, // Hunter
         0.020957f, // Rogue
         0.034178f, // Priest
         0.036640f, // DK
         0.021080f, // Shaman
         0.036587f, // Mage
         0.024211f, // Warlock
         0.0f,      // ??
         0.056097f  // Druid
    };
    // Crit/agility to dodge/agility coefficient multipliers; 3.2.0 increased required agility by 15%
    const float crit_to_dodge[MAX_CLASSES] =
    {
         0.85f/1.15f,    // Warrior
         1.00f/1.15f,    // Paladin
         1.11f/1.15f,    // Hunter
         2.00f/1.15f,    // Rogue
         1.00f/1.15f,    // Priest
         0.85f/1.15f,    // DK
         1.60f/1.15f,    // Shaman
         1.00f/1.15f,    // Mage
         0.97f/1.15f,    // Warlock (?)
         0.0f,           // ??
         2.00f/1.15f     // Druid
    };

    uint8 level = getLevel();
    uint32 pclass = getClass();

    if (level > GT_MAX_LEVEL)
        level = GT_MAX_LEVEL;

    // Dodge per agility is proportional to crit per agility, which is available from DBC files
    GtChanceToMeleeCritEntry  const* dodgeRatio = sGtChanceToMeleeCritStore.LookupEntry((pclass-1)*GT_MAX_LEVEL + level-1);
    if (dodgeRatio == NULL || pclass > MAX_CLASSES)
        return;

    // TODO: research if talents/effects that increase total agility by x% should increase non-diminishing part
    float base_agility = GetCreateStat(STAT_AGILITY) * m_auraModifiersGroup[UNIT_MOD_STAT_START + STAT_AGILITY][BASE_PCT];
    float bonus_agility = GetStat(STAT_AGILITY) - base_agility;

    // calculate diminishing (green in char screen) and non-diminishing (white) contribution
    diminishing = 100.0f * bonus_agility * dodgeRatio->ratio * crit_to_dodge[pclass-1];
    nondiminishing = 100.0f * (dodge_base[pclass-1] + base_agility * dodgeRatio->ratio * crit_to_dodge[pclass-1]);
}

float Player::GetSpellCritFromIntellect()
{
    uint8 level = getLevel();
    uint32 pclass = getClass();

    if (level > GT_MAX_LEVEL)
        level = GT_MAX_LEVEL;

    GtChanceToSpellCritBaseEntry const* critBase  = sGtChanceToSpellCritBaseStore.LookupEntry(pclass-1);
    GtChanceToSpellCritEntry     const* critRatio = sGtChanceToSpellCritStore.LookupEntry((pclass-1)*GT_MAX_LEVEL + level-1);
    if (critBase == NULL || critRatio == NULL)
        return 0.0f;

    float crit=critBase->base + GetStat(STAT_INTELLECT)*critRatio->ratio;
    return crit*100.0f;
}

float Player::GetRatingMultiplier(CombatRating cr) const
{
    uint8 level = getLevel();

    if (level > GT_MAX_LEVEL)
        level = GT_MAX_LEVEL;

    GtCombatRatingsEntry const* Rating = sGtCombatRatingsStore.LookupEntry(cr*GT_MAX_LEVEL+level-1);
    // gtOCTClassCombatRatingScalarStore.dbc starts with 1, CombatRating with zero, so cr+1
    GtOCTClassCombatRatingScalarEntry const* classRating = sGtOCTClassCombatRatingScalarStore.LookupEntry((getClass()-1)*GT_MAX_RATING+cr+1);
    if (!Rating || !classRating)
        return 1.0f;                                        // By default use minimum coefficient (not must be called)

    return classRating->ratio / Rating->ratio;
}

float Player::GetRatingBonusValue(CombatRating cr) const
{
    return float(GetUInt32Value(PLAYER_FIELD_COMBAT_RATING_1 + cr)) * GetRatingMultiplier(cr);
}

float Player::GetExpertiseDodgeOrParryReduction(WeaponAttackType attType) const
{
    switch (attType)
    {
        case BASE_ATTACK:
            return GetUInt32Value(PLAYER_EXPERTISE) / 4.0f;
        case OFF_ATTACK:
            return GetUInt32Value(PLAYER_OFFHAND_EXPERTISE) / 4.0f;
        default:
            break;
    }
    return 0.0f;
}

float Player::OCTRegenHPPerSpirit()
{
    uint8 level = getLevel();
    uint32 pclass = getClass();

    if (level > GT_MAX_LEVEL)
        level = GT_MAX_LEVEL;

    GtOCTRegenHPEntry     const* baseRatio = sGtOCTRegenHPStore.LookupEntry((pclass-1)*GT_MAX_LEVEL + level-1);
    GtRegenHPPerSptEntry  const* moreRatio = sGtRegenHPPerSptStore.LookupEntry((pclass-1)*GT_MAX_LEVEL + level-1);
    if (baseRatio == NULL || moreRatio == NULL)
        return 0.0f;

    // Formula from PaperDollFrame script
    float spirit = GetStat(STAT_SPIRIT);
    float baseSpirit = spirit;
    if (baseSpirit > 50)
        baseSpirit = 50;
    float moreSpirit = spirit - baseSpirit;
    float regen = baseSpirit * baseRatio->ratio + moreSpirit * moreRatio->ratio;
    return regen;
}

float Player::OCTRegenMPPerSpirit()
{
    uint8 level = getLevel();
    uint32 pclass = getClass();

    if (level > GT_MAX_LEVEL)
        level = GT_MAX_LEVEL;

//    GtOCTRegenMPEntry     const* baseRatio = sGtOCTRegenMPStore.LookupEntry((pclass-1)*GT_MAX_LEVEL + level-1);
    GtRegenMPPerSptEntry  const* moreRatio = sGtRegenMPPerSptStore.LookupEntry((pclass-1)*GT_MAX_LEVEL + level-1);
    if (moreRatio == NULL)
        return 0.0f;

    // Formula get from PaperDollFrame script
    float spirit    = GetStat(STAT_SPIRIT);
    float regen     = spirit * moreRatio->ratio;
    return regen;
}

void Player::ApplyRatingMod(CombatRating cr, int32 value, bool apply)
{
    m_baseRatingValue[cr]+=(apply ? value : -value);

    // explicit affected values
    switch (cr)
    {
        case CR_HASTE_MELEE:
        {
            float RatingChange = value * GetRatingMultiplier(cr);
            ApplyAttackTimePercentMod(BASE_ATTACK, RatingChange, apply);
            ApplyAttackTimePercentMod(OFF_ATTACK, RatingChange, apply);
            break;
        }
        case CR_HASTE_RANGED:
        {
            float RatingChange = value * GetRatingMultiplier(cr);
            ApplyAttackTimePercentMod(RANGED_ATTACK, RatingChange, apply);
            break;
        }
        case CR_HASTE_SPELL:
        {
            float RatingChange = value * GetRatingMultiplier(cr);
            ApplyCastTimePercentMod(RatingChange, apply);
            break;
        }
        default:
            break;
    }

    UpdateRating(cr);
}

void Player::UpdateRating(CombatRating cr)
{
    int32 amount = m_baseRatingValue[cr];
    // Apply bonus from SPELL_AURA_MOD_RATING_FROM_STAT
    // stat used stored in miscValueB for this aura
    AuraEffectList const& modRatingFromStat = GetAuraEffectsByType(SPELL_AURA_MOD_RATING_FROM_STAT);
    for (AuraEffectList::const_iterator i = modRatingFromStat.begin(); i != modRatingFromStat.end(); ++i)
        if ((*i)->GetMiscValue() & (1<<cr))
            amount += int32(CalculatePct(GetStat(Stats((*i)->GetMiscValueB())), (*i)->GetAmount()));
    if (amount < 0)
        amount = 0;
    SetUInt32Value(PLAYER_FIELD_COMBAT_RATING_1 + cr, uint32(amount));

    bool affectStats = CanModifyStats();

    switch (cr)
    {
        case CR_WEAPON_SKILL:                               // Implemented in Unit::RollMeleeOutcomeAgainst
        case CR_DEFENSE_SKILL:
            UpdateDefenseBonusesMod();
            break;
        case CR_DODGE:
            UpdateDodgePercentage();
            break;
        case CR_PARRY:
            UpdateParryPercentage();
            break;
        case CR_BLOCK:
            UpdateBlockPercentage();
            break;
        case CR_HIT_MELEE:
            UpdateMeleeHitChances();
            break;
        case CR_HIT_RANGED:
            UpdateRangedHitChances();
            break;
        case CR_HIT_SPELL:
            UpdateSpellHitChances();
            break;
        case CR_CRIT_MELEE:
            if (affectStats)
            {
                UpdateCritPercentage(BASE_ATTACK);
                UpdateCritPercentage(OFF_ATTACK);
            }
            break;
        case CR_CRIT_RANGED:
            if (affectStats)
                UpdateCritPercentage(RANGED_ATTACK);
            break;
        case CR_CRIT_SPELL:
            if (affectStats)
                UpdateAllSpellCritChances();
            break;
        case CR_HIT_TAKEN_MELEE:                            // Implemented in Unit::MeleeMissChanceCalc
        case CR_HIT_TAKEN_RANGED:
            break;
        case CR_HIT_TAKEN_SPELL:                            // Implemented in Unit::MagicSpellHitResult
            break;
        case CR_CRIT_TAKEN_MELEE:                           // Implemented in Unit::RollMeleeOutcomeAgainst (only for chance to crit)
        case CR_CRIT_TAKEN_RANGED:
            break;
        case CR_CRIT_TAKEN_SPELL:                           // Implemented in Unit::SpellCriticalBonus (only for chance to crit)
            break;
        case CR_HASTE_MELEE:                                // Implemented in Player::ApplyRatingMod
        case CR_HASTE_RANGED:
        case CR_HASTE_SPELL:
            break;
        case CR_WEAPON_SKILL_MAINHAND:                      // Implemented in Unit::RollMeleeOutcomeAgainst
        case CR_WEAPON_SKILL_OFFHAND:
        case CR_WEAPON_SKILL_RANGED:
            break;
        case CR_EXPERTISE:
            if (affectStats)
            {
                UpdateExpertise(BASE_ATTACK);
                UpdateExpertise(OFF_ATTACK);
            }
            break;
        case CR_ARMOR_PENETRATION:
            if (affectStats)
                UpdateArmorPenetration(amount);
            break;
    }
}

void Player::UpdateAllRatings()
{
    for (int cr = 0; cr < MAX_COMBAT_RATING; ++cr)
        UpdateRating(CombatRating(cr));
}

void Player::SetRegularAttackTime()
{
    for (uint8 i = 0; i < MAX_ATTACK; ++i)
    {
        Item* tmpitem = GetWeaponForAttack(WeaponAttackType(i), true);
        if (tmpitem && !tmpitem->IsBroken())
        {
            ItemTemplate const* proto = tmpitem->GetTemplate();
            if (proto->Delay)
                SetAttackTime(WeaponAttackType(i), proto->Delay);
        }
        else
            SetAttackTime(WeaponAttackType(i), BASE_ATTACK_TIME);  // If there is no weapon reset attack time to base (might have been changed from forms)
    }
}

//skill+step, checking for max value
bool Player::UpdateSkill(uint32 skill_id, uint32 step)
{
    if (!skill_id)
        return false;

    if (skill_id == SKILL_FIST_WEAPONS)
        skill_id = SKILL_UNARMED;

    SkillStatusMap::iterator itr = mSkillStatus.find(skill_id);
    if (itr == mSkillStatus.end() || itr->second.uState == SKILL_DELETED)
        return false;

    uint32 valueIndex = PLAYER_SKILL_VALUE_INDEX(itr->second.pos);
    uint32 data = GetUInt32Value(valueIndex);
    uint32 value = SKILL_VALUE(data);
    uint32 max = SKILL_MAX(data);

    if ((!max) || (!value) || (value >= max))
        return false;

    if (value < max)
    {
        uint32 new_value = value+step;
        if (new_value > max)
            new_value = max;

        SetUInt32Value(valueIndex, MAKE_SKILL_VALUE(new_value, max));
        if (itr->second.uState != SKILL_NEW)
            itr->second.uState = SKILL_CHANGED;
        UpdateSkillEnchantments(skill_id, value, new_value);
        UpdateAchievementCriteria(ACHIEVEMENT_CRITERIA_TYPE_REACH_SKILL_LEVEL, skill_id);
        return true;
    }

    return false;
}

inline int SkillGainChance(uint32 SkillValue, uint32 GrayLevel, uint32 GreenLevel, uint32 YellowLevel)
{
    if (SkillValue >= GrayLevel)
        return sWorld->getIntConfig(CONFIG_SKILL_CHANCE_GREY)*10;
    if (SkillValue >= GreenLevel)
        return sWorld->getIntConfig(CONFIG_SKILL_CHANCE_GREEN)*10;
    if (SkillValue >= YellowLevel)
        return sWorld->getIntConfig(CONFIG_SKILL_CHANCE_YELLOW)*10;
    return sWorld->getIntConfig(CONFIG_SKILL_CHANCE_ORANGE)*10;
}

bool Player::UpdateCraftSkill(uint32 spellid)
{
    sLog->outDebug(LOG_FILTER_PLAYER_SKILLS, "UpdateCraftSkill spellid %d", spellid);

    SkillLineAbilityMapBounds bounds = sSpellMgr->GetSkillLineAbilityMapBounds(spellid);

    for (SkillLineAbilityMap::const_iterator _spell_idx = bounds.first; _spell_idx != bounds.second; ++_spell_idx)
    {
        if (_spell_idx->second->skillId)
        {
            uint32 SkillValue = GetPureSkillValue(_spell_idx->second->skillId);

            // Alchemy Discoveries here
            SpellInfo const* spellEntry = sSpellMgr->GetSpellInfo(spellid);
            if (spellEntry && spellEntry->Mechanic == MECHANIC_DISCOVERY)
            {
                if (uint32 discoveredSpell = GetSkillDiscoverySpell(_spell_idx->second->skillId, spellid, this))
                    learnSpell(discoveredSpell, false);
            }

            uint32 craft_skill_gain = sWorld->getIntConfig(CONFIG_SKILL_GAIN_CRAFTING);

            return UpdateSkillPro(_spell_idx->second->skillId, SkillGainChance(SkillValue,
                _spell_idx->second->max_value,
                (_spell_idx->second->max_value + _spell_idx->second->min_value)/2,
                _spell_idx->second->min_value),
                craft_skill_gain);
        }
    }
    return false;
}

bool Player::UpdateGatherSkill(uint32 SkillId, uint32 SkillValue, uint32 RedLevel, uint32 Multiplicator)
{
    sLog->outDebug(LOG_FILTER_PLAYER_SKILLS, "UpdateGatherSkill(SkillId %d SkillLevel %d RedLevel %d)", SkillId, SkillValue, RedLevel);

    uint32 gathering_skill_gain = sWorld->getIntConfig(CONFIG_SKILL_GAIN_GATHERING);

    // For skinning and Mining chance decrease with level. 1-74 - no decrease, 75-149 - 2 times, 225-299 - 8 times
    switch (SkillId)
    {
        case SKILL_HERBALISM:
        case SKILL_LOCKPICKING:
        case SKILL_JEWELCRAFTING:
        case SKILL_INSCRIPTION:
            return UpdateSkillPro(SkillId, SkillGainChance(SkillValue, RedLevel+100, RedLevel+50, RedLevel+25)*Multiplicator, gathering_skill_gain);
        case SKILL_SKINNING:
            if (sWorld->getIntConfig(CONFIG_SKILL_CHANCE_SKINNING_STEPS) == 0)
                return UpdateSkillPro(SkillId, SkillGainChance(SkillValue, RedLevel+100, RedLevel+50, RedLevel+25)*Multiplicator, gathering_skill_gain);
            else
                return UpdateSkillPro(SkillId, (SkillGainChance(SkillValue, RedLevel+100, RedLevel+50, RedLevel+25)*Multiplicator) >> (SkillValue/sWorld->getIntConfig(CONFIG_SKILL_CHANCE_SKINNING_STEPS)), gathering_skill_gain);
        case SKILL_MINING:
            if (sWorld->getIntConfig(CONFIG_SKILL_CHANCE_MINING_STEPS) == 0)
                return UpdateSkillPro(SkillId, SkillGainChance(SkillValue, RedLevel+100, RedLevel+50, RedLevel+25)*Multiplicator, gathering_skill_gain);
            else
                return UpdateSkillPro(SkillId, (SkillGainChance(SkillValue, RedLevel+100, RedLevel+50, RedLevel+25)*Multiplicator) >> (SkillValue/sWorld->getIntConfig(CONFIG_SKILL_CHANCE_MINING_STEPS)), gathering_skill_gain);
    }
    return false;
}

bool Player::UpdateFishingSkill()
{
    sLog->outDebug(LOG_FILTER_PLAYER_SKILLS, "UpdateFishingSkill");

    uint32 SkillValue = GetPureSkillValue(SKILL_FISHING);

    int32 chance = SkillValue < 75 ? 100 : 2500/(SkillValue-50);

    uint32 gathering_skill_gain = sWorld->getIntConfig(CONFIG_SKILL_GAIN_GATHERING);

    return UpdateSkillPro(SKILL_FISHING, chance*10, gathering_skill_gain);
}

// levels sync. with spell requirement for skill levels to learn
// bonus abilities in sSkillLineAbilityStore
// Used only to avoid scan DBC at each skill grow
static uint32 bonusSkillLevels[] = {75, 150, 225, 300, 375, 450};
static const size_t bonusSkillLevelsSize = sizeof(bonusSkillLevels) / sizeof(uint32);

bool Player::UpdateSkillPro(uint16 SkillId, int32 Chance, uint32 step)
{
    sLog->outDebug(LOG_FILTER_PLAYER_SKILLS, "UpdateSkillPro(SkillId %d, Chance %3.1f%%)", SkillId, Chance / 10.0f);
    if (!SkillId)
        return false;

    if (Chance <= 0)                                         // speedup in 0 chance case
    {
        sLog->outDebug(LOG_FILTER_PLAYER_SKILLS, "Player::UpdateSkillPro Chance=%3.1f%% missed", Chance / 10.0f);
        return false;
    }

    SkillStatusMap::iterator itr = mSkillStatus.find(SkillId);
    if (itr == mSkillStatus.end() || itr->second.uState == SKILL_DELETED)
        return false;

    uint32 valueIndex = PLAYER_SKILL_VALUE_INDEX(itr->second.pos);

    uint32 data = GetUInt32Value(valueIndex);
    uint16 SkillValue = SKILL_VALUE(data);
    uint16 MaxValue   = SKILL_MAX(data);

    if (!MaxValue || !SkillValue || SkillValue >= MaxValue)
        return false;

    int32 Roll = irand(1, 1000);

    if (Roll <= Chance)
    {
        uint32 new_value = SkillValue+step;
        if (new_value > MaxValue)
            new_value = MaxValue;

        SetUInt32Value(valueIndex, MAKE_SKILL_VALUE(new_value, MaxValue));
        if (itr->second.uState != SKILL_NEW)
            itr->second.uState = SKILL_CHANGED;
        for (size_t i = 0; i < bonusSkillLevelsSize; ++i)
        {
            uint32 bsl = bonusSkillLevels[i];
            if (SkillValue < bsl && new_value >= bsl)
            {
                learnSkillRewardedSpells(SkillId, new_value);
                break;
            }
        }
        UpdateSkillEnchantments(SkillId, SkillValue, new_value);
        UpdateAchievementCriteria(ACHIEVEMENT_CRITERIA_TYPE_REACH_SKILL_LEVEL, SkillId);
        sLog->outDebug(LOG_FILTER_PLAYER_SKILLS, "Player::UpdateSkillPro Chance=%3.1f%% taken", Chance / 10.0f);
        return true;
    }

    sLog->outDebug(LOG_FILTER_PLAYER_SKILLS, "Player::UpdateSkillPro Chance=%3.1f%% missed", Chance / 10.0f);
    return false;
}

void Player::UpdateWeaponSkill(WeaponAttackType attType)
{
    // no skill gain in pvp
    Unit* victim = getVictim();
    if (victim && victim->GetTypeId() == TYPEID_PLAYER)
        return;

    if (IsInFeralForm())
        return;                                             // always maximized SKILL_FERAL_COMBAT in fact

    if (GetShapeshiftForm() == FORM_TREE)
        return;                                             // use weapon but not skill up

    if (victim && victim->GetTypeId() == TYPEID_UNIT && (victim->ToCreature()->GetCreatureTemplate()->flags_extra & CREATURE_FLAG_EXTRA_NO_SKILLGAIN))
        return;

    uint32 weapon_skill_gain = sWorld->getIntConfig(CONFIG_SKILL_GAIN_WEAPON);

    Item* tmpitem = GetWeaponForAttack(attType, true);
    if (!tmpitem && attType == BASE_ATTACK)
        UpdateSkill(SKILL_UNARMED, weapon_skill_gain);
    else if (tmpitem && tmpitem->GetTemplate()->SubClass != ITEM_SUBCLASS_WEAPON_FISHING_POLE)
        UpdateSkill(tmpitem->GetSkill(), weapon_skill_gain);

    UpdateAllCritPercentages();
}

void Player::UpdateCombatSkills(Unit* victim, WeaponAttackType attType, bool defence)
{
    uint8 plevel = getLevel();                              // if defense than victim == attacker
    uint8 greylevel = Trinity::XP::GetGrayLevel(plevel);
    uint8 moblevel = victim->getLevelForTarget(this);
    if (moblevel < greylevel)
        return;

    if (moblevel > plevel + 5)
        moblevel = plevel + 5;

    uint8 lvldif = moblevel - greylevel;
    if (lvldif < 3)
        lvldif = 3;

    uint32 skilldif = 5 * plevel - (defence ? GetBaseDefenseSkillValue() : GetBaseWeaponSkillValue(attType));
    if (skilldif <= 0)
        return;

    float chance = float(3 * lvldif * skilldif) / plevel;
    if (!defence)
        if (getClass() == CLASS_WARRIOR || getClass() == CLASS_ROGUE)
            chance += chance * 0.02f * GetStat(STAT_INTELLECT);

    chance = chance < 1.0f ? 1.0f : chance;                 //minimum chance to increase skill is 1%

    if (roll_chance_f(chance))
    {
        if (defence)
            UpdateDefense();
        else
            UpdateWeaponSkill(attType);
    }
    else
        return;
}

void Player::ModifySkillBonus(uint32 skillid, int32 val, bool talent)
{
    SkillStatusMap::const_iterator itr = mSkillStatus.find(skillid);
    if (itr == mSkillStatus.end() || itr->second.uState == SKILL_DELETED)
        return;

    uint32 bonusIndex = PLAYER_SKILL_BONUS_INDEX(itr->second.pos);

    uint32 bonus_val = GetUInt32Value(bonusIndex);
    int16 temp_bonus = SKILL_TEMP_BONUS(bonus_val);
    int16 perm_bonus = SKILL_PERM_BONUS(bonus_val);

    if (talent)                                          // permanent bonus stored in high part
        SetUInt32Value(bonusIndex, MAKE_SKILL_BONUS(temp_bonus, perm_bonus+val));
    else                                                // temporary/item bonus stored in low part
        SetUInt32Value(bonusIndex, MAKE_SKILL_BONUS(temp_bonus+val, perm_bonus));
}

void Player::UpdateSkillsForLevel()
{
    uint16 maxconfskill = sWorld->GetConfigMaxSkillValue();
    uint32 maxSkill = GetMaxSkillValueForLevel();

    bool alwaysMaxSkill = sWorld->getBoolConfig(CONFIG_ALWAYS_MAX_SKILL_FOR_LEVEL);

    for (SkillStatusMap::iterator itr = mSkillStatus.begin(); itr != mSkillStatus.end(); ++itr)
    {
        if (itr->second.uState == SKILL_DELETED)
            continue;

        uint32 pskill = itr->first;
        SkillLineEntry const* pSkill = sSkillLineStore.LookupEntry(pskill);
        if (!pSkill)
            continue;

        if (GetSkillRangeType(pSkill, false) != SKILL_RANGE_LEVEL)
            continue;

        uint32 valueIndex = PLAYER_SKILL_VALUE_INDEX(itr->second.pos);
        uint32 data = GetUInt32Value(valueIndex);
        uint32 max = SKILL_MAX(data);
        uint32 val = SKILL_VALUE(data);

        /// update only level dependent max skill values
        if (max != 1)
        {
            /// maximize skill always
            if (alwaysMaxSkill)
            {
                SetUInt32Value(valueIndex, MAKE_SKILL_VALUE(maxSkill, maxSkill));
                if (itr->second.uState != SKILL_NEW)
                    itr->second.uState = SKILL_CHANGED;
            }
            else if (max != maxconfskill)                    /// update max skill value if current max skill not maximized
            {
                SetUInt32Value(valueIndex, MAKE_SKILL_VALUE(val, maxSkill));
                if (itr->second.uState != SKILL_NEW)
                    itr->second.uState = SKILL_CHANGED;
            }
        }
    }
}

void Player::UpdateSkillsToMaxSkillsForLevel()
{
    for (SkillStatusMap::iterator itr = mSkillStatus.begin(); itr != mSkillStatus.end(); ++itr)
    {
        if (itr->second.uState == SKILL_DELETED)
            continue;

        uint32 pskill = itr->first;
        if (IsProfessionOrRidingSkill(pskill))
            continue;
        uint32 valueIndex = PLAYER_SKILL_VALUE_INDEX(itr->second.pos);
        uint32 data = GetUInt32Value(valueIndex);
        uint32 max = SKILL_MAX(data);

        if (max > 1)
        {
            SetUInt32Value(valueIndex, MAKE_SKILL_VALUE(max, max));
            if (itr->second.uState != SKILL_NEW)
                itr->second.uState = SKILL_CHANGED;
        }
        if (pskill == SKILL_DEFENSE)
            UpdateDefenseBonusesMod();
    }
}

// This functions sets a skill line value (and adds if doesn't exist yet)
// To "remove" a skill line, set it's values to zero
void Player::SetSkill(uint16 id, uint16 step, uint16 newVal, uint16 maxVal)
{
    if (!id)
        return;

    uint16 currVal;
    SkillStatusMap::iterator itr = mSkillStatus.find(id);

    //has skill
    if (itr != mSkillStatus.end() && itr->second.uState != SKILL_DELETED)
    {
        currVal = SKILL_VALUE(GetUInt32Value(PLAYER_SKILL_VALUE_INDEX(itr->second.pos)));
        if (newVal)
        {
            // if skill value is going down, update enchantments before setting the new value
            if (newVal < currVal)
                UpdateSkillEnchantments(id, currVal, newVal);
            // update step
            SetUInt32Value(PLAYER_SKILL_VALUE_INDEX(itr->second.pos), MAKE_PAIR32(id, step));
            // update value
            SetUInt32Value(PLAYER_SKILL_VALUE_INDEX(itr->second.pos), MAKE_SKILL_VALUE(newVal, maxVal));
            if (itr->second.uState != SKILL_NEW)
                itr->second.uState = SKILL_CHANGED;
            learnSkillRewardedSpells(id, newVal);
            // if skill value is going up, update enchantments after setting the new value
            if (newVal > currVal)
                UpdateSkillEnchantments(id, currVal, newVal);
            UpdateAchievementCriteria(ACHIEVEMENT_CRITERIA_TYPE_REACH_SKILL_LEVEL, id);
            UpdateAchievementCriteria(ACHIEVEMENT_CRITERIA_TYPE_LEARN_SKILL_LEVEL, id);
        }
        else                                                //remove
        {
            //remove enchantments needing this skill
            UpdateSkillEnchantments(id, currVal, 0);
            // clear skill fields
            SetUInt32Value(PLAYER_SKILL_INDEX(itr->second.pos), 0);
            SetUInt32Value(PLAYER_SKILL_VALUE_INDEX(itr->second.pos), 0);
            SetUInt32Value(PLAYER_SKILL_BONUS_INDEX(itr->second.pos), 0);

            // mark as deleted or simply remove from map if not saved yet
            if (itr->second.uState != SKILL_NEW)
                itr->second.uState = SKILL_DELETED;
            else
                mSkillStatus.erase(itr);

            // remove all spells that related to this skill
            for (uint32 j = 0; j < sSkillLineAbilityStore.GetNumRows(); ++j)
                if (SkillLineAbilityEntry const* pAbility = sSkillLineAbilityStore.LookupEntry(j))
                    if (pAbility->skillId == id)
                        removeSpell(sSpellMgr->GetFirstSpellInChain(pAbility->spellId));
        }
    }
    else if (newVal)                                        //add
    {
        currVal = 0;
        for (int i=0; i < PLAYER_MAX_SKILLS; ++i)
            if (!GetUInt32Value(PLAYER_SKILL_INDEX(i)))
        {
            SkillLineEntry const* pSkill = sSkillLineStore.LookupEntry(id);
            if (!pSkill)
            {
                sLog->outError(LOG_FILTER_PLAYER_SKILLS, "Skill not found in SkillLineStore: skill #%u", id);
                return;
            }

            SetUInt32Value(PLAYER_SKILL_INDEX(i), MAKE_PAIR32(id, step));
            SetUInt32Value(PLAYER_SKILL_VALUE_INDEX(i), MAKE_SKILL_VALUE(newVal, maxVal));
            UpdateSkillEnchantments(id, currVal, newVal);
            UpdateAchievementCriteria(ACHIEVEMENT_CRITERIA_TYPE_REACH_SKILL_LEVEL, id);
            UpdateAchievementCriteria(ACHIEVEMENT_CRITERIA_TYPE_LEARN_SKILL_LEVEL, id);

            // insert new entry or update if not deleted old entry yet
            if (itr != mSkillStatus.end())
            {
                itr->second.pos = i;
                itr->second.uState = SKILL_CHANGED;
            }
            else
                mSkillStatus.insert(SkillStatusMap::value_type(id, SkillStatusData(i, SKILL_NEW)));

            // apply skill bonuses
            SetUInt32Value(PLAYER_SKILL_BONUS_INDEX(i), 0);

            // temporary bonuses
            AuraEffectList const& mModSkill = GetAuraEffectsByType(SPELL_AURA_MOD_SKILL);
            for (AuraEffectList::const_iterator j = mModSkill.begin(); j != mModSkill.end(); ++j)
                if ((*j)->GetMiscValue() == int32(id))
                        (*j)->HandleEffect(this, AURA_EFFECT_HANDLE_SKILL, true);

            // permanent bonuses
            AuraEffectList const& mModSkillTalent = GetAuraEffectsByType(SPELL_AURA_MOD_SKILL_TALENT);
            for (AuraEffectList::const_iterator j = mModSkillTalent.begin(); j != mModSkillTalent.end(); ++j)
                if ((*j)->GetMiscValue() == int32(id))
                        (*j)->HandleEffect(this, AURA_EFFECT_HANDLE_SKILL, true);

            // Learn all spells for skill
            learnSkillRewardedSpells(id, newVal);
            return;
        }
    }
}

bool Player::HasSkill(uint32 skill) const
{
    if (!skill)
        return false;

    SkillStatusMap::const_iterator itr = mSkillStatus.find(skill);
    return (itr != mSkillStatus.end() && itr->second.uState != SKILL_DELETED);
}

uint16 Player::GetSkillStep(uint16 skill) const
{
    if (!skill)
        return 0;

    SkillStatusMap::const_iterator itr = mSkillStatus.find(skill);
    if (itr == mSkillStatus.end() || itr->second.uState == SKILL_DELETED)
        return 0;

    return PAIR32_HIPART(GetUInt32Value(PLAYER_SKILL_INDEX(itr->second.pos)));
}

uint16 Player::GetSkillValue(uint32 skill) const
{
    if (!skill)
        return 0;

    SkillStatusMap::const_iterator itr = mSkillStatus.find(skill);
    if (itr == mSkillStatus.end() || itr->second.uState == SKILL_DELETED)
        return 0;

    uint32 bonus = GetUInt32Value(PLAYER_SKILL_BONUS_INDEX(itr->second.pos));

    int32 result = int32(SKILL_VALUE(GetUInt32Value(PLAYER_SKILL_VALUE_INDEX(itr->second.pos))));
    result += SKILL_TEMP_BONUS(bonus);
    result += SKILL_PERM_BONUS(bonus);
    return result < 0 ? 0 : result;
}

uint16 Player::GetMaxSkillValue(uint32 skill) const
{
    if (!skill)
        return 0;

    SkillStatusMap::const_iterator itr = mSkillStatus.find(skill);
    if (itr == mSkillStatus.end() || itr->second.uState == SKILL_DELETED)
        return 0;

    uint32 bonus = GetUInt32Value(PLAYER_SKILL_BONUS_INDEX(itr->second.pos));

    int32 result = int32(SKILL_MAX(GetUInt32Value(PLAYER_SKILL_VALUE_INDEX(itr->second.pos))));
    result += SKILL_TEMP_BONUS(bonus);
    result += SKILL_PERM_BONUS(bonus);
    return result < 0 ? 0 : result;
}

uint16 Player::GetPureMaxSkillValue(uint32 skill) const
{
    if (!skill)
        return 0;

    SkillStatusMap::const_iterator itr = mSkillStatus.find(skill);
    if (itr == mSkillStatus.end() || itr->second.uState == SKILL_DELETED)
        return 0;

    return SKILL_MAX(GetUInt32Value(PLAYER_SKILL_VALUE_INDEX(itr->second.pos)));
}

uint16 Player::GetBaseSkillValue(uint32 skill) const
{
    if (!skill)
        return 0;

    SkillStatusMap::const_iterator itr = mSkillStatus.find(skill);
    if (itr == mSkillStatus.end() || itr->second.uState == SKILL_DELETED)
        return 0;

    int32 result = int32(SKILL_VALUE(GetUInt32Value(PLAYER_SKILL_VALUE_INDEX(itr->second.pos))));
    result += SKILL_PERM_BONUS(GetUInt32Value(PLAYER_SKILL_BONUS_INDEX(itr->second.pos)));
    return result < 0 ? 0 : result;
}

uint16 Player::GetPureSkillValue(uint32 skill) const
{
    if (!skill)
        return 0;

    SkillStatusMap::const_iterator itr = mSkillStatus.find(skill);
    if (itr == mSkillStatus.end() || itr->second.uState == SKILL_DELETED)
        return 0;

    return SKILL_VALUE(GetUInt32Value(PLAYER_SKILL_VALUE_INDEX(itr->second.pos)));
}

int16 Player::GetSkillPermBonusValue(uint32 skill) const
{
    if (!skill)
        return 0;

    SkillStatusMap::const_iterator itr = mSkillStatus.find(skill);
    if (itr == mSkillStatus.end() || itr->second.uState == SKILL_DELETED)
        return 0;

    return SKILL_PERM_BONUS(GetUInt32Value(PLAYER_SKILL_BONUS_INDEX(itr->second.pos)));
}

int16 Player::GetSkillTempBonusValue(uint32 skill) const
{
    if (!skill)
        return 0;

    SkillStatusMap::const_iterator itr = mSkillStatus.find(skill);
    if (itr == mSkillStatus.end() || itr->second.uState == SKILL_DELETED)
        return 0;

    return SKILL_TEMP_BONUS(GetUInt32Value(PLAYER_SKILL_BONUS_INDEX(itr->second.pos)));
}

void Player::SendActionButtons(uint32 state) const
{
    WorldPacket data(SMSG_ACTION_BUTTONS, 1+(MAX_ACTION_BUTTONS*4));
    data << uint8(state);
    /*
        state can be 0, 1, 2
        0 - Looks to be sent when initial action buttons get sent, however on Trinity we use 1 since 0 had some difficulties
        1 - Used in any SMSG_ACTION_BUTTONS packet with button data on Trinity. Only used after spec swaps on retail.
        2 - Clears the action bars client sided. This is sent during spec swap before unlearning and before sending the new buttons
    */
    if (state != 2)
    {
        for (uint8 button = 0; button < MAX_ACTION_BUTTONS; ++button)
        {
            ActionButtonList::const_iterator itr = m_actionButtons.find(button);
            if (itr != m_actionButtons.end() && itr->second.uState != ACTIONBUTTON_DELETED)
                data << uint32(itr->second.packedData);
            else
                data << uint32(0);
        }
    }

    GetSession()->SendPacket(&data);
    sLog->outInfo(LOG_FILTER_NETWORKIO, "SMSG_ACTION_BUTTONS sent '%u' spec '%u' Sent", GetGUIDLow(), m_activeSpec);
}

bool Player::IsActionButtonDataValid(uint8 button, uint32 action, uint8 type)
{
    if (button >= MAX_ACTION_BUTTONS)
    {
        sLog->outError(LOG_FILTER_PLAYER_LOADING, "Action %u not added into button %u for player %s: button must be < %u", action, button, GetName(), MAX_ACTION_BUTTONS);
        return false;
    }

    if (action >= MAX_ACTION_BUTTON_ACTION_VALUE)
    {
        sLog->outError(LOG_FILTER_PLAYER_LOADING, "Action %u not added into button %u for player %s: action must be < %u", action, button, GetName(), MAX_ACTION_BUTTON_ACTION_VALUE);
        return false;
    }

    switch (type)
    {
        case ACTION_BUTTON_SPELL:
            if (!sSpellMgr->GetSpellInfo(action))
            {
                sLog->outError(LOG_FILTER_PLAYER_LOADING, "Spell action %u not added into button %u for player %s: spell not exist", action, button, GetName());
                return false;
            }

            if (!HasSpell(action))
            {
                sLog->outDebug(LOG_FILTER_PLAYER_LOADING, "Player::IsActionButtonDataValid Spell action %u not added into button %u for player %s: player don't known this spell", action, button, GetName());
                return false;
            }
            break;
        case ACTION_BUTTON_ITEM:
            if (!sObjectMgr->GetItemTemplate(action))
            {
                sLog->outError(LOG_FILTER_PLAYER_LOADING, "Item action %u not added into button %u for player %s: item not exist", action, button, GetName());
                return false;
            }
            break;
        default:
            break;                                          // other cases not checked at this moment
    }

    return true;
}

ActionButton* Player::addActionButton(uint8 button, uint32 action, uint8 type)
{
    if (!IsActionButtonDataValid(button, action, type))
        return NULL;

    // it create new button (NEW state) if need or return existed
    ActionButton& ab = m_actionButtons[button];

    // set data and update to CHANGED if not NEW
    ab.SetActionAndType(action, ActionButtonType(type));

    sLog->outInfo(LOG_FILTER_PLAYER_LOADING, "Player '%u' Added Action '%u' (type %u) to Button '%u'", GetGUIDLow(), action, type, button);
    return &ab;
}

void Player::removeActionButton(uint8 button)
{
    ActionButtonList::iterator buttonItr = m_actionButtons.find(button);
    if (buttonItr == m_actionButtons.end() || buttonItr->second.uState == ACTIONBUTTON_DELETED)
        return;

    if (buttonItr->second.uState == ACTIONBUTTON_NEW)
        m_actionButtons.erase(buttonItr);                   // new and not saved
    else
        buttonItr->second.uState = ACTIONBUTTON_DELETED;    // saved, will deleted at next save

    sLog->outInfo(LOG_FILTER_PLAYER_LOADING, "Action Button '%u' Removed from Player '%u'", button, GetGUIDLow());
}

ActionButton const* Player::GetActionButton(uint8 button)
{
    ActionButtonList::iterator buttonItr = m_actionButtons.find(button);
    if (buttonItr == m_actionButtons.end() || buttonItr->second.uState == ACTIONBUTTON_DELETED)
        return NULL;

    return &buttonItr->second;
}

bool Player::UpdatePosition(float x, float y, float z, float orientation, bool teleport)
{
    if (!Unit::UpdatePosition(x, y, z, orientation, teleport))
        return false;

    //if (movementInfo.flags & MOVEMENTFLAG_MOVING)
    //    mover->RemoveAurasWithInterruptFlags(AURA_INTERRUPT_FLAG_MOVE);
    //if (movementInfo.flags & MOVEMENTFLAG_TURNING)
    //    mover->RemoveAurasWithInterruptFlags(AURA_INTERRUPT_FLAG_TURNING);
    //AURA_INTERRUPT_FLAG_JUMP not sure

    // group update
    if (GetGroup())
        SetGroupUpdateFlag(GROUP_UPDATE_FLAG_POSITION);

    if (GetTrader() && !IsWithinDistInMap(GetTrader(), INTERACTION_DISTANCE))
        GetSession()->SendCancelTrade();

    CheckAreaExploreAndOutdoor();

    return true;
}

void Player::SaveRecallPosition()
{
    m_recallMap = GetMapId();
    m_recallX = GetPositionX();
    m_recallY = GetPositionY();
    m_recallZ = GetPositionZ();
    m_recallO = GetOrientation();
}

void Player::SendMessageToSetInRange(WorldPacket* data, float dist, bool self)
{
    if (self)
        GetSession()->SendPacket(data);

    Trinity::MessageDistDeliverer notifier(this, data, dist);
    VisitNearbyWorldObject(dist, notifier);
}

void Player::SendMessageToSetInRange(WorldPacket* data, float dist, bool self, bool own_team_only)
{
    if (self)
        GetSession()->SendPacket(data);

    Trinity::MessageDistDeliverer notifier(this, data, dist, own_team_only);
    VisitNearbyWorldObject(dist, notifier);
}

void Player::SendMessageToSet(WorldPacket* data, Player const* skipped_rcvr)
{
    if (skipped_rcvr != this)
        GetSession()->SendPacket(data);

    // we use World::GetMaxVisibleDistance() because i cannot see why not use a distance
    // update: replaced by GetMap()->GetVisibilityDistance()
    Trinity::MessageDistDeliverer notifier(this, data, GetVisibilityRange(), false, skipped_rcvr);
    VisitNearbyWorldObject(GetVisibilityRange(), notifier);
}

void Player::SendDirectMessage(WorldPacket* data)
{
    m_session->SendPacket(data);
}

void Player::SendCinematicStart(uint32 CinematicSequenceId)
{
    WorldPacket data(SMSG_TRIGGER_CINEMATIC, 4);
    data << uint32(CinematicSequenceId);
    SendDirectMessage(&data);
}

void Player::SendMovieStart(uint32 MovieId)
{
    WorldPacket data(SMSG_TRIGGER_MOVIE, 4);
    data << uint32(MovieId);
    SendDirectMessage(&data);
}

void Player::CheckAreaExploreAndOutdoor()
{
    if (!isAlive())
        return;

    if (isInFlight())
        return;

    bool isOutdoor;
    uint16 areaFlag = GetBaseMap()->GetAreaFlag(GetPositionX(), GetPositionY(), GetPositionZ(), &isOutdoor);

    if (sWorld->getBoolConfig(CONFIG_VMAP_INDOOR_CHECK) && !isOutdoor)
        RemoveAurasWithAttribute(SPELL_ATTR0_OUTDOORS_ONLY);

    if (areaFlag == 0xffff)
        return;
    int offset = areaFlag / 32;

    if (offset >= PLAYER_EXPLORED_ZONES_SIZE)
    {
        sLog->outError(LOG_FILTER_PLAYER, "Wrong area flag %u in map data for (X: %f Y: %f) point to field PLAYER_EXPLORED_ZONES_1 + %u ( %u must be < %u ).", areaFlag, GetPositionX(), GetPositionY(), offset, offset, PLAYER_EXPLORED_ZONES_SIZE);
        return;
    }

    uint32 val = (uint32)(1 << (areaFlag % 32));
    uint32 currFields = GetUInt32Value(PLAYER_EXPLORED_ZONES_1 + offset);

    if (!(currFields & val))
    {
        SetUInt32Value(PLAYER_EXPLORED_ZONES_1 + offset, (uint32)(currFields | val));

        UpdateAchievementCriteria(ACHIEVEMENT_CRITERIA_TYPE_EXPLORE_AREA);

        AreaTableEntry const* areaEntry = GetAreaEntryByAreaFlagAndMap(areaFlag, GetMapId());
        if (!areaEntry)
        {
            sLog->outError(LOG_FILTER_PLAYER, "Player %u discovered unknown area (x: %f y: %f z: %f map: %u", GetGUIDLow(), GetPositionX(), GetPositionY(), GetPositionZ(), GetMapId());
            return;
        }

        if (areaEntry->area_level > 0)
        {
            uint32 area = areaEntry->ID;
            if (getLevel() >= sWorld->getIntConfig(CONFIG_MAX_PLAYER_LEVEL))
            {
                SendExplorationExperience(area, 0);
            }
            else
            {
                int32 diff = int32(getLevel()) - areaEntry->area_level;
                uint32 XP = 0;
                if (diff < -5)
                {
                    XP = uint32(sObjectMgr->GetBaseXP(getLevel()+5)*sWorld->getRate(RATE_XP_EXPLORE));
                }
                else if (diff > 5)
                {
                    int32 exploration_percent = (100-((diff-5)*5));
                    if (exploration_percent > 100)
                        exploration_percent = 100;
                    else if (exploration_percent < 0)
                        exploration_percent = 0;

                    XP = uint32(sObjectMgr->GetBaseXP(areaEntry->area_level)*exploration_percent/100*sWorld->getRate(RATE_XP_EXPLORE));
                }
                else
                {
                    XP = uint32(sObjectMgr->GetBaseXP(areaEntry->area_level)*sWorld->getRate(RATE_XP_EXPLORE));
                }

                GiveXP(XP, NULL);
                SendExplorationExperience(area, XP);
            }
            sLog->outInfo(LOG_FILTER_PLAYER, "Player %u discovered a new area: %u", GetGUIDLow(), area);
        }
    }
}

uint32 Player::TeamForRace(uint8 race)
{
    if (ChrRacesEntry const* rEntry = sChrRacesStore.LookupEntry(race))
    {
        switch (rEntry->TeamID)
        {
            case 1: return HORDE;
            case 7: return ALLIANCE;
        }
        sLog->outError(LOG_FILTER_PLAYER, "Race (%u) has wrong teamid (%u) in DBC: wrong DBC files?", uint32(race), rEntry->TeamID);
    }
    else
        sLog->outError(LOG_FILTER_PLAYER, "Race (%u) not found in DBC: wrong DBC files?", uint32(race));

    return ALLIANCE;
}

void Player::setFactionForRace(uint8 race)
{
    m_team = TeamForRace(race);

    ChrRacesEntry const* rEntry = sChrRacesStore.LookupEntry(race);
    setFaction(rEntry ? rEntry->FactionID : 0);
}

ReputationRank Player::GetReputationRank(uint32 faction) const
{
    FactionEntry const* factionEntry = sFactionStore.LookupEntry(faction);
    return GetReputationMgr().GetRank(factionEntry);
}

//Calculate total reputation percent player gain with quest/creature level
int32 Player::CalculateReputationGain(uint32 creatureOrQuestLevel, int32 rep, int32 faction, bool for_quest, bool noQuestBonus)
{
    float percent = 100.0f;

    // Get the generic rate first
    if (RepRewardRate const* repData = sObjectMgr->GetRepRewardRate(faction))
    {
        float repRate = for_quest ? repData->quest_rate : repData->creature_rate;
        percent *= repRate;
    }

    float rate = for_quest ? sWorld->getRate(RATE_REPUTATION_LOWLEVEL_QUEST) : sWorld->getRate(RATE_REPUTATION_LOWLEVEL_KILL);

    if (rate != 1.0f && creatureOrQuestLevel <= Trinity::XP::GetGrayLevel(getLevel()))
        percent *= rate;

    float repMod = noQuestBonus ? 0.0f : (float)GetTotalAuraModifier(SPELL_AURA_MOD_REPUTATION_GAIN);

    if (!for_quest)
        repMod += GetTotalAuraModifierByMiscValue(SPELL_AURA_MOD_FACTION_REPUTATION_GAIN, faction);

    percent += rep > 0 ? repMod : -repMod;

    if (percent <= 0.0f)
        return 0;

    return int32(rep*percent/100);
}

//Calculates how many reputation points player gains in victim's enemy factions
void Player::RewardReputation(Unit* victim, float rate)
{
    if (!victim || victim->GetTypeId() == TYPEID_PLAYER)
        return;

    if (victim->ToCreature()->IsReputationGainDisabled())
        return;

    ReputationOnKillEntry const* Rep = sObjectMgr->GetReputationOnKilEntry(victim->ToCreature()->GetCreatureTemplate()->Entry);

    if (!Rep)
        return;

    uint32 ChampioningFaction = 0;

    if (GetChampioningFaction())
    {
        // support for: Championing - http://www.wowwiki.com/Championing

        Map const* map = GetMap();
        if (map && map->IsDungeon())
        {
            InstanceTemplate const* instance = sObjectMgr->GetInstanceTemplate(map->GetId());
            if (instance)
            {
                AccessRequirement const* pAccessRequirement = sObjectMgr->GetAccessRequirement(map->GetId(), ((InstanceMap*)map)->GetDifficulty());
                if (pAccessRequirement)
                {
                    if (!map->IsRaid() && pAccessRequirement->levelMin == 80)
                        ChampioningFaction = GetChampioningFaction();
                }
            }
        }
    }

    // Favored reputation increase START
    uint32 zone = GetZoneId();
    uint32 team = GetTeam();
    float favored_rep_mult = 0;

    if ((HasAura(32096) || HasAura(32098)) && (zone == 3483 || zone == 3562 || zone == 3836 || zone == 3713 || zone == 3714)) favored_rep_mult = 0.25; // Thrallmar's Favor and Honor Hold's Favor
    else if (HasAura(30754) && (Rep->RepFaction1 == 609 || Rep->RepFaction2 == 609) && !ChampioningFaction)                   favored_rep_mult = 0.25; // Cenarion Favor

    if (favored_rep_mult > 0) favored_rep_mult *= 2; // Multiplied by 2 because the reputation is divided by 2 for some reason (See "donerep1 / 2" and "donerep2 / 2") -- if you know why this is done, please update/explain :)
    // Favored reputation increase END

    bool recruitAFriend = GetsRecruitAFriendBonus(false);

    if (Rep->RepFaction1 && (!Rep->TeamDependent || team == ALLIANCE))
    {
        int32 donerep1 = CalculateReputationGain(victim->getLevel(), Rep->RepValue1, ChampioningFaction ? ChampioningFaction : Rep->RepFaction1, false);
        donerep1 = int32(donerep1*(rate + favored_rep_mult));

        if (recruitAFriend)
            donerep1 = int32(donerep1 * (1 + sWorld->getRate(RATE_REPUTATION_RECRUIT_A_FRIEND_BONUS)));

        FactionEntry const* factionEntry1 = sFactionStore.LookupEntry(ChampioningFaction ? ChampioningFaction : Rep->RepFaction1);
        uint32 current_reputation_rank1 = GetReputationMgr().GetRank(factionEntry1);
        if (factionEntry1 && current_reputation_rank1 <= Rep->ReputationMaxCap1)
            GetReputationMgr().ModifyReputation(factionEntry1, donerep1);
    }

    if (Rep->RepFaction2 && (!Rep->TeamDependent || team == HORDE))
    {
        int32 donerep2 = CalculateReputationGain(victim->getLevel(), Rep->RepValue2, ChampioningFaction ? ChampioningFaction : Rep->RepFaction2, false);
        donerep2 = int32(donerep2*(rate + favored_rep_mult));

        if (recruitAFriend)
            donerep2 = int32(donerep2 * (1 + sWorld->getRate(RATE_REPUTATION_RECRUIT_A_FRIEND_BONUS)));

        FactionEntry const* factionEntry2 = sFactionStore.LookupEntry(ChampioningFaction ? ChampioningFaction : Rep->RepFaction2);
        uint32 current_reputation_rank2 = GetReputationMgr().GetRank(factionEntry2);
        if (factionEntry2 && current_reputation_rank2 <= Rep->ReputationMaxCap2)
            GetReputationMgr().ModifyReputation(factionEntry2, donerep2);
    }
}

//Calculate how many reputation points player gain with the quest
void Player::RewardReputation(Quest const* quest)
{
    bool recruitAFriend = GetsRecruitAFriendBonus(false);

    // quest reputation reward/loss
    for (uint8 i = 0; i < QUEST_REPUTATIONS_COUNT; ++i)
    {
        if (!quest->RewardFactionId[i])
            continue;
        if (quest->RewardFactionValueIdOverride[i])
        {
            int32 rep = CalculateReputationGain(GetQuestLevel(quest), quest->RewardFactionValueIdOverride[i]/100, quest->RewardFactionId[i], true, true);

            if (recruitAFriend)
                rep = int32(rep * (1 + sWorld->getRate(RATE_REPUTATION_RECRUIT_A_FRIEND_BONUS)));

            if (FactionEntry const* factionEntry = sFactionStore.LookupEntry(quest->RewardFactionId[i]))
                GetReputationMgr().ModifyReputation(factionEntry, rep);
        }
        else
        {
            uint32 row = ((quest->RewardFactionValueId[i] < 0) ? 1 : 0) + 1;
            uint32 field = abs(quest->RewardFactionValueId[i]);

            if (const QuestFactionRewEntry* pRow = sQuestFactionRewardStore.LookupEntry(row))
            {
                int32 repPoints = pRow->QuestRewFactionValue[field];

                if (!repPoints)
                    continue;

                repPoints = CalculateReputationGain(GetQuestLevel(quest), repPoints, quest->RewardFactionId[i], true);

                if (recruitAFriend)
                    repPoints = int32(repPoints * (1 + sWorld->getRate(RATE_REPUTATION_RECRUIT_A_FRIEND_BONUS)));

                if (const FactionEntry* factionEntry = sFactionStore.LookupEntry(quest->RewardFactionId[i]))
                    GetReputationMgr().ModifyReputation(factionEntry, repPoints);
            }
        }
    }
}

void Player::UpdateHonorFields()
{
    /// called when rewarding honor and at each save
    time_t now = time_t(time(NULL));
    time_t today = time_t(time(NULL) / DAY) * DAY;

    if (m_lastHonorUpdateTime < today)
    {
        time_t yesterday = today - DAY;

        uint16 kills_today = PAIR32_LOPART(GetUInt32Value(PLAYER_FIELD_KILLS));

        // update yesterday's contribution
        if (m_lastHonorUpdateTime >= yesterday)
        {
            SetUInt32Value(PLAYER_FIELD_YESTERDAY_CONTRIBUTION, GetUInt32Value(PLAYER_FIELD_TODAY_CONTRIBUTION));

            // this is the first update today, reset today's contribution
            SetUInt32Value(PLAYER_FIELD_TODAY_CONTRIBUTION, 0);
            SetUInt32Value(PLAYER_FIELD_KILLS, MAKE_PAIR32(0, kills_today));
        }
        else
        {
            // no honor/kills yesterday or today, reset
            SetUInt32Value(PLAYER_FIELD_YESTERDAY_CONTRIBUTION, 0);
            SetUInt32Value(PLAYER_FIELD_KILLS, 0);
        }
    }

    m_lastHonorUpdateTime = now;
}

///Calculate the amount of honor gained based on the victim
///and the size of the group for which the honor is divided
///An exact honor value can also be given (overriding the calcs)
bool Player::RewardHonor(Unit* uVictim, uint32 groupsize, int32 honor, bool pvptoken)
{
    // do not reward honor in arenas, but enable onkill spellproc
    if (InArena())
    {
        if (!uVictim || uVictim == this || uVictim->GetTypeId() != TYPEID_PLAYER)
            return false;

        if (GetBGTeam() == uVictim->ToPlayer()->GetBGTeam())
            return false;

        return true;
    }

    // 'Inactive' this aura prevents the player from gaining honor points and battleground tokens
    if (HasAura(SPELL_AURA_PLAYER_INACTIVE))
        return false;

    uint64 victim_guid = 0;
    uint32 victim_rank = 0;

    // need call before fields update to have chance move yesterday data to appropriate fields before today data change.
    UpdateHonorFields();

    // do not reward honor in arenas, but return true to enable onkill spellproc
    if (InBattleground() && GetBattleground() && GetBattleground()->isArena())
        return true;

    // Promote to float for calculations
    float honor_f = (float)honor;

    if (honor_f <= 0)
    {
        if (!uVictim || uVictim == this || uVictim->HasAuraType(SPELL_AURA_NO_PVP_CREDIT))
            return false;

        victim_guid = uVictim->GetGUID();

        if (uVictim->GetTypeId() == TYPEID_PLAYER)
        {
            Player* victim = uVictim->ToPlayer();

            if (GetTeam() == victim->GetTeam() && !sWorld->IsFFAPvPRealm())
                return false;

            uint8 k_level = getLevel();
            uint8 k_grey = Trinity::XP::GetGrayLevel(k_level);
            uint8 v_level = victim->getLevel();

            if (v_level <= k_grey)
                return false;

            // PLAYER_CHOSEN_TITLE VALUES DESCRIPTION
            //  [0]      Just name
            //  [1..14]  Alliance honor titles and player name
            //  [15..28] Horde honor titles and player name
            //  [29..38] Other title and player name
            //  [39+]    Nothing
            uint32 victim_title = victim->GetUInt32Value(PLAYER_CHOSEN_TITLE);
                                                        // Get Killer titles, CharTitlesEntry::bit_index
            // Ranks:
            //  title[1..14]  -> rank[5..18]
            //  title[15..28] -> rank[5..18]
            //  title[other]  -> 0
            if (victim_title == 0)
                victim_guid = 0;                        // Don't show HK: <rank> message, only log.
            else if (victim_title < 15)
                victim_rank = victim_title + 4;
            else if (victim_title < 29)
                victim_rank = victim_title - 14 + 4;
            else
                victim_guid = 0;                        // Don't show HK: <rank> message, only log.

            honor_f = ceil(Trinity::Honor::hk_honor_at_level_f(k_level) * (v_level - k_grey) / (k_level - k_grey));

            // count the number of playerkills in one day
            ApplyModUInt32Value(PLAYER_FIELD_KILLS, 1, true);
            // and those in a lifetime
            ApplyModUInt32Value(PLAYER_FIELD_LIFETIME_HONORABLE_KILLS, 1, true);
            UpdateAchievementCriteria(ACHIEVEMENT_CRITERIA_TYPE_EARN_HONORABLE_KILL);
            UpdateAchievementCriteria(ACHIEVEMENT_CRITERIA_TYPE_HK_CLASS, victim->getClass());
            UpdateAchievementCriteria(ACHIEVEMENT_CRITERIA_TYPE_HK_RACE, victim->getRace());
            UpdateAchievementCriteria(ACHIEVEMENT_CRITERIA_TYPE_HONORABLE_KILL_AT_AREA, GetAreaId());
            UpdateAchievementCriteria(ACHIEVEMENT_CRITERIA_TYPE_HONORABLE_KILL, 1, 0, victim);
        }
        else
        {
            if (!uVictim->ToCreature()->isRacialLeader())
                return false;

            honor_f = 100.0f;                               // ??? need more info
            victim_rank = 19;                               // HK: Leader
        }
    }

    if (uVictim != NULL)
    {
        if (groupsize > 1)
            honor_f /= groupsize;

        // apply honor multiplier from aura (not stacking-get highest)
        AddPct(honor_f, GetMaxPositiveAuraModifier(SPELL_AURA_MOD_HONOR_GAIN_PCT));
    }

    honor_f *= sWorld->getRate(RATE_HONOR);
    // Back to int now
    honor = int32(honor_f);
    // honor - for show honor points in log
    // victim_guid - for show victim name in log
    // victim_rank [1..4]  HK: <dishonored rank>
    // victim_rank [5..19] HK: <alliance\horde rank>
    // victim_rank [0, 20+] HK: <>
    WorldPacket data(SMSG_PVP_CREDIT, 4+8+4);
    data << honor;
    data << victim_guid;
    data << victim_rank;

    GetSession()->SendPacket(&data);

    // add honor points
    ModifyHonorPoints(honor);

    ApplyModUInt32Value(PLAYER_FIELD_TODAY_CONTRIBUTION, honor, true);

    if (InBattleground() && honor > 0)
    {
        if (Battleground* bg = GetBattleground())
        {
            bg->UpdatePlayerScore(this, SCORE_BONUS_HONOR, honor, false); //false: prevent looping
        }
    }

    if (sWorld->getBoolConfig(CONFIG_PVP_TOKEN_ENABLE) && pvptoken)
    {
        if (!uVictim || uVictim == this || uVictim->HasAuraType(SPELL_AURA_NO_PVP_CREDIT))
            return true;

        if (uVictim->GetTypeId() == TYPEID_PLAYER)
        {
            // Check if allowed to receive it in current map
            uint8 MapType = sWorld->getIntConfig(CONFIG_PVP_TOKEN_MAP_TYPE);
            if ((MapType == 1 && !InBattleground() && !HasByteFlag(UNIT_FIELD_BYTES_2, 1, UNIT_BYTE2_FLAG_FFA_PVP))
                || (MapType == 2 && !HasByteFlag(UNIT_FIELD_BYTES_2, 1, UNIT_BYTE2_FLAG_FFA_PVP))
                || (MapType == 3 && !InBattleground()))
                return true;

            uint32 itemId = sWorld->getIntConfig(CONFIG_PVP_TOKEN_ID);
            int32 count = sWorld->getIntConfig(CONFIG_PVP_TOKEN_COUNT);

            if (AddItem(itemId, count))
                ChatHandler(this).PSendSysMessage("You have been awarded a token for slaying another player.");
        }
    }

    return true;
}

void Player::SetHonorPoints(uint32 value)
{
    if (value > sWorld->getIntConfig(CONFIG_MAX_HONOR_POINTS))
        value = sWorld->getIntConfig(CONFIG_MAX_HONOR_POINTS);
    SetUInt32Value(PLAYER_FIELD_HONOR_CURRENCY, value);
    if (value)
        AddKnownCurrency(ITEM_HONOR_POINTS_ID);
}

void Player::SetArenaPoints(uint32 value)
{
    if (value > sWorld->getIntConfig(CONFIG_MAX_ARENA_POINTS))
        value = sWorld->getIntConfig(CONFIG_MAX_ARENA_POINTS);
    SetUInt32Value(PLAYER_FIELD_ARENA_CURRENCY, value);
    if (value)
        AddKnownCurrency(ITEM_ARENA_POINTS_ID);
}

void Player::ModifyHonorPoints(int32 value, SQLTransaction* trans /*=NULL*/)
{
    int32 newValue = int32(GetHonorPoints()) + value;
    if (newValue < 0)
        newValue = 0;
    SetHonorPoints(uint32(newValue));

    if (trans && !trans->null())
    {
        PreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_UDP_CHAR_HONOR_POINTS);
        stmt->setUInt32(0, newValue);
        stmt->setUInt32(1, GetGUIDLow());
        (*trans)->Append(stmt);
    }
}

void Player::ModifyArenaPoints(int32 value, SQLTransaction* trans /*=NULL*/)
{
    int32 newValue = int32(GetArenaPoints()) + value;
    if (newValue < 0)
        newValue = 0;
    SetArenaPoints(uint32(newValue));

    if (trans && !trans->null())
    {
        PreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_UDP_CHAR_ARENA_POINTS);
        stmt->setUInt32(0, newValue);
        stmt->setUInt32(1, GetGUIDLow());
        (*trans)->Append(stmt);
    }
}

uint32 Player::GetGuildIdFromDB(uint64 guid)
{
    PreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_GUILD_MEMBER);
    stmt->setUInt32(0, GUID_LOPART(guid));
    PreparedQueryResult result = CharacterDatabase.Query(stmt);

    if (!result)
        return 0;

    uint32 id = result->Fetch()[0].GetUInt32();
    return id;
}

uint8 Player::GetRankFromDB(uint64 guid)
{
    PreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_GUILD_MEMBER);
    stmt->setUInt32(0, GUID_LOPART(guid));
    PreparedQueryResult result = CharacterDatabase.Query(stmt);

    if (result)
    {
        uint32 v = result->Fetch()[1].GetUInt8();
        return v;
    }
    else
        return 0;
}

uint32 Player::GetArenaTeamIdFromDB(uint64 guid, uint8 type)
{
    PreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_ARENA_TEAM_ID_BY_PLAYER_GUID);
    stmt->setUInt32(0, GUID_LOPART(guid));
    stmt->setUInt8(1, type);
    PreparedQueryResult result = CharacterDatabase.Query(stmt);

    if (!result)
        return 0;

    uint32 id = (*result)[0].GetUInt32();
    return id;
}

uint32 Player::GetZoneIdFromDB(uint64 guid)
{
    uint32 guidLow = GUID_LOPART(guid);
    PreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHAR_ZONE);
    stmt->setUInt32(0, guidLow);
    PreparedQueryResult result = CharacterDatabase.Query(stmt);

    if (!result)
        return 0;
    Field* fields = result->Fetch();
    uint32 zone = fields[0].GetUInt16();

    if (!zone)
    {
        // stored zone is zero, use generic and slow zone detection
        PreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHAR_POSITION_XYZ);
        stmt->setUInt32(0, guidLow);
        PreparedQueryResult result = CharacterDatabase.Query(stmt);

        if (!result)
            return 0;
        fields = result->Fetch();
        uint32 map = fields[0].GetUInt16();
        float posx = fields[1].GetFloat();
        float posy = fields[2].GetFloat();
        float posz = fields[3].GetFloat();

        zone = sMapMgr->GetZoneId(map, posx, posy, posz);

        if (zone > 0)
        {
            PreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_UPD_ZONE);

            stmt->setUInt16(0, uint16(zone));
            stmt->setUInt32(1, guidLow);

            CharacterDatabase.Execute(stmt);
        }
    }

    return zone;
}

uint32 Player::GetLevelFromDB(uint64 guid)
{
    PreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHAR_LEVEL);
    stmt->setUInt32(0, GUID_LOPART(guid));
    PreparedQueryResult result = CharacterDatabase.Query(stmt);

    if (!result)
        return 0;

    Field* fields = result->Fetch();
    uint8 level = fields[0].GetUInt8();

    return level;
}

void Player::UpdateArea(uint32 newArea)
{
    // FFA_PVP flags are area and not zone id dependent
    // so apply them accordingly
    m_areaUpdateId    = newArea;

    AreaTableEntry const* area = GetAreaEntryByAreaID(newArea);
    pvpInfo.inFFAPvPArea = area && (area->flags & AREA_FLAG_ARENA);
    UpdatePvPState(true);

    UpdateAreaDependentAuras(newArea);

    // previously this was in UpdateZone (but after UpdateArea) so nothing will break
    pvpInfo.inNoPvPArea = false;
    if (area && area->IsSanctuary())    // in sanctuary
    {
        SetByteFlag(UNIT_FIELD_BYTES_2, 1, UNIT_BYTE2_FLAG_SANCTUARY);
        pvpInfo.inNoPvPArea = true;
        CombatStopWithPets();
    }
    else
        RemoveByteFlag(UNIT_FIELD_BYTES_2, 1, UNIT_BYTE2_FLAG_SANCTUARY);
}

void Player::UpdateZone(uint32 newZone, uint32 newArea)
{
    if (m_zoneUpdateId != newZone)
    {
        sOutdoorPvPMgr->HandlePlayerLeaveZone(this, m_zoneUpdateId);
        sOutdoorPvPMgr->HandlePlayerEnterZone(this, newZone);
        sBattlefieldMgr->HandlePlayerLeaveZone(this, m_zoneUpdateId);
        sBattlefieldMgr->HandlePlayerEnterZone(this, newZone);
        SendInitWorldStates(newZone, newArea);              // only if really enters to new zone, not just area change, works strange...
    }

    // group update
    if (Group* group = GetGroup())
    {
        SetGroupUpdateFlag(GROUP_UPDATE_FULL);
        if (GetSession() && group->isLFGGroup() && sLFGMgr->IsTeleported(GetGUID()))
        {
            for (GroupReference* itr = group->GetFirstMember(); itr != NULL; itr = itr->next())
            {
                if (Player* member = itr->getSource())
                    GetSession()->SendNameQueryOpcode(member->GetGUID());
            }
        }
    }

    m_zoneUpdateId    = newZone;
    m_zoneUpdateTimer = ZONE_UPDATE_INTERVAL;

    // zone changed, so area changed as well, update it
    UpdateArea(newArea);

    AreaTableEntry const* zone = GetAreaEntryByAreaID(newZone);
    if (!zone)
        return;

    if (sWorld->getBoolConfig(CONFIG_WEATHER))
    {
        if (Weather* weather = WeatherMgr::FindWeather(zone->ID))
            weather->SendWeatherUpdateToPlayer(this);
        else
        {
            if (!WeatherMgr::AddWeather(zone->ID))
            {
                // send fine weather packet to remove old zone's weather
                WeatherMgr::SendFineWeatherUpdateToPlayer(this);
            }
        }
    }

    sScriptMgr->OnPlayerUpdateZone(this, newZone, newArea);

    // in PvP, any not controlled zone (except zone->team == 6, default case)
    // in PvE, only opposition team capital
    switch (zone->team)
    {
        case AREATEAM_ALLY:
            pvpInfo.inHostileArea = GetTeam() != ALLIANCE && (sWorld->IsPvPRealm() || zone->flags & AREA_FLAG_CAPITAL);
            break;
        case AREATEAM_HORDE:
            pvpInfo.inHostileArea = GetTeam() != HORDE && (sWorld->IsPvPRealm() || zone->flags & AREA_FLAG_CAPITAL);
            break;
        case AREATEAM_NONE:
            // overwrite for battlegrounds, maybe batter some zone flags but current known not 100% fit to this
            pvpInfo.inHostileArea = sWorld->IsPvPRealm() || InBattleground() || zone->flags & AREA_FLAG_WINTERGRASP;
            break;
        default:                                            // 6 in fact
            pvpInfo.inHostileArea = false;
            break;
    }

    if (zone->flags & AREA_FLAG_CAPITAL)                     // Is in a capital city
    {
        if (!pvpInfo.inHostileArea || zone->IsSanctuary())
        {
            SetFlag(PLAYER_FLAGS, PLAYER_FLAGS_RESTING);
            SetRestType(REST_TYPE_IN_CITY);
            InnEnter(time(0), GetMapId(), 0, 0, 0);
        }
        pvpInfo.inNoPvPArea = true;
    }
    else
    {
        if (HasFlag(PLAYER_FLAGS, PLAYER_FLAGS_RESTING))
        {
            if (GetRestType() == REST_TYPE_IN_TAVERN)        // Still inside a tavern or has recently left
            {
                // Remove rest state if we have recently left a tavern.
                if (GetMapId() != GetInnPosMapId() || GetExactDist(GetInnPosX(), GetInnPosY(), GetInnPosZ()) > 1.0f)
                {
                    RemoveFlag(PLAYER_FLAGS, PLAYER_FLAGS_RESTING);
                    SetRestType(REST_TYPE_NO);
                }
            }
            else                                             // Recently left a capital city
            {
                RemoveFlag(PLAYER_FLAGS, PLAYER_FLAGS_RESTING);
                SetRestType(REST_TYPE_NO);
            }
        }
    }

    UpdatePvPState();

    // remove items with area/map limitations (delete only for alive player to allow back in ghost mode)
    // if player resurrected at teleport this will be applied in resurrect code
    if (isAlive())
        DestroyZoneLimitedItem(true, newZone);

    // check some item equip limitations (in result lost CanTitanGrip at talent reset, for example)
    AutoUnequipOffhandIfNeed();

    // recent client version not send leave/join channel packets for built-in local channels
    UpdateLocalChannels(newZone);

    UpdateZoneDependentAuras(newZone);
}

//If players are too far away from the duel flag... they lose the duel
void Player::CheckDuelDistance(time_t currTime)
{
    if (!duel)
        return;

    uint64 duelFlagGUID = GetUInt64Value(PLAYER_DUEL_ARBITER);
    GameObject* obj = GetMap()->GetGameObject(duelFlagGUID);
    if (!obj)
        return;

    if (duel->outOfBound == 0)
    {
        if (!IsWithinDistInMap(obj, 50))
        {
            duel->outOfBound = currTime;

            WorldPacket data(SMSG_DUEL_OUTOFBOUNDS, 0);
            GetSession()->SendPacket(&data);
        }
    }
    else
    {
        if (IsWithinDistInMap(obj, 40))
        {
            duel->outOfBound = 0;

            WorldPacket data(SMSG_DUEL_INBOUNDS, 0);
            GetSession()->SendPacket(&data);
        }
        else if (currTime >= (duel->outOfBound+10))
            DuelComplete(DUEL_FLED);
    }
}

bool Player::IsOutdoorPvPActive()
{
    return isAlive() && !HasInvisibilityAura() && !HasStealthAura() && IsPvP() && !HasUnitMovementFlag(MOVEMENTFLAG_FLYING) && !isInFlight();
}

void Player::DuelComplete(DuelCompleteType type)
{
    // duel not requested
    if (!duel)
        return;

    sLog->outDebug(LOG_FILTER_UNITS, "Duel Complete %s %s", GetName(), duel->opponent->GetName());

    WorldPacket data(SMSG_DUEL_COMPLETE, (1));
    data << (uint8)((type != DUEL_INTERRUPTED) ? 1 : 0);
    GetSession()->SendPacket(&data);

    if (duel->opponent->GetSession())
        duel->opponent->GetSession()->SendPacket(&data);

    if (type != DUEL_INTERRUPTED)
    {
        data.Initialize(SMSG_DUEL_WINNER, (1+20));          // we guess size
        data << uint8(type == DUEL_WON ? 0 : 1);            // 0 = just won; 1 = fled
        data << duel->opponent->GetName();
        data << GetName();
        SendMessageToSet(&data, true);
    }

    sScriptMgr->OnPlayerDuelEnd(duel->opponent, this, type);

    switch (type)
    {
        case DUEL_FLED:
            // if initiator and opponent are on the same team
            // or initiator and opponent are not PvP enabled, forcibly stop attacking
            if (duel->initiator->GetTeam() == duel->opponent->GetTeam())
            {
                duel->initiator->AttackStop();
                duel->opponent->AttackStop();
            }
            else
            {
                if (!duel->initiator->IsPvP())
                    duel->initiator->AttackStop();
                if (!duel->opponent->IsPvP())
                    duel->opponent->AttackStop();
            }
            break;
        case DUEL_WON:
            UpdateAchievementCriteria(ACHIEVEMENT_CRITERIA_TYPE_LOSE_DUEL, 1);
            duel->opponent->UpdateAchievementCriteria(ACHIEVEMENT_CRITERIA_TYPE_WIN_DUEL, 1);

            // Credit for quest Death's Challenge
            if (getClass() == CLASS_DEATH_KNIGHT && duel->opponent->GetQuestStatus(12733) == QUEST_STATUS_INCOMPLETE)
                duel->opponent->CastSpell(duel->opponent, 52994, true);

            break;
        default:
            break;
    }

    // Victory emote spell
    if (type != DUEL_INTERRUPTED)
        duel->opponent->CastSpell(duel->opponent, 52852, true);

    //Remove Duel Flag object
    GameObject* obj = GetMap()->GetGameObject(GetUInt64Value(PLAYER_DUEL_ARBITER));
    if (obj)
        duel->initiator->RemoveGameObject(obj, true);

    /* remove auras */
    AuraApplicationMap &itsAuras = duel->opponent->GetAppliedAuras();
    for (AuraApplicationMap::iterator i = itsAuras.begin(); i != itsAuras.end();)
    {
        Aura const* aura = i->second->GetBase();
        if (!i->second->IsPositive() && aura->GetCasterGUID() == GetGUID() && aura->GetApplyTime() >= duel->startTime)
            duel->opponent->RemoveAura(i);
        else
            ++i;
    }

    AuraApplicationMap &myAuras = GetAppliedAuras();
    for (AuraApplicationMap::iterator i = myAuras.begin(); i != myAuras.end();)
    {
        Aura const* aura = i->second->GetBase();
        if (!i->second->IsPositive() && aura->GetCasterGUID() == duel->opponent->GetGUID() && aura->GetApplyTime() >= duel->startTime)
            RemoveAura(i);
        else
            ++i;
    }

    // cleanup combo points
    if (GetComboTarget() == duel->opponent->GetGUID())
        ClearComboPoints();
    else if (GetComboTarget() == duel->opponent->GetPetGUID())
        ClearComboPoints();

    if (duel->opponent->GetComboTarget() == GetGUID())
        duel->opponent->ClearComboPoints();
    else if (duel->opponent->GetComboTarget() == GetPetGUID())
        duel->opponent->ClearComboPoints();

    // Honor points after duel (the winner) - ImpConfig
    if (uint32 amount = sWorld->getIntConfig(CONFIG_HONOR_AFTER_DUEL))
        duel->opponent->RewardHonor(NULL, 1, amount);

    //cleanups
    SetUInt64Value(PLAYER_DUEL_ARBITER, 0);
    SetUInt32Value(PLAYER_DUEL_TEAM, 0);
    duel->opponent->SetUInt64Value(PLAYER_DUEL_ARBITER, 0);
    duel->opponent->SetUInt32Value(PLAYER_DUEL_TEAM, 0);

    delete duel->opponent->duel;
    duel->opponent->duel = NULL;
    delete duel;
    duel = NULL;
}

//---------------------------------------------------------//

void Player::_ApplyItemMods(Item* item, uint8 slot, bool apply)
{
    if (slot >= INVENTORY_SLOT_BAG_END || !item)
        return;

    ItemTemplate const* proto = item->GetTemplate();

    if (!proto)
        return;

    // not apply/remove mods for broken item
    if (item->IsBroken())
        return;

    sLog->outInfo(LOG_FILTER_PLAYER_ITEMS, "applying mods for item %u ", item->GetGUIDLow());

    uint8 attacktype = Player::GetAttackBySlot(slot);

    if (proto->Socket[0].Color)                              //only (un)equipping of items with sockets can influence metagems, so no need to waste time with normal items
        CorrectMetaGemEnchants(slot, apply);

    if (attacktype < MAX_ATTACK)
        _ApplyWeaponDependentAuraMods(item, WeaponAttackType(attacktype), apply);

    _ApplyItemBonuses(proto, slot, apply);

    if (slot == EQUIPMENT_SLOT_RANGED)
        _ApplyAmmoBonuses();

    ApplyItemEquipSpell(item, apply);
    ApplyEnchantment(item, apply);

    sLog->outDebug(LOG_FILTER_PLAYER_ITEMS, "_ApplyItemMods complete.");
}

void Player::_ApplyItemBonuses(ItemTemplate const* proto, uint8 slot, bool apply, bool only_level_scale /*= false*/)
{
    if (slot >= INVENTORY_SLOT_BAG_END || !proto)
        return;

    ScalingStatDistributionEntry const* ssd = proto->ScalingStatDistribution ? sScalingStatDistributionStore.LookupEntry(proto->ScalingStatDistribution) : NULL;
    if (only_level_scale && !ssd)
        return;

    // req. check at equip, but allow use for extended range if range limit max level, set proper level
    uint32 ssd_level = getLevel();
    if (ssd && ssd_level > ssd->MaxLevel)
        ssd_level = ssd->MaxLevel;

    ScalingStatValuesEntry const* ssv = proto->ScalingStatValue ? sScalingStatValuesStore.LookupEntry(ssd_level) : NULL;
    if (only_level_scale && !ssv)
        return;

    for (uint8 i = 0; i < MAX_ITEM_PROTO_STATS; ++i)
    {
        uint32 statType = 0;
        int32  val = 0;
        // If set ScalingStatDistribution need get stats and values from it
        if (ssd && ssv)
        {
            if (ssd->StatMod[i] < 0)
                continue;
            statType = ssd->StatMod[i];
            val = (ssv->getssdMultiplier(proto->ScalingStatValue) * ssd->Modifier[i]) / 10000;
        }
        else
        {
            if (i >= proto->StatsCount)
                continue;
            statType = proto->ItemStat[i].ItemStatType;
            val = proto->ItemStat[i].ItemStatValue;
        }

        if (val == 0)
            continue;

        switch (statType)
        {
            case ITEM_MOD_MANA:
                HandleStatModifier(UNIT_MOD_MANA, BASE_VALUE, float(val), apply);
                break;
            case ITEM_MOD_HEALTH:                           // modify HP
                HandleStatModifier(UNIT_MOD_HEALTH, BASE_VALUE, float(val), apply);
                break;
            case ITEM_MOD_AGILITY:                          // modify agility
                HandleStatModifier(UNIT_MOD_STAT_AGILITY, BASE_VALUE, float(val), apply);
                ApplyStatBuffMod(STAT_AGILITY, float(val), apply);
                break;
            case ITEM_MOD_STRENGTH:                         //modify strength
                HandleStatModifier(UNIT_MOD_STAT_STRENGTH, BASE_VALUE, float(val), apply);
                ApplyStatBuffMod(STAT_STRENGTH, float(val), apply);
                break;
            case ITEM_MOD_INTELLECT:                        //modify intellect
                HandleStatModifier(UNIT_MOD_STAT_INTELLECT, BASE_VALUE, float(val), apply);
                ApplyStatBuffMod(STAT_INTELLECT, float(val), apply);
                break;
            case ITEM_MOD_SPIRIT:                           //modify spirit
                HandleStatModifier(UNIT_MOD_STAT_SPIRIT, BASE_VALUE, float(val), apply);
                ApplyStatBuffMod(STAT_SPIRIT, float(val), apply);
                break;
            case ITEM_MOD_STAMINA:                          //modify stamina
                HandleStatModifier(UNIT_MOD_STAT_STAMINA, BASE_VALUE, float(val), apply);
                ApplyStatBuffMod(STAT_STAMINA, float(val), apply);
                break;
            case ITEM_MOD_DEFENSE_SKILL_RATING:
                ApplyRatingMod(CR_DEFENSE_SKILL, int32(val), apply);
                break;
            case ITEM_MOD_DODGE_RATING:
                ApplyRatingMod(CR_DODGE, int32(val), apply);
                break;
            case ITEM_MOD_PARRY_RATING:
                ApplyRatingMod(CR_PARRY, int32(val), apply);
                break;
            case ITEM_MOD_BLOCK_RATING:
                ApplyRatingMod(CR_BLOCK, int32(val), apply);
                break;
            case ITEM_MOD_HIT_MELEE_RATING:
                ApplyRatingMod(CR_HIT_MELEE, int32(val), apply);
                break;
            case ITEM_MOD_HIT_RANGED_RATING:
                ApplyRatingMod(CR_HIT_RANGED, int32(val), apply);
                break;
            case ITEM_MOD_HIT_SPELL_RATING:
                ApplyRatingMod(CR_HIT_SPELL, int32(val), apply);
                break;
            case ITEM_MOD_CRIT_MELEE_RATING:
                ApplyRatingMod(CR_CRIT_MELEE, int32(val), apply);
                break;
            case ITEM_MOD_CRIT_RANGED_RATING:
                ApplyRatingMod(CR_CRIT_RANGED, int32(val), apply);
                break;
            case ITEM_MOD_CRIT_SPELL_RATING:
                ApplyRatingMod(CR_CRIT_SPELL, int32(val), apply);
                break;
            case ITEM_MOD_HIT_TAKEN_MELEE_RATING:
                ApplyRatingMod(CR_HIT_TAKEN_MELEE, int32(val), apply);
                break;
            case ITEM_MOD_HIT_TAKEN_RANGED_RATING:
                ApplyRatingMod(CR_HIT_TAKEN_RANGED, int32(val), apply);
                break;
            case ITEM_MOD_HIT_TAKEN_SPELL_RATING:
                ApplyRatingMod(CR_HIT_TAKEN_SPELL, int32(val), apply);
                break;
            case ITEM_MOD_CRIT_TAKEN_MELEE_RATING:
                ApplyRatingMod(CR_CRIT_TAKEN_MELEE, int32(val), apply);
                break;
            case ITEM_MOD_CRIT_TAKEN_RANGED_RATING:
                ApplyRatingMod(CR_CRIT_TAKEN_RANGED, int32(val), apply);
                break;
            case ITEM_MOD_CRIT_TAKEN_SPELL_RATING:
                ApplyRatingMod(CR_CRIT_TAKEN_SPELL, int32(val), apply);
                break;
            case ITEM_MOD_HASTE_MELEE_RATING:
                ApplyRatingMod(CR_HASTE_MELEE, int32(val), apply);
                break;
            case ITEM_MOD_HASTE_RANGED_RATING:
                ApplyRatingMod(CR_HASTE_RANGED, int32(val), apply);
                break;
            case ITEM_MOD_HASTE_SPELL_RATING:
                ApplyRatingMod(CR_HASTE_SPELL, int32(val), apply);
                break;
            case ITEM_MOD_HIT_RATING:
                ApplyRatingMod(CR_HIT_MELEE, int32(val), apply);
                ApplyRatingMod(CR_HIT_RANGED, int32(val), apply);
                ApplyRatingMod(CR_HIT_SPELL, int32(val), apply);
                break;
            case ITEM_MOD_CRIT_RATING:
                ApplyRatingMod(CR_CRIT_MELEE, int32(val), apply);
                ApplyRatingMod(CR_CRIT_RANGED, int32(val), apply);
                ApplyRatingMod(CR_CRIT_SPELL, int32(val), apply);
                break;
            case ITEM_MOD_HIT_TAKEN_RATING:
                ApplyRatingMod(CR_HIT_TAKEN_MELEE, int32(val), apply);
                ApplyRatingMod(CR_HIT_TAKEN_RANGED, int32(val), apply);
                ApplyRatingMod(CR_HIT_TAKEN_SPELL, int32(val), apply);
                break;
            case ITEM_MOD_CRIT_TAKEN_RATING:
                ApplyRatingMod(CR_CRIT_TAKEN_MELEE, int32(val), apply);
                ApplyRatingMod(CR_CRIT_TAKEN_RANGED, int32(val), apply);
                ApplyRatingMod(CR_CRIT_TAKEN_SPELL, int32(val), apply);
                break;
            case ITEM_MOD_RESILIENCE_RATING:
                ApplyRatingMod(CR_CRIT_TAKEN_MELEE, int32(val), apply);
                ApplyRatingMod(CR_CRIT_TAKEN_RANGED, int32(val), apply);
                ApplyRatingMod(CR_CRIT_TAKEN_SPELL, int32(val), apply);
                break;
            case ITEM_MOD_HASTE_RATING:
                ApplyRatingMod(CR_HASTE_MELEE, int32(val), apply);
                ApplyRatingMod(CR_HASTE_RANGED, int32(val), apply);
                ApplyRatingMod(CR_HASTE_SPELL, int32(val), apply);
                break;
            case ITEM_MOD_EXPERTISE_RATING:
                ApplyRatingMod(CR_EXPERTISE, int32(val), apply);
                break;
            case ITEM_MOD_ATTACK_POWER:
                HandleStatModifier(UNIT_MOD_ATTACK_POWER, TOTAL_VALUE, float(val), apply);
                HandleStatModifier(UNIT_MOD_ATTACK_POWER_RANGED, TOTAL_VALUE, float(val), apply);
                break;
            case ITEM_MOD_RANGED_ATTACK_POWER:
                HandleStatModifier(UNIT_MOD_ATTACK_POWER_RANGED, TOTAL_VALUE, float(val), apply);
                break;
//            case ITEM_MOD_FERAL_ATTACK_POWER:
//                ApplyFeralAPBonus(int32(val), apply);
//                break;
            case ITEM_MOD_MANA_REGENERATION:
                ApplyManaRegenBonus(int32(val), apply);
                break;
            case ITEM_MOD_ARMOR_PENETRATION_RATING:
                ApplyRatingMod(CR_ARMOR_PENETRATION, int32(val), apply);
                break;
            case ITEM_MOD_SPELL_POWER:
                ApplySpellPowerBonus(int32(val), apply);
                break;
            case ITEM_MOD_HEALTH_REGEN:
                ApplyHealthRegenBonus(int32(val), apply);
                break;
            case ITEM_MOD_SPELL_PENETRATION:
                ApplyModInt32Value(PLAYER_FIELD_MOD_TARGET_RESISTANCE, -val, apply);
                m_spellPenetrationItemMod += apply ? val : -val;
                break;
            case ITEM_MOD_BLOCK_VALUE:
                HandleBaseModValue(SHIELD_BLOCK_VALUE, FLAT_MOD, float(val), apply);
                break;
            // deprecated item mods
            case ITEM_MOD_SPELL_HEALING_DONE:
            case ITEM_MOD_SPELL_DAMAGE_DONE:
                break;
        }
    }

    // Apply Spell Power from ScalingStatValue if set
    if (ssv)
        if (int32 spellbonus = ssv->getSpellBonus(proto->ScalingStatValue))
            ApplySpellPowerBonus(spellbonus, apply);

    // If set ScalingStatValue armor get it or use item armor
    uint32 armor = proto->Armor;
    if (ssv)
        if (uint32 ssvarmor = ssv->getArmorMod(proto->ScalingStatValue))
            armor = ssvarmor;

    if (armor)
    {
        UnitModifierType modType = TOTAL_VALUE;
        if (proto->Class == ITEM_CLASS_ARMOR)
        {
            switch (proto->SubClass)
            {
                case ITEM_SUBCLASS_ARMOR_CLOTH:
                case ITEM_SUBCLASS_ARMOR_LEATHER:
                case ITEM_SUBCLASS_ARMOR_MAIL:
                case ITEM_SUBCLASS_ARMOR_PLATE:
                case ITEM_SUBCLASS_ARMOR_SHIELD:
                    modType = BASE_VALUE;
                break;
            }
        }
        HandleStatModifier(UNIT_MOD_ARMOR, modType, float(armor), apply);
    }

    // Add armor bonus from ArmorDamageModifier if > 0
    if (proto->ArmorDamageModifier > 0)
        HandleStatModifier(UNIT_MOD_ARMOR, TOTAL_VALUE, float(proto->ArmorDamageModifier), apply);

    if (proto->Block)
        HandleBaseModValue(SHIELD_BLOCK_VALUE, FLAT_MOD, float(proto->Block), apply);

    if (proto->HolyRes)
        HandleStatModifier(UNIT_MOD_RESISTANCE_HOLY, BASE_VALUE, float(proto->HolyRes), apply);

    if (proto->FireRes)
        HandleStatModifier(UNIT_MOD_RESISTANCE_FIRE, BASE_VALUE, float(proto->FireRes), apply);

    if (proto->NatureRes)
        HandleStatModifier(UNIT_MOD_RESISTANCE_NATURE, BASE_VALUE, float(proto->NatureRes), apply);

    if (proto->FrostRes)
        HandleStatModifier(UNIT_MOD_RESISTANCE_FROST, BASE_VALUE, float(proto->FrostRes), apply);

    if (proto->ShadowRes)
        HandleStatModifier(UNIT_MOD_RESISTANCE_SHADOW, BASE_VALUE, float(proto->ShadowRes), apply);

    if (proto->ArcaneRes)
        HandleStatModifier(UNIT_MOD_RESISTANCE_ARCANE, BASE_VALUE, float(proto->ArcaneRes), apply);

    WeaponAttackType attType = BASE_ATTACK;

    if (slot == EQUIPMENT_SLOT_RANGED && (
        proto->InventoryType == INVTYPE_RANGED || proto->InventoryType == INVTYPE_THROWN ||
        proto->InventoryType == INVTYPE_RANGEDRIGHT))
    {
        attType = RANGED_ATTACK;
    }
    else if (slot == EQUIPMENT_SLOT_OFFHAND)
    {
        attType = OFF_ATTACK;
    }

    if (CanUseAttackType(attType))
        _ApplyWeaponDamage(slot, proto, ssv, apply);


    // Druids get feral AP bonus from weapon dps (also use DPS from ScalingStatValue)
    if (getClass() == CLASS_DRUID)
    {
        int32 dpsMod = 0;
        int32 feral_bonus = 0;
        if (ssv)
        {
            dpsMod = ssv->getDPSMod(proto->ScalingStatValue);
            feral_bonus += ssv->getFeralBonus(proto->ScalingStatValue);
        }

        feral_bonus += proto->getFeralBonus(dpsMod);
        if (feral_bonus)
            ApplyFeralAPBonus(feral_bonus, apply);
    }
}

void Player::_ApplyWeaponDamage(uint8 slot, ItemTemplate const* proto, ScalingStatValuesEntry const* ssv, bool apply)
{
    WeaponAttackType attType = BASE_ATTACK;
    float damage = 0.0f;

    if (slot == EQUIPMENT_SLOT_RANGED && (
        proto->InventoryType == INVTYPE_RANGED || proto->InventoryType == INVTYPE_THROWN ||
        proto->InventoryType == INVTYPE_RANGEDRIGHT))
    {
        attType = RANGED_ATTACK;
    }
    else if (slot == EQUIPMENT_SLOT_OFFHAND)
    {
        attType = OFF_ATTACK;
    }

    float minDamage = proto->Damage[0].DamageMin;
    float maxDamage = proto->Damage[0].DamageMax;

    // If set dpsMod in ScalingStatValue use it for min (70% from average), max (130% from average) damage
    if (ssv)
    {
        int32 extraDPS = ssv->getDPSMod(proto->ScalingStatValue);
        if (extraDPS)
        {
            float average = extraDPS * proto->Delay / 1000.0f;
            minDamage = 0.7f * average;
            maxDamage = 1.3f * average;
        }
    }

    if (minDamage > 0)
    {
        damage = apply ? minDamage : BASE_MINDAMAGE;
        SetBaseWeaponDamage(attType, MINDAMAGE, damage);
    }

    if (maxDamage  > 0)
    {
        damage = apply ? maxDamage : BASE_MAXDAMAGE;
        SetBaseWeaponDamage(attType, MAXDAMAGE, damage);
    }

    if (proto->Delay && !IsInFeralForm())
    {
        if (slot == EQUIPMENT_SLOT_RANGED)
            SetAttackTime(RANGED_ATTACK, apply ? proto->Delay: BASE_ATTACK_TIME);
        else if (slot == EQUIPMENT_SLOT_MAINHAND)
            SetAttackTime(BASE_ATTACK, apply ? proto->Delay: BASE_ATTACK_TIME);
        else if (slot == EQUIPMENT_SLOT_OFFHAND)
            SetAttackTime(OFF_ATTACK, apply ? proto->Delay: BASE_ATTACK_TIME);
    }

    // No need to modify any physical damage for ferals as it is calculated from stats only
    if (IsInFeralForm())
        return;

    if (CanModifyStats() && (damage || proto->Delay))
        UpdateDamagePhysical(attType);
}

void Player::_ApplyWeaponDependentAuraMods(Item* item, WeaponAttackType attackType, bool apply)
{
    AuraEffectList const& auraCritList = GetAuraEffectsByType(SPELL_AURA_MOD_WEAPON_CRIT_PERCENT);
    for (AuraEffectList::const_iterator itr = auraCritList.begin(); itr != auraCritList.end(); ++itr)
        _ApplyWeaponDependentAuraCritMod(item, attackType, *itr, apply);

    AuraEffectList const& auraDamageFlatList = GetAuraEffectsByType(SPELL_AURA_MOD_DAMAGE_DONE);
    for (AuraEffectList::const_iterator itr = auraDamageFlatList.begin(); itr != auraDamageFlatList.end(); ++itr)
        _ApplyWeaponDependentAuraDamageMod(item, attackType, *itr, apply);

    AuraEffectList const& auraDamagePctList = GetAuraEffectsByType(SPELL_AURA_MOD_DAMAGE_PERCENT_DONE);
    for (AuraEffectList::const_iterator itr = auraDamagePctList.begin(); itr != auraDamagePctList.end(); ++itr)
        _ApplyWeaponDependentAuraDamageMod(item, attackType, *itr, apply);
}

void Player::_ApplyWeaponDependentAuraCritMod(Item* item, WeaponAttackType attackType, AuraEffect const* aura, bool apply)
{
    // don't apply mod if item is broken or cannot be used
    if (item->IsBroken() || !CanUseAttackType(attackType))
        return;

    // generic not weapon specific case processes in aura code
    if (aura->GetSpellInfo()->EquippedItemClass == -1)
        return;

    BaseModGroup mod = BASEMOD_END;
    switch (attackType)
    {
        case BASE_ATTACK:   mod = CRIT_PERCENTAGE;        break;
        case OFF_ATTACK:    mod = OFFHAND_CRIT_PERCENTAGE;break;
        case RANGED_ATTACK: mod = RANGED_CRIT_PERCENTAGE; break;
        default: return;
    }

    if (item->IsFitToSpellRequirements(aura->GetSpellInfo()))
        HandleBaseModValue(mod, FLAT_MOD, float (aura->GetAmount()), apply);
}

void Player::_ApplyWeaponDependentAuraDamageMod(Item* item, WeaponAttackType attackType, AuraEffect const* aura, bool apply)
{
    // don't apply mod if item is broken or cannot be used
    if (item->IsBroken() || !CanUseAttackType(attackType))
        return;

    // ignore spell mods for not wands
    if ((aura->GetMiscValue() & SPELL_SCHOOL_MASK_NORMAL) == 0 && (getClassMask() & CLASSMASK_WAND_USERS) == 0)
        return;

    // generic not weapon specific case processes in aura code
    if (aura->GetSpellInfo()->EquippedItemClass == -1)
        return;

    UnitMods unitMod = UNIT_MOD_END;
    switch (attackType)
    {
        case BASE_ATTACK:   unitMod = UNIT_MOD_DAMAGE_MAINHAND; break;
        case OFF_ATTACK:    unitMod = UNIT_MOD_DAMAGE_OFFHAND;  break;
        case RANGED_ATTACK: unitMod = UNIT_MOD_DAMAGE_RANGED;   break;
        default: return;
    }

    UnitModifierType unitModType = TOTAL_VALUE;
    switch (aura->GetAuraType())
    {
        case SPELL_AURA_MOD_DAMAGE_DONE:         unitModType = TOTAL_VALUE; break;
        case SPELL_AURA_MOD_DAMAGE_PERCENT_DONE: unitModType = TOTAL_PCT;   break;
        default: return;
    }

    if (item->IsFitToSpellRequirements(aura->GetSpellInfo()))
    {
        HandleStatModifier(unitMod, unitModType, float(aura->GetAmount()), apply);
        if (unitModType == TOTAL_VALUE)
            ApplyModUInt32Value(PLAYER_FIELD_MOD_DAMAGE_DONE_POS, aura->GetAmount(), apply);
    }
}

void Player::ApplyItemEquipSpell(Item* item, bool apply, bool form_change)
{
    if (!item)
        return;

    ItemTemplate const* proto = item->GetTemplate();
    if (!proto)
        return;

    for (uint8 i = 0; i < MAX_ITEM_PROTO_SPELLS; ++i)
    {
        _Spell const& spellData = proto->Spells[i];

        // no spell
        if (!spellData.SpellId)
            continue;

        // wrong triggering type
        if (apply && spellData.SpellTrigger != ITEM_SPELLTRIGGER_ON_EQUIP)
            continue;

        // check if it is valid spell
        SpellInfo const* spellproto = sSpellMgr->GetSpellInfo(spellData.SpellId);
        if (!spellproto)
            continue;

        ApplyEquipSpell(spellproto, item, apply, form_change);
    }
}

void Player::ApplyEquipSpell(SpellInfo const* spellInfo, Item* item, bool apply, bool form_change)
{
    if (apply)
    {
        // Cannot be used in this stance/form
        if (spellInfo->CheckShapeshift(GetShapeshiftForm()) != SPELL_CAST_OK)
            return;

        if (form_change)                                    // check aura active state from other form
        {
            AuraApplicationMap const& auras = GetAppliedAuras();
            for (AuraApplicationMap::const_iterator itr = auras.lower_bound(spellInfo->Id); itr != auras.upper_bound(spellInfo->Id); ++itr)
                if (!item || itr->second->GetBase()->GetCastItemGUID() == item->GetGUID())
                    return;
        }

        sLog->outDebug(LOG_FILTER_PLAYER, "WORLD: cast %s Equip spellId - %i", (item ? "item" : "itemset"), spellInfo->Id);

        CastSpell(this, spellInfo, true, item);
    }
    else
    {
        if (form_change)                                     // check aura compatibility
        {
            // Cannot be used in this stance/form
            if (spellInfo->CheckShapeshift(GetShapeshiftForm()) == SPELL_CAST_OK)
                return;                                     // and remove only not compatible at form change
        }

        if (item)
            RemoveAurasDueToItemSpell(item, spellInfo->Id);  // un-apply all spells, not only at-equipped
        else
            RemoveAurasDueToSpell(spellInfo->Id);           // un-apply spell (item set case)
    }
}

void Player::UpdateEquipSpellsAtFormChange()
{
    for (uint8 i = 0; i < INVENTORY_SLOT_BAG_END; ++i)
    {
        if (m_items[i] && !m_items[i]->IsBroken() && CanUseAttackType(GetAttackBySlot(i)))
        {
            ApplyItemEquipSpell(m_items[i], false, true);     // remove spells that not fit to form
            ApplyItemEquipSpell(m_items[i], true, true);      // add spells that fit form but not active
        }
    }

    // item set bonuses not dependent from item broken state
    for (size_t setindex = 0; setindex < ItemSetEff.size(); ++setindex)
    {
        ItemSetEffect* eff = ItemSetEff[setindex];
        if (!eff)
            continue;

        for (uint32 y = 0; y < MAX_ITEM_SET_SPELLS; ++y)
        {
            SpellInfo const* spellInfo = eff->spells[y];
            if (!spellInfo)
                continue;

            ApplyEquipSpell(spellInfo, NULL, false, true);       // remove spells that not fit to form
            ApplyEquipSpell(spellInfo, NULL, true, true);        // add spells that fit form but not active
        }
    }
}
void Player::CastItemCombatSpell(Unit* target, WeaponAttackType attType, uint32 procVictim, uint32 procEx)
{
    if (!target || !target->isAlive() || target == this)
        return;

    for (uint8 i = EQUIPMENT_SLOT_START; i < EQUIPMENT_SLOT_END; ++i)
    {
        // If usable, try to cast item spell
        if (Item* item = GetItemByPos(INVENTORY_SLOT_BAG_0, i))
            if (!item->IsBroken() && CanUseAttackType(attType))
                if (ItemTemplate const* proto = item->GetTemplate())
                {
                    // Additional check for weapons
                    if (proto->Class == ITEM_CLASS_WEAPON)
                    {
                        // offhand item cannot proc from main hand hit etc
                        EquipmentSlots slot;
                        switch (attType)
                        {
                            case BASE_ATTACK:   slot = EQUIPMENT_SLOT_MAINHAND; break;
                            case OFF_ATTACK:    slot = EQUIPMENT_SLOT_OFFHAND;  break;
                            case RANGED_ATTACK: slot = EQUIPMENT_SLOT_RANGED;   break;
                            default: slot = EQUIPMENT_SLOT_END; break;
                        }
                        if (slot != i)
                            continue;
                        // Check if item is useable (forms or disarm)
                        if (attType == BASE_ATTACK)
                            if (!IsUseEquipedWeapon(true) && !IsInFeralForm())
                                continue;
                    }
                    CastItemCombatSpell(target, attType, procVictim, procEx, item, proto);
                }
    }
}

void Player::CastItemCombatSpell(Unit* target, WeaponAttackType attType, uint32 procVictim, uint32 procEx, Item* item, ItemTemplate const* proto)
{
    // Can do effect if any damage done to target
    if (procVictim & PROC_FLAG_TAKEN_DAMAGE)
    //if (damageInfo->procVictim & PROC_FLAG_TAKEN_ANY_DAMAGE)
    {
        for (uint8 i = 0; i < MAX_ITEM_SPELLS; ++i)
        {
            _Spell const& spellData = proto->Spells[i];

            // no spell
            if (!spellData.SpellId)
                continue;

            // wrong triggering type
            if (spellData.SpellTrigger != ITEM_SPELLTRIGGER_CHANCE_ON_HIT)
                continue;

            SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellData.SpellId);
            if (!spellInfo)
            {
                sLog->outError(LOG_FILTER_PLAYER_ITEMS, "WORLD: unknown Item spellid %i", spellData.SpellId);
                continue;
            }

            // not allow proc extra attack spell at extra attack
            if (m_extraAttacks && spellInfo->HasEffect(SPELL_EFFECT_ADD_EXTRA_ATTACKS))
                return;

            float chance = (float)spellInfo->ProcChance;

            if (spellData.SpellPPMRate)
            {
                if (spellData.SpellId == 52781) // Persuasive Strike
                {
                    switch (target->GetEntry())
                    {
                        default:
                            return;
                        case 28939:
                        case 28940:
                        case 28610:
                            break;
                    }
                }
                uint32 WeaponSpeed = GetAttackTime(attType);
                chance = GetPPMProcChance(WeaponSpeed, spellData.SpellPPMRate, spellInfo);
            }
            else if (chance > 100.0f)
            {
                chance = GetWeaponProcChance();
            }

            if (roll_chance_f(chance))
                CastSpell(target, spellInfo->Id, true, item);
        }
    }

    // item combat enchantments
    for (uint8 e_slot = 0; e_slot < MAX_ENCHANTMENT_SLOT; ++e_slot)
    {
        uint32 enchant_id = item->GetEnchantmentId(EnchantmentSlot(e_slot));
        SpellItemEnchantmentEntry const* pEnchant = sSpellItemEnchantmentStore.LookupEntry(enchant_id);
        if (!pEnchant)
            continue;

        for (uint8 s = 0; s < MAX_ITEM_ENCHANTMENT_EFFECTS; ++s)
        {
            if (pEnchant->type[s] != ITEM_ENCHANTMENT_TYPE_COMBAT_SPELL)
                continue;

            SpellEnchantProcEntry const* entry = sSpellMgr->GetSpellEnchantProcEvent(enchant_id);

            if (entry && entry->procEx)
            {
                // Check hit/crit/dodge/parry requirement
                if ((entry->procEx & procEx) == 0)
                    continue;
            }
            else
            {
                // Can do effect if any damage done to target
                if (!(procVictim & PROC_FLAG_TAKEN_DAMAGE))
                //if (!(damageInfo->procVictim & PROC_FLAG_TAKEN_ANY_DAMAGE))
                    continue;
            }

            SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(pEnchant->spellid[s]);
            if (!spellInfo)
            {
                sLog->outError(LOG_FILTER_PLAYER_ITEMS, "Player::CastItemCombatSpell(GUID: %u, name: %s, enchant: %i): unknown spell %i is casted, ignoring...",
                    GetGUIDLow(), GetName(), pEnchant->ID, pEnchant->spellid[s]);
                continue;
            }

            float chance = pEnchant->amount[s] != 0 ? float(pEnchant->amount[s]) : GetWeaponProcChance();

            if (entry)
            {
                if (entry->PPMChance)
                    chance = GetPPMProcChance(proto->Delay, entry->PPMChance, spellInfo);
                else if (entry->customChance)
                    chance = (float)entry->customChance;
            }

            // Apply spell mods
            ApplySpellMod(pEnchant->spellid[s], SPELLMOD_CHANCE_OF_SUCCESS, chance);

            // Shiv has 100% chance to apply the poison
            if (FindCurrentSpellBySpellId(5938) && e_slot == TEMP_ENCHANTMENT_SLOT)
                chance = 100.0f;

            if (roll_chance_f(chance))
            {
                if (spellInfo->IsPositive())
                    CastSpell(this, spellInfo, true, item);
                else
                    CastSpell(target, spellInfo, true, item);
            }
        }
    }
}

void Player::CastItemUseSpell(Item* item, SpellCastTargets const& targets, uint8 cast_count, uint32 glyphIndex)
{
    ItemTemplate const* proto = item->GetTemplate();
    // special learning case
    if (proto->Spells[0].SpellId == 483 || proto->Spells[0].SpellId == 55884)
    {
        uint32 learn_spell_id = proto->Spells[0].SpellId;
        uint32 learning_spell_id = proto->Spells[1].SpellId;

        SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(learn_spell_id);
        if (!spellInfo)
        {
            sLog->outError(LOG_FILTER_PLAYER, "Player::CastItemUseSpell: Item (Entry: %u) in have wrong spell id %u, ignoring ", proto->ItemId, learn_spell_id);
            SendEquipError(EQUIP_ERR_NONE, item, NULL);
            return;
        }

        Spell* spell = new Spell(this, spellInfo, TRIGGERED_NONE);
        spell->m_CastItem = item;
        spell->m_cast_count = cast_count;                   //set count of casts
        spell->SetSpellValue(SPELLVALUE_BASE_POINT0, learning_spell_id);
        spell->prepare(&targets);
        return;
    }

    // use triggered flag only for items with many spell casts and for not first cast
    uint8 count = 0;

    // item spells casted at use
    for (uint8 i = 0; i < MAX_ITEM_PROTO_SPELLS; ++i)
    {
        _Spell const& spellData = proto->Spells[i];

        // no spell
        if (!spellData.SpellId)
            continue;

        // wrong triggering type
        if (spellData.SpellTrigger != ITEM_SPELLTRIGGER_ON_USE)
            continue;

        SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellData.SpellId);
        if (!spellInfo)
        {
            sLog->outError(LOG_FILTER_PLAYER, "Player::CastItemUseSpell: Item (Entry: %u) in have wrong spell id %u, ignoring", proto->ItemId, spellData.SpellId);
            continue;
        }

        Spell* spell = new Spell(this, spellInfo, (count > 0) ? TRIGGERED_FULL_MASK : TRIGGERED_NONE);
        spell->m_CastItem = item;
        spell->m_cast_count = cast_count;                   // set count of casts
        spell->m_glyphIndex = glyphIndex;                   // glyph index
        spell->prepare(&targets);

        ++count;
    }

    // Item enchantments spells casted at use
    for (uint8 e_slot = 0; e_slot < MAX_ENCHANTMENT_SLOT; ++e_slot)
    {
        uint32 enchant_id = item->GetEnchantmentId(EnchantmentSlot(e_slot));
        SpellItemEnchantmentEntry const* pEnchant = sSpellItemEnchantmentStore.LookupEntry(enchant_id);
        if (!pEnchant)
            continue;
        for (uint8 s = 0; s < MAX_ITEM_ENCHANTMENT_EFFECTS; ++s)
        {
            if (pEnchant->type[s] != ITEM_ENCHANTMENT_TYPE_USE_SPELL)
                continue;

            SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(pEnchant->spellid[s]);
            if (!spellInfo)
            {
                sLog->outError(LOG_FILTER_PLAYER, "Player::CastItemUseSpell Enchant %i, cast unknown spell %i", pEnchant->ID, pEnchant->spellid[s]);
                continue;
            }

            Spell* spell = new Spell(this, spellInfo, (count > 0) ? TRIGGERED_FULL_MASK : TRIGGERED_NONE);
            spell->m_CastItem = item;
            spell->m_cast_count = cast_count;               // set count of casts
            spell->m_glyphIndex = glyphIndex;               // glyph index
            spell->prepare(&targets);

            ++count;
        }
    }
}

void Player::_RemoveAllItemMods()
{
    sLog->outDebug(LOG_FILTER_PLAYER_ITEMS, "_RemoveAllItemMods start.");

    for (uint8 i = 0; i < INVENTORY_SLOT_BAG_END; ++i)
    {
        if (m_items[i])
        {
            ItemTemplate const* proto = m_items[i]->GetTemplate();
            if (!proto)
                continue;

            // item set bonuses not dependent from item broken state
            if (proto->ItemSet)
                RemoveItemsSetItem(this, proto);

            if (m_items[i]->IsBroken() || !CanUseAttackType(GetAttackBySlot(i)))
                continue;

            ApplyItemEquipSpell(m_items[i], false);
            ApplyEnchantment(m_items[i], false);
        }
    }

    for (uint8 i = 0; i < INVENTORY_SLOT_BAG_END; ++i)
    {
        if (m_items[i])
        {
            if (m_items[i]->IsBroken() || !CanUseAttackType(GetAttackBySlot(i)))
                continue;
            ItemTemplate const* proto = m_items[i]->GetTemplate();
            if (!proto)
                continue;

            uint32 attacktype = Player::GetAttackBySlot(i);
            if (attacktype < MAX_ATTACK)
                _ApplyWeaponDependentAuraMods(m_items[i], WeaponAttackType(attacktype), false);

            _ApplyItemBonuses(proto, i, false);

            if (i == EQUIPMENT_SLOT_RANGED)
                _ApplyAmmoBonuses();
        }
    }

    sLog->outDebug(LOG_FILTER_PLAYER_ITEMS, "_RemoveAllItemMods complete.");
}

void Player::_ApplyAllItemMods()
{
    sLog->outDebug(LOG_FILTER_PLAYER_ITEMS, "_ApplyAllItemMods start.");

    for (uint8 i = 0; i < INVENTORY_SLOT_BAG_END; ++i)
    {
        if (m_items[i])
        {
            if (m_items[i]->IsBroken() || !CanUseAttackType(GetAttackBySlot(i)))
                continue;

            ItemTemplate const* proto = m_items[i]->GetTemplate();
            if (!proto)
                continue;

            uint32 attacktype = Player::GetAttackBySlot(i);
            if (attacktype < MAX_ATTACK)
                _ApplyWeaponDependentAuraMods(m_items[i], WeaponAttackType(attacktype), true);

            _ApplyItemBonuses(proto, i, true);

            if (i == EQUIPMENT_SLOT_RANGED)
                _ApplyAmmoBonuses();
        }
    }

    for (uint8 i = 0; i < INVENTORY_SLOT_BAG_END; ++i)
    {
        if (m_items[i])
        {
            ItemTemplate const* proto = m_items[i]->GetTemplate();
            if (!proto)
                continue;

            // item set bonuses not dependent from item broken state
            if (proto->ItemSet)
                AddItemsSetItem(this, m_items[i]);

            if (m_items[i]->IsBroken() || !CanUseAttackType(GetAttackBySlot(i)))
                continue;

            ApplyItemEquipSpell(m_items[i], true);
            ApplyEnchantment(m_items[i], true);
        }
    }

    sLog->outDebug(LOG_FILTER_PLAYER_ITEMS, "_ApplyAllItemMods complete.");
}

void Player::_ApplyAllLevelScaleItemMods(bool apply)
{
    for (uint8 i = 0; i < INVENTORY_SLOT_BAG_END; ++i)
    {
        if (m_items[i])
        {
            if (m_items[i]->IsBroken() || !CanUseAttackType(GetAttackBySlot(i)))
                continue;

            ItemTemplate const* proto = m_items[i]->GetTemplate();
            if (!proto)
                continue;

            _ApplyItemBonuses(proto, i, apply, true);
        }
    }
}

void Player::_ApplyAmmoBonuses()
{
    // check ammo
    uint32 ammo_id = GetUInt32Value(PLAYER_AMMO_ID);
    if (!ammo_id)
        return;

    float currentAmmoDPS;

    ItemTemplate const* ammo_proto = sObjectMgr->GetItemTemplate(ammo_id);
    if (!ammo_proto || ammo_proto->Class != ITEM_CLASS_PROJECTILE || !CheckAmmoCompatibility(ammo_proto))
        currentAmmoDPS = 0.0f;
    else
        currentAmmoDPS = (ammo_proto->Damage[0].DamageMin + ammo_proto->Damage[0].DamageMax) / 2;

    if (currentAmmoDPS == GetAmmoDPS())
        return;

    m_ammoDPS = currentAmmoDPS;

    if (CanModifyStats())
        UpdateDamagePhysical(RANGED_ATTACK);
}

bool Player::CheckAmmoCompatibility(const ItemTemplate* ammo_proto) const
{
    if (!ammo_proto)
        return false;

    // check ranged weapon
    Item* weapon = GetWeaponForAttack(RANGED_ATTACK);
    if (!weapon  || weapon->IsBroken())
        return false;

    ItemTemplate const* weapon_proto = weapon->GetTemplate();
    if (!weapon_proto || weapon_proto->Class != ITEM_CLASS_WEAPON)
        return false;

    // check ammo ws. weapon compatibility
    switch (weapon_proto->SubClass)
    {
        case ITEM_SUBCLASS_WEAPON_BOW:
        case ITEM_SUBCLASS_WEAPON_CROSSBOW:
            if (ammo_proto->SubClass != ITEM_SUBCLASS_ARROW)
                return false;
            break;
        case ITEM_SUBCLASS_WEAPON_GUN:
            if (ammo_proto->SubClass != ITEM_SUBCLASS_BULLET)
                return false;
            break;
        default:
            return false;
    }

    return true;
}

/*  If in a battleground a player dies, and an enemy removes the insignia, the player's bones is lootable
    Called by remove insignia spell effect    */
void Player::RemovedInsignia(Player* looterPlr)
{
    if (!GetBattlegroundId())
        return;

    // If not released spirit, do it !
    if (m_deathTimer > 0)
    {
        m_deathTimer = 0;
        BuildPlayerRepop();
        RepopAtGraveyard();
    }

    // We have to convert player corpse to bones, not to be able to resurrect there
    // SpawnCorpseBones isn't handy, 'cos it saves player while he in BG
    Corpse* bones = sObjectAccessor->ConvertCorpseForPlayer(GetGUID(), true);
    if (!bones)
        return;

    // Now we must make bones lootable, and send player loot
    bones->SetFlag(CORPSE_FIELD_DYNAMIC_FLAGS, CORPSE_DYNFLAG_LOOTABLE);

    // We store the level of our player in the gold field
    // We retrieve this information at Player::SendLoot()
    bones->loot.gold = getLevel();
    bones->lootRecipient = looterPlr;
    looterPlr->SendLoot(bones->GetGUID(), LOOT_INSIGNIA);
}

void Player::SendLootRelease(uint64 guid)
{
    WorldPacket data(SMSG_LOOT_RELEASE_RESPONSE, (8+1));
    data << uint64(guid) << uint8(1);
    SendDirectMessage(&data);
}

void Player::SendLoot(uint64 guid, LootType loot_type)
{
    if (uint64 lguid = GetLootGUID())
        m_session->DoLootRelease(lguid);

    Loot* loot = 0;
    PermissionTypes permission = ALL_PERMISSION;

    sLog->outDebug(LOG_FILTER_LOOT, "Player::SendLoot");
    if (IS_GAMEOBJECT_GUID(guid))
    {
        sLog->outDebug(LOG_FILTER_LOOT, "IS_GAMEOBJECT_GUID(guid)");
        GameObject* go = GetMap()->GetGameObject(guid);

        // not check distance for GO in case owned GO (fishing bobber case, for example)
        // And permit out of range GO with no owner in case fishing hole
        if (!go || (loot_type != LOOT_FISHINGHOLE && (loot_type != LOOT_FISHING || go->GetOwnerGUID() != GetGUID()) && !go->IsWithinDistInMap(this, INTERACTION_DISTANCE)) || (loot_type == LOOT_CORPSE && go->GetRespawnTime() && go->isSpawnedByDefault()))
        {
            SendLootRelease(guid);
            return;
        }

        loot = &go->loot;

        if (go->getLootState() == GO_READY)
        {
            uint32 lootid = go->GetGOInfo()->GetLootId();

            //TODO: fix this big hack
            if ((go->GetEntry() == BG_AV_OBJECTID_MINE_N || go->GetEntry() == BG_AV_OBJECTID_MINE_S))
                if (Battleground* bg = GetBattleground())
                    if (bg->GetTypeID(true) == BATTLEGROUND_AV)
                        if (!(((BattlegroundAV*)bg)->PlayerCanDoMineQuest(go->GetEntry(), GetTeam())))
                        {
                            SendLootRelease(guid);
                            return;
                        }

            if (lootid)
            {
                loot->clear();

                Group* group = GetGroup();
                bool groupRules = (group && go->GetGOInfo()->type == GAMEOBJECT_TYPE_CHEST && go->GetGOInfo()->chest.groupLootRules);

                // check current RR player and get next if necessary
                if (groupRules)
                    group->UpdateLooterGuid(go, true);

                loot->FillLoot(lootid, LootTemplates_Gameobject, this, !groupRules, false, go->GetLootMode());

                // get next RR player (for next loot)
                if (groupRules)
                    group->UpdateLooterGuid(go);
            }

            if (loot_type == LOOT_FISHING)
                go->getFishLoot(loot, this);

            if (go->GetGOInfo()->type == GAMEOBJECT_TYPE_CHEST && go->GetGOInfo()->chest.groupLootRules)
            {
                if (Group* group = GetGroup())
                {
                    switch (group->GetLootMethod())
                    {
                        case GROUP_LOOT:
                            // GroupLoot: rolls items over threshold. Items with quality < threshold, round robin
                            group->GroupLoot(loot, go);
                            break;
                        case NEED_BEFORE_GREED:
                            group->NeedBeforeGreed(loot, go);
                            break;
                        case MASTER_LOOT:
                            group->MasterLoot(loot, go);
                            break;
                        default:
                            break;
                    }
                }
            }

            go->SetLootState(GO_ACTIVATED, this);
        }

        if (go->getLootState() == GO_ACTIVATED)
        {
            if (Group* group = GetGroup())
            {
                switch (group->GetLootMethod())
                {
                    case MASTER_LOOT:
                        permission = MASTER_PERMISSION;
                        break;
                    case FREE_FOR_ALL:
                        permission = ALL_PERMISSION;
                        break;
                    case ROUND_ROBIN:
                        permission = ROUND_ROBIN_PERMISSION;
                        break;
                    default:
                        permission = GROUP_PERMISSION;
                        break;
                }
            }
            else
                permission = ALL_PERMISSION;
        }
    }
    else if (IS_ITEM_GUID(guid))
    {
        Item* item = GetItemByGuid(guid);

        if (!item)
        {
            SendLootRelease(guid);
            return;
        }

        permission = OWNER_PERMISSION;

        loot = &item->loot;

        if (!item->m_lootGenerated)
        {
            item->m_lootGenerated = true;
            loot->clear();

            switch (loot_type)
            {
                case LOOT_DISENCHANTING:
                    loot->FillLoot(item->GetTemplate()->DisenchantID, LootTemplates_Disenchant, this, true);
                    break;
                case LOOT_PROSPECTING:
                    loot->FillLoot(item->GetEntry(), LootTemplates_Prospecting, this, true);
                    break;
                case LOOT_MILLING:
                    loot->FillLoot(item->GetEntry(), LootTemplates_Milling, this, true);
                    break;
                default:
                    loot->generateMoneyLoot(item->GetTemplate()->MinMoneyLoot, item->GetTemplate()->MaxMoneyLoot);
                    loot->FillLoot(item->GetEntry(), LootTemplates_Item, this, true, loot->gold != 0);
                    break;
            }
        }
    }
    else if (IS_CORPSE_GUID(guid))                          // remove insignia
    {
        Corpse* bones = ObjectAccessor::GetCorpse(*this, guid);

        if (!bones || !(loot_type == LOOT_CORPSE || loot_type == LOOT_INSIGNIA) || bones->GetType() != CORPSE_BONES)
        {
            SendLootRelease(guid);
            return;
        }

        loot = &bones->loot;

        if (!bones->lootForBody)
        {
            bones->lootForBody = true;
            uint32 pLevel = bones->loot.gold;
            bones->loot.clear();
            if (Battleground* bg = GetBattleground())
                if (bg->GetTypeID(true) == BATTLEGROUND_AV)
                    loot->FillLoot(1, LootTemplates_Creature, this, true);
            // It may need a better formula
            // Now it works like this: lvl10: ~6copper, lvl70: ~9silver
            bones->loot.gold = uint32(urand(50, 150) * 0.016f * pow(float(pLevel)/5.76f, 2.5f) * sWorld->getRate(RATE_DROP_MONEY));
        }

        if (bones->lootRecipient != this)
            permission = NONE_PERMISSION;
        else
            permission = OWNER_PERMISSION;
    }
    else
    {
        Creature* creature = GetMap()->GetCreature(guid);

        // must be in range and creature must be alive for pickpocket and must be dead for another loot
        if (!creature || creature->isAlive() != (loot_type == LOOT_PICKPOCKETING) || !creature->IsWithinDistInMap(this, INTERACTION_DISTANCE))
        {
            SendLootRelease(guid);
            return;
        }

        if (loot_type == LOOT_PICKPOCKETING && IsFriendlyTo(creature))
        {
            SendLootRelease(guid);
            return;
        }

        loot = &creature->loot;

        if (loot_type == LOOT_PICKPOCKETING)
        {
            if (!creature->lootForPickPocketed)
            {
                creature->lootForPickPocketed = true;
                loot->clear();

                if (uint32 lootid = creature->GetCreatureTemplate()->pickpocketLootId)
                    loot->FillLoot(lootid, LootTemplates_Pickpocketing, this, true);

                // Generate extra money for pick pocket loot
                const uint32 a = urand(0, creature->getLevel()/2);
                const uint32 b = urand(0, getLevel()/2);
                loot->gold = uint32(10 * (a + b) * sWorld->getRate(RATE_DROP_MONEY));
                permission = OWNER_PERMISSION;
            }
        }
        else
        {
            // the player whose group may loot the corpse
            Player* recipient = creature->GetLootRecipient();
            if (!recipient)
                return;

            if (!creature->lootForBody)
            {
                creature->lootForBody = true;

                // for creature, loot is filled when creature is killed.

                if (Group* group = recipient->GetGroup())
                {
                    switch (group->GetLootMethod())
                    {
                        case GROUP_LOOT:
                            // GroupLoot: rolls items over threshold. Items with quality < threshold, round robin
                            group->GroupLoot(loot, creature);
                            break;
                        case NEED_BEFORE_GREED:
                            group->NeedBeforeGreed(loot, creature);
                            break;
                        case MASTER_LOOT:
                            group->MasterLoot(loot, creature);
                            break;
                        default:
                            break;
                    }
                }
            }

            // possible only if creature->lootForBody && loot->empty() at spell cast check
            if (loot_type == LOOT_SKINNING)
            {
                loot->clear();
                loot->FillLoot(creature->GetCreatureTemplate()->SkinLootId, LootTemplates_Skinning, this, true);
                permission = OWNER_PERMISSION;
            }
            // set group rights only for loot_type != LOOT_SKINNING
            else
            {
                if (Group* group = GetGroup())
                {
                    if (group == recipient->GetGroup())
                    {
                        switch (group->GetLootMethod())
                        {
                            case MASTER_LOOT:
                                permission = MASTER_PERMISSION;
                                break;
                            case FREE_FOR_ALL:
                                permission = ALL_PERMISSION;
                                break;
                            case ROUND_ROBIN:
                                permission = ROUND_ROBIN_PERMISSION;
                                break;
                            default:
                                permission = GROUP_PERMISSION;
                                break;
                        }
                    }
                    else
                        permission = NONE_PERMISSION;
                }
                else if (recipient == this)
                    permission = OWNER_PERMISSION;
                else
                    permission = NONE_PERMISSION;
            }
        }
    }

    SetLootGUID(guid);

    // LOOT_INSIGNIA and LOOT_FISHINGHOLE unsupported by client
    switch (loot_type)
    {
        case LOOT_INSIGNIA:    loot_type = LOOT_SKINNING; break;
        case LOOT_FISHINGHOLE: loot_type = LOOT_FISHING; break;
        default: break;
    }

    // need know merged fishing/corpse loot type for achievements
    loot->loot_type = loot_type;

    WorldPacket data(SMSG_LOOT_RESPONSE, (9+50));           // we guess size
    data << uint64(guid);
    data << uint8(loot_type);
    data << LootView(*loot, this, permission);

    SendDirectMessage(&data);

    // add 'this' player as one of the players that are looting 'loot'
    if (permission != NONE_PERMISSION)
        loot->AddLooter(GetGUID());

    if (loot_type == LOOT_CORPSE && !IS_ITEM_GUID(guid))
        SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_LOOTING);
}

void Player::SendNotifyLootMoneyRemoved()
{
    WorldPacket data(SMSG_LOOT_CLEAR_MONEY, 0);
    GetSession()->SendPacket(&data);
}

void Player::SendNotifyLootItemRemoved(uint8 lootSlot)
{
    WorldPacket data(SMSG_LOOT_REMOVED, 1);
    data << uint8(lootSlot);
    GetSession()->SendPacket(&data);
}

void Player::SendUpdateWorldState(uint32 Field, uint32 Value)
{
    WorldPacket data(SMSG_UPDATE_WORLD_STATE, 8);
    data << Field;
    data << Value;
    GetSession()->SendPacket(&data);
}

void Player::SendInitWorldStates(uint32 zoneid, uint32 areaid)
{
    // data depends on zoneid/mapid...
    Battleground* bg = GetBattleground();
    uint32 mapid = GetMapId();
    OutdoorPvP* pvp = sOutdoorPvPMgr->GetOutdoorPvPToZoneId(zoneid);
    InstanceScript* instance = GetInstanceScript();
    Battlefield* bf = sBattlefieldMgr->GetBattlefieldToZoneId(zoneid);

    sLog->outDebug(LOG_FILTER_NETWORKIO, "Sending SMSG_INIT_WORLD_STATES to Map: %u, Zone: %u", mapid, zoneid);

    WorldPacket data(SMSG_INIT_WORLD_STATES, (4+4+4+2+(12*8)));
    data << uint32(mapid);                                  // mapid
    data << uint32(zoneid);                                 // zone id
    data << uint32(areaid);                                 // area id, new 2.1.0
    size_t countPos = data.wpos();
    data << uint16(0);                                      // count of uint64 blocks
    data << uint32(0x8d8) << uint32(0x0);                   // 1
    data << uint32(0x8d7) << uint32(0x0);                   // 2
    data << uint32(0x8d6) << uint32(0x0);                   // 3
    data << uint32(0x8d5) << uint32(0x0);                   // 4
    data << uint32(0x8d4) << uint32(0x0);                   // 5
    data << uint32(0x8d3) << uint32(0x0);                   // 6
                                                            // 7 1 - Arena season in progress, 0 - end of season
    data << uint32(0xC77) << uint32(sWorld->getBoolConfig(CONFIG_ARENA_SEASON_IN_PROGRESS));
                                                            // 8 Arena season id
    data << uint32(0xF3D) << uint32(sWorld->getIntConfig(CONFIG_ARENA_SEASON_ID));

    if (mapid == 530)                                       // Outland
    {
        data << uint32(0x9bf) << uint32(0x0);               // 7
        data << uint32(0x9bd) << uint32(0xF);               // 8
        data << uint32(0x9bb) << uint32(0xF);               // 9
    }

    // insert <field> <value>
    switch (zoneid)
    {
        case 1:                                             // Dun Morogh
        case 11:                                            // Wetlands
        case 12:                                            // Elwynn Forest
        case 38:                                            // Loch Modan
        case 40:                                            // Westfall
        case 51:                                            // Searing Gorge
        case 1519:                                          // Stormwind City
        case 1537:                                          // Ironforge
        case 2257:                                          // Deeprun Tram
        case 3703:                                          // Shattrath City
            break;
        case 139:                                           // Eastern Plaguelands
            if (pvp && pvp->GetTypeId() == OUTDOOR_PVP_EP)
                pvp->FillInitialWorldStates(data);
            else
            {
                data << uint32(0x97a) << uint32(0x0); // 10 2426
                data << uint32(0x917) << uint32(0x0); // 11 2327
                data << uint32(0x918) << uint32(0x0); // 12 2328
                data << uint32(0x97b) << uint32(0x32); // 13 2427
                data << uint32(0x97c) << uint32(0x32); // 14 2428
                data << uint32(0x933) << uint32(0x1); // 15 2355
                data << uint32(0x946) << uint32(0x0); // 16 2374
                data << uint32(0x947) << uint32(0x0); // 17 2375
                data << uint32(0x948) << uint32(0x0); // 18 2376
                data << uint32(0x949) << uint32(0x0); // 19 2377
                data << uint32(0x94a) << uint32(0x0); // 20 2378
                data << uint32(0x94b) << uint32(0x0); // 21 2379
                data << uint32(0x932) << uint32(0x0); // 22 2354
                data << uint32(0x934) << uint32(0x0); // 23 2356
                data << uint32(0x935) << uint32(0x0); // 24 2357
                data << uint32(0x936) << uint32(0x0); // 25 2358
                data << uint32(0x937) << uint32(0x0); // 26 2359
                data << uint32(0x938) << uint32(0x0); // 27 2360
                data << uint32(0x939) << uint32(0x1); // 28 2361
                data << uint32(0x930) << uint32(0x1); // 29 2352
                data << uint32(0x93a) << uint32(0x0); // 30 2362
                data << uint32(0x93b) << uint32(0x0); // 31 2363
                data << uint32(0x93c) << uint32(0x0); // 32 2364
                data << uint32(0x93d) << uint32(0x0); // 33 2365
                data << uint32(0x944) << uint32(0x0); // 34 2372
                data << uint32(0x945) << uint32(0x0); // 35 2373
                data << uint32(0x931) << uint32(0x1); // 36 2353
                data << uint32(0x93e) << uint32(0x0); // 37 2366
                data << uint32(0x931) << uint32(0x1); // 38 2367 ??  grey horde not in dbc! send for consistency's sake, and to match field count
                data << uint32(0x940) << uint32(0x0); // 39 2368
                data << uint32(0x941) << uint32(0x0); // 7 2369
                data << uint32(0x942) << uint32(0x0); // 8 2370
                data << uint32(0x943) << uint32(0x0); // 9 2371
            }
            break;
        case 1377:                                          // Silithus
            if (pvp && pvp->GetTypeId() == OUTDOOR_PVP_SI)
                pvp->FillInitialWorldStates(data);
            else
            {
                // states are always shown
                data << uint32(2313) << uint32(0x0); // 7 ally silityst gathered
                data << uint32(2314) << uint32(0x0); // 8 horde silityst gathered
                data << uint32(2317) << uint32(0x0); // 9 max silithyst
            }
            // dunno about these... aq opening event maybe?
            data << uint32(2322) << uint32(0x0); // 10 sandworm N
            data << uint32(2323) << uint32(0x0); // 11 sandworm S
            data << uint32(2324) << uint32(0x0); // 12 sandworm SW
            data << uint32(2325) << uint32(0x0); // 13 sandworm E
            break;
        case 2597:                                          // Alterac Valley
            if (bg && bg->GetTypeID(true) == BATTLEGROUND_AV)
                bg->FillInitialWorldStates(data);
            else
            {
                data << uint32(0x7ae) << uint32(0x1);           // 7 snowfall n
                data << uint32(0x532) << uint32(0x1);           // 8 frostwolfhut hc
                data << uint32(0x531) << uint32(0x0);           // 9 frostwolfhut ac
                data << uint32(0x52e) << uint32(0x0);           // 10 stormpike firstaid a_a
                data << uint32(0x571) << uint32(0x0);           // 11 east frostwolf tower horde assaulted -unused
                data << uint32(0x570) << uint32(0x0);           // 12 west frostwolf tower horde assaulted - unused
                data << uint32(0x567) << uint32(0x1);           // 13 frostwolfe c
                data << uint32(0x566) << uint32(0x1);           // 14 frostwolfw c
                data << uint32(0x550) << uint32(0x1);           // 15 irondeep (N) ally
                data << uint32(0x544) << uint32(0x0);           // 16 ice grave a_a
                data << uint32(0x536) << uint32(0x0);           // 17 stormpike grave h_c
                data << uint32(0x535) << uint32(0x1);           // 18 stormpike grave a_c
                data << uint32(0x518) << uint32(0x0);           // 19 stoneheart grave a_a
                data << uint32(0x517) << uint32(0x0);           // 20 stoneheart grave h_a
                data << uint32(0x574) << uint32(0x0);           // 21 1396 unk
                data << uint32(0x573) << uint32(0x0);           // 22 iceblood tower horde assaulted -unused
                data << uint32(0x572) << uint32(0x0);           // 23 towerpoint horde assaulted - unused
                data << uint32(0x56f) << uint32(0x0);           // 24 1391 unk
                data << uint32(0x56e) << uint32(0x0);           // 25 iceblood a
                data << uint32(0x56d) << uint32(0x0);           // 26 towerp a
                data << uint32(0x56c) << uint32(0x0);           // 27 frostwolfe a
                data << uint32(0x56b) << uint32(0x0);           // 28 froswolfw a
                data << uint32(0x56a) << uint32(0x1);           // 29 1386 unk
                data << uint32(0x569) << uint32(0x1);           // 30 iceblood c
                data << uint32(0x568) << uint32(0x1);           // 31 towerp c
                data << uint32(0x565) << uint32(0x0);           // 32 stoneh tower a
                data << uint32(0x564) << uint32(0x0);           // 33 icewing tower a
                data << uint32(0x563) << uint32(0x0);           // 34 dunn a
                data << uint32(0x562) << uint32(0x0);           // 35 duns a
                data << uint32(0x561) << uint32(0x0);           // 36 stoneheart bunker alliance assaulted - unused
                data << uint32(0x560) << uint32(0x0);           // 37 icewing bunker alliance assaulted - unused
                data << uint32(0x55f) << uint32(0x0);           // 38 dunbaldar south alliance assaulted - unused
                data << uint32(0x55e) << uint32(0x0);           // 39 dunbaldar north alliance assaulted - unused
                data << uint32(0x55d) << uint32(0x0);           // 40 stone tower d
                data << uint32(0x3c6) << uint32(0x0);           // 41 966 unk
                data << uint32(0x3c4) << uint32(0x0);           // 42 964 unk
                data << uint32(0x3c2) << uint32(0x0);           // 43 962 unk
                data << uint32(0x516) << uint32(0x1);           // 44 stoneheart grave a_c
                data << uint32(0x515) << uint32(0x0);           // 45 stonheart grave h_c
                data << uint32(0x3b6) << uint32(0x0);           // 46 950 unk
                data << uint32(0x55c) << uint32(0x0);           // 47 icewing tower d
                data << uint32(0x55b) << uint32(0x0);           // 48 dunn d
                data << uint32(0x55a) << uint32(0x0);           // 49 duns d
                data << uint32(0x559) << uint32(0x0);           // 50 1369 unk
                data << uint32(0x558) << uint32(0x0);           // 51 iceblood d
                data << uint32(0x557) << uint32(0x0);           // 52 towerp d
                data << uint32(0x556) << uint32(0x0);           // 53 frostwolfe d
                data << uint32(0x555) << uint32(0x0);           // 54 frostwolfw d
                data << uint32(0x554) << uint32(0x1);           // 55 stoneh tower c
                data << uint32(0x553) << uint32(0x1);           // 56 icewing tower c
                data << uint32(0x552) << uint32(0x1);           // 57 dunn c
                data << uint32(0x551) << uint32(0x1);           // 58 duns c
                data << uint32(0x54f) << uint32(0x0);           // 59 irondeep (N) horde
                data << uint32(0x54e) << uint32(0x0);           // 60 irondeep (N) ally
                data << uint32(0x54d) << uint32(0x1);           // 61 mine (S) neutral
                data << uint32(0x54c) << uint32(0x0);           // 62 mine (S) horde
                data << uint32(0x54b) << uint32(0x0);           // 63 mine (S) ally
                data << uint32(0x545) << uint32(0x0);           // 64 iceblood h_a
                data << uint32(0x543) << uint32(0x1);           // 65 iceblod h_c
                data << uint32(0x542) << uint32(0x0);           // 66 iceblood a_c
                data << uint32(0x540) << uint32(0x0);           // 67 snowfall h_a
                data << uint32(0x53f) << uint32(0x0);           // 68 snowfall a_a
                data << uint32(0x53e) << uint32(0x0);           // 69 snowfall h_c
                data << uint32(0x53d) << uint32(0x0);           // 70 snowfall a_c
                data << uint32(0x53c) << uint32(0x0);           // 71 frostwolf g h_a
                data << uint32(0x53b) << uint32(0x0);           // 72 frostwolf g a_a
                data << uint32(0x53a) << uint32(0x1);           // 73 frostwolf g h_c
                data << uint32(0x539) << uint32(0x0);           // 74 frostwolf g a_c
                data << uint32(0x538) << uint32(0x0);           // 75 stormpike grave h_a
                data << uint32(0x537) << uint32(0x0);           // 76 stormpike grave a_a
                data << uint32(0x534) << uint32(0x0);           // 77 frostwolf hut h_a
                data << uint32(0x533) << uint32(0x0);           // 78 frostwolf hut a_a
                data << uint32(0x530) << uint32(0x0);           // 79 stormpike first aid h_a
                data << uint32(0x52f) << uint32(0x0);           // 80 stormpike first aid h_c
                data << uint32(0x52d) << uint32(0x1);           // 81 stormpike first aid a_c
            }
            break;
        case 3277:                                          // Warsong Gulch
            if (bg && bg->GetTypeID(true) == BATTLEGROUND_WS)
                bg->FillInitialWorldStates(data);
            else
            {
                data << uint32(0x62d) << uint32(0x0);       // 7 1581 alliance flag captures
                data << uint32(0x62e) << uint32(0x0);       // 8 1582 horde flag captures
                data << uint32(0x609) << uint32(0x0);       // 9 1545 unk, set to 1 on alliance flag pickup...
                data << uint32(0x60a) << uint32(0x0);       // 10 1546 unk, set to 1 on horde flag pickup, after drop it's -1
                data << uint32(0x60b) << uint32(0x2);       // 11 1547 unk
                data << uint32(0x641) << uint32(0x3);       // 12 1601 unk (max flag captures?)
                data << uint32(0x922) << uint32(0x1);       // 13 2338 horde (0 - hide, 1 - flag ok, 2 - flag picked up (flashing), 3 - flag picked up (not flashing)
                data << uint32(0x923) << uint32(0x1);       // 14 2339 alliance (0 - hide, 1 - flag ok, 2 - flag picked up (flashing), 3 - flag picked up (not flashing)
            }
            break;
        case 3358:                                          // Arathi Basin
            if (bg && bg->GetTypeID(true) == BATTLEGROUND_AB)
                bg->FillInitialWorldStates(data);
            else
            {
                data << uint32(0x6e7) << uint32(0x0);       // 7 1767 stables alliance
                data << uint32(0x6e8) << uint32(0x0);       // 8 1768 stables horde
                data << uint32(0x6e9) << uint32(0x0);       // 9 1769 unk, ST?
                data << uint32(0x6ea) << uint32(0x0);       // 10 1770 stables (show/hide)
                data << uint32(0x6ec) << uint32(0x0);       // 11 1772 farm (0 - horde controlled, 1 - alliance controlled)
                data << uint32(0x6ed) << uint32(0x0);       // 12 1773 farm (show/hide)
                data << uint32(0x6ee) << uint32(0x0);       // 13 1774 farm color
                data << uint32(0x6ef) << uint32(0x0);       // 14 1775 gold mine color, may be FM?
                data << uint32(0x6f0) << uint32(0x0);       // 15 1776 alliance resources
                data << uint32(0x6f1) << uint32(0x0);       // 16 1777 horde resources
                data << uint32(0x6f2) << uint32(0x0);       // 17 1778 horde bases
                data << uint32(0x6f3) << uint32(0x0);       // 18 1779 alliance bases
                data << uint32(0x6f4) << uint32(0x7d0);     // 19 1780 max resources (2000)
                data << uint32(0x6f6) << uint32(0x0);       // 20 1782 blacksmith color
                data << uint32(0x6f7) << uint32(0x0);       // 21 1783 blacksmith (show/hide)
                data << uint32(0x6f8) << uint32(0x0);       // 22 1784 unk, bs?
                data << uint32(0x6f9) << uint32(0x0);       // 23 1785 unk, bs?
                data << uint32(0x6fb) << uint32(0x0);       // 24 1787 gold mine (0 - horde contr, 1 - alliance contr)
                data << uint32(0x6fc) << uint32(0x0);       // 25 1788 gold mine (0 - conflict, 1 - horde)
                data << uint32(0x6fd) << uint32(0x0);       // 26 1789 gold mine (1 - show/0 - hide)
                data << uint32(0x6fe) << uint32(0x0);       // 27 1790 gold mine color
                data << uint32(0x700) << uint32(0x0);       // 28 1792 gold mine color, wtf?, may be LM?
                data << uint32(0x701) << uint32(0x0);       // 29 1793 lumber mill color (0 - conflict, 1 - horde contr)
                data << uint32(0x702) << uint32(0x0);       // 30 1794 lumber mill (show/hide)
                data << uint32(0x703) << uint32(0x0);       // 31 1795 lumber mill color color
                data << uint32(0x732) << uint32(0x1);       // 32 1842 stables (1 - uncontrolled)
                data << uint32(0x733) << uint32(0x1);       // 33 1843 gold mine (1 - uncontrolled)
                data << uint32(0x734) << uint32(0x1);       // 34 1844 lumber mill (1 - uncontrolled)
                data << uint32(0x735) << uint32(0x1);       // 35 1845 farm (1 - uncontrolled)
                data << uint32(0x736) << uint32(0x1);       // 36 1846 blacksmith (1 - uncontrolled)
                data << uint32(0x745) << uint32(0x2);       // 37 1861 unk
                data << uint32(0x7a3) << uint32(0x708);     // 38 1955 warning limit (1800)
            }
            break;
        case 3820:                                          // Eye of the Storm
            if (bg && bg->GetTypeID(true) == BATTLEGROUND_EY)
                bg->FillInitialWorldStates(data);
            else
            {
                data << uint32(0xac1) << uint32(0x0);       // 7  2753 Horde Bases
                data << uint32(0xac0) << uint32(0x0);       // 8  2752 Alliance Bases
                data << uint32(0xab6) << uint32(0x0);       // 9  2742 Mage Tower - Horde conflict
                data << uint32(0xab5) << uint32(0x0);       // 10 2741 Mage Tower - Alliance conflict
                data << uint32(0xab4) << uint32(0x0);       // 11 2740 Fel Reaver - Horde conflict
                data << uint32(0xab3) << uint32(0x0);       // 12 2739 Fel Reaver - Alliance conflict
                data << uint32(0xab2) << uint32(0x0);       // 13 2738 Draenei - Alliance conflict
                data << uint32(0xab1) << uint32(0x0);       // 14 2737 Draenei - Horde conflict
                data << uint32(0xab0) << uint32(0x0);       // 15 2736 unk // 0 at start
                data << uint32(0xaaf) << uint32(0x0);       // 16 2735 unk // 0 at start
                data << uint32(0xaad) << uint32(0x0);       // 17 2733 Draenei - Horde control
                data << uint32(0xaac) << uint32(0x0);       // 18 2732 Draenei - Alliance control
                data << uint32(0xaab) << uint32(0x1);       // 19 2731 Draenei uncontrolled (1 - yes, 0 - no)
                data << uint32(0xaaa) << uint32(0x0);       // 20 2730 Mage Tower - Alliance control
                data << uint32(0xaa9) << uint32(0x0);       // 21 2729 Mage Tower - Horde control
                data << uint32(0xaa8) << uint32(0x1);       // 22 2728 Mage Tower uncontrolled (1 - yes, 0 - no)
                data << uint32(0xaa7) << uint32(0x0);       // 23 2727 Fel Reaver - Horde control
                data << uint32(0xaa6) << uint32(0x0);       // 24 2726 Fel Reaver - Alliance control
                data << uint32(0xaa5) << uint32(0x1);       // 25 2725 Fel Reaver uncontrolled (1 - yes, 0 - no)
                data << uint32(0xaa4) << uint32(0x0);       // 26 2724 Boold Elf - Horde control
                data << uint32(0xaa3) << uint32(0x0);       // 27 2723 Boold Elf - Alliance control
                data << uint32(0xaa2) << uint32(0x1);       // 28 2722 Boold Elf uncontrolled (1 - yes, 0 - no)
                data << uint32(0xac5) << uint32(0x1);       // 29 2757 Flag (1 - show, 0 - hide) - doesn't work exactly this way!
                data << uint32(0xad2) << uint32(0x1);       // 30 2770 Horde top-stats (1 - show, 0 - hide) // 02 -> horde picked up the flag
                data << uint32(0xad1) << uint32(0x1);       // 31 2769 Alliance top-stats (1 - show, 0 - hide) // 02 -> alliance picked up the flag
                data << uint32(0xabe) << uint32(0x0);       // 32 2750 Horde resources
                data << uint32(0xabd) << uint32(0x0);       // 33 2749 Alliance resources
                data << uint32(0xa05) << uint32(0x8e);      // 34 2565 unk, constant?
                data << uint32(0xaa0) << uint32(0x0);       // 35 2720 Capturing progress-bar (100 -> empty (only grey), 0 -> blue|red (no grey), default 0)
                data << uint32(0xa9f) << uint32(0x0);       //