using WinApiUndefGeneration;

if (args.Length != 4)
{
    Console.Error.WriteLine("Usage: WinApiUndefGenerator <winmd> <sdk-include-dir> <existing> <output>");
    return 2;
}

WinApiUndefGenerator.Generate(args[0], args[1], args[2], args[3]);
return 0;
