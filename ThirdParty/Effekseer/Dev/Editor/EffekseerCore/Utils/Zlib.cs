using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Effekseer.Utils
{
	/// <summary>
	/// zlib compatible with python zlib level=6
	/// </summary>
	public class Zlib
	{
		public static unsafe byte[] Compress(byte[] buffer)
		{
			// (FOnline Patch) Keep upstream container helpers buildable without the editor scripting subsystem.
			using (var compressStream = new System.IO.MemoryStream())
			using (var compressor = new System.IO.Compression.DeflateStream(compressStream, System.IO.Compression.CompressionLevel.Optimal))
			{
				UInt32 adder = 0;
				fixed (byte* b = buffer)
				{
					adder = CalcAdler32(b, (UInt32)buffer.Length);
				}

				compressor.Write(buffer, 0, buffer.Count());
				compressor.Close();
				var compressed = compressStream.ToArray();

				List<byte[]> dst = new List<byte[]>();
				dst.Add(new byte[] { 0x78, 0x9c });
				dst.Add(compressed);
				dst.Add(BitConverter.GetBytes(adder).Reverse().ToArray());

				return dst.SelectMany(_ => _).ToArray();
			}
		}

		public static byte[] Decompress(byte[] buffer)
		{
			// (FOnline Patch) Keep upstream container helpers buildable without the editor scripting subsystem.
			if (buffer == null || buffer.Length < 6 || (buffer[0] & 0x0f) != 8 || ((buffer[0] << 8) + buffer[1]) % 31 != 0)
			{
				throw new System.IO.InvalidDataException("Invalid zlib stream");
			}

			var decompressBuffer = new List<byte>();
			using (var decompressStream = new System.IO.MemoryStream(buffer.Skip(2).Take(buffer.Length - 6).ToArray()))
			using (var decompressor = new System.IO.Compression.DeflateStream(decompressStream, System.IO.Compression.CompressionMode.Decompress))
			{
				while (true)
				{
					byte[] temp = new byte[1024];
					int readSize = decompressor.Read(temp, 0, temp.Length);
					if (readSize == 0) break;
					decompressBuffer.AddRange(temp.Take(readSize));
				}
			}

			var result = decompressBuffer.ToArray();
			UInt32 actualAdder = 0;
			unsafe
			{
				fixed (byte* b = result)
				{
					actualAdder = CalcAdler32(b, (UInt32)result.Length);
				}
			}

			UInt32 expectedAdder = ((UInt32)buffer[buffer.Length - 4] << 24) |
				((UInt32)buffer[buffer.Length - 3] << 16) |
				((UInt32)buffer[buffer.Length - 2] << 8) |
				buffer[buffer.Length - 1];
			if (actualAdder != expectedAdder)
			{
				throw new System.IO.InvalidDataException("Invalid zlib Adler-32 checksum");
			}

			return result;
		}

		static unsafe UInt32 CalcAdler32(byte* data, UInt32 len)
		{
			UInt32 a = 1, b = 0;

			while (len > 0)
			{
				UInt32 rest = len > 5550 ? 5550 : len;
				len -= rest;
				do
				{
					a += *data++;
					b += a;
					rest--;
				} while (rest > 0);

				a %= 65521;
				b %= 65521;
			}

			return (b << 16) | a;
		}
	}
}
