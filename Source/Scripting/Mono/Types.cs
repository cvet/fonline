using System;

namespace FOnline
{
    public struct hash 
    {
        public uint Value { get; private set; }

        public override string ToString()
        {
            return Game.GetHashStr(Value);
        }
    }

    // mpos -> MapId, HexX, HexY, Height
    // wpos
    // point
    // xyz
    // ivec3 X Y Z
    // ivec2 X Y
}
