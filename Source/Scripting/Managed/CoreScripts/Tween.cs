#nullable enable

namespace FOnline
{
    // Ported from Engine/Source/Scripting/AngelScript/CoreScripts/Tween.fos (the engine Tween:: core script):
    // two animation helpers (a pixel-position Tween and a scalar ValueTween) plus an EasingType enum and the
    // easing functions, used by the GUI core script and generated GUI screens. Consumers call `Tween::Tween`,
    // `Tween::ValueTween`, `Tween::EasingType::OutCubic`, so the AngelScript `namespace Tween` + same-named
    // `class Tween` collapse into one C# class `Tween` (a nested type may not share its enclosing type's name --
    // CS0542), and the rest of the namespace (ValueTween, EasingType, the easing functions) nest inside it. So
    // `Tween::ValueTween` ports to `Tween.ValueTween` and `Tween::EasingType::OutCubic` to
    // `Tween.EasingType.OutCubic`.
    //
    // Depends on nanotime subtraction (`nanotime - nanotime -> timespan`) and `timespan.milliseconds`, provided
    // by the nanotime/timespan partials in ValueTypeExtensions.cs. `.milliseconds` is a long, so the percent
    // math is cast to int to match the AngelScript int64->int arithmetic.
    public class Tween
    {
        public enum EasingType
        {
            Linear,
            InQuad,
            OutQuad,
            InOutQuad,
            OutCubic,
        }

        public void Init(int beginX, int beginY, int endX, int endY, int pixelsPerSecond)
        {
            x1 = beginX;
            y1 = beginY;
            x2 = endX;
            y2 = endY;
            x3 = beginX;
            y3 = beginY;
            end = false;
            pps = pixelsPerSecond;
        }

        public void UpdatePos(int newX, int newY)
        {
            x2 = newX;
            y2 = newY;
            x3 = newX;
            y3 = newY;
        }

        public void Move(bool toEnd)
        {
            if (end == toEnd) {
                return;
            }

            x3 = CurX;
            y3 = CurY;
            end = toEnd;
            startTick = Game.FrameTime;
        }

        public void ForceMove(bool toEnd)
        {
            end = toEnd;
            x3 = (end ? x2 : x1);
            y3 = (end ? y2 : y1);
        }

        public int CurX => x3 + Percent * ((end ? x2 : x1) - x3) / 100;

        public int CurY => y3 + Percent * ((end ? y2 : y1) - y3) / 100;

        public int Dist => Math.RoundToInt((float)System.Math.Sqrt(Math.Pow2((end ? x2 : x1) - x3) + Math.Pow2((end ? y2 : y1) - y3)));

        public int Percent
        {
            get
            {
                int dist = Dist;
                return dist > 0 ? Math.Min((int)((Game.FrameTime - startTick).milliseconds * pps / 1000 * 100 / dist), 100) : 100;
            }
        }

        private bool end;
        private int x1;
        private int y1;
        private int x2;
        private int y2;
        private int x3;
        private int y3;
        private int pps;
        private nanotime startTick;

        public class ValueTween
        {
            public void Reset(int value)
            {
                _From = value;
                _To = value;
                _Current = value;
                _DurationMs = 0;
                _Easing = EasingType.OutCubic;
                _Active = false;
                _StartTick = new nanotime();
            }

            public void MoveTo(int fromValue, int toValue, int durationMs, EasingType easing = EasingType.OutCubic)
            {
                _From = fromValue;
                _To = toValue;
                _Current = fromValue;
                _DurationMs = Math.Max(durationMs, 0);
                _Easing = easing;
                _StartTick = Game.FrameTime;
                _Active = (_From != _To && _DurationMs > 0);

                if (!_Active) {
                    _Current = _To;
                }
            }

            public void SnapTo(int value)
            {
                Reset(value);
            }

            public int Update()
            {
                if (!_Active) {
                    return _Current;
                }

                float t = Math.ClampF((float)((Game.FrameTime - _StartTick).milliseconds) / (float)_DurationMs, 0.0f, 1.0f);
                _Current = Math.Lerp(_From, _To, _ApplyEasing(t));

                if (t >= 1.0f || _Current == _To) {
                    _Current = _To;
                    _Active = false;
                }

                return _Current;
            }

            public bool Active => _Active;

            public int Current => _Current;

            public int Target => _To;

            private float _ApplyEasing(float t)
            {
                switch (_Easing) {
                case EasingType.InQuad:
                    return InQuad(t);
                case EasingType.OutQuad:
                    return OutQuad(t);
                case EasingType.InOutQuad:
                    return InOutQuad(t);
                case EasingType.OutCubic:
                    return OutCubic(t);
                case EasingType.Linear:
                default:
                    return Linear(t);
                }
            }

            private int _From;
            private int _To;
            private int _Current;
            private int _DurationMs;
            private EasingType _Easing;
            private bool _Active;
            private nanotime _StartTick;
        }

        public static float Linear(float t)
        {
            return Math.ClampF(t, 0.0f, 1.0f);
        }

        public static float InQuad(float t)
        {
            t = Linear(t);
            return t * t;
        }

        public static float OutQuad(float t)
        {
            t = Linear(t);
            float inv = 1.0f - t;
            return 1.0f - inv * inv;
        }

        public static float InOutQuad(float t)
        {
            t = Linear(t);

            if (t < 0.5f) {
                return 2.0f * t * t;
            }

            float inv = -2.0f * t + 2.0f;
            return 1.0f - inv * inv / 2.0f;
        }

        public static float OutCubic(float t)
        {
            t = Linear(t);
            float inv = 1.0f - t;
            return 1.0f - inv * inv * inv;
        }
    }
}
