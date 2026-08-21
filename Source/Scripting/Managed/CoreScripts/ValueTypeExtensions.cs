#nullable enable annotations

namespace FOnline
{
    // Converts managed text to the engine hashed-string handle through the native hash registry.
    public static class HStringExtensions
    {
        public static hstring hstr(this string? value) => new hstring(value ?? string.Empty);
    }

    // AngelScript `mdir` methods are geometry-dependent. `hex` routes through the engine because square-map
    // rounding is engine-owned; hex rotation only needs the direction count.
    public partial struct mdir
    {
        public mdir(hdir dir)
        {
            angle = global::FOnline.Native.HdirToMdir(dir.value);
        }

        public mdir(int angle)
        {
            int mod = angle % 360;
            this.angle = (short)(mod < 0 ? mod + 360 : mod);
        }

        public hdir hex => new hdir(global::FOnline.Native.MdirHex(angle));

        public static implicit operator mdir(hdir dir) => new mdir(dir);

        public mdir incHex()
        {
            return new mdir(global::FOnline.Native.MdirRotateHex(angle, 1));
        }

        public mdir decHex()
        {
            return new mdir(global::FOnline.Native.MdirRotateHex(angle, -1));
        }

        public mdir rotateHex(int steps)
        {
            return new mdir(global::FOnline.Native.MdirRotateHex(angle, steps));
        }

        public mdir reverse()
        {
            return new mdir(global::FOnline.Native.MdirReverse(angle));
        }
    }

    // Hand-written members for generated engine value-structs that the StructLayout baker cannot derive
    // (it only emits the raw field + its constructor). These mirror the AngelScript registrations in
    // Engine/Source/Scripting/AngelScript/AngelScriptTypes.cpp so a ported module sees the same surface.
    // ==/!=/Equals/GetHashCode come from the generic value-struct emission and are intentionally not
    // repeated here.

    // timespan/nanotime store raw nanoseconds. synctime stores raw milliseconds (TimeRelated.h), so synctime
    // arithmetic must convert timespan deltas to and from milliseconds to match the native ABI.
    public partial struct timespan
    {
        // Unit-tagged constructor mirroring AngelScriptTypes.cpp Time_ConstructWithPlace: `place` selects the
        // unit of `value` (0 ns, 1 us, 2 ms, 3 s) and the stored `value` is normalized to nanoseconds. Used by
        // the Time core script (Time.Milliseconds/Seconds/…).
        public timespan(long value, int place)
        {
            switch (place)
            {
                case 0: this.value = value; break;
                case 1: this.value = value * 1_000L; break;
                case 2: this.value = value * 1_000_000L; break;
                case 3: this.value = value * 1_000_000_000L; break;
                default: throw new System.ArgumentException("Invalid time place");
            }
        }

        public static bool operator <(timespan a, timespan b) => a.value < b.value;
        public static bool operator >(timespan a, timespan b) => a.value > b.value;
        public static bool operator <=(timespan a, timespan b) => a.value <= b.value;
        public static bool operator >=(timespan a, timespan b) => a.value >= b.value;
        public static timespan operator +(timespan a, timespan b) => new timespan(a.value + b.value);
        public static timespan operator -(timespan a, timespan b) => new timespan(a.value - b.value);

        public long nanoseconds => value;
        public long microseconds => value / 1_000L;
        public long milliseconds => value / 1_000_000L;
        public long seconds => value / 1_000_000_000L;

        // Mirrors the engine std::formatter<steady_time_point::duration> (Essentials/TimeRelated.h), which
        // picks the scale from the magnitude. Without it a duration reaches a log through the default
        // ValueType.ToString and prints its type name, which is what the combat timeout lines were showing.
        public override string ToString() => FormatNanoseconds(value);

        internal static string FormatNanoseconds(long ns)
        {
            var culture = System.Globalization.CultureInfo.InvariantCulture;

            if (ns < 1_000_000L) {
                return string.Format(culture, "{0}.{1:000} us", ns / 1_000L % 1_000L, ns % 1_000L);
            }

            if (ns < 1_000_000_000L) {
                return string.Format(culture, "{0}.{1:000} ms", ns / 1_000_000L % 1_000L, ns / 1_000L % 1_000L);
            }

            if (ns < 60_000_000_000L) {
                return string.Format(culture, "{0}.{1:000} sec", ns / 1_000_000_000L, ns / 1_000_000L % 1_000L);
            }

            long totalSeconds = ns / 1_000_000_000L;

            if (totalSeconds < 24L * 60L * 60L) {
                return string.Format(culture, "{0:00}:{1:00}:{2:00} sec", totalSeconds / 3600L, totalSeconds / 60L % 60L, totalSeconds % 60L);
            }

            long days = totalSeconds / (24L * 60L * 60L);

            return string.Format(culture, "{0} day{1} {2:00}:{3:00}:{4:00} sec",
                days, days > 1L ? "s" : "", totalSeconds / 3600L % 24L, totalSeconds / 60L % 60L, totalSeconds % 60L);
        }
    }

    public partial struct synctime
    {
        // Place-based constructor mirroring native synctime: `place` (0 ns, 1 us, 2 ms, 3 s) normalizes `value`
        // to the stored millisecond count. Sub-millisecond inputs truncate just like duration_cast<milliseconds>.
        public synctime(long value, int place)
        {
            switch (place)
            {
                case 0: this.value = value / 1_000_000L; break;
                case 1: this.value = value / 1_000L; break;
                case 2: this.value = value; break;
                case 3: this.value = value * 1_000L; break;
                default: throw new System.ArgumentException("Invalid time place");
            }
        }

        public static bool operator <(synctime a, synctime b) => a.value < b.value;
        public static bool operator >(synctime a, synctime b) => a.value > b.value;
        public static bool operator <=(synctime a, synctime b) => a.value <= b.value;
        public static bool operator >=(synctime a, synctime b) => a.value >= b.value;
        public static synctime operator +(synctime a, timespan b) => new synctime(a.value + b.milliseconds);
        public static synctime operator -(synctime a, timespan b) => new synctime(a.value - b.milliseconds);
        public static timespan operator -(synctime a, synctime b) => new timespan(a.value - b.value, 2);

        public long milliseconds => value;
        public long seconds => value / 1_000L;
        public timespan timeSinceEpoch => new timespan(value, 2);

        // The native formatter renders a synctime through its duration value, and synctime stores milliseconds.
        public override string ToString() => timespan.FormatNanoseconds(value * 1_000_000L);
    }

    public partial struct ident
    {
        public ident(string text)
        {
            text = text.Trim();
            if (text.Length == 0)
            {
                value = 0;
                return;
            }
            if (text.StartsWith("0x", System.StringComparison.OrdinalIgnoreCase))
            {
                value = long.Parse(text.Substring(2), System.Globalization.NumberStyles.HexNumber, System.Globalization.CultureInfo.InvariantCulture);
            }
            else
            {
                value = long.Parse(text, System.Globalization.NumberStyles.Integer, System.Globalization.CultureInfo.InvariantCulture);
            }
        }

        public override string ToString()
        {
            return value.ToString(System.Globalization.CultureInfo.InvariantCulture);
        }
    }

    // ucolor is a union of a uint32 `value` (rgba) and four bytes laid out r, g, b, a from the low byte up
    // (ExtendedTypes.h), so on the little-endian targets the components pack as r | g<<8 | b<<16 | a<<24.
    public partial struct ucolor
    {
        public ucolor(int r, int g, int b, int a = 255)
        {
            value = (uint)((r & 0xFF) | ((g & 0xFF) << 8) | ((b & 0xFF) << 16) | ((a & 0xFF) << 24));
        }

        // red/green/blue/alpha are read/write (AngelScript registers them as direct-field properties on ucolor and
        // game scripts assign `color.alpha = ...`); the setter param `value` is the byte component, `this.value` the
        // packed uint. The per-channel mask clears that channel's byte before OR-ing in the shifted new value.
        public byte red
        {
            get => (byte)(value & 0xFF);
            set => this.value = (this.value & 0xFFFFFF00u) | value;
        }
        public byte green
        {
            get => (byte)((value >> 8) & 0xFF);
            set => this.value = (this.value & 0xFFFF00FFu) | ((uint)value << 8);
        }
        public byte blue
        {
            get => (byte)((value >> 16) & 0xFF);
            set => this.value = (this.value & 0xFF00FFFFu) | ((uint)value << 16);
        }
        public byte alpha
        {
            get => (byte)((value >> 24) & 0xFF);
            set => this.value = (this.value & 0x00FFFFFFu) | ((uint)value << 24);
        }

        // Mirrors the engine std::formatter<ucolor> ("0x{:x}", Essentials/ExtendedTypes.h) that backs AngelScript's
        // value->string path (Type_GetStr / Type_AnyConv via strex), so a ported `"@color:" + color` yields
        // `@color:0x<hex>@` exactly as before (the FontManager @color tag parser reads the value with strtoul base 16).
        public override string ToString() => $"0x{value:x}";
    }

    // nanotime is a nanosecond timestamp (like synctime above): comparisons operate on the raw `value`,
    // arithmetic is with timespan deltas, and subtracting two nanotimes yields a timespan. Mirrors the
    // AngelScriptTypes.cpp nanotime registration.
    public partial struct nanotime
    {
        public static bool operator <(nanotime a, nanotime b) => a.value < b.value;
        public static bool operator >(nanotime a, nanotime b) => a.value > b.value;
        public static bool operator <=(nanotime a, nanotime b) => a.value <= b.value;
        public static bool operator >=(nanotime a, nanotime b) => a.value >= b.value;
        public static nanotime operator +(nanotime a, timespan b) => new nanotime(a.value + b.value);
        public static nanotime operator -(nanotime a, timespan b) => new nanotime(a.value - b.value);
        public static timespan operator -(nanotime a, nanotime b) => new timespan(a.value - b.value);

        public long nanoseconds => value;
        public long microseconds => value / 1_000L;
        public long milliseconds => value / 1_000_000L;
        public long seconds => value / 1_000_000_000L;
        public timespan timeSinceEpoch => new timespan(value);

        public override string ToString() => timespan.FormatNanoseconds(value);
    }

    // Spatial value-structs: comparison operators mirror the AngelScript opCmp (lexicographic field order,
    // i.e. std::tie), and ipos/fpos additionally get add/sub/neg. The gen emits ==/!=/Equals/GetHashCode; these
    // add the rest. Multi-field structs that AngelScript does not register an opCmp for (frect, ipos8, ipos16)
    // intentionally get no comparison operators here, preserving parity.
    //
    // ToString is a separate question from opCmp parity: every one of these has a native std::formatter
    // (FO_DECLARE_TYPE_FORMATTER in Essentials/ExtendedTypes.h and Common/Geometry.h) that renders the fields
    // space-separated, so a struct without the managed mirror reaches a log as its own type name instead. That
    // is silent -- the line still prints -- so the mirrors are kept complete rather than added on demand.
    internal static class SpatialFormat
    {
        public static string Fields(params object[] fields)
        {
            var parts = new string[fields.Length];

            for (int i = 0; i < fields.Length; i++) {
                parts[i] = System.Convert.ToString(fields[i], System.Globalization.CultureInfo.InvariantCulture) ?? string.Empty;
            }

            return string.Join(" ", parts);
        }
    }

    public partial struct ipos8
    {
        public override string ToString() => SpatialFormat.Fields(x, y);
    }

    public partial struct ipos16
    {
        public override string ToString() => SpatialFormat.Fields(x, y);
    }

    public partial struct frect
    {
        public override string ToString() => SpatialFormat.Fields(x, y, width, height);
    }

    public partial struct ipos
    {
        public static bool operator <(ipos a, ipos b) => a.x != b.x ? a.x < b.x : a.y < b.y;
        public static bool operator >(ipos a, ipos b) => b < a;
        public static bool operator <=(ipos a, ipos b) => !(b < a);
        public static bool operator >=(ipos a, ipos b) => !(a < b);
        public static ipos operator +(ipos a, ipos b) => new ipos(a.x + b.x, a.y + b.y);
        public static ipos operator -(ipos a, ipos b) => new ipos(a.x - b.x, a.y - b.y);
        public static ipos operator +(ipos a, isize b) => new ipos(a.x + b.width, a.y + b.height);
        public static ipos operator -(ipos a, isize b) => new ipos(a.x - b.width, a.y - b.height);
        public static ipos operator -(ipos a) => new ipos(-a.x, -a.y);

        public override string ToString() => SpatialFormat.Fields(x, y);
    }

    public partial struct fpos
    {
        public static bool operator <(fpos a, fpos b) => a.x != b.x ? a.x < b.x : a.y < b.y;
        public static bool operator >(fpos a, fpos b) => b < a;
        public static bool operator <=(fpos a, fpos b) => !(b < a);
        public static bool operator >=(fpos a, fpos b) => !(a < b);
        public static fpos operator +(fpos a, fpos b) => new fpos(a.x + b.x, a.y + b.y);
        public static fpos operator -(fpos a, fpos b) => new fpos(a.x - b.x, a.y - b.y);
        public static fpos operator -(fpos a) => new fpos(-a.x, -a.y);

        public override string ToString() => SpatialFormat.Fields(x, y);
    }

    public partial struct isize
    {
        public static bool operator <(isize a, isize b) => a.width != b.width ? a.width < b.width : a.height < b.height;
        public static bool operator >(isize a, isize b) => b < a;
        public static bool operator <=(isize a, isize b) => !(b < a);
        public static bool operator >=(isize a, isize b) => !(a < b);

        public override string ToString() => SpatialFormat.Fields(width, height);
    }

    public partial struct fsize
    {
        public static bool operator <(fsize a, fsize b) => a.width != b.width ? a.width < b.width : a.height < b.height;
        public static bool operator >(fsize a, fsize b) => b < a;
        public static bool operator <=(fsize a, fsize b) => !(b < a);
        public static bool operator >=(fsize a, fsize b) => !(a < b);

        public override string ToString() => SpatialFormat.Fields(width, height);
    }

    public partial struct irect
    {
        public static bool operator <(irect a, irect b) =>
            a.x != b.x ? a.x < b.x : a.y != b.y ? a.y < b.y : a.width != b.width ? a.width < b.width : a.height < b.height;
        public static bool operator >(irect a, irect b) => b < a;
        public static bool operator <=(irect a, irect b) => !(b < a);
        public static bool operator >=(irect a, irect b) => !(a < b);

        public override string ToString() => SpatialFormat.Fields(x, y, width, height);
    }

    public partial struct mpos
    {
        public mpos(string text)
        {
            string[] parts = text.Split(' ', System.StringSplitOptions.RemoveEmptyEntries);
            x = short.Parse(parts[0], System.Globalization.CultureInfo.InvariantCulture);
            y = short.Parse(parts[1], System.Globalization.CultureInfo.InvariantCulture);
        }

        public static bool operator <(mpos a, mpos b) => a.x != b.x ? a.x < b.x : a.y < b.y;
        public static bool operator >(mpos a, mpos b) => b < a;
        public static bool operator <=(mpos a, mpos b) => !(b < a);
        public static bool operator >=(mpos a, mpos b) => !(a < b);
        public bool fitTo(msize size) => x >= 0 && y >= 0 && x < size.width && y < size.height;
        public override string ToString() => SpatialFormat.Fields(x, y);
    }

    public partial struct msize
    {
        public static bool operator <(msize a, msize b) => a.width != b.width ? a.width < b.width : a.height < b.height;
        public static bool operator >(msize a, msize b) => b < a;
        public static bool operator <=(msize a, msize b) => !(b < a);
        public static bool operator >=(msize a, msize b) => !(a < b);

        public override string ToString() => SpatialFormat.Fields(width, height);
    }

    // Convenience constructors for TextPackKey mirroring the C++ defaulted-arg ctors (TextPack.h) and the
    // AngelScript FromPack registration: the generated value-struct emits only the raw 4-field ctor, so a ported
    // `TextPackKey(TextPackName.Game, "Key")` (1-2 keys) needs these. String overloads hash via `new hstring(...)`
    // exactly as TextPackKey::FromParts/ToHashedString does; omitted keys default to the empty hstring.
    public partial struct TextPackKey
    {
        public TextPackKey(TextPackName collection, hstring key1) : this(collection, key1, default, default) { }
        public TextPackKey(TextPackName collection, hstring key1, hstring key2) : this(collection, key1, key2, default) { }
        public TextPackKey(TextPackName collection, string key1) : this(collection, new hstring(key1), default, default) { }
        public TextPackKey(TextPackName collection, string key1, string key2) : this(collection, new hstring(key1), new hstring(key2), default) { }

        public override string ToString() => $"{{{Collection}}}{{{Key1}}}{{{Key2}}}{{{Key3}}}";
    }
}
