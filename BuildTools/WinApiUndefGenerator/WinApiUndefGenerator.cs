using System.Reflection;
using System.Reflection.Metadata;
using System.Reflection.PortableExecutable;
using System.Text;
using System.Text.RegularExpressions;

namespace WinApiUndefGeneration;

public static class WinApiUndefGenerator
{
    private static readonly Regex MacroDefinitionPattern = new(
        @"^\s*#\s*define\s+(?<name>[A-Za-z_]\w*)",
        RegexOptions.Compiled | RegexOptions.CultureInvariant);

    private static readonly Regex UndefDefinitionPattern = new(
        @"^\s*#undef\s+(?<name>[A-Za-z_]\w*)",
        RegexOptions.Compiled | RegexOptions.CultureInvariant);

    private static readonly Regex CommentUndefDefinitionPattern = new(
        @"^\s*//\s*#undef\s+(?<name>[A-Za-z_]\w*)",
        RegexOptions.Compiled | RegexOptions.CultureInvariant);

    public static void Generate(string winmdPath, string sdkIncludeDirectory, string existingPath, string outputPath)
    {
        if (!File.Exists(winmdPath))
        {
            throw new FileNotFoundException("WinMD file was not found", winmdPath);
        }
        if (!Directory.Exists(sdkIncludeDirectory))
        {
            throw new DirectoryNotFoundException($"SDK include directory was not found: {sdkIncludeDirectory}");
        }
        if (!File.Exists(existingPath))
        {
            throw new FileNotFoundException("Existing WinApiUndef file was not found", existingPath);
        }

        HashSet<string> macroNames = ReadMacroNames(sdkIncludeDirectory);
        HashSet<string> pinvokeNames = ReadPInvokeNames(winmdPath);
        HashSet<string> existingNames = ReadNames(existingPath, UndefDefinitionPattern);
        HashSet<string> allowlistNames = ReadNames(existingPath, CommentUndefDefinitionPattern);

        HashSet<string> candidates = new(macroNames);
        candidates.IntersectWith(pinvokeNames);
        AddGenericAliases(candidates, macroNames, pinvokeNames);
        candidates.ExceptWith(existingNames);

        HashSet<string> outputNames = new(existingNames);
        outputNames.UnionWith(candidates);
        outputNames.ExceptWith(allowlistNames);

        string existingText = File.ReadAllText(existingPath);
        string newLine = existingText.Contains("\r\n", StringComparison.Ordinal) ? "\r\n" : "\n";
        string[] existingLines = existingText.Split(new[] { "\r\n", "\n" }, StringSplitOptions.None);

        int finalEndifLine = Array.FindLastIndex(existingLines, line => line.Contains("#endif", StringComparison.Ordinal));
        if (finalEndifLine < 0)
        {
            throw new InvalidDataException("WinApiUndef file has no final #endif");
        }

        int firstUndefLine = Array.FindIndex(existingLines, line => UndefDefinitionPattern.IsMatch(line));
        int blockStart = firstUndefLine >= 0 ? firstUndefLine : finalEndifLine;
        string[] sortedUndefLines = outputNames.OrderBy(name => name, StringComparer.Ordinal).Select(name => $"#undef {name}").ToArray();
        string[] outputLines = existingLines.Take(blockStart).Concat(sortedUndefLines).Concat(existingLines.Skip(finalEndifLine)).ToArray();
        File.WriteAllText(outputPath, string.Join(newLine, outputLines), new UTF8Encoding(false));
    }

    private static HashSet<string> ReadPInvokeNames(string winmdPath)
    {
        using FileStream stream = File.OpenRead(winmdPath);
        using var peReader = new PEReader(stream);
        MetadataReader reader = peReader.GetMetadataReader();
        HashSet<string> names = new(StringComparer.Ordinal);

        foreach (MethodDefinitionHandle handle in reader.MethodDefinitions)
        {
            MethodDefinition method = reader.GetMethodDefinition(handle);
            if ((method.Attributes & MethodAttributes.PinvokeImpl) != 0)
            {
                names.Add(reader.GetString(method.Name));
            }
        }
        return names;
    }

    private static HashSet<string> ReadMacroNames(string sdkIncludeDirectory)
    {
        HashSet<string> names = new(StringComparer.Ordinal);
        foreach (string headerPath in Directory.EnumerateFiles(sdkIncludeDirectory, "*.h", SearchOption.AllDirectories))
        {
            foreach (string line in File.ReadLines(headerPath))
            {
                Match match = MacroDefinitionPattern.Match(line);
                if (match.Success)
                {
                    names.Add(match.Groups["name"].Value);
                }
            }
        }
        return names;
    }

    private static HashSet<string> ReadNames(string path, Regex pattern)
    {
        HashSet<string> names = new(StringComparer.Ordinal);
        foreach (string line in File.ReadLines(path))
        {
            Match match = pattern.Match(line);
            if (match.Success)
            {
                names.Add(match.Groups["name"].Value);
            }
        }
        return names;
    }

    private static void AddGenericAliases(HashSet<string> candidates, HashSet<string> macroNames, HashSet<string> pinvokeNames)
    {
        foreach (string name in pinvokeNames)
        {
            if (name.Length > 1 && (name.EndsWith('A') || name.EndsWith('W')))
            {
                string genericName = name[..^1];
                if (macroNames.Contains(genericName))
                {
                    candidates.Add(genericName);
                }
            }
        }
    }
}
