using System;

namespace FOnline
{
    public struct hash 
    {
        public uint Value { get; private set; }

        public hash(uint value)
        {
            Value = value;
        }

        public override string ToString()
        {
            return Game.GetHashStr(Value);
        }
    }
}
