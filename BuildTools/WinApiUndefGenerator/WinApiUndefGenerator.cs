using System.Reflection;
using System.Reflection.Metadata;
using System.Reflection.PortableExecutable;
using System.Text;
using System.Text.RegularExpressions;

namespace WinApiUndefGeneration;

public static class WinApiUndefGenerator
{
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
            string originalText = File.ReadAllText(headerPath);
            string headerText = SpliceLines(originalText, out List<int> originalOffsets);
            CollectMacroNames(headerText, originalText, originalOffsets, names);
        }
        return names;
    }

    private static string SpliceLines(string text, out List<int> originalOffsets)
    {
        StringBuilder spliced = new(text.Length);
        originalOffsets = new(text.Length + 1);
        for (int index = 0; index < text.Length; index++)
        {
            char current = text[index];
            if (current == '\\' && index + 1 < text.Length)
            {
                if (text[index + 1] == '\n')
                {
                    index++;
                    continue;
                }
                if (text[index + 1] == '\r' && index + 2 < text.Length && text[index + 2] == '\n')
                {
                    index += 2;
                    continue;
                }
            }
            originalOffsets.Add(index);
            spliced.Append(current);
        }
        originalOffsets.Add(text.Length);
        return spliced.ToString();
    }

    private static void CollectMacroNames(string text, string originalText, List<int> originalOffsets, HashSet<string> names)
    {
        bool lineHasToken = false;
        int index = 0;
        while (index < text.Length)
        {
            char current = text[index];
            if (current == '\n')
            {
                lineHasToken = false;
                index++;
                continue;
            }
            if (char.IsWhiteSpace(current))
            {
                index++;
                continue;
            }
            if (current == '/' && index + 1 < text.Length)
            {
                if (text[index + 1] == '/')
                {
                    index += 2;
                    while (index < text.Length && text[index] != '\n')
                    {
                        index++;
                    }
                    continue;
                }
                if (text[index + 1] == '*')
                {
                    index = SkipBlockComment(text, index, ref lineHasToken, out _);
                    continue;
                }
            }
            if (current == '#' && !lineHasToken)
            {
                lineHasToken = true;
                index = ReadDefineDirective(text, index + 1, names, ref lineHasToken);
                continue;
            }
            if (char.IsDigit(current) || (current == '.' && index + 1 < text.Length && char.IsDigit(text[index + 1])))
            {
                index = SkipNumber(text, index);
                lineHasToken = true;
                continue;
            }
            if (TrySkipLiteral(text, originalText, originalOffsets, ref index))
            {
                lineHasToken = true;
                continue;
            }

            lineHasToken = true;
            index++;
        }
    }

    private static int SkipBlockComment(string text, int index, ref bool lineHasToken, out bool sawNewline)
    {
        sawNewline = false;
        index += 2;
        while (index < text.Length)
        {
            if (text[index] == '\n')
            {
                sawNewline = true;
                lineHasToken = false;
            }
            else if (text[index] == '*' && index + 1 < text.Length && text[index + 1] == '/')
            {
                return index + 2;
            }
            index++;
        }
        return index;
    }

    private static int SkipNumber(string text, int index)
    {
        index++;
        while (index < text.Length)
        {
            char current = text[index];
            if (IsIdentifierPart(current) || current == '.')
            {
                index++;
            }
            else if (current == '\'' && index + 1 < text.Length && IsIdentifierPart(text[index + 1]))
            {
                index += 2;
            }
            else if ((current == '+' || current == '-') && "eEpP".Contains(text[index - 1]))
            {
                index++;
            }
            else
            {
                break;
            }
        }
        return index;
    }

    private static bool TrySkipLiteral(string text, string originalText, List<int> originalOffsets, ref int index)
    {
        char current = text[index];
        if (current == '"' || current == '\'')
        {
            index++;
            while (index < text.Length && text[index] != current && text[index] != '\n')
            {
                index += (text[index] == '\\' && index + 1 < text.Length) ? 2 : 1;
            }
            if (index < text.Length && text[index] == current)
            {
                index++;
            }
            return true;
        }

        int prefixLength = current switch
        {
            'R' => 1,
            'u' when index + 2 < text.Length && text[index + 1] == '8' && text[index + 2] == 'R' => 3,
            'u' or 'U' or 'L' when index + 1 < text.Length && text[index + 1] == 'R' => 2,
            _ => 0,
        };
        if (prefixLength == 0)
        {
            return false;
        }

        if (index > 0 && IsIdentifierPart(text[index - 1]))
        {
            return false;
        }

        int rawStart = index + prefixLength;
        if (rawStart >= text.Length || text[rawStart] != '"')
        {
            return false;
        }

        int open = rawStart + 1;
        while (open < text.Length && text[open] != '(')
        {
            if (open - rawStart > 16 || !IsRawDelimiterChar(text[open]))
            {
                return false;
            }
            open++;
        }
        if (open >= text.Length)
        {
            return false;
        }

        string closing = ")" + text.Substring(rawStart + 1, open - rawStart - 1) + "\"";
        // Raw-string contents retain physical backslash-newline pairs
        int closingIndex = originalText.IndexOf(closing, originalOffsets[open] + 1, StringComparison.Ordinal);
        int originalEnd = closingIndex >= 0 ? closingIndex + closing.Length : originalText.Length;
        while (index < text.Length && originalOffsets[index] < originalEnd)
        {
            index++;
        }
        return true;
    }

    private static int ReadDefineDirective(string text, int index, HashSet<string> names, ref bool lineHasToken)
    {
        if (!SkipDirectiveWhitespace(text, ref index, ref lineHasToken) || !HasIdentifierAt(text, index, "define"))
        {
            return index;
        }

        index += "define".Length;
        if (!SkipDirectiveWhitespace(text, ref index, ref lineHasToken)
            || index >= text.Length
            || !IsIdentifierStart(text[index]))
        {
            return index;
        }

        int nameStart = index;
        while (index < text.Length && IsIdentifierPart(text[index]))
        {
            index++;
        }
        names.Add(text[nameStart..index]);
        return index;
    }

    private static bool SkipDirectiveWhitespace(string text, ref int index, ref bool lineHasToken)
    {
        while (index < text.Length)
        {
            char current = text[index];
            if (current == '\n' || current == '\r')
            {
                return false;
            }
            if (char.IsWhiteSpace(current))
            {
                index++;
                continue;
            }
            if (current == '/' && index + 1 < text.Length && text[index + 1] == '/')
            {
                while (index < text.Length && text[index] != '\n')
                {
                    index++;
                }
                return false;
            }
            if (current == '/' && index + 1 < text.Length && text[index + 1] == '*')
            {
                index = SkipBlockComment(text, index, ref lineHasToken, out bool sawNewline);
                if (sawNewline)
                {
                    return false;
                }
                continue;
            }
            return true;
        }
        return true;
    }

    private static bool HasIdentifierAt(string text, int index, string identifier)
    {
        return index + identifier.Length <= text.Length
            && string.Compare(text, index, identifier, 0, identifier.Length, StringComparison.Ordinal) == 0
            && (index + identifier.Length == text.Length || !IsIdentifierPart(text[index + identifier.Length]));
    }

    private static bool IsRawDelimiterChar(char value)
    {
        return value > ' ' && value <= '~' && value != '(' && value != ')' && value != '\\';
    }

    private static bool IsIdentifierStart(char value)
    {
        return value == '_' || (value >= 'A' && value <= 'Z') || (value >= 'a' && value <= 'z');
    }

    private static bool IsIdentifierPart(char value)
    {
        return value == '_' || char.IsLetterOrDigit(value);
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
