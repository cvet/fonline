#nullable enable

using System.Collections.Generic;
using System.Linq;

namespace FOnline
{
    // Hand-finished port of Engine/Source/Scripting/AngelScript/CoreScripts/Serializator.fos. Byte-level
    // (de)serialization over a List<byte> buffer. Manual changes over the port draft:
    //  - string Set/Get use UTF-8 byte conversion (AngelScript's raw-byte string access has no C# equivalent);
    //  - the integer Set/Get bit-packing gets explicit (byte)/(long)/(ulong)/(uint) casts because C# promotes a
    //    `byte` shift operand to *signed* int while AngelScript promotes uint8 to *unsigned*, so the casts
    //    reproduce the original zero-extension and narrowing semantics (and satisfy C#'s strict store typing);
    //  - the array Get overloads read each element into a local before storing it back, because a List<T> indexer
    //    is not a ref-returning location (AngelScript's `Get(values[i])` inout has no direct C# form).
    // The namespace is FOnline (engine CoreScript convention). Cache/Time/hstring all resolve through the
    // generated managed surface; `resize` is the List<T> extension in ValueTypeExtensions.
    public class Serializator
    {
        private List<byte> dataArray;
        private int curPos;
        private int bufSize;
        private int dataSize;

        public Serializator()
        {
            dataArray = new List<byte>();
            curPos = 0;
            bufSize = 0;
            dataSize = 0;
        }

        public Serializator(int approxSize)
        {
            dataArray = new List<byte>();
            curPos = 0;
            bufSize = 0;
            dataSize = 0;
            GrowBuffer(approxSize);
        }

        public void GrowBuffer(int size)
        {
            if (size <= bufSize) {
                return;
            }
            if (bufSize == 0) {
                bufSize = 1;
            }
            while (size > bufSize) {
                bufSize *= 2;
            }
            dataArray.resize(bufSize);
        }

#if CLIENT
        public void SaveToCache(string name)
        {
            Game.SetCacheData(name, dataArray, dataSize);
            Clear();
        }

        public int LoadFromCache(string name)
        {
            Clear();
            dataArray = Game.GetCacheData(name);
            bufSize = dataArray.Count;
            dataSize = bufSize;
            return bufSize;
        }
#endif

        public List<byte> GetData()
        {
            List<byte> data = dataArray.ToList();
            data.resize(dataSize);
            return data;
        }

        public int SetData(List<byte> data)
        {
            dataArray = data.ToList();
            bufSize = data.Count;
            dataSize = data.Count;
            return dataSize;
        }

        public void Clear()
        {
            curPos = 0;
            bufSize = 0;
            dataSize = 0;
        }

        public Serializator SetCurPos(int pos)
        {
            GrowBuffer(pos);
            curPos = pos;
            if (curPos > dataSize) {
                dataSize = curPos;
            }
            return this;
        }

        public Serializator Fill(byte value, int length)
        {
            GrowBuffer(curPos + length);
            for (int i = 0; i < length; i++) {
                dataArray[curPos++] = value;
            }
            if (curPos > dataSize) {
                dataSize = curPos;
            }
            return this;
        }

        public Serializator Set(in long value)
        {
            GrowBuffer(curPos + 8);
            dataArray[curPos++] = (byte)((value >> 56) & 0xFF);
            dataArray[curPos++] = (byte)((value >> 48) & 0xFF);
            dataArray[curPos++] = (byte)((value >> 40) & 0xFF);
            dataArray[curPos++] = (byte)((value >> 32) & 0xFF);
            dataArray[curPos++] = (byte)((value >> 24) & 0xFF);
            dataArray[curPos++] = (byte)((value >> 16) & 0xFF);
            dataArray[curPos++] = (byte)((value >> 8) & 0xFF);
            dataArray[curPos++] = (byte)(value & 0xFF);
            if (curPos > dataSize) {
                dataSize = curPos;
            }
            return this;
        }

        public Serializator Set(in int value)
        {
            GrowBuffer(curPos + 4);
            dataArray[curPos++] = (byte)((value >> 24) & 0xFF);
            dataArray[curPos++] = (byte)((value >> 16) & 0xFF);
            dataArray[curPos++] = (byte)((value >> 8) & 0xFF);
            dataArray[curPos++] = (byte)(value & 0xFF);
            if (curPos > dataSize) {
                dataSize = curPos;
            }
            return this;
        }

        public Serializator Set(in short value)
        {
            GrowBuffer(curPos + 2);
            dataArray[curPos++] = (byte)((value >> 8) & 0xFF);
            dataArray[curPos++] = (byte)(value & 0xFF);
            if (curPos > dataSize) {
                dataSize = curPos;
            }
            return this;
        }

        public Serializator Set(in sbyte value)
        {
            GrowBuffer(curPos + 1);
            dataArray[curPos++] = (byte)value;
            if (curPos > dataSize) {
                dataSize = curPos;
            }
            return this;
        }

        public Serializator Set(in ulong value)
        {
            GrowBuffer(curPos + 8);
            dataArray[curPos++] = (byte)((value >> 56) & 0xFF);
            dataArray[curPos++] = (byte)((value >> 48) & 0xFF);
            dataArray[curPos++] = (byte)((value >> 40) & 0xFF);
            dataArray[curPos++] = (byte)((value >> 32) & 0xFF);
            dataArray[curPos++] = (byte)((value >> 24) & 0xFF);
            dataArray[curPos++] = (byte)((value >> 16) & 0xFF);
            dataArray[curPos++] = (byte)((value >> 8) & 0xFF);
            dataArray[curPos++] = (byte)(value & 0xFF);
            if (curPos > dataSize) {
                dataSize = curPos;
            }
            return this;
        }

        public Serializator Set(in uint value)
        {
            GrowBuffer(curPos + 4);
            dataArray[curPos++] = (byte)((value >> 24) & 0xFF);
            dataArray[curPos++] = (byte)((value >> 16) & 0xFF);
            dataArray[curPos++] = (byte)((value >> 8) & 0xFF);
            dataArray[curPos++] = (byte)(value & 0xFF);
            if (curPos > dataSize) {
                dataSize = curPos;
            }
            return this;
        }

        public Serializator Set(in ushort value)
        {
            GrowBuffer(curPos + 2);
            dataArray[curPos++] = (byte)((value >> 8) & 0xFF);
            dataArray[curPos++] = (byte)(value & 0xFF);
            if (curPos > dataSize) {
                dataSize = curPos;
            }
            return this;
        }

        public Serializator Set(in byte value)
        {
            GrowBuffer(curPos + 1);
            dataArray[curPos++] = value;
            if (curPos > dataSize) {
                dataSize = curPos;
            }
            return this;
        }

        public Serializator Set(in bool value)
        {
            GrowBuffer(curPos + 1);
            dataArray[curPos++] = (byte)(value ? 1 : 0);
            if (curPos > dataSize) {
                dataSize = curPos;
            }
            return this;
        }

        public Serializator Set(in string value)
        {
            byte[] bytes = System.Text.Encoding.UTF8.GetBytes(value);
            int len = bytes.Length;
            GrowBuffer(curPos + len + 1);
            for (int i = 0; i < len; i++) {
                dataArray[curPos++] = bytes[i];
            }
            dataArray[curPos++] = 0;
            if (curPos > dataSize) {
                dataSize = curPos;
            }
            return this;
        }

        public Serializator Set(in hstring value)
        {
            return Set(value.ToString());
        }

        public Serializator Set(in ident value)
        {
            return Set(value.value);
        }

        public Serializator Set(in timespan value)
        {
            return Set(value.nanoseconds);
        }

        public Serializator Set(in synctime value)
        {
            return Set(value.milliseconds);
        }

        public Serializator Set(in ucolor value)
        {
            return Set(value.value);
        }

        public Serializator Set(in object value)
        {
            return Set(value.ToString() ?? "");
        }

        public Serializator Set(List<long> values)
        {
            int valuesLen = values.Count;
            int len = 4 + valuesLen * 8;
            GrowBuffer(curPos + len);
            Set(valuesLen);
            for (int i = 0; i < valuesLen; i++) {
                Set(values[i]);
            }
            if (curPos > dataSize) {
                dataSize = curPos;
            }
            return this;
        }

        public Serializator Set(List<int> values)
        {
            int valuesLen = values.Count;
            int len = 4 + valuesLen * 4;
            GrowBuffer(curPos + len);
            Set(valuesLen);
            for (int i = 0; i < valuesLen; i++) {
                Set(values[i]);
            }
            if (curPos > dataSize) {
                dataSize = curPos;
            }
            return this;
        }

        public Serializator Set(List<short> values)
        {
            int valuesLen = values.Count;
            int len = 4 + valuesLen * 2;
            GrowBuffer(curPos + len);
            Set(valuesLen);
            for (int i = 0; i < valuesLen; i++) {
                Set(values[i]);
            }
            if (curPos > dataSize) {
                dataSize = curPos;
            }
            return this;
        }

        public Serializator Set(List<sbyte> values)
        {
            int valuesLen = values.Count;
            int len = 4 + valuesLen;
            GrowBuffer(curPos + len);
            Set(valuesLen);
            for (int i = 0; i < valuesLen; i++) {
                Set(values[i]);
            }
            if (curPos > dataSize) {
                dataSize = curPos;
            }
            return this;
        }

        public Serializator Set(List<ulong> values)
        {
            int valuesLen = values.Count;
            int len = 4 + valuesLen * 8;
            GrowBuffer(curPos + len);
            Set(valuesLen);
            for (int i = 0; i < valuesLen; i++) {
                Set(values[i]);
            }
            if (curPos > dataSize) {
                dataSize = curPos;
            }
            return this;
        }

        public Serializator Set(List<uint> values)
        {
            int valuesLen = values.Count;
            int len = 4 + valuesLen * 4;
            GrowBuffer(curPos + len);
            Set(valuesLen);
            for (int i = 0; i < valuesLen; i++) {
                Set(values[i]);
            }
            if (curPos > dataSize) {
                dataSize = curPos;
            }
            return this;
        }

        public Serializator Set(List<ushort> values)
        {
            int valuesLen = values.Count;
            int len = 4 + valuesLen * 2;
            GrowBuffer(curPos + len);
            Set(valuesLen);
            for (int i = 0; i < valuesLen; i++) {
                Set(values[i]);
            }
            if (curPos > dataSize) {
                dataSize = curPos;
            }
            return this;
        }

        public Serializator Set(List<byte> values)
        {
            int valuesLen = values.Count;
            int len = 4 + valuesLen;
            GrowBuffer(curPos + len);
            Set(valuesLen);
            for (int i = 0; i < valuesLen; i++) {
                Set(values[i]);
            }
            if (curPos > dataSize) {
                dataSize = curPos;
            }
            return this;
        }

        public Serializator Set(List<bool> values)
        {
            int valuesLen = values.Count;
            int len = 4 + valuesLen;
            GrowBuffer(curPos + len);
            Set(valuesLen);
            for (int i = 0; i < valuesLen; i++) {
                Set(values[i]);
            }
            if (curPos > dataSize) {
                dataSize = curPos;
            }
            return this;
        }

        public Serializator Set(List<string> values)
        {
            int valuesLen = values.Count;
            int len = 4 + valuesLen; // Length and zeros
            for (int i = 0; i < valuesLen; i++) {
                len += values[i].Length;
            }
            GrowBuffer(curPos + len);
            Set(valuesLen);
            for (int i = 0; i < valuesLen; i++) {
                Set(values[i]);
            }
            if (curPos > dataSize) {
                dataSize = curPos;
            }
            return this;
        }

        public Serializator Set(List<hstring> values)
        {
            int valuesLen = values.Count;
            int len = 4 + valuesLen; // Length and zeros
            for (int i = 0; i < valuesLen; i++) {
                len += values[i].ToString().Length;
            }
            GrowBuffer(curPos + len);
            Set(valuesLen);
            for (int i = 0; i < valuesLen; i++) {
                Set(values[i]);
            }
            if (curPos > dataSize) {
                dataSize = curPos;
            }
            return this;
        }

        public Serializator Set(List<ident> values)
        {
            int valuesLen = values.Count;
            int len = 4 + valuesLen * 8;
            GrowBuffer(curPos + len);
            Set(valuesLen);
            for (int i = 0; i < valuesLen; i++) {
                Set(values[i]);
            }
            if (curPos > dataSize) {
                dataSize = curPos;
            }
            return this;
        }

        public Serializator Get(ref long value)
        {
            value = 0;
            if (curPos + 8 > dataSize) {
                return this;
            }
            value |= (long)(dataArray[curPos++]) << 56;
            value |= (long)(dataArray[curPos++]) << 48;
            value |= (long)(dataArray[curPos++]) << 40;
            value |= (long)(dataArray[curPos++]) << 32;
            value |= (long)(dataArray[curPos++]) << 24;
            value |= (long)(dataArray[curPos++]) << 16;
            value |= (long)(dataArray[curPos++]) << 8;
            value |= (long)(dataArray[curPos++]);
            return this;
        }

        public Serializator Get(ref int value)
        {
            value = 0;
            if (curPos + 4 > dataSize) {
                return this;
            }
            value |= dataArray[curPos++] << 24;
            value |= dataArray[curPos++] << 16;
            value |= dataArray[curPos++] << 8;
            value |= dataArray[curPos++];
            return this;
        }

        public Serializator Get(ref short value)
        {
            value = 0;
            if (curPos + 2 > dataSize) {
                return this;
            }
            int hi = dataArray[curPos++];
            int lo = dataArray[curPos++];
            value = (short)((hi << 8) | lo);
            return this;
        }

        public Serializator Get(ref sbyte value)
        {
            value = 0;
            if (curPos + 1 > dataSize) {
                return this;
            }
            value = (sbyte)dataArray[curPos++];
            return this;
        }

        public Serializator Get(ref ulong value)
        {
            value = 0;
            if (curPos + 8 > dataSize) {
                return this;
            }
            value |= (ulong)(dataArray[curPos++]) << 56;
            value |= (ulong)(dataArray[curPos++]) << 48;
            value |= (ulong)(dataArray[curPos++]) << 40;
            value |= (ulong)(dataArray[curPos++]) << 32;
            value |= (ulong)(dataArray[curPos++]) << 24;
            value |= (ulong)(dataArray[curPos++]) << 16;
            value |= (ulong)(dataArray[curPos++]) << 8;
            value |= (ulong)(dataArray[curPos++]);
            return this;
        }

        public Serializator Get(ref uint value)
        {
            value = 0;
            if (curPos + 4 > dataSize) {
                return this;
            }
            value |= (uint)dataArray[curPos++] << 24;
            value |= (uint)dataArray[curPos++] << 16;
            value |= (uint)dataArray[curPos++] << 8;
            value |= (uint)dataArray[curPos++];
            return this;
        }

        public Serializator Get(ref ushort value)
        {
            value = 0;
            if (curPos + 2 > dataSize) {
                return this;
            }
            int hi = dataArray[curPos++];
            int lo = dataArray[curPos++];
            value = (ushort)((hi << 8) | lo);
            return this;
        }

        public Serializator Get(ref byte value)
        {
            value = 0;
            if (curPos + 1 > dataSize) {
                return this;
            }
            value = dataArray[curPos++];
            return this;
        }

        public Serializator Get(ref bool value)
        {
            value = false;
            if (curPos + 1 > dataSize) {
                return this;
            }
            value = dataArray[curPos++] == 1 ? true : false;
            return this;
        }

        public Serializator Get(ref string str)
        {
            int len = 0;
            for (int i = curPos;; i++) {
                if (i == dataSize) {
                    str = "";
                    return this;
                }
                if (dataArray[i] == 0) {
                    len = i - curPos;
                    break;
                }
            }
            byte[] bytes = new byte[len];
            for (int i = 0; i < len; i++) {
                bytes[i] = dataArray[curPos++];
            }
            str = System.Text.Encoding.UTF8.GetString(bytes);
            curPos++; // Skip zero
            return this;
        }

        public Serializator Get(ref hstring value)
        {
            string str = "";
            Get(ref str);
            value = str.hstr();
            return this;
        }

        public Serializator Get(ref ident value)
        {
            Get(ref value.value);
            return this;
        }

        public Serializator Get(ref timespan value)
        {
            long i = 0;
            Get(ref i);
            value = new timespan(i, Time.NanosecondsPlace);
            return this;
        }

        public Serializator Get(ref synctime value)
        {
            long i = 0;
            Get(ref i);
            value = new synctime(i, Time.MillisecondsPlace);
            return this;
        }

        public Serializator Get(ref ucolor value)
        {
            Get(ref value.value);
            return this;
        }

        public Serializator Get(ref object value)
        {
            string str = "";
            Get(ref str);
            value = str;
            return this;
        }

        public Serializator Get(ref List<long> values)
        {
            int valuesLen = 0;
            Get(ref valuesLen);
            values.resize(valuesLen);
            for (int i = 0; i < valuesLen; i++) {
                var item = values[i];
                Get(ref item);
                values[i] = item;
            }
            return this;
        }

        public Serializator Get(ref List<int> values)
        {
            int valuesLen = 0;
            Get(ref valuesLen);
            values.resize(valuesLen);
            for (int i = 0; i < valuesLen; i++) {
                var item = values[i];
                Get(ref item);
                values[i] = item;
            }
            return this;
        }

        public Serializator Get(ref List<short> values)
        {
            int valuesLen = 0;
            Get(ref valuesLen);
            values.resize(valuesLen);
            for (int i = 0; i < valuesLen; i++) {
                var item = values[i];
                Get(ref item);
                values[i] = item;
            }
            return this;
        }

        public Serializator Get(ref List<sbyte> values)
        {
            int valuesLen = 0;
            Get(ref valuesLen);
            values.resize(valuesLen);
            for (int i = 0; i < valuesLen; i++) {
                var item = values[i];
                Get(ref item);
                values[i] = item;
            }
            return this;
        }

        public Serializator Get(ref List<ulong> values)
        {
            int valuesLen = 0;
            Get(ref valuesLen);
            values.resize(valuesLen);
            for (int i = 0; i < valuesLen; i++) {
                var item = values[i];
                Get(ref item);
                values[i] = item;
            }
            return this;
        }

        public Serializator Get(ref List<uint> values)
        {
            int valuesLen = 0;
            Get(ref valuesLen);
            values.resize(valuesLen);
            for (int i = 0; i < valuesLen; i++) {
                var item = values[i];
                Get(ref item);
                values[i] = item;
            }
            return this;
        }

        public Serializator Get(ref List<ushort> values)
        {
            int valuesLen = 0;
            Get(ref valuesLen);
            values.resize(valuesLen);
            for (int i = 0; i < valuesLen; i++) {
                var item = values[i];
                Get(ref item);
                values[i] = item;
            }
            return this;
        }

        public Serializator Get(ref List<byte> values)
        {
            int valuesLen = 0;
            Get(ref valuesLen);
            values.resize(valuesLen);
            for (int i = 0; i < valuesLen; i++) {
                var item = values[i];
                Get(ref item);
                values[i] = item;
            }
            return this;
        }

        public Serializator Get(ref List<bool> values)
        {
            int valuesLen = 0;
            Get(ref valuesLen);
            values.resize(valuesLen);
            for (int i = 0; i < valuesLen; i++) {
                var item = values[i];
                Get(ref item);
                values[i] = item;
            }
            return this;
        }

        public Serializator Get(ref List<string> values)
        {
            int valuesLen = 0;
            Get(ref valuesLen);
            values.resize(valuesLen);
            for (int i = 0; i < valuesLen; i++) {
                var item = values[i];
                Get(ref item);
                values[i] = item;
            }
            return this;
        }

        public Serializator Get(ref List<hstring> values)
        {
            int valuesLen = 0;
            Get(ref valuesLen);
            values.resize(valuesLen);
            for (int i = 0; i < valuesLen; i++) {
                var item = values[i];
                Get(ref item);
                values[i] = item;
            }
            return this;
        }

        public Serializator Get(ref List<ident> values)
        {
            int valuesLen = 0;
            Get(ref valuesLen);
            values.resize(valuesLen);
            for (int i = 0; i < valuesLen; i++) {
                var item = values[i];
                Get(ref item);
                values[i] = item;
            }
            return this;
        }
    }
}
