namespace FOnline;

// Always-on managed invariant checks use the engine exception layout: the message followed by one
// "\n- <arg>" line per context value. A thrown managed exception is caught by
// Native.InvokeEvent, logged via Native.Log, and converts the event to StopChain -- mirroring AngelScript's
// "violated verify => logged, chain stopped" behavior, so no new native binding is needed.
//
// The port tool (Tools/ManagedPort) rewrites `verify(...)` -> `Game.Verify(...)`; ported modules import
// `FOnline`, so `Game` resolves here without an extra `using`. `Game` is a partial static class (its other
// parts are generated per target), so this file only contributes the invariant helpers.
public static partial class Game
{
    // `[DoesNotReturnIf(false)]` on the condition is what makes an invariant check mean something to the
    // compiler, exactly as it does for System.Diagnostics.Debug.Assert. It states that the method does not
    // return when `condition` is false, so past the call the flow analysis may assume it was true. For the
    // dominant form `Verify(x != null, ...)` that narrows `x` to non-null for the rest of the method, and
    // the `x!` suppressions that would otherwise be needed afterwards become unnecessary.
    //
    // This only reaches conditions the null-state analysis can follow -- `Verify(x != null, ...)` narrows,
    // `Verify(IsValid(x), ...)` cannot, since nothing ties the helper's result to x's null state. Annotate
    // such a helper with `[MemberNotNullWhen]`/`[NotNullWhen]` rather than reaching for `!` at the call.

    // verify(cond, message) -- the common form. Throws when the invariant is broken.
    public static void Verify([System.Diagnostics.CodeAnalysis.DoesNotReturnIf(false)] bool condition, string message)
    {
        if (!condition) {
            throw new System.InvalidOperationException(message);
        }
    }

    // verify(cond, message, arg0[, arg1...]) -- the variadic context form. The AngelScript `throw` appends
    // each context value on its own "\n- <value>" line; this reproduces that layout so logs read identically.
    //
    // The one-to-three-argument forms below exist because `params object?[]` allocates the array and boxes
    // every value type on EVERY call, not only on the failing one -- and Verify is an always-on production
    // check. Overload resolution prefers an applicable non-params form, so the common shapes stop allocating
    // without a single call site changing. Only a caller passing an `object?[]` of its own is affected: it
    // now lands in the single-argument form and prints as one value.
    public static void Verify<T0>([System.Diagnostics.CodeAnalysis.DoesNotReturnIf(false)] bool condition,
                                  string message, T0 arg0)
    {
        if (!condition) {
            throw new System.InvalidOperationException(BuildMessage(message, arg0));
        }
    }

    public static void Verify<T0, T1>([System.Diagnostics.CodeAnalysis.DoesNotReturnIf(false)] bool condition,
                                      string message, T0 arg0, T1 arg1)
    {
        if (!condition) {
            throw new System.InvalidOperationException(BuildMessage(message, arg0, arg1));
        }
    }

    public static void Verify<T0, T1, T2>([System.Diagnostics.CodeAnalysis.DoesNotReturnIf(false)] bool condition,
                                          string message, T0 arg0, T1 arg1, T2 arg2)
    {
        if (!condition) {
            throw new System.InvalidOperationException(BuildMessage(message, arg0, arg1, arg2));
        }
    }

    public static void Verify([System.Diagnostics.CodeAnalysis.DoesNotReturnIf(false)] bool condition, string message,
                              params object?[] args)
    {
        if (!condition) {
            throw new System.InvalidOperationException(BuildMessage(message, args));
        }
    }

    // The end of a path that cannot be reached -- the arm after a switch that handles every enum member,
    // the tail of a search that always returns from inside its loop. Written as `throw Game.Unreachable(...)`.
    //
    // It returns the exception rather than throwing it, because only a `throw` ends a path as far as the
    // compiler is concerned: C# reachability is structural, so neither `Verify(false, ...)` nor a
    // `[DoesNotReturn]` method satisfies CS0161 -- which is why such tails previously needed a *second*,
    // messageless `throw` after the Verify purely to compile. This keeps one statement that both states
    // the invariant with a real message and ends the path, and it formats context values in the same
    // "\n- <value>" layout as Verify.
    public static System.Exception Unreachable(string message)
    {
        return new System.InvalidOperationException(message);
    }

    public static System.Exception Unreachable(string message, params object?[] args)
    {
        return new System.InvalidOperationException(BuildMessage(message, args));
    }

    // verify(x != null, message) narrowing form: returns the value typed non-null so the C# nullable-flow
    // analysis treats it as live afterward (honoring the repo's zero-warning rule on nullable references).
    // `[NotNull]` narrows the *argument* too, so a caller that ignores the return value still gets the
    // narrowing -- otherwise only the returned copy would be known non-null.
    public static T VerifyNotNull<T>([System.Diagnostics.CodeAnalysis.NotNull] T? value, string message)
        where T : class
    {
        if (value == null) {
            throw new System.InvalidOperationException(message);
        }

        return value;
    }

    public static void RunScriptGC()
    {
        System.GC.Collect();
        System.GC.WaitForPendingFinalizers();
        System.GC.Collect();
    }

    private static string BuildMessage(string message, object?[] args)
    {
        if (args == null || args.Length == 0) {
            return message;
        }

        System.Text.StringBuilder builder = new System.Text.StringBuilder(message);

        for (int i = 0; i < args.Length; i++) {
            AppendContext(builder, args[i]);
        }

        return builder.ToString();
    }

    // The generic forms format on the failing path only, so the context values are still boxed here -- but by
    // then the invariant is already broken and the process is throwing
    private static string BuildMessage<T0>(string message, T0 arg0)
    {
        System.Text.StringBuilder builder = new System.Text.StringBuilder(message);
        AppendContext(builder, arg0);
        return builder.ToString();
    }

    private static string BuildMessage<T0, T1>(string message, T0 arg0, T1 arg1)
    {
        System.Text.StringBuilder builder = new System.Text.StringBuilder(message);
        AppendContext(builder, arg0);
        AppendContext(builder, arg1);
        return builder.ToString();
    }

    private static string BuildMessage<T0, T1, T2>(string message, T0 arg0, T1 arg1, T2 arg2)
    {
        System.Text.StringBuilder builder = new System.Text.StringBuilder(message);
        AppendContext(builder, arg0);
        AppendContext(builder, arg1);
        AppendContext(builder, arg2);
        return builder.ToString();
    }

    private static void AppendContext<T>(System.Text.StringBuilder builder, T value)
    {
        builder.Append("\n- ");
        builder.Append(value?.ToString() ?? "null");
    }
}
