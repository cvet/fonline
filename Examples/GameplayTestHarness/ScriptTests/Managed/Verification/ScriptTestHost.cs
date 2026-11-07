// Stand-in for the generated server API surface that the managed harness uses, driven by a virtual clock.
// It proves the harness's control flow and markers; Engine dispatch is proven by a project's real compile and bake
namespace FOnline;

using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Threading.Tasks;

public enum EventPriority
{
    Lowest = 0,
    Low = 1000000,
    Normal = 2000000,
    High = 3000000,
    Highest = 4000000,
}

public enum EventResult
{
    ContinueChain = 0,
    StopChain = 1,
}

public readonly struct hstring
{
    private readonly string Text;

    public hstring(string text)
    {
        Text = text;
    }

    public override string ToString() => Text ?? string.Empty;
}

public partial struct timespan
{
    public long value;

    public timespan(long value)
    {
        this.value = value;
    }

    // Mirrors the place constructor in CoreScripts/ValueTypeExtensions.cs, which needs the full generated type set
    public timespan(long value, int place)
    {
        this.value = place switch {
            TimePlace.Nanoseconds => value,
            TimePlace.Microseconds => value * 1_000L,
            TimePlace.Milliseconds => value * 1_000_000L,
            TimePlace.Seconds => value * 1_000_000_000L,
            _ => throw new ArgumentException("Invalid time place"),
        };
    }

    public long milliseconds => value / 1_000_000L;
}

public delegate void Callback_void();
public delegate Task Callback_voidAsync();
public delegate void Callback_void_any(any arg0);
public delegate Task Callback_void_anyAsync(any arg0);
public delegate void GameOnStartEventHandler();
public delegate Task GameOnStartEventHandlerAsync();
public delegate void GameOnTestingInitEventHandler();
public delegate Task GameOnTestingInitEventHandlerAsync();

public readonly struct GameOnStartEvent
{
    public void Subscribe(GameOnStartEventHandler handler, EventPriority priority = EventPriority.Normal) =>
        ScriptTestHost.Subscribe(ScriptTestHost.StartHandlers, handler);

    public void Subscribe(GameOnStartEventHandlerAsync handler, EventPriority priority = EventPriority.Normal) =>
        ScriptTestHost.Subscribe(ScriptTestHost.StartHandlers, handler);
}

public readonly struct GameOnTestingInitEvent
{
    public void Subscribe(GameOnTestingInitEventHandler handler, EventPriority priority = EventPriority.Normal) =>
        ScriptTestHost.Subscribe(ScriptTestHost.TestingInitHandlers, handler);

    public void Subscribe(GameOnTestingInitEventHandlerAsync handler, EventPriority priority = EventPriority.Normal) =>
        ScriptTestHost.Subscribe(ScriptTestHost.TestingInitHandlers, handler);

    public EventResult Fire() => ScriptTestHost.Fire(ScriptTestHost.TestingInitHandlers);
}

public static partial class Game
{
    public static GameOnStartEvent OnStart => new GameOnStartEvent();
    public static GameOnTestingInitEvent OnTestingInit => new GameOnTestingInitEvent();

    public static void Log(string text) => ScriptTestHost.Log(text);
    public static void RequestQuit(bool success = true) => ScriptTestHost.RequestQuit(success);

    public static uint StartTimeEvent(timespan delay, Callback_void? func) =>
        ScriptTestHost.Schedule(delay, func, () => ScriptTestHost.Invoke(func));

    public static uint StartTimeEvent(timespan delay, Callback_voidAsync? func) =>
        ScriptTestHost.Schedule(delay, func, () => ScriptTestHost.Invoke(func));

    public static uint StartTimeEvent(timespan delay, Callback_void_any? func, any data)
    {
        return ScriptTestHost.Schedule(delay, func, () => ScriptTestHost.Invoke(func, data));
    }

    public static uint StartTimeEvent(timespan delay, Callback_void_anyAsync? func, any data)
    {
        return ScriptTestHost.Schedule(delay, func, () => ScriptTestHost.Invoke(func, data));
    }

    public static uint StartTimeEvent(timespan delay, timespan repeat, Callback_void? func) =>
        throw new NotSupportedException("Repeating time events are outside the harness stand-in");

    public static void StopTimeEvent(uint id) => ScriptTestHost.Stop(id);
}

public static partial class Settings
{
    public static class Testing
    {
        public static bool Enabled => ScriptTestHost.GetBool("Testing.Enabled");
        public static string Filter => ScriptTestHost.GetString("Testing.Filter");
        public static bool StopOnFirstFailure => ScriptTestHost.GetBool("Testing.StopOnFirstFailure");
        public static bool ExitOnFinish => ScriptTestHost.GetBool("Testing.ExitOnFinish");
        public static int TestTimeout => ScriptTestHost.GetInt("Testing.TestTimeout");
    }

    // Host-only switch that adds the self-check suites; it is not part of the harness contract
    public static class HarnessHost
    {
        public static string Mode => ScriptTestHost.GetString("HarnessHost.Mode");
    }
}

// Exit codes: 0 or 1 from RequestQuit like a headless server, 2 for host usage or setting errors, 3 when the queue
// drains without a quit request (a real server keeps running until the runner deadline), 4 for a raising time event
public static class ScriptTestHost
{
    internal static readonly List<Delegate> StartHandlers = new List<Delegate>();
    internal static readonly List<Delegate> TestingInitHandlers = new List<Delegate>();

    private static readonly Dictionary<string, string> Values = new Dictionary<string, string>(StringComparer.Ordinal);
    private static readonly SortedDictionary<(long Due, uint Id), Func<Task?>> Queue =
        new SortedDictionary<(long, uint), Func<Task?>>();
    private static readonly Dictionary<uint, long> DueById = new Dictionary<uint, long>();
    private static long Now;
    private static uint LastId;
    private static bool QuitRequested;
    private static bool QuitSuccess = true;

    public static int Main(string[] args)
    {
        try {
            ApplyCommandLine(args);
            Values.TryAdd("HarnessHost.Mode", string.Empty);
            return Run();
        }
        catch (InvalidDataException exception) {
            Console.Error.WriteLine("[host] " + exception.Message);
            return 2;
        }
        catch (TargetInvocationException exception) when (exception.InnerException is InvalidDataException inner) {
            Console.Error.WriteLine("[host] " + inner.Message);
            return 2;
        }
    }

    private static int Run()
    {
        // Module initializers run like the Engine's Initializator, in a deterministic order
        foreach (MethodInfo method in FindAttributed(typeof(ModuleInitAttribute))
                     .OrderBy(m => m.DeclaringType?.FullName + "." + m.Name, StringComparer.Ordinal)) {
            method.Invoke(null, null);
        }

        Fire(StartHandlers);

        while (!QuitRequested) {
            if (Queue.Count == 0) {
                Console.WriteLine("[host] no pending time event and no quit request");
                return 3;
            }

            KeyValuePair<(long Due, uint Id), Func<Task?>> next = Queue.First();
            Queue.Remove(next.Key);
            DueById.Remove(next.Key.Id);
            Now = next.Key.Due;

            Task? task = null;

            try {
                task = next.Value();
            }
            catch (Exception exception) when (exception is not InvalidDataException) {
                Console.WriteLine("[host] time event raised: " + exception.Message);
                return 4;
            }

            if (task != null && task.IsFaulted) {
                Console.WriteLine("[host] time event faulted: " + task.Exception?.GetBaseException().Message);
                return 4;
            }
        }

        return QuitSuccess ? 0 : 1;
    }

    internal static void Log(string text)
    {
        Console.WriteLine(text);
        Console.Out.Flush();
    }

    internal static void RequestQuit(bool success)
    {
        QuitRequested = true;
        QuitSuccess = QuitSuccess && success;
    }

    internal static void Subscribe(List<Delegate> handlers, Delegate handler)
    {
        RequireAttribute(handler, typeof(EventAttribute));
        handlers.Add(handler);
    }

    internal static EventResult Fire(List<Delegate> handlers)
    {
        // Like the Engine dispatcher, a throwing subscriber is reported and the remaining subscribers still run
        foreach (Delegate handler in handlers.ToArray()) {
            try {
                object? result = handler.DynamicInvoke();
                if (result is Task task && !task.IsCompleted) {
                    throw new InvalidOperationException("The stand-in fires only synchronously completing handlers");
                }
            }
            catch (TargetInvocationException exception) when (exception.InnerException is not InvalidDataException) {
                Console.WriteLine("[host] event handler raised: " + exception.InnerException?.Message);
            }
        }

        return EventResult.ContinueChain;
    }

    internal static uint Schedule(timespan delay, Delegate? func, Func<Task?> invoke)
    {
        Invariant.Verify(func != null, "Time event callback is null");
        RequireAttribute(func, typeof(TimeEventAttribute));
        Invariant.Verify(delay.value >= 0, "Time event delay is negative", delay.value);

        uint id = ++LastId;
        long due = Now + delay.milliseconds;
        Queue.Add((due, id), invoke);
        DueById.Add(id, due);
        return id;
    }

    internal static void Stop(uint id)
    {
        if (DueById.Remove(id, out long due)) {
            Queue.Remove((due, id));
        }
    }

    internal static Task? Invoke(Callback_void? func)
    {
        func?.Invoke();
        return null;
    }

    internal static Task? Invoke(Callback_voidAsync? func) => func?.Invoke();

    internal static Task? Invoke(Callback_void_any? func, any data)
    {
        func?.Invoke(data);
        return null;
    }

    internal static Task? Invoke(Callback_void_anyAsync? func, any data) => func?.Invoke(data);

    internal static bool GetBool(string name)
    {
        string text = GetString(name);

        return text.ToLowerInvariant() switch {
            "true" or "1" => true,
            "false" or "0" => false,
            _ => throw new InvalidDataException("Setting " + name + " is not a bool: " + text),
        };
    }

    internal static int GetInt(string name)
    {
        string text = GetString(name);

        if (!int.TryParse(text, NumberStyles.Integer, CultureInfo.InvariantCulture, out int value)) {
            throw new InvalidDataException("Setting " + name + " is not an int32: " + text);
        }

        return value;
    }

    internal static string GetString(string name)
    {
        if (!Values.TryGetValue(name, out string? value)) {
            throw new InvalidDataException("Setting has no configured value: " + name);
        }

        return value;
    }

    private static void RequireAttribute(Delegate handler, Type attribute)
    {
        if (handler.Method.GetCustomAttribute(attribute) == null) {
            throw new InvalidOperationException(handler.Method.Name + " lacks " + attribute.Name);
        }
    }

    private static IEnumerable<MethodInfo> FindAttributed(Type attribute)
    {
        const BindingFlags Flags = BindingFlags.Static | BindingFlags.Public | BindingFlags.NonPublic;
        return typeof(ScriptTestHost)
            .Assembly.GetTypes()
            .SelectMany(type => type.GetMethods(Flags))
            .Where(method => method.GetCustomAttribute(attribute) != null);
    }

    // The Engine command-line subset the harness needs: -ApplyConfig, -ApplySubConfig and -Group.Name value
    private static void ApplyCommandLine(string[] args)
    {
        List<(string Name, string Value)> options = new List<(string, string)>();

        for (int i = 0; i < args.Length; i++) {
            if (!IsOption(args[i])) {
                throw new InvalidDataException("Unexpected argument: " + args[i]);
            }

            string name = args[i].TrimStart('-');
            string value = "1";

            if (i + 1 < args.Length && !IsOption(args[i + 1])) {
                value = args[++i];
            }

            options.Add((name, value));
        }

        Dictionary<string, Dictionary<string, string>> subConfigs =
            new Dictionary<string, Dictionary<string, string>>(StringComparer.Ordinal);

        foreach ((string name, string value) in options.Where(option => option.Name == "ApplyConfig")) {
            ReadConfig(value, subConfigs);
        }

        foreach ((string name, string value) in options.Where(option => option.Name == "ApplySubConfig")) {
            if (!subConfigs.TryGetValue(value, out Dictionary<string, string>? section)) {
                throw new InvalidDataException("Sub-config not found: " + value);
            }

            foreach (KeyValuePair<string, string> entry in section) {
                Values[entry.Key] = entry.Value;
            }
        }

        foreach ((string name, string value)
                     in options.Where(option => option.Name != "ApplyConfig" && option.Name != "ApplySubConfig")) {
            Values[name] = value;
        }
    }

    private static bool IsOption(string arg)
    {
        string name = arg.StartsWith("--", StringComparison.Ordinal) ? arg[2..]
                    : arg.StartsWith('-')                            ? arg[1..]
                                                                     : string.Empty;
        return name.Length != 0 && char.IsAsciiLetter(name[0]);
    }

    private static void ReadConfig(string path, Dictionary<string, Dictionary<string, string>> subConfigs)
    {
        Dictionary<string, string>? section = Values;
        bool inSubConfig = false;

        foreach (string rawLine in File.ReadAllLines(path)) {
            string line = rawLine.Trim();

            if (line.Length == 0 || line.StartsWith('#')) {
                continue;
            }

            if (line.StartsWith('[')) {
                inSubConfig = line == "[SubConfig]";
                section = inSubConfig ? null : new Dictionary<string, string>(StringComparer.Ordinal);
                continue;
            }

            int separator = line.IndexOf('=');
            if (separator < 0) {
                throw new InvalidDataException("Config line has no '=': " + line);
            }

            string key = line[..separator].Trim();
            string value = line[(separator + 1)..].Trim();

            if (inSubConfig && key == "Name") {
                section = new Dictionary<string, string>(StringComparer.Ordinal);
                subConfigs.Add(value, section);
                continue;
            }

            if (section == null) {
                throw new InvalidDataException("Sub-config entry precedes its Name: " + line);
            }

            section[key] = value;
        }
    }
}
