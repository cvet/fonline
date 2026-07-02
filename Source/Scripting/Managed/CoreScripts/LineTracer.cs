using System.Collections.Generic;

namespace FOnline
{
    // Managed port of the engine AngelScript core script
    // Engine/Source/Scripting/AngelScript/CoreScripts/LineTracer.fos: hex line-tracing utilities used by the
    // server AI/combat modules (Ai, AiAttackMode, Combat, CritterState, GuardDog) via `LineTracer::ITraceContext`
    // and `LineTracer::LineTracerHex`. Mirrors the AngelScript surface name-for-name over the
    // `Game.TraceHexLine` / `Game.GetDistance` natives, so ported callers resolve `LineTracer.*` through their
    // `using FOnline;`. Interface-implementing methods drop the AngelScript `override` (C# interface members are
    // not overrides), and the `ref mpos` out-parameter of the directional `TraceHexLine` overload is supplied a
    // definitely-assigned local.
    public static class LineTracer
    {
        public interface ITraceContext
        {
            void StartExec(Map map, mpos fromHex, mpos toHex, int maxDist);
            void FinishExec(Map map, int resultDist);
            bool Exec(Map map, mpos hex); // Return true to stop tracing
        }

        public class SimpleTracer : ITraceContext
        {
            public mpos ResultHex;

            public void StartExec(Map map, mpos fromHex, mpos toHex, int maxDist)
            {
                ResultHex = fromHex;
            }

            public void FinishExec(Map map, int resultDist)
            {
            }

            public bool Exec(Map map, mpos hex)
            {
                ResultHex = hex;
                return false;
            }
        }

        public class MapBorderTracer : ITraceContext
        {
            public mpos ResultHex;
            public int ResultDist;

            public void StartExec(Map map, mpos fromHex, mpos toHex, int maxDist)
            {
                ResultHex = fromHex;
            }

            public void FinishExec(Map map, int resultDist)
            {
                ResultDist = resultDist;
            }

            public bool Exec(Map map, mpos hex)
            {
                if (map.IsOutsideArea(hex)) {
                    return true;
                }

                ResultHex = hex;
                return false;
            }
        }

        public static int ExecuteHexTrace(Map map, mpos fromHex, mpos toHex, int dist, List<mpos> traceLine, ITraceContext context)
        {
            context.StartExec(map, fromHex, toHex, dist);

            int traceCount = traceLine.Count;
            int resultDist = traceCount;

            for (int i = 0; i < traceCount; i++) {
                if (context.Exec(map, traceLine[i])) {
                    resultDist = i + 1;
                    break;
                }
            }

            context.FinishExec(map, resultDist);
            return resultDist;
        }

        public static int LineTracerHex(Map map, mpos fromHex, mpos toHex, ITraceContext context)
        {
            return LineTracerHex(map, fromHex, toHex, 0, 0.0f, default(ipos), default(ipos), context);
        }

        public static int LineTracerHex(Map map, mpos fromHex, mpos toHex, ipos startOffset, ipos targetOffset, ITraceContext context)
        {
            return LineTracerHex(map, fromHex, toHex, 0, 0.0f, startOffset, targetOffset, context);
        }

        public static int LineTracerHex(Map map, mpos fromHex, mpos toHex, int dist, ITraceContext context)
        {
            return LineTracerHex(map, fromHex, toHex, dist, 0.0f, default(ipos), default(ipos), context);
        }

        public static int LineTracerHex(Map map, mpos fromHex, mpos toHex, int dist, ipos startOffset, ipos targetOffset, ITraceContext context)
        {
            return LineTracerHex(map, fromHex, toHex, dist, 0.0f, startOffset, targetOffset, context);
        }

        public static int LineTracerHex(Map map, mpos fromHex, float dirAngle, int dist, ITraceContext context)
        {
            return LineTracerHex(map, fromHex, dirAngle, dist, default(ipos), default(ipos), context);
        }

        public static int LineTracerHex(Map map, mpos fromHex, mdir dir, int dist, ITraceContext context)
        {
            return LineTracerHex(map, fromHex, (float)(dir.angle), dist, default(ipos), default(ipos), context);
        }

        public static int LineTracerHex(Map map, mpos fromHex, float dirAngle, int dist, ipos startOffset, ipos targetOffset, ITraceContext context)
        {
            mpos toHex = default(mpos);
            List<mpos> traceLine = Game.TraceHexLine(map.Size, fromHex, dirAngle, dist, startOffset, targetOffset, ref toHex);
            return ExecuteHexTrace(map, fromHex, toHex, dist, traceLine, context);
        }

        public static int LineTracerHex(Map map, mpos fromHex, mdir dir, int dist, ipos startOffset, ipos targetOffset, ITraceContext context)
        {
            return LineTracerHex(map, fromHex, (float)(dir.angle), dist, startOffset, targetOffset, context);
        }

        public static int LineTracerHex(Map map, mpos fromHex, mpos toHex, int dist, float dirAngleOffset, ITraceContext context)
        {
            return LineTracerHex(map, fromHex, toHex, dist, dirAngleOffset, default(ipos), default(ipos), context);
        }

        public static int LineTracerHex(Map map, mpos fromHex, mpos toHex, int dist, float dirAngleOffset, ipos startOffset, ipos targetOffset, ITraceContext context)
        {
            if (dist == 0) {
                dist = Game.GetDistance(fromHex, toHex);
            }

            List<mpos> traceLine = Game.TraceHexLine(map.Size, fromHex, toHex, dist, dirAngleOffset, startOffset, targetOffset);
            return ExecuteHexTrace(map, fromHex, toHex, dist, traceLine, context);
        }
    }
}
