#nullable enable

namespace FOnline
{
    // Ported from Engine/Source/Scripting/AngelScript/CoreScripts/Math.fos (the engine Math:: core script).
    // Kept name-for-name and behaviour-for-behaviour so ported game modules' `Math.Foo(...)` calls resolve
    // (the AngelScript `Math::Foo`). The AngelScript `math::` addon maps to System.Math (fully qualified to
    // avoid shadowing this class); AngelScript `int(x)` truncation maps to `(int)x`. No `using System;` here,
    // so a bare `Math` stays this class rather than becoming ambiguous with System.Math.
    public static class Math
    {
        public static int Pow2(int x) => x * x;
        public static float Pow2F(float x) => x * x;
        public static int Pow3(int x) => x * x * x;
        public static float Pow3F(float x) => x * x * x;

        public static int Min(int x, int y) => x < y ? x : y;
        public static float MinF(float x, float y) => x < y ? x : y;
        public static int Max(int x, int y) => x > y ? x : y;
        public static float MaxF(float x, float y) => x > y ? x : y;

        public static int Clamp(int x, int min, int max) => x > max ? max : (x < min ? min : x);
        public static float ClampF(float x, float min, float max) => x > max ? max : (x < min ? min : x);

        public static int Abs(int x) => x > 0 ? x : -x;
        public static float AbsF(float x) => x > 0 ? x : -x;

        public static int Distance(int x, int y) => RoundToInt((float)System.Math.Sqrt(Pow2(x) + Pow2(y)));
        public static int Distance(int x1, int y1, int x2, int y2) => RoundToInt((float)System.Math.Sqrt(Pow2(x1 - x2) + Pow2(y1 - y2)));
        public static int Distance(ipos pos1, ipos pos2) => RoundToInt((float)System.Math.Sqrt(Pow2(pos1.x - pos2.x) + Pow2(pos1.y - pos2.y)));
        public static float DistanceF(float x, float y) => (float)System.Math.Sqrt(Pow2F(x) + Pow2F(y));
        public static float DistanceF(float x1, float y1, float x2, float y2) => (float)System.Math.Sqrt(Pow2F(x1 - x2) + Pow2F(y1 - y2));
        public static float Distance(fpos pos1, fpos pos2) => RoundToInt((float)System.Math.Sqrt(Pow2F(pos1.x - pos2.x) + Pow2F(pos1.y - pos2.y)));

        public static bool IsCollision(int x1, int y1, int x2, int y2, int w, int h) => x1 >= x2 && x1 < x2 + w && y1 >= y2 && y1 < y2 + h;
        public static bool IsCollision(ipos pos1, ipos pos2, isize size) => pos1.x >= pos2.x && pos1.x < pos2.x + size.width && pos1.y >= pos2.y && pos1.y < pos2.y + size.height;

        public static int LbsToGramm(int lbs) => lbs * 453;
        public static int NumericalNumber(int num) => num % 2 != 0 ? num * num / 2 + 1 : num * num / 2 + num / 2;

        // Targets non-negative inputs (sqrt outputs); for those `x - (int)x` is the fractional part and matches
        // the AngelScript math::fraction + int(x) pair used in the source.
        public static int RoundToInt(float x)
        {
            float fraction = x - (int)x;
            return fraction >= 0.5f ? (int)x + 1 : (int)x;
        }

        public static int Fib(int n)
        {
            int x = 1;
            int y = 0;

            for (int i = 1; i < n; i++)
            {
                x += y;
                y = x - y;
            }

            return y;
        }

        public static int Lerp(int v1, int v2, float t) => (t <= 0.0f) ? v1 : ((t >= 1.0f) ? v2 : v1 + (int)((v2 - v1) * t));
        public static float LerpF(float v1, float v2, float t) => (t <= 0.0f) ? v1 : ((t >= 1.0f) ? v2 : v1 + (v2 - v1) * t);
        public static ipos Lerp(ipos v1, ipos v2, float t) => new ipos(Lerp(v1.x, v2.x, t), Lerp(v1.y, v2.y, t));
        public static fpos Lerp(fpos v1, fpos v2, float t) => new fpos(LerpF(v1.x, v2.x, t), LerpF(v1.y, v2.y, t));

        // Signed shortest difference between two directions in degrees, wrapped to [-180, 180].
        public static float AngleDelta(float from, float to)
        {
            float delta = to - from;

            while (delta > 180.0f)
            {
                delta -= 360.0f;
            }

            while (delta < -180.0f)
            {
                delta += 360.0f;
            }

            return delta;
        }

        // Middle of the smaller arc between two directions in degrees.
        public static float AngleBisect(float a, float b) => a + AngleDelta(a, b) * 0.5f;

        public static float MoveTowards(float current, float target, float step)
        {
            float delta = target - current;

            if (System.Math.Abs(delta) <= step)
            {
                return target;
            }

            return current + (delta > 0 ? step : -step);
        }

        public static fpos MoveTowards(fpos v1, fpos v2, float step)
        {
            float dx = v2.x - v1.x;
            float dy = v2.y - v1.y;
            float distance = (float)System.Math.Sqrt(dx * dx + dy * dy);

            if (distance == 0 || step >= distance)
            {
                return v2;
            }
            else
            {
                return new fpos(v1.x + (dx / distance) * step, v1.y + (dy / distance) * step);
            }
        }

        public static int DistanceToRect(ipos p, irect r)
        {
            float dx = MaxF(MaxF(r.x - p.x, p.x - (r.x + r.width)), 0.0f);
            float dy = MaxF(MaxF(r.y - p.y, p.y - (r.y + r.height)), 0.0f);
            return RoundToInt((float)System.Math.Sqrt(dx * dx + dy * dy));
        }
    }
}
