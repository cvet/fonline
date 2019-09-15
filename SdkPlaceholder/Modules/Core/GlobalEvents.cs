#pragma warning disable CS0067

using System;
using System.IO;
using System.Linq;
using System.Reflection;

namespace FOnlineEngine
{
    public static class GlobalEvents
    {
        static bool Initialize()
        {
            // Invoke static constructor calls
            int errors = 0;
            foreach (Assembly assembly in AppDomain.CurrentDomain.GetAssemblies())
            {
                foreach (Type type in assembly.GetTypes())
                {
                    try
                    {
                        System.Runtime.CompilerServices.RuntimeHelpers.RunClassConstructor(type.TypeHandle);
                    }
                    catch (Exception ex)
                    {
                        Engine.Log("Failed to run type: " + type.Name);
                        Engine.Log(ex.ToString());
                        errors++;
                    }
                }
            }
            return errors == 0;
        }

#if SERVER
        public delegate void ResourcesGeneratedEventDelegate();
        public static event ResourcesGeneratedEventDelegate ResourcesGeneratedEvent;
        internal static bool OnResourcesGeneratedEvent()
        {
            foreach (var eventDelegate in ResourcesGeneratedEvent.GetInvocationList().Cast<ResourcesGeneratedEventDelegate>())
            {
                try
                {
                    eventDelegate();
                }
                catch (Exception ex)
                {
                    Engine.Log(ex.ToString());
                }
            }
            return true;
        }

        /*public delegate void GenerateWorldEventDelegate();
        public delegate void StartEventDelegate();
        public delegate void FinishEventDelegate();
        public delegate void LoopEventDelegate();
        public delegate void GlobalMapCritterInEventDelegate(Critter critter);
        public delegate void GlobalMapCritterOutEventDelegate(Critter critter);
        public delegate void LocationInitEventDelegate(Location location, bool firstTime);
        public delegate void LocationFinishEventDelegate(Location location);
        public delegate void MapInitEventDelegate(Map map, bool firstTime);
        public delegate void MapFinishEventDelegate(Map map);
        public delegate void MapLoopEventDelegate(Map map, uint loopIndex);
        public delegate void MapCritterInEventDelegate(Map map, Critter critter);
        public delegate void MapCritterOutEventDelegate(Map map, Critter critter);
        public delegate void MapCheckLookEventDelegate(Map map, Critter critter, Critter target);
        public delegate void MapCheckTrapLookEventDelegate(Map map, Critter critter, Item item);*/

        public delegate void CritterInitEventDelegate(Critter cr, bool firstTime);
        public static event CritterInitEventDelegate CritterInitEvent;
        internal static void OnCritterInitEvent(Critter cr, bool firstTime)
        {
            foreach (var eventDelegate in CritterInitEvent.GetInvocationList().Cast<CritterInitEventDelegate>())
            {
                try
                {
                    if (cr.IsDestroyed)
                        break;

                    eventDelegate(cr, firstTime);
                }
                catch (Exception ex)
                {
                    Engine.Log(ex.ToString());
                }
            }
        }

        public delegate void CritterFinishEventDelegate(Critter cr);
        public static event CritterFinishEventDelegate CritterFinishEvent;
        internal static void OnCritterFinishEvent(Critter cr)
        {
            foreach (var eventDelegate in CritterInitEvent.GetInvocationList().Cast<CritterFinishEventDelegate>())
            {
                try
                {
                    if (cr.IsDestroyed)
                        break;

                    eventDelegate(cr);
                }
                catch (Exception ex)
                {
                    Engine.Log(ex.ToString());
                }
            }
        }

        /*public delegate void CritterIdleEventDelegate(Critter critter);
        public delegate void CritterGlobalMapIdleEventDelegate(Critter critter);
        public delegate void CritterCheckMoveItemEventDelegate(Critter critter, Item item, int toSlot);
        public delegate void CritterMoveItemEventDelegate(Critter critter, Item item, int fromSlot);
        public delegate void CritterShowEventDelegate(Critter critter, Critter showCritter);
        public delegate void CritterShowDist1EventDelegate(Critter critter, Critter showCritter);
        public delegate void CritterShowDist2EventDelegate(Critter critter, Critter showCritter);
        public delegate void CritterShowDist3EventDelegate(Critter critter, Critter showCritter);
        public delegate void CritterHideEventDelegate(Critter critter, Critter hideCritter);
        public delegate void CritterHideDist1EventDelegate(Critter critter, Critter hideCritter);
        public delegate void CritterHideDist2EventDelegate(Critter critter, Critter hideCritter);
        public delegate void CritterHideDist3EventDelegate(Critter critter, Critter hideCritter);
        public delegate void CritterShowItemOnMapEventDelegate(Critter critter, Item item, bool added, Critter fromCritter);
        public delegate void CritterHideItemOnMapEventDelegate(Critter critter, Item item, bool removed, Critter toCritter);
        public delegate void CritterChangeItemOnMapEventDelegate(Critter critter, Item item);
        public delegate void CritterMessageEventDelegate(Critter critter, Critter receiver, int num, int value);
        public delegate void CritterTalkEventDelegate(Critter critter, Critter who, bool begin, int talkers);
        public delegate void CritterBarterEventDelegate(Critter critter, Critter trader, bool begin, int barterCount);
        public delegate void PlayerRegistrationEventDelegate(string ip, string name);
        public delegate void PlayerLoginEventDelegate(string ip, string name);
        public delegate void PlayerLogoutEventDelegate(Critter player);
        public delegate void ItemInitEventDelegate(Item item, bool firstTime);
        public delegate void ItemFinishEventDelegate(Item item);
        public delegate void ItemWalkEventDelegate(Item item, Critter critter, bool isIn, int dir);
        public delegate void ItemCheckMoveEventDelegate(Item item, int count, Entity from, Entity to);
        public delegate void StaticItemWalkEventDelegate(Item item, Critter critter, bool isIn, int dir);*/

#elif CLIENT
        /*public delegate void StartEventDelegate();
        public delegate void FinishEventDelegate();
        public delegate void LoopEventDelegate();
        public delegate void GetActiveScreensEventDelegate(int[] screens);
        //public delegate void ScreenChangeEventDelegate( bool show, int screen, dictionary params );
        public delegate void ScreenScrollEventDelegate(int offsetX, int offsetY);
        public delegate void RenderIfaceEventDelegate();
        public delegate void RenderMapEventDelegate();
        public delegate void MouseDownEventDelegate(int button);
        public delegate void MouseUpEventDelegate(int button);
        public delegate void MouseMoveEventDelegate(int offsetX, int offsetY);
        public delegate void KeyDownEventDelegate(byte key, string text);
        public delegate void KeyUpEventDelegate(byte key);
        public delegate void InputLostEventDelegate();
        public delegate void CritterInEventDelegate(Critter critter);
        public delegate void CritterOutEventDelegate(Critter critter);
        public delegate void ItemMapInEventDelegate(Item item);
        public delegate void ItemMapChangedEventDelegate(Item item, Item oldItem);
        public delegate void ItemMapOutEventDelegate(Item item);
        public delegate void ItemInvAllInEventDelegate();
        public delegate void ItemInvInEventDelegate(Item item);
        public delegate void ItemInvChangedEventDelegate(Item item, Item oldItem);
        public delegate void ItemInvOutEventDelegate(Item item);
        public delegate void ReceiveItemsEventDelegate(Item[] items, int param);
        //public delegate void MapMessageEventDelegate( string& text, uint16& hexX, uint16& hexY, uint& color, uint& delay );
        //public delegate void InMessageEventDelegate( string text, int& sayType, uint& critterId, uint& delay );
        //public delegate void OutMessageEventDelegate( string& text, int& sayType );
        //public delegate void MessageBoxEventDelegate( string& text, int type, bool scriptCall );
        public delegate void CombatResultEventDelegate(uint[] result);
        public delegate void ItemCheckMoveEventDelegate(Item item, uint count, Entity from, Entity to);
        public delegate void CritterActionEventDelegate(bool localCall, Critter critter, int action, int actionExt, Item actionItem);
        //public delegate void Animation2dProcessEventDelegate( bool, Critter, uint, uint, Item );
        //public delegate void Animation3dProcessEventDelegate( bool, Critter, uint, uint, Item );
        //public delegate void CritterAnimationEventDelegate( hash, uint, uint, uint&, uint&, int&, int&, string& );
        //public delegate void CritterAnimationSubstituteEventDelegate( hash, uint, uint, hash&, uint&, uint& );
        //public delegate void CritterAnimationFalloutEventDelegate( hash, uint&, uint&, uint&, uint&, uint& );
        //public delegate void CritterCheckMoveItemEventDelegate(  Critter critter,  Item item, uint8 toSlot );
        //public delegate void CritterGetAttackDistantionEventDelegate(  Critter critter,  Item item, uint8 itemMode, uint& dist );*/

#elif MAPPER
        /*public delegate void StartEventDelegate();
        public static event StartEventDelegate StartEvent;
        public delegate void FinishEventDelegate();
        public static event FinishEventDelegate FinishEvent;
        public delegate void LoopEventDelegate();
        public static event LoopEventDelegate LoopEvent;
        //public delegate void ConsoleMessageEventDelegate( string& text );
        public delegate void RenderIfaceEventDelegate(uint layer);
        public static event RenderIfaceEventDelegate RenderIfaceEvent;
        public delegate void RenderMapEventDelegate();
        public static event RenderMapEventDelegate RenderMapEvent;
        public delegate void MouseDownEventDelegate(int button);
        public static event MouseDownEventDelegate MouseDownEvent;
        public delegate void MouseUpEventDelegate(int button);
        public static event MouseUpEventDelegate MouseUpEvent;
        public delegate void MouseMoveEventDelegate(int offsetX, int offsetY);
        public static event MouseMoveEventDelegate MouseMoveEvent;
        public delegate void KeyDownEventDelegate(byte key, string text);
        public static event KeyDownEventDelegate KeyDownEvent;
        public delegate void KeyUpEventDelegate(byte key, string text);
        public static event KeyUpEventDelegate KeyUpEvent;
        public delegate void InputLostEventDelegate();
        public static event InputLostEventDelegate InputLostEvent;
        //public delegate void CritterAnimationEventDelegate( hash, uint, uint, uint&, uint&, int&, int&, string& );
        //public delegate void CritterAnimationSubstituteEventDelegate( hash, uint, uint, hash&, uint&, uint& );
        //public delegate void CritterAnimationFalloutEventDelegate( hash, uint&, uint&, uint&, uint&, uint& );
        public delegate void MapLoadEventDelegate(Map map);
        public static event MapLoadEventDelegate MapLoadEvent;
        public delegate void MapSaveEventDelegate(Map map);
        public static event MapSaveEventDelegate MapSaveEvent;
        public delegate void InspectorPropertiesEventDelegate(Entity entity, int[] properties);
        public static event InspectorPropertiesEventDelegate InspectorPropertiesEvent;*/
#endif
    }
}
