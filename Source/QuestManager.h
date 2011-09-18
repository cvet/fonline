#ifndef __QUEST_MANAGER__
#define __QUEST_MANAGER__

#include "Common.h"
#include "MsgFiles.h"

#define QUEST_MUL    1000

struct Quest
{
    ushort num;
    string str;
    string info;
    bool   isInfo;

    bool operator==( const ushort& _num ) { return _num == num; }
    Quest( uint _num, string _info ): num( _num ), info( _info ), isInfo( false ) {}
};
typedef vector< Quest >             QuestVec;
typedef vector< Quest >::value_type QuestVecVal;

class QuestTab
{
private:
    QuestVec quests;
    string   text;
    FOMsg*   msg;

    void ReparseText()
    {
        text = "";

        char str[ 128 ];
        for( uint i = 0; i < quests.size(); ++i )
        {
            sprintf( str, msg->GetStr( STR_QUEST_NUMBER ), i + 1 );

            text += str;
            text += quests[ i ].info;
            text += "\n";
            text += msg->GetStr( STR_QUEST_PROCESS );
            text += quests[ i ].str;
            text += "\n\n";
        }
    }

public:
    bool   IsEmpty() { return quests.empty(); }
    Quest* AddQuest( ushort num, string& info )
    {
        quests.push_back( Quest( num, info ) );
        return &quests[ quests.size() - 1 ];
        ReparseText();
    }
    void RefreshQuest( ushort num, string& str )
    {
        Quest* quest = GetQuest( num );
        if( !quest ) return;
        quest->str = str;
        ReparseText();
    }
    Quest* GetQuest( ushort num )
    {
        auto it = std::find( quests.begin(), quests.end(), num );
        return it != quests.end() ? &( *it ) : NULL;
    }
    void EraseQuest( ushort num )
    {
        auto it = std::find( quests.begin(), quests.end(), num );
        if( it != quests.end() ) quests.erase( it );
        ReparseText();
    }
    QuestVec*   GetQuests() { return &quests; }
    const char* GetText()   { return text.c_str(); }
    QuestTab( FOMsg* _msg ): msg( _msg ) {}
};
typedef map< string, QuestTab, less< string > >             QuestTabMap;
typedef map< string, QuestTab, less< string > >::value_type QuestTabMapVal;

class QuestManager
{
private:
    FOMsg*      msg;
    QuestTabMap tabs;

public:
    void Init( FOMsg* quest_msg )
    {
        msg = quest_msg;
    }

    void Clear()
    {
        tabs.clear();
    }

    void OnQuest( uint num )
    {
        // Split
        ushort q_num = num / QUEST_MUL;
        ushort val = num % QUEST_MUL;

        // Check valid Name of Tab
        if( !msg->Count( STR_QUEST_MAP_( q_num ) ) ) return;

        // Get Name of Tab
        string tab_name = string( msg->GetStr( STR_QUEST_MAP_( q_num ) ) );

        // Try get Tab
        QuestTab* tab = NULL;
        auto      it_tab = tabs.find( tab_name );
        if( it_tab != tabs.end() ) tab = &( *it_tab ).second;

        // Try get Quest
        Quest* quest = NULL;
        if( tab ) quest = tab->GetQuest( q_num );

        // Erase	quest
        if( !val )
        {
            if( tab )
            {
                tab->EraseQuest( q_num );
                if( tab->IsEmpty() ) tabs.erase( tab_name );
            }
            return;
        }

        // Add Tab if not exists
        if( !tab ) tab = &( *( tabs.insert( QuestTabMapVal( tab_name, QuestTab( msg ) ) ) ).first ).second;

        // Add Quest if not exists
        if( !quest ) quest = tab->AddQuest( q_num, string( msg->GetStr( STR_QUEST_INFO_( q_num ) ) ) );

        // Get name of quest
        tab->RefreshQuest( q_num, string( msg->GetStr( num ) ) );
    }

    QuestTabMap* GetTabs()
    {
        return &tabs;
    }

    QuestTab* GetTab( uint tab_num )
    {
        if( tabs.empty() ) return NULL;

        auto it = tabs.begin();
        while( tab_num )
        {
            ++it;
            --tab_num;
            if( it == tabs.end() ) return NULL;
        }

        return &( *it ).second;
    }

    Quest* GetQuest( uint tab_num, ushort quest_num )
    {
        QuestTab* tab = GetTab( tab_num );
        return tab ? tab->GetQuest( quest_num ) : NULL;
    }

    Quest* GetQuest( uint num )
    {
        if( !msg->Count( STR_QUEST_MAP_( num / QUEST_MUL ) ) ) return NULL;
        string tab_name = string( msg->GetStr( STR_QUEST_MAP_( num / QUEST_MUL ) ) );
        auto   it_tab = tabs.find( tab_name );
        return it_tab != tabs.end() ? ( *it_tab ).second.GetQuest( num / QUEST_MUL ) : NULL;
    }
};

#endif // __QUEST_MANAGER__
