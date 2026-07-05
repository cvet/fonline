#nullable enable

namespace FOnline
{
    // Ported from Engine/Source/Scripting/AngelScript/CoreScripts/Color.fos (the engine Color:: core script):
    // the engine color constants. `ucolor` is a struct, so these are `static readonly` (not `const`), built
    // with the ucolor(r, g, b) constructor from ValueTypeExtensions.cs (alpha defaults to 255).
    //
    // NOTE: AngelScript `namespace Color` is OPEN — the game's Scripts/ColorExt.fos also declares
    // `namespace Color` and adds helpers. C# cannot split a static class across namespaces, so this class is
    // `partial`: when ColorExt is ported, its helpers can extend `FOnline.Color` (the recommended resolution
    // of the "namespace Color clash"). Until then, game modules' `Color.Red`/etc. resolve here via
    // `using FOnline;`.
    public static partial class Color
    {
        public static readonly ucolor Neutral = new ucolor(128, 128, 128);
        public static readonly ucolor Text = new ucolor(80, 255, 0);           // Default text color
        public static readonly ucolor TextFocused = new ucolor(20, 190, 20);   // Default focused text color
        public static readonly ucolor CritterName = new ucolor(193, 193, 205); // Default critter nickname color
        public static readonly ucolor ContourRed = new ucolor(170, 20, 20);
        public static readonly ucolor ContourYellow = new ucolor(170, 170, 20);
        public static readonly ucolor ContourGreen = new ucolor(20, 170, 20);
        public static readonly ucolor LGray = new ucolor(191, 191, 191);
        public static readonly ucolor Gray = new ucolor(147, 147, 147);
        public static readonly ucolor White = new ucolor(255, 255, 255);
        public static readonly ucolor Red = new ucolor(220, 20, 20);
        public static readonly ucolor DRed = new ucolor(190, 20, 20);
        public static readonly ucolor DDRed = new ucolor(122, 20, 20);
        public static readonly ucolor LRed = new ucolor(255, 20, 20);
        public static readonly ucolor Blue = new ucolor(20, 20, 220);
        public static readonly ucolor DBlue = new ucolor(20, 20, 190);
        public static readonly ucolor LBlue = new ucolor(20, 20, 255);
        public static readonly ucolor Green = new ucolor(20, 220, 20);
        public static readonly ucolor DGreen = new ucolor(20, 190, 20);
        public static readonly ucolor DDGreen = new ucolor(20, 122, 20);
        public static readonly ucolor LGreen = new ucolor(20, 255, 20);
        public static readonly ucolor Black = new ucolor(20, 20, 20);
        public static readonly ucolor SBlack = new ucolor(36, 36, 36);
        public static readonly ucolor Dark = new ucolor(68, 68, 68);
        public static readonly ucolor GreenRed = new ucolor(20, 220, 220);
        public static readonly ucolor Sand = new ucolor(163, 131, 20);
        public static readonly ucolor Lemon = new ucolor(255, 255, 20);
        public static readonly ucolor Yellow = new ucolor(220, 220, 20);
    }
}
