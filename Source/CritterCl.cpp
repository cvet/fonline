#include "StdAfx.h"
#include "CritterCl.h"
#include "CritterType.h"
#include "ResourceManager.h"
#include "ItemManager.h"

#ifdef FONLINE_CLIENT
# include "SoundManager.h"
# include "Script.h"
#endif

AnyFrames* CritterCl::DefaultAnim = NULL;
int        CritterCl::ParamsChangeScript[ MAX_PARAMS ] = { 0 };
int        CritterCl::ParamsGetScript[ MAX_PARAMS ] = { 0 };
bool       CritterCl::ParamsRegEnabled[ MAX_PARAMS ] = { 0 };
int        CritterCl::ParamsReg[ MAX_PARAMS ] = { 0 };
uint       CritterCl::ParametersMin[ MAX_PARAMETERS_ARRAYS ] = { 0 };
uint       CritterCl::ParametersMax[ MAX_PARAMETERS_ARRAYS ] = { MAX_PARAMS - 1 };
bool       CritterCl::ParametersOffset[ MAX_PARAMETERS_ARRAYS ] = { false };
bool       CritterCl::SlotEnabled[ 0x100 ] = { true, true, true, true, false };

CritterCl::CritterCl(): CrDir( 0 ), SprId( 0 ), Id( 0 ), Pid( 0 ), NameColor( 0 ), ContourColor( 0 ),
                        Cond( 0 ), Anim1Life( 0 ), Anim1Knockout( 0 ), Anim1Dead( 0 ), Anim2Life( 0 ), Anim2Knockout( 0 ), Anim2Dead( 0 ),
                        Flags( 0 ), BaseType( 0 ), BaseTypeAlias( 0 ), curSpr( 0 ), lastEndSpr( 0 ), animStartTick( 0 ),
                        SprOx( 0 ), SprOy( 0 ), StartTick( 0 ), TickCount( 0 ), ApRegenerationTick( 0 ),
                        tickTextDelay( 0 ), textOnHeadColor( COLOR_TEXT ), Alpha( 0 ),
                        fadingEnable( false ), FadingTick( 0 ), fadeUp( false ), finishingTime( 0 ),
                        staySprDir( 0 ), staySprTick( 0 ), needReSet( false ), reSetTick( 0 ), CurMoveStep( 0 ),
                        Visible( true ), SprDrawValid( false ), IsNotValid( false ), RefCounter( 1 ),
                        OxExtI( 0 ), OyExtI( 0 ), OxExtF( 0 ), OyExtF( 0 ), OxExtSpeed( 0 ), OyExtSpeed( 0 ), OffsExtNextTick( 0 ),
                        Anim3d( NULL ), Anim3dStay( NULL ), Layers3d( NULL ), Multihex( 0 )
{
    Name = "";
    NameOnHead = "";
    Pass = "";
    memzero( Params, sizeof( Params ) );
    ItemSlotMain = ItemSlotExt = DefItemSlotHand = new Item();
    ItemSlotArmor = DefItemSlotArmor = new Item();
    ItemSlotMain->Init( ItemMngr.GetProtoItem( ITEM_DEF_SLOT ) );
    ItemSlotArmor->Init( ItemMngr.GetProtoItem( ITEM_DEF_ARMOR ) );
    tickFidget = Timer::GameTick() + Random( GameOpt.CritterFidgetTime, GameOpt.CritterFidgetTime * 2 );
    for( int i = 0; i < MAX_PARAMETERS_ARRAYS; i++ )
        ThisPtr[ i ] = this;
    memzero( ParamsIsChanged, sizeof( ParamsIsChanged ) );
    ParamLocked = -1;
    memzero( &stayAnim, sizeof( stayAnim ) );
    DrawEffect = Effect::Critter;
}

CritterCl::~CritterCl()
{
    SprMngr.FreePure3dAnimation( Anim3d );
    SprMngr.FreePure3dAnimation( Anim3dStay );
    Anim3d = Anim3dStay = NULL;

    if( Layers3d )
    {
        #ifdef FONLINE_CLIENT
        ( (ScriptArray*) Layers3d )->Release();
        #else
        uint* layers = (uint*) Layers3d;
        SAFEDELA( layers );
        Layers3d = NULL;
        #endif
    }

    SAFEREL( DefItemSlotHand );
    SAFEREL( DefItemSlotArmor );
    ItemSlotMain = ItemSlotExt = DefItemSlotHand = ItemSlotArmor = DefItemSlotArmor = NULL;
}

void CritterCl::Init()
{
    textOnHeadColor = COLOR_CRITTER_NAME;
    AnimateStay();
    SpriteInfo* si = SprMngr.GetSpriteInfo( SprId );
    if( si )
        textRect( 0, 0, si->Width, si->Height );
    SetFade( true );
}

void CritterCl::InitForRegistration()
{
    Name = "";
    Pass = "";
    BaseType = 0;

    memzero( Params, sizeof( Params ) );
    memzero( ParamsReg, sizeof( ParamsReg ) );
    ParamsReg[ 0 ] = ParamsReg[ 1 ] = ParamsReg[ 2 ] = ParamsReg[ 3 ] =
                                                           ParamsReg[ 4 ] = ParamsReg[ 5 ] = ParamsReg[ 6 ] = 5;
    ParamsReg[ ST_AGE ] = Random( AGE_MIN, AGE_MAX );
    ParamsReg[ ST_GENDER ] = GENDER_MALE;
    GenParams();
}

void CritterCl::Finish()
{
    SetFade( false );
    finishingTime = FadingTick;
}

void CritterCl::GenParams()
{
    #ifdef FONLINE_CLIENT
    if( Script::PrepareContext( ClientFunctions.PlayerGeneration, _FUNC_, "Registration" ) )
    {
        ScriptArray* arr = Script::CreateArray( "int[]" );
        if( !arr )
            return;
        arr->Resize( MAX_PARAMS );
        for( int i = 0; i < MAX_PARAMS; i++ )
            ( *(int*) arr->At( i ) ) = ParamsReg[ i ];
        Script::SetArgObject( arr );
        if( Script::RunPrepared() && arr->GetSize() == MAX_PARAMS )
            for( int i = 0; i < MAX_PARAMS; i++ )
                Params[ i ] = ( *(int*) arr->At( i ) );
        arr->Release();
    }
    #endif
}

void CritterCl::SetFade( bool fade_up )
{
    uint tick = Timer::GameTick();
    FadingTick = tick + FADING_PERIOD - ( FadingTick > tick ? FadingTick - tick : 0 );
    fadeUp = fade_up;
    fadingEnable = true;
}

uchar CritterCl::GetFadeAlpha()
{
    uint tick = Timer::GameTick();
    int  fading_proc = 100 - Procent( FADING_PERIOD, FadingTick > tick ? FadingTick - tick : 0 );
    fading_proc = CLAMP( fading_proc, 0, 100 );
    if( fading_proc >= 100 )
    {
        fading_proc = 100;
        fadingEnable = false;
    }

    return fadeUp == true ? ( fading_proc * 0xFF ) / 100 : ( ( 100 - fading_proc ) * 0xFF ) / 100;
}

void CritterCl::AddItem( Item* item )
{
    item->Accessory = ITEM_ACCESSORY_CRITTER;
    item->AccCritter.Id = this->Id;

    bool anim_stay = true;
    switch( item->AccCritter.Slot )
    {
    case SLOT_HAND1:
        ItemSlotMain = item;
        break;
    case SLOT_HAND2:
        ItemSlotExt = item;
        break;
    case SLOT_ARMOR:
        ItemSlotArmor = item;
        break;
    default:
        if( item == ItemSlotMain )
            ItemSlotMain = DefItemSlotHand;
        else if( item == ItemSlotExt )
            ItemSlotExt = DefItemSlotHand;
        else if( item == ItemSlotArmor )
            ItemSlotArmor = DefItemSlotArmor;
        else
            anim_stay = false;
        break;
    }

    InvItems.push_back( item );
    if( anim_stay && !IsAnim() )
        AnimateStay();
}

void CritterCl::EraseItem( Item* item, bool animate )
{
    if( !item )
        return;

    if( ItemSlotMain == item )
        ItemSlotMain = DefItemSlotHand;
    if( ItemSlotExt == item )
        ItemSlotExt = DefItemSlotHand;
    if( ItemSlotArmor == item )
        ItemSlotArmor = DefItemSlotArmor;
    item->Accessory = 0;

    for( auto it = InvItems.begin(), end = InvItems.end(); it != end; ++it )
    {
        Item* item_ = *it;
        if( item_ == item )
        {
            InvItems.erase( it );
            break;
        }
    }

    item->Release();
    if( animate && !IsAnim() )
        AnimateStay();
}

void CritterCl::EraseAllItems()
{
    ItemPtrVec items = InvItems;
    for( auto it = items.begin(), end = items.end(); it != end; ++it )
        EraseItem( *it, false );
    InvItems.clear();
}

Item* CritterCl::GetItem( uint item_id )
{
    for( auto it = InvItems.begin(), end = InvItems.end(); it != end; ++it )
    {
        Item* item = *it;
        if( item->GetId() == item_id )
            return item;
    }
    return NULL;
}

Item* CritterCl::GetItemByPid( ushort item_pid )
{
    for( auto it = InvItems.begin(), end = InvItems.end(); it != end; ++it )
        if( ( *it )->GetProtoId() == item_pid )
            return *it;
    return NULL;
}

Item* CritterCl::GetItemByPidInvPriority( ushort item_pid )
{
    ProtoItem* proto_item = ItemMngr.GetProtoItem( item_pid );
    if( !proto_item )
        return NULL;

    if( proto_item->Stackable )
    {
        for( auto it = InvItems.begin(), end = InvItems.end(); it != end; ++it )
        {
            Item* item = *it;
            if( item->GetProtoId() == item_pid )
                return item;
        }
    }
    else
    {
        Item* another_slot = NULL;
        for( auto it = InvItems.begin(), end = InvItems.end(); it != end; ++it )
        {
            Item* item = *it;
            if( item->GetProtoId() == item_pid )
            {
                if( item->AccCritter.Slot == SLOT_INV )
                    return item;
                another_slot = item;
            }
        }
        return another_slot;
    }
    return NULL;
}

Item* CritterCl::GetItemByPidSlot( ushort item_pid, int slot )
{
    for( auto it = InvItems.begin(), end = InvItems.end(); it != end; ++it )
    {
        Item* item = *it;
        if( item->GetProtoId() == item_pid && item->AccCritter.Slot == slot )
            return item;
    }
    return NULL;
}

Item* CritterCl::GetAmmo( uint caliber )
{
    for( auto it = InvItems.begin(), end = InvItems.end(); it != end; ++it )
        if( ( *it )->GetType() == ITEM_TYPE_AMMO && ( *it )->Proto->Ammo_Caliber == (int) caliber )
            return *it;
    return NULL;
}

Item* CritterCl::GetItemSlot( int slot )
{
    for( auto it = InvItems.begin(), end = InvItems.end(); it != end; ++it )
        if( ( *it )->AccCritter.Slot == slot )
            return *it;
    return NULL;
}

void CritterCl::GetItemsSlot( int slot, ItemPtrVec& items )
{
    for( auto it = InvItems.begin(), end = InvItems.end(); it != end; ++it )
    {
        Item* item = *it;
        if( slot == -1 || item->AccCritter.Slot == slot )
            items.push_back( item );
    }
}

void CritterCl::GetItemsType( int type, ItemPtrVec& items )
{
    for( auto it = InvItems.begin(), end = InvItems.end(); it != end; ++it )
    {
        Item* item = *it;
        if( item->GetType() == type )
            items.push_back( item );
    }
}

uint CritterCl::CountItemPid( ushort item_pid )
{
    uint result = 0;
    for( auto it = InvItems.begin(), end = InvItems.end(); it != end; ++it )
        if( ( *it )->GetProtoId() == item_pid )
            result += ( *it )->GetCount();
    return result;
}

uint CritterCl::CountItemType( uchar type )
{
    uint res = 0;
    for( auto it = InvItems.begin(), end = InvItems.end(); it != end; ++it )
        if( ( *it )->GetType() == type )
            res += ( *it )->GetCount();
    return res;
}

bool CritterCl::IsCanSortItems()
{
    uint inv_items = 0;
    for( auto it = InvItems.begin(), end = InvItems.end(); it != end; ++it )
    {
        if( ( *it )->AccCritter.Slot != SLOT_INV )
            continue;
        inv_items++;
        if( inv_items > 1 )
            return true;
    }
    return false;
}

Item* CritterCl::GetItemHighSortValue()
{
    Item* result = NULL;
    for( auto it = InvItems.begin(), end = InvItems.end(); it != end; ++it )
    {
        Item* item = *it;
        if( !result )
            result = item;
        else if( item->GetSortValue() > result->GetSortValue() )
            result = item;
    }
    return result;
}

Item* CritterCl::GetItemLowSortValue()
{
    Item* result = NULL;
    for( auto it = InvItems.begin(), end = InvItems.end(); it != end; ++it )
    {
        Item* item = *it;
        if( !result )
            result = item;
        else if( item->GetSortValue() < result->GetSortValue() )
            result = item;
    }
    return result;
}

void CritterCl::GetInvItems( ItemVec& items )
{
    items.clear();
    for( auto it = InvItems.begin(), end = InvItems.end(); it != end; ++it )
    {
        Item* item = *it;
        if( item->AccCritter.Slot == SLOT_INV )
            items.push_back( *item );
    }

    Item::SortItems( items );
}

uint CritterCl::GetItemsCount()
{
    uint count = 0;
    for( auto it = InvItems.begin(), end = InvItems.end(); it != end; ++it )
        count += ( *it )->GetCount();
    return count;
}

uint CritterCl::GetItemsCountInv()
{
    uint res = 0;
    for( auto it = InvItems.begin(), end = InvItems.end(); it != end; ++it )
        if( ( *it )->AccCritter.Slot == SLOT_INV )
            res++;
    return res;
}

uint CritterCl::GetItemsWeight()
{
    uint res = 0;
    for( auto it = InvItems.begin(), end = InvItems.end(); it != end; ++it )
        res += ( *it )->GetWeight();
    return res;
}

uint CritterCl::GetItemsWeightKg()
{
    return GetItemsWeight() / 1000;
}

uint CritterCl::GetItemsVolume()
{
    uint res = 0;
    for( auto it = InvItems.begin(); it != InvItems.end(); ++it )
        res += ( *it )->GetVolume();
    return res;
}

int CritterCl::GetFreeWeight()
{
    int cur_weight = GetItemsWeight();
    int max_weight = GetParam( ST_CARRY_WEIGHT );
    return max_weight - cur_weight;
}

int CritterCl::GetFreeVolume()
{
    int cur_volume = GetItemsVolume();
    int max_volume = GetMaxVolume();
    return max_volume - cur_volume;
}

Item* CritterCl::GetSlotUse( uchar num_slot, uchar& use )
{
    Item* item = NULL;
    switch( num_slot )
    {
    case SLOT_HAND1:
        item = ItemSlotMain;
        use = GetUse();
        break;
    case SLOT_HAND2:
        item = ItemSlotExt;
        if( item->IsWeapon() )
            use = USE_PRIMARY;
        else if( item->IsCanUse() || item->IsCanUseOnSmth() )
            use = USE_USE;
        else
            use = 0xF;
        break;
    default:
        break;
    }
    return item;
}

uint CritterCl::GetUsePicName( uchar num_slot )
{
    static uint use_on_pic = Str::GetHash( "art\\intrface\\useon.frm" );
    static uint use_pic = Str::GetHash( "art\\intrface\\uset.frm" );
    static uint reload_pic = Str::GetHash( "art\\intrface\\reload.frm" );

    uchar       use;
    Item*       item = GetSlotUse( num_slot, use );
    if( !item )
        return 0;
    if( item->IsWeapon() )
    {
        if( use == USE_RELOAD )
            return reload_pic;
        if( use == USE_USE )
            return use_on_pic;
        if( use >= MAX_USES )
            return 0;
        return item->Proto->Weapon_PicUse[ use ];
    }
    if( item->IsCanUseOnSmth() )
        return use_on_pic;
    if( item->IsCanUse() )
        return use_pic;
    return 0;
}

bool CritterCl::IsItemAim( uchar num_slot )
{
    uchar use;
    Item* item = GetSlotUse( num_slot, use );
    if( !item )
        return false;
    if( item->IsWeapon() && use < MAX_USES )
        return item->Proto->Weapon_Aim[ use ] != 0;
    return false;
}

bool CritterCl::CheckFind( int find_type )
{
    if( IsNpc() )
    {
        if( FLAG( find_type, FIND_ONLY_PLAYERS ) )
            return false;
    }
    else
    {
        if( FLAG( find_type, FIND_ONLY_NPC ) )
            return false;
    }
    return FLAG( find_type, FIND_ALL ) ||
           ( IsLife() && FLAG( find_type, FIND_LIFE ) ) ||
           ( IsKnockout() && FLAG( find_type, FIND_KO ) ) ||
           ( IsDead() && FLAG( find_type, FIND_DEAD ) );
}

uint CritterCl::GetLook()
{
    int look = GameOpt.LookNormal + GetParam( ST_PERCEPTION ) * 3 + GetParam( ST_BONUS_LOOK ) + GetMultihex();
    if( look < (int) GameOpt.LookMinimum )
        look = GameOpt.LookMinimum;
    return look;
}

uint CritterCl::GetTalkDistance()
{
    int dist = GetParam( ST_TALK_DISTANCE );
    if( dist <= 0 )
        dist = GameOpt.TalkDistance;
    return dist + GetMultihex();
}

uint CritterCl::GetAttackDist()
{
    uchar use;
    Item* weap = GetSlotUse( SLOT_HAND1, use );
    if( !weap->IsWeapon() )
        return 0;
    return GameOpt.GetAttackDistantion ? GameOpt.GetAttackDistantion( this, weap, use ) : 1;
}

uint CritterCl::GetUseDist()
{
    return 1 + GetMultihex();
}

uint CritterCl::GetMultihex()
{
    int mh = Multihex;
    if( mh < 0 )
        mh = CritType::GetMultihex( GetCrType() );
    return CLAMP( mh, 0, MAX_HEX_OFFSET );
}

int CritterCl::GetParam( uint index )
{
    #ifdef FONLINE_CLIENT
    if( ParamsGetScript[ index ] && Script::PrepareContext( ParamsGetScript[ index ], _FUNC_, GetInfo() ) )
    {
        Script::SetArgObject( this );
        Script::SetArgUInt( index - ( ParametersOffset[ index ] ? ParametersMin[ index ] : 0 ) );
        if( Script::RunPrepared() )
            return Script::GetReturnedUInt();
    }
    #endif
    return Params[ index ];
}

void CritterCl::ChangeParam( uint index )
{
    if( !ParamsIsChanged[ index ] && ParamLocked != (int) index )
    {
        ParamsChanged.push_back( index );
        ParamsChanged.push_back( Params[ index ] );
        ParamsIsChanged[ index ] = true;
    }
}

IntVec CallChange_;
void CritterCl::ProcessChangedParams()
{
    if( ParamsChanged.size() )
    {
        CallChange_.clear();
        for( uint i = 0, j = (uint) ParamsChanged.size(); i < j; i += 2 )
        {
            int index = ParamsChanged[ i ];
            int old_val = ParamsChanged[ i + 1 ];
            if( ParamsChangeScript[ index ] && Params[ index ] != old_val )
            {
                CallChange_.push_back( ParamsChangeScript[ index ] );
                CallChange_.push_back( index );
                CallChange_.push_back( old_val );
            }
            ParamsIsChanged[ index ] = false;

            // Internal processing
            if( index == ST_HANDS_ITEM_AND_MODE )
            {
                int        value = Params[ ST_HANDS_ITEM_AND_MODE ];
                ProtoItem* unarmed = ItemMngr.GetProtoItem( value >> 16 );
                if( !unarmed || !unarmed->IsWeapon() || !unarmed->Weapon_IsUnarmed )
                    unarmed = NULL;
                if( !unarmed )
                    unarmed = GetUnarmedItem( 0, 0 );
                DefItemSlotHand->Init( unarmed );
                DefItemSlotHand->SetMode( value & 0xFF );
            }
            else if( index == ST_SCALE_FACTOR )
            {
                if( Anim3d )
                {
                    int   value = Params[ ST_SCALE_FACTOR ];
                    float scale = (float) ( value ? value : 1000 ) / 1000.0f;
                    Anim3d->SetScale( scale, scale, scale );
                }
            }
        }
        ParamsChanged.clear();

        if( CallChange_.size() )
        {
            for( uint i = 0, j = (uint) CallChange_.size(); i < j; i += 3 )
            {
                uint index = CallChange_[ i + 1 ];
                ParamLocked = index;
                #ifdef FONLINE_CLIENT
                if( Script::PrepareContext( CallChange_[ i ], _FUNC_, GetInfo() ) )
                {
                    Script::SetArgObject( this );
                    Script::SetArgUInt( index - ( ParametersOffset[ index ] ? ParametersMin[ index ] : 0 ) );
                    Script::SetArgUInt( CallChange_[ i + 2 ] );
                    Script::RunPrepared();
                }
                #endif
                ParamLocked = -1;
            }
        }
    }
}

void CritterCl::DrawStay( Rect r )
{
    if( Timer::FastTick() - staySprTick > 500 )
    {
        staySprDir++;
        if( staySprDir >= DIRS_COUNT )
            staySprDir = 0;
        staySprTick = Timer::FastTick();
    }

    int  dir = ( !IsLife() ? CrDir : staySprDir );
    uint anim1 = GetAnim1();
    uint anim2 = GetAnim2();

    if( !Anim3d )
    {
        uint       crtype = GetCrType();
        AnyFrames* anim = ResMngr.GetCrit2dAnim( crtype, anim1, anim2, dir );
        if( anim )
        {
            uint spr_id = ( IsLife() ? anim->Ind[ 0 ] : anim->Ind[ anim->CntFrm - 1 ] );
            SprMngr.DrawSpriteSize( spr_id, r.L, r.T, (float) r.W(), (float) r.H(), false, true );
        }
    }
    else if( Anim3dStay )
    {
        Anim3dStay->SetDir( dir );
        Anim3dStay->SetAnimation( anim1, anim2, GetLayers3dData(), IsLife() ? 0 : ANIMATION_STAY | ANIMATION_PERIOD( 100 ) | ANIMATION_NO_SMOOTH );
        RectF r1 = RectF( (float) r.L, (float) r.T, (float) r.R, (float) r.B );
        RectF r2 = RectF( (float) r.L, (float) r.T, (float) r.R, (float) r.B );
        SprMngr.Draw3dSize( r1, false, true, Anim3dStay, &r2, COLOR_IFACE );
    }
}

#pragma MESSAGE("Exclude PID_BOTTLE_CAPS.")
const char* CritterCl::GetMoneyStr()
{
    static char money_str[ 64 ];
    uint        money_count = CountItemPid( 41 /*PID_BOTTLE_CAPS*/ );
    Str::Format( money_str, "%u$", money_count );
    return money_str;
}

bool CritterCl::NextRateItem( bool prev )
{
    bool  result = false;
    uchar old_rate = ItemSlotMain->Data.Mode;
    if( !ItemSlotMain->IsWeapon() )
    {
        if( ItemSlotMain->IsCanUse() || ItemSlotMain->IsCanUseOnSmth() )
            ItemSlotMain->Data.Mode = USE_USE;
        else
            ItemSlotMain->Data.Mode = USE_NONE;
    }
    else
    {
        // Unarmed
        if( !ItemSlotMain->GetId() )
        {
            ProtoItem* old_unarmed = ItemSlotMain->Proto;
            ProtoItem* unarmed = ItemSlotMain->Proto;
            uchar      tree = ItemSlotMain->Proto->Weapon_UnarmedTree;
            uchar      priority = ItemSlotMain->Proto->Weapon_UnarmedPriority;
            while( true )
            {
                if( prev )
                {
                    if( IsItemAim( SLOT_HAND1 ) && IsAim() )
                    {
                        SetAim( HIT_LOCATION_NONE );
                        break;
                    }
                    if( !priority )
                    {
                        // Find prev tree
                        if( tree )
                        {
                            tree--;
                        }
                        else
                        {
                            // Find last tree
                            for( int i = 0; i < 200; i++ )
                            {
                                ProtoItem* ua = GetUnarmedItem( i, 0 );
                                if( ua )
                                    unarmed = ua;
                                else
                                    break;
                            }
                            tree = unarmed->Weapon_UnarmedTree;
                        }

                        // Find last priority
                        for( int i = 0; i < 200; i++ )
                        {
                            ProtoItem* ua = GetUnarmedItem( tree, i );
                            if( ua )
                                unarmed = ua;
                            else
                                break;
                        }
                    }
                    else
                    {
                        priority--;
                        unarmed = GetUnarmedItem( tree, priority );
                    }
                    ItemSlotMain->Init( unarmed );
                    if( IsItemAim( SLOT_HAND1 ) && !IsRawParam( MODE_NO_AIM ) && CritType::IsCanAim( GetCrType() ) )
                    {
                        SetAim( HIT_LOCATION_TORSO );
                        break;
                    }
                }
                else
                {
                    if( IsItemAim( SLOT_HAND1 ) && !IsAim() && !IsRawParam( MODE_NO_AIM ) && CritType::IsCanAim( GetCrType() ) )
                    {
                        SetAim( HIT_LOCATION_TORSO );
                        break;
                    }
                    priority++;
                    unarmed = GetUnarmedItem( tree, priority );
                    if( !unarmed )
                    {
                        // Find next tree
                        tree++;
                        unarmed = GetUnarmedItem( tree, 0 );
                        if( !unarmed )
                            unarmed = GetUnarmedItem( 0, 0 );
                    }
                    ItemSlotMain->Init( unarmed );
                    SetAim( HIT_LOCATION_NONE );
                }
                break;
            }
            result = ( old_unarmed != unarmed );
        }
        // Armed
        else
        {
            while( true )
            {
                if( prev )
                {
                    if( IsItemAim( SLOT_HAND1 ) && IsAim() )
                    {
                        SetAim( HIT_LOCATION_NONE );
                        break;
                    }
                    if( !ItemSlotMain->Data.Mode )
                        ItemSlotMain->Data.Mode = ( ItemSlotMain->IsCanUseOnSmth() ? USE_USE : USE_RELOAD );
                    else
                        ItemSlotMain->Data.Mode--;
                    if( IsItemAim( SLOT_HAND1 ) && !IsRawParam( MODE_NO_AIM ) && CritType::IsCanAim( GetCrType() ) )
                    {
                        SetAim( HIT_LOCATION_TORSO );
                        break;
                    }
                }
                else
                {
                    if( IsItemAim( SLOT_HAND1 ) && !IsAim() && !IsRawParam( MODE_NO_AIM ) && CritType::IsCanAim( GetCrType() ) )
                    {
                        SetAim( HIT_LOCATION_TORSO );
                        break;
                    }
                    ItemSlotMain->Data.Mode++;
                    SetAim( HIT_LOCATION_NONE );
                }

                switch( GetUse() )
                {
                case USE_PRIMARY:
                    if( ItemSlotMain->Proto->Weapon_ActiveUses & 0x1 )
                        break;
                    continue;
                case USE_SECONDARY:
                    if( ItemSlotMain->Proto->Weapon_ActiveUses & 0x2 )
                        break;
                    continue;
                case USE_THIRD:
                    if( ItemSlotMain->Proto->Weapon_ActiveUses & 0x4 )
                        break;
                    continue;
                case USE_RELOAD:
                    if( ItemSlotMain->Proto->Weapon_MaxAmmoCount )
                        break;
                    continue;
                case USE_USE:
                    if( ItemSlotMain->IsCanUseOnSmth() )
                        break;
                    continue;
                default:
                    ItemSlotMain->Data.Mode = USE_PRIMARY;
                    break;
                }
                break;
            }
        }
    }
    ItemSlotMain->SetMode( ItemSlotMain->Data.Mode );
    return ItemSlotMain->Data.Mode != old_rate || result;
}

void CritterCl::SetAim( uchar hit_location )
{
    UNSETFLAG( ItemSlotMain->Data.Mode, 0xF0 );
    if( !IsItemAim( SLOT_HAND1 ) )
        return;
    SETFLAG( ItemSlotMain->Data.Mode, hit_location << 4 );
}

uint CritterCl::GetUseApCost( Item* item, uchar rate )
{
    return GameOpt.GetUseApCost ? GameOpt.GetUseApCost( this, item, rate ) : 1;
}

ProtoItem* CritterCl::GetUnarmedItem( uchar tree, uchar priority )
{
    ProtoItem* best_unarmed = NULL;
    for( int i = ITEM_SLOT_BEGIN; i <= ITEM_SLOT_END; i++ )
    {
        ProtoItem* unarmed = ItemMngr.GetProtoItem( i );
        if( !unarmed || !unarmed->IsWeapon() || !unarmed->Weapon_IsUnarmed )
            continue;
        if( unarmed->Weapon_UnarmedTree != tree || unarmed->Weapon_UnarmedPriority != priority )
            continue;
        if( GetParam( ST_STRENGTH ) < unarmed->Weapon_MinStrength || GetParam( ST_AGILITY ) < unarmed->Weapon_UnarmedMinAgility )
            break;
        if( GetParam( ST_LEVEL ) < unarmed->Weapon_UnarmedMinLevel || GetRawParam( SK_UNARMED ) < unarmed->Weapon_UnarmedMinUnarmed )
            break;
        best_unarmed = unarmed;
    }
    return best_unarmed;
}

Item* CritterCl::GetAmmoAvialble( Item* weap )
{
    Item* ammo = GetItemByPid( weap->Data.AmmoPid );
    if( !ammo && weap->WeapIsEmpty() )
        ammo = GetItemByPid( weap->Proto->Weapon_DefaultAmmoPid );
    if( !ammo && weap->WeapIsEmpty() )
        ammo = GetAmmo( weap->Proto->Weapon_Caliber );
    return ammo;
}

bool CritterCl::IsLastHexes()
{
    return !LastHexX.empty() && !LastHexY.empty();
}

void CritterCl::FixLastHexes()
{
    if( IsLastHexes() && LastHexX[ LastHexX.size() - 1 ] == HexX && LastHexY[ LastHexY.size() - 1 ] == HexY )
        return;
    LastHexX.push_back( HexX );
    LastHexY.push_back( HexY );
}

ushort CritterCl::PopLastHexX()
{
    ushort hx = LastHexX[ LastHexX.size() - 1 ];
    LastHexX.pop_back();
    return hx;
}

ushort CritterCl::PopLastHexY()
{
    ushort hy = LastHexY[ LastHexY.size() - 1 ];
    LastHexY.pop_back();
    return hy;
}

void CritterCl::Move( int dir )
{
    if( dir < 0 || dir >= DIRS_COUNT || !CritType::IsCanRotate( GetCrType() ) )
        dir = 0;
    CrDir = dir;

    uint crtype = GetCrType();
    int  time_move = 0;

    if( !IsRunning )
    {
        time_move = GetRawParam( ST_WALK_TIME );
        if( time_move <= 0 )
            time_move = CritType::GetTimeWalk( crtype );
        if( time_move <= 0 )
            time_move = 400;
    }
    else
    {
        time_move = GetRawParam( ST_RUN_TIME );
        if( time_move <= 0 )
            time_move = CritType::GetTimeRun( crtype );
        if( time_move <= 0 )
            time_move = 200;
    }

    // Todo: move faster in turn-based, if(IsTurnBased()) time_move/=2;
    TickStart( time_move );
    animStartTick = Timer::GameTick();

    if( !Anim3d )
    {
        if( CritType::GetAnimType( crtype ) == ANIM_TYPE_FALLOUT )
        {
            uint       anim1 = ( IsRunning ? ANIM1_UNARMED : GetAnim1() );
            uint       anim2 = ( IsRunning ? ANIM2_RUN : ANIM2_WALK );
            AnyFrames* anim = ResMngr.GetCrit2dAnim( crtype, anim1, anim2, dir );
            if( !anim )
                anim = DefaultAnim;

            int step, beg_spr, end_spr;
            curSpr = lastEndSpr;

            if( !IsRunning )
            {
                int s0 = CritType::GetWalkFrmCnt( crtype, 0 );
                int s1 = CritType::GetWalkFrmCnt( crtype, 1 );
                int s2 = CritType::GetWalkFrmCnt( crtype, 2 );
                int s3 = CritType::GetWalkFrmCnt( crtype, 3 );

                if( (int) curSpr == s0 - 1 && s1 )
                {
                    beg_spr = s0;
                    end_spr = s1 - 1;
                    step = 2;
                }
                else if( (int) curSpr == s1 - 1 && s2 )
                {
                    beg_spr = s1;
                    end_spr = s2 - 1;
                    step = 3;
                }
                else if( (int) curSpr == s2 - 1 && s3 )
                {
                    beg_spr = s2;
                    end_spr = s3 - 1;
                    step = 4;
                }
                else
                {
                    beg_spr = 0;
                    end_spr = s0 - 1;
                    step = 1;
                }
            }
            else
            {
                switch( curSpr )
                {
                default:
                case 0:
                    beg_spr = 0;
                    end_spr = 1;
                    step = 1;
                    break;
                case 1:
                    beg_spr = 2;
                    end_spr = 3;
                    step = 2;
                    break;
                case 3:
                    beg_spr = 4;
                    end_spr = 6;
                    step = 3;
                    break;
                case 6:
                    beg_spr = 7;
                    end_spr = anim->CntFrm - 1;
                    step = 4;
                    break;
                }
            }

            ClearAnim();
            animSequence.push_back( CritterAnim( anim, time_move, beg_spr, end_spr, true, 0, crtype, anim1, anim2, NULL ) );
            NextAnim( false );

            for( int i = 0; i < step; i++ )
            {
                int ox, oy;
                GetWalkHexOffsets( dir, ox, oy );
                ChangeOffs( ox, oy, true );
            }
        }
        else
        {
            uint anim1 = GetAnim1();
            uint anim2 = ( IsRunning ? ANIM2_RUN : ANIM2_WALK );
            if( IsHideMode() )
                anim2 = ( IsRunning ? ANIM2_SNEAK_RUN : ANIM2_SNEAK_WALK );
            if( IsDmgLeg() )
                anim2 = ANIM2_LIMP;

            AnyFrames* anim = ResMngr.GetCrit2dAnim( crtype, anim1, anim2, dir );
            if( !anim )
                anim = DefaultAnim;

            int m1 = CritType::GetWalkFrmCnt( crtype, 0 );
            if( m1 <= 0 )
                m1 = 5;
            int m2 = CritType::GetWalkFrmCnt( crtype, 1 );
            if( m2 <= 0 )
                m2 = 2;

            int beg_spr = lastEndSpr + 1;
            int end_spr = beg_spr + ( IsRunning ? m2 : m1 );

            ClearAnim();
            animSequence.push_back( CritterAnim( anim, time_move, beg_spr, end_spr, true, dir + 1, crtype, anim1, anim2, NULL ) );
            NextAnim( false );

            int ox, oy;
            GetWalkHexOffsets( dir, ox, oy );
            ChangeOffs( ox, oy, true );
        }
    }
    else
    {
        uint anim1 = GetAnim1();
        uint anim2 = ( IsRunning ? ANIM2_RUN : ANIM2_WALK );
        if( IsHideMode() )
            anim2 = ( IsRunning ? ANIM2_SNEAK_RUN : ANIM2_SNEAK_WALK );
        if( IsDmgLeg() )
            anim2 = ANIM2_LIMP;

        Anim3d->CheckAnimation( anim1, anim2 );
        Anim3d->SetDir( dir );

        ClearAnim();
        animSequence.push_back( CritterAnim( NULL, time_move, 0, 0, true, dir + 1, crtype, anim1, anim2, NULL ) );
        NextAnim( false );

        int ox, oy;
        GetWalkHexOffsets( dir, ox, oy );
        ChangeOffs( ox, oy, true );
    }
}

void CritterCl::Action( int action, int action_ext, Item* item, bool local_call /* = true */ )
{
    #ifdef FONLINE_CLIENT
    if( Script::PrepareContext( ClientFunctions.CritterAction, _FUNC_, "CritterAction" ) )
    {
        if( item )
            item = item->Clone();

        Script::SetArgBool( local_call );
        Script::SetArgObject( this );
        Script::SetArgUInt( action );
        Script::SetArgUInt( action_ext );
        Script::SetArgObject( item );
        Script::RunPrepared();

        SAFEREL( item );
    }
    #endif

    switch( action )
    {
    case ACTION_KNOCKOUT:
        Cond = COND_KNOCKOUT;
        Anim2Knockout = action_ext;
        break;
    case ACTION_STANDUP:
        Cond = COND_LIFE;
        break;
    case ACTION_DEAD:
    {
        Cond = COND_DEAD;
        Anim2Dead = action_ext;
        CritterAnim* anim = GetCurAnim();
        needReSet = true;
        reSetTick = Timer::GameTick() + ( anim && anim->Anim ? anim->Anim->Ticks : 1000 );
    }
    break;
    case ACTION_CONNECT:
        UNSETFLAG( Flags, FCRIT_DISCONNECT );
        break;
    case ACTION_DISCONNECT:
        SETFLAG( Flags, FCRIT_DISCONNECT );
        break;
    case ACTION_RESPAWN:
        Cond = COND_LIFE;
        Alpha = 0;
        SetFade( true );
        AnimateStay();
        needReSet = true;
        reSetTick = Timer::GameTick();       // Fast
        break;
    case ACTION_REFRESH:
        if( !IsAnim() )
            AnimateStay();
        break;
    default:
        break;
    }

    if( !IsAnim() )
        AnimateStay();
}

void CritterCl::NextAnim( bool erase_front )
{
    if( animSequence.empty() )
        return;
    if( erase_front )
    {
        SAFEREL( ( *animSequence.begin() ).ActiveItem );
        animSequence.erase( animSequence.begin() );
    }
    if( animSequence.empty() )
        return;

    CritterAnim& cr_anim = animSequence[ 0 ];
    animStartTick = Timer::GameTick();

    ProcessAnim( false, !Anim3d, cr_anim.IndAnim1, cr_anim.IndAnim2, cr_anim.ActiveItem );

    if( !Anim3d )
    {
        lastEndSpr = cr_anim.EndFrm;
        curSpr = cr_anim.BeginFrm;
        SprId = cr_anim.Anim->GetSprId( curSpr );

        short ox = 0, oy = 0;
        for( int i = 0, j = curSpr % cr_anim.Anim->GetCnt(); i <= j; i++ )
        {
            ox += cr_anim.Anim->NextX[ i ];
            oy += cr_anim.Anim->NextY[ i ];
        }
        SetOffs( ox, oy, cr_anim.MoveText );
    }
    else
    {
        SetOffs( 0, 0, cr_anim.MoveText );
        Anim3d->SetAnimation( cr_anim.IndAnim1, cr_anim.IndAnim2, GetLayers3dData(), cr_anim.DirOffs ? 0 : ANIMATION_ONE_TIME );
    }
}

void CritterCl::Animate( uint anim1, uint anim2, Item* item )
{
    uint  crtype = GetCrType();
    uchar dir = GetDir();
    if( !anim1 )
        anim1 = GetAnim1( item );
    if( item )
        item = item->Clone();

    if( !Anim3d )
    {
        AnyFrames* anim = ResMngr.GetCrit2dAnim( crtype, anim1, anim2, dir );
        if( !anim )
        {
            if( !IsAnim() )
                AnimateStay();
            return;
        }

        #pragma MESSAGE( "Migrate critter on head text moving in scripts." )
        bool move_text = true;
//			(Cond==COND_DEAD || Cond==COND_KNOCKOUT ||
//			(anim2!=ANIM2_SHOW_WEAPON && anim2!=ANIM2_HIDE_WEAPON && anim2!=ANIM2_PREPARE_WEAPON && anim2!=ANIM2_TURNOFF_WEAPON &&
//			anim2!=ANIM2_DODGE_FRONT && anim2!=ANIM2_DODGE_BACK && anim2!=ANIM2_USE && anim2!=ANIM2_PICKUP &&
//			anim2!=ANIM2_DAMAGE_FRONT && anim2!=ANIM2_DAMAGE_BACK && anim2!=ANIM2_IDLE && anim2!=ANIM2_IDLE_COMBAT));

        animSequence.push_back( CritterAnim( anim, anim->Ticks, 0, anim->GetCnt() - 1, move_text, 0, crtype, anim->Anim1, anim->Anim2, item ) );
    }
    else
    {
        if( !Anim3d->CheckAnimation( anim1, anim2 ) )
        {
            if( !IsAnim() )
                AnimateStay();
            return;
        }

        animSequence.push_back( CritterAnim( NULL, 0, 0, 0, true, 0, crtype, anim1, anim2, item ) );
    }

    if( animSequence.size() == 1 )
        NextAnim( false );
}

void CritterCl::AnimateStay()
{
    uint crtype = GetCrType();
    uint anim1 = GetAnim1();
    uint anim2 = GetAnim2();

    if( !Anim3d )
    {
        AnyFrames* anim = ResMngr.GetCrit2dAnim( crtype, anim1, anim2, CrDir );
        if( !anim )
            anim = DefaultAnim;

        if( stayAnim.Anim != anim )
        {
            ProcessAnim( true, true, anim1, anim2, NULL );

            stayAnim.Anim = anim;
            stayAnim.AnimTick = anim->Ticks;
            stayAnim.BeginFrm = 0;
            stayAnim.EndFrm = anim->GetCnt() - 1;
            if( Cond == COND_DEAD )
                stayAnim.BeginFrm = stayAnim.EndFrm;
            curSpr = stayAnim.BeginFrm;
        }

        SprId = anim->GetSprId( curSpr );

        SetOffs( 0, 0, true );
        short ox = 0, oy = 0;
        for( uint i = 0, j = curSpr % anim->GetCnt(); i <= j; i++ )
        {
            ox += anim->NextX[ i ];
            oy += anim->NextY[ i ];
        }
        ChangeOffs( ox, oy, false );
    }
    else
    {
        Anim3d->CheckAnimation( anim1, anim2 );

        ProcessAnim( true, false, anim1, anim2, NULL );
        SetOffs( 0, 0, true );

        if( Cond == COND_LIFE || Cond == COND_KNOCKOUT )
            Anim3d->SetAnimation( anim1, anim2, GetLayers3dData(), Animation3d::Is2dEmulation() ? ANIMATION_STAY : 0 );
        else
            Anim3d->SetAnimation( anim1, anim2, GetLayers3dData(), ANIMATION_STAY | ANIMATION_PERIOD( 100 ) );
    }
}

bool CritterCl::IsWalkAnim()
{
    if( animSequence.size() )
    {
        int anim2 = animSequence[ 0 ].IndAnim2;
        return anim2 == ANIM2_WALK || anim2 == ANIM2_RUN || anim2 == ANIM2_LIMP || anim2 == ANIM2_PANIC_RUN;
    }
    return false;
}

void CritterCl::ClearAnim()
{
    for( uint i = 0, j = (uint) animSequence.size(); i < j; i++ )
        SAFEREL( animSequence[ i ].ActiveItem );
    animSequence.clear();
}

bool CritterCl::IsHaveLightSources()
{
    for( auto it = InvItems.begin(), end = InvItems.end(); it != end; ++it )
    {
        Item* item = *it;
        if( item->IsLight() )
            return true;
    }
    return false;
}

uint CritterCl::GetCrType()
{
    return BaseType;
}

uint CritterCl::GetCrTypeAlias()
{
    return CritType::GetAlias( GetCrType() );
}

uint CritterCl::GetAnim1( Item* anim_item /* = NULL */ )
{
    if( !anim_item )
        anim_item = ItemSlotMain;

    switch( Cond )
    {
    case COND_LIFE:
        return ( Anim1Life ) | ( anim_item->IsWeapon() ? anim_item->Proto->Weapon_Anim1 : ANIM1_UNARMED );
    case COND_KNOCKOUT:
        return ( Anim1Knockout ) | ( anim_item->IsWeapon() ? anim_item->Proto->Weapon_Anim1 : ANIM1_UNARMED );
    case COND_DEAD:
        return ( Anim1Dead ) | ( anim_item->IsWeapon() ? anim_item->Proto->Weapon_Anim1 : ANIM1_UNARMED );
    default:
        break;
    }
    return ANIM1_UNARMED;
}

uint CritterCl::GetAnim2()
{
    switch( Cond )
    {
    case COND_LIFE:
        return Anim2Life ? Anim2Life : ( ( IsCombatMode() && GameOpt.Anim2CombatIdle ) ? GameOpt.Anim2CombatIdle : ANIM2_IDLE );
    case COND_KNOCKOUT:
        return Anim2Knockout ? Anim2Knockout : ANIM2_IDLE_PRONE_FRONT;
    case COND_DEAD:
        return Anim2Dead ? Anim2Dead : ANIM2_DEAD_FRONT;
    default:
        break;
    }
    return ANIM2_IDLE;
}

void CritterCl::ProcessAnim( bool animate_stay, bool is2d, uint anim1, uint anim2, Item* item )
{
    #ifdef FONLINE_CLIENT
    if( Script::PrepareContext( is2d ? ClientFunctions.Animation2dProcess : ClientFunctions.Animation3dProcess, _FUNC_, "AnimationProcess" ) )
    {
        Script::SetArgBool( animate_stay );
        Script::SetArgObject( this );
        Script::SetArgUInt( anim1 );
        Script::SetArgUInt( anim2 );
        Script::SetArgObject( item );
        Script::RunPrepared();
    }
    #endif
}

int* CritterCl::GetLayers3dData()
{
    #ifdef FONLINE_CLIENT
    static int   layers[ LAYERS3D_COUNT ];
    ScriptArray* arr = (ScriptArray*) Layers3d;
    memcpy( layers, arr->At( 0 ), sizeof( layers ) );
    return layers;
    #endif

    memcpy( Layers3d, &Params[ ST_ANIM3D_LAYER_BEGIN ], LAYERS3D_COUNT * sizeof( int ) );
    return (int*) Layers3d;
}

bool CritterCl::IsAnimAviable( uint anim1, uint anim2 )
{
    if( !anim1 )
        anim1 = GetAnim1();
    // 3d
    if( Anim3d )
        return Anim3d->IsAnimation( anim1, anim2 );
    // 2d
    return ResMngr.GetCrit2dAnim( GetCrType(), anim1, anim2, GetDir() ) != NULL;
}

void CritterCl::SetBaseType( uint type )
{
    BaseType = type;
    BaseTypeAlias = CritType::GetAlias( type );

    // Check 3d availability
    SprMngr.FreePure3dAnimation( Anim3d );
    SprMngr.FreePure3dAnimation( Anim3dStay );
    Anim3d = Anim3dStay = NULL;
    Animation3d* anim3d = SprMngr.LoadPure3dAnimation( Str::FormatBuf( "%s.fo3d", CritType::GetName( BaseType ) ), PT_ART_CRITTERS );
    if( anim3d )
    {
        Anim3d = anim3d;
        Anim3dStay = SprMngr.LoadPure3dAnimation( Str::FormatBuf( "%s.fo3d", CritType::GetName( BaseType ) ), PT_ART_CRITTERS );

        Anim3d->SetDir( CrDir );
        SprId = Anim3d->GetSprId();

        #ifdef FONLINE_CLIENT
        if( !Layers3d )
        {
            Layers3d = Script::CreateArray( "int[]" );
            ( (ScriptArray*) Layers3d )->Resize( LAYERS3D_COUNT );
        }
        memzero( ( (ScriptArray*) Layers3d )->At( 0 ), LAYERS3D_COUNT * sizeof( int ) );
        #else
        if( !Layers3d )
            Layers3d = new int[ LAYERS3D_COUNT ];
        memzero( Layers3d, LAYERS3D_COUNT * sizeof( int ) );
        #endif

        Anim3d->SetAnimation( ANIM1_UNARMED, ANIM2_IDLE, GetLayers3dData(), 0 );
    }

    // Allow influence of scale factor
    ChangeParam( ST_SCALE_FACTOR );
    ProcessChangedParams();
}

void CritterCl::SetDir( uchar dir )
{
    if( dir >= DIRS_COUNT || !CritType::IsCanRotate( GetCrType() ) )
        dir = 0;
    if( CrDir == dir )
        return;
    CrDir = dir;
    if( Anim3d )
        Anim3d->SetDir( dir );
    if( !IsAnim() )
        AnimateStay();
}

void CritterCl::Process()
{
    // Changed parameters
    ProcessChangedParams();

    // Fading
    if( fadingEnable == true )
        Alpha = GetFadeAlpha();

    // Extra offsets
    if( OffsExtNextTick && Timer::GameTick() >= OffsExtNextTick )
    {
        OffsExtNextTick = Timer::GameTick() + 30;
        uint dist = DistSqrt( 0, 0, OxExtI, OyExtI );
        SprOx -= OxExtI;
        SprOy -= OyExtI;
        float mul = (float) ( dist / 10 );
        if( mul < 1.0f )
            mul = 1.0f;
        OxExtF += OxExtSpeed * mul;
        OyExtF += OyExtSpeed * mul;
        OxExtI = (int) OxExtF;
        OyExtI = (int) OyExtF;
        if( DistSqrt( 0, 0, OxExtI, OyExtI ) > dist ) // End of work
        {
            OffsExtNextTick = 0;
            OxExtI = 0;
            OyExtI = 0;
        }
        SetOffs( SprOx, SprOy, true );
    }

    // Animation
    CritterAnim& cr_anim = ( animSequence.size() ? animSequence[ 0 ] : stayAnim );
    int          anim_proc = ( Timer::GameTick() - animStartTick ) * 100 / ( cr_anim.AnimTick ? cr_anim.AnimTick : 1 );
    if( anim_proc >= 100 )
    {
        if( animSequence.size() )
            anim_proc = 100;
        else
            anim_proc %= 100;
    }

    // Change frames
    if( !Anim3d && anim_proc < 100 )
    {
        uint cur_spr = cr_anim.BeginFrm + ( ( cr_anim.EndFrm - cr_anim.BeginFrm + 1 ) * anim_proc ) / 100;
        if( cur_spr != curSpr )
        {
            // Change frame
            uint old_spr = curSpr;
            curSpr = cur_spr;
            SprId = cr_anim.Anim->GetSprId( curSpr );

            // Change offsets
            short ox = 0, oy = 0;
            for( uint i = 0, j = old_spr % cr_anim.Anim->GetCnt(); i <= j; i++ )
            {
                ox -= cr_anim.Anim->NextX[ i ];
                oy -= cr_anim.Anim->NextY[ i ];
            }
            for( uint i = 0, j = cur_spr % cr_anim.Anim->GetCnt(); i <= j; i++ )
            {
                ox += cr_anim.Anim->NextX[ i ];
                oy += cr_anim.Anim->NextY[ i ];
            }
            ChangeOffs( ox, oy, cr_anim.MoveText );
        }
    }

    if( animSequence.size() )
    {
        // Move offsets
        if( cr_anim.DirOffs )
        {
            int ox, oy;
            GetWalkHexOffsets( cr_anim.DirOffs - 1, ox, oy );
            SetOffs( ox - ( ox * anim_proc / 100 ), oy - ( oy * anim_proc / 100 ), true );

            if( anim_proc >= 100 )
            {
                NextAnim( true );
                if( MoveSteps.size() )
                    return;
            }
        }
        else
        {
            if( !Anim3d )
            {
                if( anim_proc >= 100 )
                    NextAnim( true );
            }
            else
            {
                if( !Anim3d->IsAnimationPlaying() )
                    NextAnim( true );
            }
        }

        if( MoveSteps.size() )
            return;
        if( !animSequence.size() )
            AnimateStay();
    }

    // Battle 3d mode
    // Todo: do same for 2d animations
    if( Anim3d && GameOpt.Anim2CombatIdle && !animSequence.size() && Cond == COND_LIFE && !Anim2Life )
    {
        if( GameOpt.Anim2CombatBegin && IsCombatMode() && Anim3d->GetAnim2() != (int) GameOpt.Anim2CombatIdle )
            Animate( 0, GameOpt.Anim2CombatBegin, NULL );
        else if( GameOpt.Anim2CombatEnd && !IsCombatMode() && Anim3d->GetAnim2() == (int) GameOpt.Anim2CombatIdle )
            Animate( 0, GameOpt.Anim2CombatEnd, NULL );
    }

    // Fidget animation
    if( Timer::GameTick() >= tickFidget )
    {
        if( !animSequence.size() && Cond == COND_LIFE && IsFree() && !MoveSteps.size() && !IsCombatMode() )
            Action( ACTION_FIDGET, 0, NULL );
        tickFidget = Timer::GameTick() + Random( GameOpt.CritterFidgetTime, GameOpt.CritterFidgetTime * 2 );
    }
}

void CritterCl::ChangeOffs( short change_ox, short change_oy, bool move_text )
{
    SetOffs( SprOx - OxExtI + change_ox, SprOy - OyExtI + change_oy, move_text );
}

void CritterCl::SetOffs( short set_ox, short set_oy, bool move_text )
{
    SprOx = set_ox + OxExtI;
    SprOy = set_oy + OyExtI;
    if( SprDrawValid )
    {
        if( !Anim3d )
        {
            SprMngr.GetDrawRect( SprDraw, DRect );
            if( move_text )
                textRect = DRect;
        }
        if( Anim3d )
            Anim3d->SetDrawPos( SprDraw->ScrX + SprOx + GameOpt.ScrOx, SprDraw->ScrY + SprOy + GameOpt.ScrOy );
        if( IsChosen() )
            SprMngr.SetEgg( HexX, HexY, SprDraw );
    }
}

void CritterCl::SetSprRect()
{
    if( SprDrawValid )
    {
        if( !Anim3d )
        {
            Rect old = DRect;
            SprMngr.GetDrawRect( SprDraw, DRect );
            textRect.L += DRect.L - old.L;
            textRect.R += DRect.L - old.L;
            textRect.T += DRect.T - old.T;
            textRect.B += DRect.T - old.T;
        }
        else
        {
            Anim3d->SetDrawPos( SprDraw->ScrX + SprOx + GameOpt.ScrOx, SprDraw->ScrY + SprOy + GameOpt.ScrOy );
        }
        if( IsChosen() )
            SprMngr.SetEgg( HexX, HexY, SprDraw );
    }
}

Rect CritterCl::GetTextRect()
{
    if( SprDrawValid )
    {
        if( Anim3d )
        {
            SprMngr.GetDrawRect( SprDraw, textRect );
            textRect( -GameOpt.ScrOx, -GameOpt.ScrOy - 3 );
        }
        return textRect;
    }
    return Rect();
}

/*
   short CritterCl::GetSprOffX()
   {
        short res=0;
        for(int i=0;i<curSpr;++i)

        SpriteInfo*
        res=lpSM->->next_x[lpSM->CrAnim[crtype][anim1][anim2]->dir_offs[Ori]];

        for(int i=1;i<=num_frame;++i)
                ChangeCur_offs(lpSM->CrAnim[crtype][anim1][anim2]->next_x[lpSM->CrAnim[crtype][anim1][anim2]->dir_offs[Ori]+i],
                lpSM->CrAnim[crtype][anim1][anim2]->next_y[lpSM->CrAnim[crtype][anim1][anim2]->dir_offs[Ori]+i]);
   }

   short CritterCl::GetSprOffY()
   {

   }
 */
void CritterCl::AccamulateOffs()
{
//	if(!cur_anim) return;
//	if(cur_afrm<cnt_per_turn) return;

//	for(int i=cur_afrm-cnt_per_turn;i<=cur_afrm;i++)
//	{
//		if(i<0) i=0;
    //	SetCur_offs(cur_anim->next_x[cur_anim->dir_offs[cur_dir]+cur_afrm],
    //		cur_anim->next_y[cur_anim->dir_offs[cur_dir]+cur_afrm]);
//	}
}

void CritterCl::AddOffsExt( short ox, short oy )
{
    SprOx -= OxExtI;
    SprOy -= OyExtI;
    ox += OxExtI;
    oy += OyExtI;
    OxExtI = ox;
    OyExtI = oy;
    OxExtF = (float) ox;
    OyExtF = (float) oy;
    GetStepsXY( OxExtSpeed, OyExtSpeed, 0, 0, ox, oy );
    OxExtSpeed = -OxExtSpeed;
    OyExtSpeed = -OyExtSpeed;
    OffsExtNextTick = Timer::GameTick() + 30;
    SetOffs( SprOx, SprOy, true );
}

void CritterCl::GetWalkHexOffsets( int dir, int& ox, int& oy )
{
    int hx = 1, hy = 1;
    MoveHexByDirUnsafe( hx, hy, dir );
    GetHexInterval( hx, hy, 1, 1, ox, oy );
}

void CritterCl::SetText( const char* str, uint color, uint text_delay )
{
    tickStartText = Timer::GameTick();
    strTextOnHead = str;
    tickTextDelay = text_delay;
    textOnHeadColor = color;
}

void CritterCl::GetNameTextInfo( bool& nameVisible, int& x, int& y, int& w, int& h, int& lines )
{
    nameVisible = false;

    char str[ MAX_FOTEXT ];
    if( strTextOnHead.empty() )
    {
        if( IsPlayer() && !GameOpt.ShowPlayerNames )
            return;
        if( IsNpc() && !GameOpt.ShowNpcNames )
            return;

        nameVisible = true;

        if( NameOnHead.length() )
            Str::Copy( str, NameOnHead.c_str() );
        else
            Str::Copy( str, Name.c_str() );
        if( GameOpt.ShowCritId )
            Str::Append( str, Str::FormatBuf( " <%u>", GetId() ) );
        if( FLAG( Flags, FCRIT_DISCONNECT ) )
            Str::Append( str, GameOpt.PlayerOffAppendix.c_str() );
    }
    else
        Str::Copy( str, strTextOnHead.c_str() );

    Rect tr = GetTextRect();
    x = (int) ( (float) ( tr.L + tr.W() / 2 + GameOpt.ScrOx ) / GameOpt.SpritesZoom - 100.0f );
    y = (int) ( (float) ( tr.T + GameOpt.ScrOy ) / GameOpt.SpritesZoom - 70.0f );

    SprMngr.GetTextInfo( 200, 70, str, -1, FT_CENTERX | FT_BOTTOM | FT_BORDERED, w, h, lines );
    x += 100 - ( w / 2 );
    y += 70 - h;
}

void CritterCl::DrawTextOnHead()
{
    if( strTextOnHead.empty() )
    {
        if( IsPlayer() && !GameOpt.ShowPlayerNames )
            return;
        if( IsNpc() && !GameOpt.ShowNpcNames )
            return;
    }

    if( SprDrawValid )
    {
        Rect tr = GetTextRect();
        int  x = (int) ( (float) ( tr.L + tr.W() / 2 + GameOpt.ScrOx ) / GameOpt.SpritesZoom - 100.0f );
        int  y = (int) ( (float) ( tr.T + GameOpt.ScrOy ) / GameOpt.SpritesZoom - 70.0f );
        Rect r( x, y, x + 200, y + 70 );

        char str[ MAX_FOTEXT ];
        uint color;
        if( strTextOnHead.empty() )
        {
            if( NameOnHead.length() )
                Str::Copy( str, NameOnHead.c_str() );
            else
                Str::Copy( str, Name.c_str() );
            if( GameOpt.ShowCritId )
                Str::Append( str, Str::FormatBuf( " <%u>", GetId() ) );
            if( FLAG( Flags, FCRIT_DISCONNECT ) )
                Str::Append( str, GameOpt.PlayerOffAppendix.c_str() );
            color = ( NameColor ? NameColor : COLOR_CRITTER_NAME );
        }
        else
        {
            Str::Copy( str, strTextOnHead.c_str() );
            color = textOnHeadColor;

            if( tickTextDelay > 500 )
            {
                uint dt = Timer::GameTick() - tickStartText;
                uint hide = tickTextDelay - 200;
                if( dt >= hide )
                {
                    uint alpha = 0xFF * ( 100 - Procent( tickTextDelay - hide, dt - hide ) ) / 100;
                    color = ( alpha << 24 ) | ( color & 0xFFFFFF );
                }
            }
        }

        if( fadingEnable )
        {
            uint alpha = GetFadeAlpha();
            SprMngr.DrawStr( r, str, FT_CENTERX | FT_BOTTOM | FT_BORDERED, ( alpha << 24 ) | ( color & 0xFFFFFF ) );
        }
        else if( !IsFinishing() )
        {
            SprMngr.DrawStr( r, str, FT_CENTERX | FT_BOTTOM | FT_BORDERED, color );
        }
    }

    if( Timer::GameTick() - tickStartText >= tickTextDelay && !strTextOnHead.empty() )
        strTextOnHead = "";
}
