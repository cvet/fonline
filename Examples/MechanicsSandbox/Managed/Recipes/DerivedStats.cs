// Recipe: derived stats, Managed C# shape of Scripts/Recipes/DerivedStats.fos
// Parsed by Engine tests for declaration parity; not compiled by Engine CI
namespace MechanicsSandbox;

using FOnline;

///@ Property Critter Common int32 Strength Mutable Persistent PublicSync
///@ Property Critter Common int32 Endurance Mutable Persistent OwnerSync
///@ Property Critter Common int32 CurLife Mutable Persistent PublicSync
///@ Property Critter Common bool Collapsed Mutable Persistent PublicSync
///@ Property Critter Common int32 CarryWeight Virtual
///@ Property Critter Common int32 MaxLife Virtual
///@ Property Critter Common int32 MaxLifeShown Mutable Persistent PublicSync

public static class DerivedStats
{
    public const int BaseCarryWeight = 20;
    public const int CarryPerStrength = 10;
    public const int BaseLife = 20;
    public const int LifePerEndurance = 5;

#if SERVER || CLIENT
    // Getters are registered on every side that reads the property: a side without one cannot read it
    [ModuleInit]
    internal static void InitDerivedStats()
    {
        Game.SetPropertyGetter(CritterProperty.CarryWeight, GetCarryWeight);
        Game.SetPropertyGetter(CritterProperty.MaxLife, GetMaxLife);
    }

    // Pure functions of replicated inputs: no random, no clock, no other entity
    [PropertyGetter]
    internal static int GetCarryWeight([RequiresCover] Critter cr)
    {
        return BaseCarryWeight + cr.Strength * CarryPerStrength;
    }

    [PropertyGetter]
    internal static int GetMaxLife([RequiresCover] Critter cr)
    {
        return BaseLife + cr.Endurance * LifePerEndurance;
    }
#endif

#if SERVER
    [ModuleInit]
    internal static void InitDerivedStatsServer()
    {
        Game.AddPropertySetter(CritterProperty.CurLife, ClampCurLife);
        Game.AddPropertyDeferredSetter(CritterProperty.CurLife, OnCurLifeChanged);
        Game.AddPropertyDeferredSetter(CritterProperty.Endurance, OnEnduranceChanged);
    }

    // The transforming setter runs before storage and rewrites the proposed value; the entity still holds the old one
    [PropertySetter]
    internal static void ClampCurLife([RequiresCover] Critter cr, ref int value)
    {
        int maxLife = cr.MaxLife;
        if (value > maxLife) {
            value = maxLife;
        }

        if (value < 0) {
            value = 0;
        }
    }

    // The reaction setter runs after storage and sees the committed value; a request clamped back to the stored value still runs it
    [PropertySetter]
    internal static void OnCurLifeChanged([RequiresCover] Critter cr)
    {
        bool collapsed = cr.CurLife == 0;
        if (cr.Collapsed != collapsed) {
            cr.Collapsed = collapsed;
            Game.Log("derived_stats_collapsed_changed critter=" + cr.Id + " collapsed=" + (collapsed ? "true" : "false"));
        }
    }

    // Endurance is owner-only, so observers cannot compute MaxLife; a public mirror carries the result to them
    [PropertySetter]
    internal static void OnEnduranceChanged([RequiresCover] Critter cr)
    {
        cr.MaxLifeShown = cr.MaxLife;

        // A smaller cap also lowers the stored life; the assignment goes through the clamping setter
        if (cr.CurLife > cr.MaxLife) {
            cr.CurLife = cr.MaxLife;
        }
    }
#endif
}
