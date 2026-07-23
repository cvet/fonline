#nullable enable annotations

using System.Collections.Generic;

namespace FOnline
{
    // AngelScript `string::hstr()` -> the hashed-string handle for that text. The managed mirror is a string
    // extension named `hstr` so a ported `name.hstr()` resolves verbatim (the port tool emits it unchanged).
    // `new hstring(string)` hashes and registers via Native.GetHash, identical to the engine's string::hstr()
    // (it round-trips through hstring.ToString()/Native.GetHashStr). Nullable receivers use the empty string so
    // managed `object.ToString()` callback payloads can follow the same safe fallback path as other string helpers.
    public static class StringExtensions
    {
        public static hstring hstr(this string? value) => new hstring(value ?? string.Empty);

        // AngelScript string find* (AngelScriptString.cpp): findFirstOf/findFirstNotOf/findLastOf/findLastNotOf
        // are char-set searches (`int start`), findLast is a substring search. They return -1 when not found or
        // `start` is out of range. AngelScript indexes by UTF-8 codepoint; C# indexes by UTF-16 code unit --
        // these agree for BMP text (the game's Russian/English), so the C# string methods are exact mirrors.
        public static int findFirstOf(this string s, string chars, int start = 0)
        {
            if (start < 0 || start >= s.Length) {
                return -1;
            }
            return s.IndexOfAny(chars.ToCharArray(), start);
        }

        public static int findFirstNotOf(this string s, string chars, int start = 0)
        {
            if (start < 0) {
                return -1;
            }
            for (int i = start; i < s.Length; i++) {
                if (chars.IndexOf(s[i]) < 0) {
                    return i;
                }
            }
            return -1;
        }

        public static int findLastOf(this string s, string chars, int start = -1)
        {
            if (s.Length == 0) {
                return -1;
            }
            int from = start < 0 || start >= s.Length ? s.Length - 1 : start;
            return s.LastIndexOfAny(chars.ToCharArray(), from);
        }

        public static int findLastNotOf(this string s, string chars, int start = -1)
        {
            int from = start < 0 || start >= s.Length ? s.Length - 1 : start;
            for (int i = from; i >= 0; i--) {
                if (chars.IndexOf(s[i]) < 0) {
                    return i;
                }
            }
            return -1;
        }

        public static int findLast(this string s, string sub, int start = -1)
        {
            int pos = s.LastIndexOf(sub, System.StringComparison.Ordinal);
            return pos >= 0 && pos >= start ? pos : -1;
        }

        // AngelScript `int find(const string &in, int start = 0)` (AngelScriptString.cpp ScriptString_FindFirst): a
        // forward substring search returning -1 when not found or `start` is out of range. Ordinal so the match is
        // byte-for-byte (no culture folding), mirroring std::string::find; `start` is guarded so an out-of-range
        // index returns -1 instead of throwing (IndexUtf8ToRaw fails the same way). C# indexes UTF-16 code units,
        // which agree with the AngelScript UTF-8 codepoint index for the BMP text the scripts use.
        public static int find(this string s, string sub, int start = 0)
        {
            if (start < 0 || start > s.Length) {
                return -1;
            }
            return s.IndexOf(sub, start, System.StringComparison.Ordinal);
        }

        // AngelScript `string substr(int start = 0, int count = -1)` (AngelScriptString.cpp ScriptString_SubString):
        // an out-of-range `start` yields "" and a negative `count` runs to the end; otherwise the count is clamped
        // to the remaining length so an overshoot never throws (C# Substring would otherwise raise).
        public static string substr(this string s, int start = 0, int count = -1)
        {
            if (start < 0 || start >= s.Length) {
                return "";
            }
            int take = count < 0 ? s.Length - start : System.Math.Min(count, s.Length - start);
            return s.Substring(start, take);
        }

        // AngelScript `bool tryToInt(int& result)` / `tryToInt64(int64& result)` (AngelScriptString.cpp): parse the
        // string, write the value into the out parameter, return whether parsing succeeded. Nullable receivers are
        // accepted because managed `object.ToString()` and callback payloads may be nullable; TryParse treats null as
        // a failed parse, which matches the script-side "not a number" path. The port maps the AngelScript `&` to
        // `ref`, so the parameter is `ref` (the caller's variable is already initialized).
        public static bool tryToInt(this string? s, ref int result) => int.TryParse(s, out result);

        public static bool tryToInt64(this string? s, ref long result) => long.TryParse(s, out result);

        // AngelScript `bool tryToFloat(float& result)` (AngelScriptString.cpp, strtof / C locale) -> invariant-culture
        // parse so a '.' decimal separator is honored regardless of the current culture.
        public static bool tryToFloat(this string? s, ref float result) =>
            float.TryParse(s, System.Globalization.NumberStyles.Float, System.Globalization.CultureInfo.InvariantCulture, out result);

        // AngelScript `int rawLength()` (AngelScriptString.cpp) is the std::string BYTE length (UTF-8 bytes), not the
        // UTF-16 char count C# `string.Length` returns -- so it maps to the UTF-8 byte count.
        public static int rawLength(this string s) => System.Text.Encoding.UTF8.GetByteCount(s);

        // AngelScript raw string accessors operate on the underlying UTF-8 byte buffer. C# strings are immutable, so
        // rawResize/rawSet return the mutated string; the port tool rewrites statement-form calls back into the
        // receiver assignment (`s = StringExtensions.rawSet(s, i, ch);`).
        public static byte rawGet(this string s, int index)
        {
            return System.Text.Encoding.UTF8.GetBytes(s)[index];
        }

        public static string rawResize(this string s, int size)
        {
            byte[] bytes = System.Text.Encoding.UTF8.GetBytes(s);
            System.Array.Resize(ref bytes, size);
            return System.Text.Encoding.UTF8.GetString(bytes);
        }

        public static string rawSet(this string s, int index, int value)
        {
            byte[] bytes = System.Text.Encoding.UTF8.GetBytes(s);
            bytes[index] = (byte)(value & 0xFF);
            return System.Text.Encoding.UTF8.GetString(bytes);
        }

        // AngelScript `toInt` / `toInt64` / `toFloat` (AngelScriptString.cpp): parse, or return the default when
        // parsing fails (same contract as the matching tryTo* helpers).
        public static int toInt(this string? s, int defaultValue = 0) => int.TryParse(s, out int v) ? v : defaultValue;

        public static long toInt64(this string? s, long defaultValue = 0) => long.TryParse(s, out long v) ? v : defaultValue;

        public static float toFloat(this string? s, float defaultValue = 0.0f) =>
            float.TryParse(s, System.Globalization.NumberStyles.Float, System.Globalization.CultureInfo.InvariantCulture, out float v) ? v : defaultValue;
    }

    // AngelScript `dict<K,V>.getKey(int index)` / `getValue(int index)` (AngelScriptDict.cpp) return the key/value
    // at an iteration index. C# Dictionary has no indexed access, so these mirror it via the ordered Keys/Values
    // collections. (C# preserves insertion order until a removal, matching the AngelScript ScriptDict iteration the
    // scripts rely on.)
    public static class DictionaryExtensions
    {
        public static K getKey<K, V>(this Dictionary<K, V> dict, int index) where K : notnull =>
            System.Linq.Enumerable.ElementAt(dict.Keys, index);

        public static V getValue<K, V>(this Dictionary<K, V> dict, int index) where K : notnull =>
            System.Linq.Enumerable.ElementAt(dict.Values, index);

        public static List<K> getKeys<K, V>(this Dictionary<K, V> dict) where K : notnull
        {
            return new List<K>(dict.Keys);
        }

        public static void set<K, V>(this Dictionary<K, V> dict, K key, V value) where K : notnull
        {
            dict[key] = value;
        }
    }

    // AngelScript `array<T>.resize(n)` -> a List<T> extension named `resize` so a ported `.resize(n)` resolves
    // verbatim. Grows with default(T) (matching AngelScript's default-constructed fill) or truncates.
    public static class ListExtensions
    {
        public static void resize<T>(this List<T> list, int size)
        {
            if (size < list.Count)
            {
                list.RemoveRange(size, list.Count - size);
            }
            else
            {
                while (list.Count < size)
                {
                    list.Add(default!);
                }
            }
        }

        // AngelScript `array.set(values)` replaces the whole array with the supplied values.
        public static void set<T>(this List<T> list, IEnumerable<T> values)
        {
            list.Clear();
            list.AddRange(values);
        }

        // AngelScript `.clone()` is a shallow copy available on BOTH array and dictionary. The port maps it to this
        // extension, resolved by receiver type, so a `dict.clone()` stays a Dictionary (mapping it to `.ToList()`
        // wrongly produced a `List<KeyValuePair<K, V>>`, CS0029 when assigned back to the dictionary).
        public static List<T> clone<T>(this List<T> list)
        {
            return new List<T>(list);
        }

        public static Dictionary<K, V> clone<K, V>(this Dictionary<K, V> dict) where K : notnull
        {
            return new Dictionary<K, V>(dict);
        }

        // AngelScript `array.reserve(count)` pre-allocates backing storage without changing the element count
        // (AngelScriptArray.cpp). The managed mirror grows List.Capacity (never below the current count, which the
        // property setter would reject); a smaller request is a no-op, matching the reserve-is-a-hint semantics.
        public static void reserve<T>(this List<T> list, int count)
        {
            if (count > list.Capacity)
            {
                list.Capacity = count;
            }
        }

        public static void removeRange<T>(this List<T> list, int index, int count)
        {
            list.RemoveRange(index, count);
        }

        // AngelScript `array.find(value)` / `array.find(int startAt, value)` (AngelScriptArray.cpp ScriptArray::Find)
        // return the index of the first matching element (from `startAt`, default 0), or -1. The argument order is
        // (startAt, value) -- the reverse of C# List<T>.IndexOf(value, startAt) -- so these overloads adapt it; an
        // out-of-range `startAt` yields the empty search and returns -1, matching List.IndexOf's range guard.
        public static int find<T>(this List<T> list, T value)
        {
            return list.IndexOf(value);
        }

        public static int find<T>(this List<T> list, int startAt, T value)
        {
            if (startAt < 0 || startAt > list.Count) {
                return -1;
            }
            return list.IndexOf(value, startAt);
        }

        // AngelScript `array.exists(value)` is membership (AngelScriptArray.cpp), so the array form maps to Contains.
        // The dictionary form `dict.exists(key)` is ContainsKey. Both are resolved here by receiver type, covering the
        // call sites the type-aware port rewrite misses (e.g. a static property-group member receiver).
        public static bool exists<T>(this List<T> list, T value)
        {
            return list.Contains(value);
        }

        public static bool exists<K, V>(this Dictionary<K, V> dict, K key) where K : notnull
        {
            return dict.ContainsKey(key);
        }
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
            set => this.value = (this.value & 0xFFFFFF00u) | (uint)value;
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
    }

    // Spatial value-structs: comparison operators mirror the AngelScript opCmp (lexicographic field order,
    // i.e. std::tie), and ipos/fpos additionally get add/sub/neg. The gen emits ==/!=/Equals/GetHashCode; these
    // add the rest. Multi-field structs that AngelScript does not register an opCmp for (frect, ipos8, ipos16)
    // intentionally get no comparison operators here, preserving parity.
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
    }

    public partial struct isize
    {
        public static bool operator <(isize a, isize b) => a.width != b.width ? a.width < b.width : a.height < b.height;
        public static bool operator >(isize a, isize b) => b < a;
        public static bool operator <=(isize a, isize b) => !(b < a);
        public static bool operator >=(isize a, isize b) => !(a < b);
    }

    public partial struct fsize
    {
        public static bool operator <(fsize a, fsize b) => a.width != b.width ? a.width < b.width : a.height < b.height;
        public static bool operator >(fsize a, fsize b) => b < a;
        public static bool operator <=(fsize a, fsize b) => !(b < a);
        public static bool operator >=(fsize a, fsize b) => !(a < b);
    }

    public partial struct irect
    {
        public static bool operator <(irect a, irect b) =>
            a.x != b.x ? a.x < b.x : a.y != b.y ? a.y < b.y : a.width != b.width ? a.width < b.width : a.height < b.height;
        public static bool operator >(irect a, irect b) => b < a;
        public static bool operator <=(irect a, irect b) => !(b < a);
        public static bool operator >=(irect a, irect b) => !(a < b);
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
        public override string ToString() => x.ToString(System.Globalization.CultureInfo.InvariantCulture) + " " + y.ToString(System.Globalization.CultureInfo.InvariantCulture);
    }

    public partial struct msize
    {
        public static bool operator <(msize a, msize b) => a.width != b.width ? a.width < b.width : a.height < b.height;
        public static bool operator >(msize a, msize b) => b < a;
        public static bool operator <=(msize a, msize b) => !(b < a);
        public static bool operator >=(msize a, msize b) => !(a < b);
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
