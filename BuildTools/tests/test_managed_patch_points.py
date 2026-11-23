"""Weave patch points into a stand-in script assembly, compile patches with the production compiler and apply them."""

from __future__ import annotations

from pathlib import Path
import os
import shutil
import subprocess

import pytest


ENGINE = Path(__file__).resolve().parents[2]
MANAGED = ENGINE / "Source/Scripting/Managed"
WEAVER_PROJECT = MANAGED / "PatchPoints/FOnline.PatchPointWeaver.csproj"
COMPILER_PROJECT = MANAGED / "Compiler/FOnline.ScriptCompiler.csproj"
CORE_FILES = ("Attributes.cs", "ScriptPatches.cs")

PROBE_PROJECT = """<Project Sdk="Microsoft.NET.Sdk">
  <PropertyGroup><TargetFramework>net10.0</TargetFramework><OutputType>Exe</OutputType><AssemblyName>Probe.Scripts</AssemblyName><DefineConstants>SERVER</DefineConstants><Nullable>enable</Nullable><ImplicitUsings>disable</ImplicitUsings><TreatWarningsAsErrors>true</TreatWarningsAsErrors><DebugType>embedded</DebugType><Optimize>true</Optimize><TieredPGO>false</TieredPGO></PropertyGroup>
  <ItemGroup><ProjectReference Include="{compiler}" /></ItemGroup>
</Project>
"""

SCRIPTS_SOURCE = r"""
using System;
using System.Threading.Tasks;
using FOnline;

namespace FOnline
{
    public static class Settings
    {
        public static class ManagedScript
        {
            public static bool ServerPatchesEnabled => Environment.GetEnvironmentVariable("PROBE_PATCHES_DISABLED") == null;
        }
    }
}

namespace Probe
{
    public static class World
    {
        public static int Twice(int value) => value * 2;
        public static int CallTwice(int value) => Twice(value) + 1;
        public static int Tiny(int value) => value + 1;
        public static int UseTiny(int value) => Tiny(value) * 10;
        private static int Storage = 7;
        public static ref readonly int ReadOnlyValue() => ref Storage;
        public static int Calculated => Storage + 2;
        private static string Secret(string text) => "secret:" + text;
        public static string CallSecret(string text) => Secret(text);
        public static void Swap(ref int left, ref int right) { int t = left; left = right; right = t; }
        public static bool TryHalf(int value, out int half) { half = value / 2; return value % 2 == 0; }
        public static async Task<int> AsyncValue() { await Task.Yield(); return 1; }
        public static Holder MakeHolder(int value) => new Holder(value);
        public static int Guarded(int value)
        {
            try {
                return 100 / value;
            }
            catch (DivideByZeroException) {
                return -1;
            }
        }
        private sealed class Hidden { public int Value = 4; }
        private static int UseHidden(Hidden hidden) => hidden.Value;
        public static int CallHidden() => UseHidden(new Hidden());
        [NoPatchPoint]
        public static int Excluded(int value) => value;
        public static T Echo<T>(T value) => value;
    }

    public sealed class Holder
    {
        public Holder(int value) { Value = value; }
        public int Value;
        public int Scaled(int factor) => Value * factor;
    }

    public struct Point
    {
        public int X;
        public int Sum(int add) => X + add;
    }
}

internal static class Program
{
    private static int Main(string[] args)
    {
        try {
            Probe.Checks.Run(args[0]).GetAwaiter().GetResult();
            return 0;
        }
        catch (Exception ex) {
            Console.WriteLine("FAILED " + ex);
            return 1;
        }
    }
}
"""

CHECKS_SOURCE = r"""
using System;
using System.Linq;
using System.Reflection;
using System.Reflection.Emit;
using System.Runtime.Loader;
using System.IO;
using System.Threading;
using System.Threading.Tasks;
using FOnline;
using FOnline.ScriptCompiler;

internal static class DynamicPatchManifest
{
    internal static ScriptPatchFunction[] Functions = Array.Empty<ScriptPatchFunction>();
    public static ScriptPatchFunction[] GetFunctions() => Functions;
}

namespace Probe
{
    public static class Checks
    {
        private static DynamicCompileResult Compile(string source)
        {
            return DynamicScriptCompiler.Compile(new DynamicCompileRequest {
                Source = source,
                Kind = DynamicCompileKind.Patch,
                ScriptsAssembly = typeof(World).Assembly,
                Usings = new[] { "System", "System.Threading.Tasks", "FOnline", "Probe" },
            });
        }

        private static ScriptPatchSet Apply(string source)
        {
            DynamicCompileResult result = Compile(source);

            if (!result.Succeeded) {
                throw new InvalidOperationException(string.Join(" | ", result.Errors));
            }

            return ScriptPatches.Apply(AssemblyLoadContext.Default.LoadFromStream(new MemoryStream(result.Image)));
        }

        private static string Failure(Action action)
        {
            try {
                action();
                return "none";
            }
            catch (Exception ex) {
                return ex.GetType().Name + ":" + ex.Message;
            }
        }

        public static async Task Run(string kind)
        {
            if (kind == "points") {
                Console.WriteLine("POINTS available=" + ScriptPatches.IsAvailable + " twice=" + ScriptPatches.HasPatchPoint(typeof(World).GetMethod("Twice")!) +
                                  " excluded=" + ScriptPatches.HasPatchPoint(typeof(World).GetMethod("Excluded")!) +
                                  " generic=" + ScriptPatches.HasPatchPoint(typeof(World).GetMethod("Echo")!) +
                                  " ctor=" + ScriptPatches.HasPatchPoint(typeof(Holder).GetConstructors()[0]) +
                                  " engine=" + ScriptPatches.HasPatchPoint(typeof(ScriptPatches).GetMethod("Apply")!));
                return;
            }
            if (kind == "apply") {
                Func<int, int> early = World.Twice;
                Holder holder = new Holder(3);
                Point point = new Point { X = 5 };
                Console.WriteLine("BEFORE " + World.CallTwice(4) + " " + World.UseTiny(4) + " " + World.CallSecret("a") + " " + holder.Scaled(2) + " " + point.Sum(1) +
                                  " " + await World.AsyncValue() + " " + World.Guarded(0) + " " + World.MakeHolder(1).Value + " " + World.CallHidden());

                ScriptPatchSet set = Apply(@"
public static class Fix
{
    [ReplacesMethod(typeof(World), nameof(World.Twice))]
    internal static int Twice(int value) => value * 3;

    [ReplacesMethod(typeof(World), nameof(World.Tiny))]
    public static int Tiny(int value) => value + 2;

    [ReplacesMethod(typeof(World), ""Secret"")]
    private static string Secret(string text) => ""fixed:"" + text;

    [ReplacesMethod(typeof(World), ""UseHidden"")]
    private static int UseHidden(World.Hidden hidden) => hidden.Value * 10;

    [ReplacesMethod(typeof(Holder), nameof(Holder.Scaled))]
    internal static int Scaled(Holder self, int factor) => self.Value * factor * 100;

    [ReplacesMethod(typeof(Point), nameof(Point.Sum))]
    internal static int Sum(ref Point self, int add) => self.X - add;

    [ReplacesMethod(typeof(World), nameof(World.AsyncValue))]
    internal static Task<int> AsyncValue() => Task.FromResult(2);

    [ReplacesMethod(typeof(World), nameof(World.Swap))]
    internal static void Swap(ref int left, ref int right) { left = 0; right = 0; }

    [ReplacesMethod(typeof(World), nameof(World.TryHalf))]
    internal static bool TryHalf(int value, out int half) { half = -value; return true; }

    [ReplacesMethod(typeof(World), nameof(World.Guarded))]
    internal static int Guarded(int value) => -2;

    [ReplacesMethod(typeof(World), nameof(World.MakeHolder))]
    internal static Holder MakeHolder(int value) => new Holder(value * 7);
}");
                int left = 1, right = 2;
                World.Swap(ref left, ref right);
                bool even = World.TryHalf(3, out int half);
                Console.WriteLine("AFTER " + World.CallTwice(4) + " " + early(4) + " " + World.UseTiny(4) + " " + World.CallSecret("a") + " " + holder.Scaled(2) + " " +
                                  point.Sum(1) + " " + await World.AsyncValue() + " " + left + right + " " + even + half + " " + World.Guarded(0) + " " + World.MakeHolder(1).Value +
                                  " " + World.CallHidden() + " targets=" + set.Targets.Count + " applied=" + set.IsApplied);

                ScriptPatches.Revert(set);
                Console.WriteLine("REVERTED " + World.CallTwice(4) + " " + World.UseTiny(4) + " " + World.CallSecret("a") + " " + holder.Scaled(2) + " " + point.Sum(1) +
                                  " applied=" + set.IsApplied + " count=" + ScriptPatches.Applied.Count);
                return;
            }
            if (kind == "layers") {
                ScriptPatchSet first = Apply("public static class A { [ReplacesMethod(typeof(World), nameof(World.Twice))] public static int Twice(int value) => 10; }");
                ScriptPatchSet second = Apply("public static class B { [ReplacesMethod(typeof(World), nameof(World.Twice))] public static int Twice(int value) => 20; }");
                int both = World.Twice(1);
                ScriptPatches.Revert(second);
                int firstOnly = World.Twice(1);
                Apply("public static class C { [ReplacesMethod(typeof(World), nameof(World.Twice))] public static int Twice(int value) => 30; }");
                int third = World.Twice(1);
                ScriptPatches.RevertAll();
                Console.WriteLine("LAYERS " + both + " " + firstOnly + " " + third + " " + World.Twice(1) + " applied=" + first.IsApplied);
                return;
            }
            if (kind == "readonly") {
                Console.WriteLine("READONLY BEFORE " + World.ReadOnlyValue());
                ScriptPatchSet set = Apply("public static class F { private static int Value = 42; [ReplacesMethod(typeof(World), nameof(World.ReadOnlyValue))] public static ref readonly int ReadOnlyValue() => ref Value; }");
                Console.WriteLine("READONLY AFTER " + World.ReadOnlyValue());
                ScriptPatches.Revert(set);
                Console.WriteLine("READONLY REVERTED " + World.ReadOnlyValue());
                return;
            }
            if (kind == "accessor") {
                ScriptPatchSet set = Apply("public static class F { [ReplacesMethod(typeof(World), \"get_Calculated\")] public static int GetCalculated() => 42; }");
                Console.WriteLine("ACCESSOR " + World.Calculated);
                ScriptPatches.Revert(set);
                Console.WriteLine("ACCESSOR REVERTED " + World.Calculated);
                return;
            }
            if (kind == "keyword") {
                ScriptPatchSet set = Apply("public static class F { [ReplacesMethod(typeof(World), nameof(World.Twice))] public static int @return(int value) => value * 5; }");
                Console.WriteLine("KEYWORD " + World.Twice(3));
                ScriptPatches.Revert(set);
                return;
            }
            if (kind == "concurrent") {
                using ManualResetEventSlim started = new ManualResetEventSlim();
                Task caller = Task.Run(() => {
                    started.Set();
                    while (World.Tiny(1) == 2) { }
                });
                started.Wait();
                await Task.Delay(100);
                ScriptPatchSet set = Apply("public static class F { [ReplacesMethod(typeof(World), nameof(World.Tiny))] public static int Tiny(int value) => value + 2; }");
                bool observed = await Task.WhenAny(caller, Task.Delay(3000)) == caller;
                ScriptPatches.Revert(set);
                Console.WriteLine("CONCURRENT " + observed);
                return;
            }
            if (kind == "fallback") {
                // A revert between the prologue's two reads of a slot hands the redirect an empty one
                Apply("public static class T { [ReplacesMethod(typeof(World), nameof(World.Tiny))] public static int Tiny(int value) => 0; }");
                MethodInfo redirect = typeof(World).Assembly.GetType("FOnline.PatchPointRedirects", true)!
                    .GetMethods(BindingFlags.Static | BindingFlags.NonPublic)
                    .Single(m => m.ReturnType == typeof(int) && m.GetParameters().Select(p => p.ParameterType).SequenceEqual(new[] { typeof(int), typeof(int), typeof(RuntimeMethodHandle) }));
                MethodInfo twice = typeof(World).GetMethod(nameof(World.Twice))!;
                using Stream table = typeof(World).Assembly.GetManifestResourceStream(ScriptPatches.TableResourceName)!;
                using BinaryReader tokens = new BinaryReader(table);
                int slot = 0;
                while (tokens.ReadInt32() != twice.MetadataToken) {
                    slot++;
                }
                Console.WriteLine("FALLBACK " + redirect.Invoke(null, new object[] { 5, slot, twice.MethodHandle }) + " " + World.Twice(5) + " " + World.UseTiny(1));
                MethodInfo sum = typeof(Point).GetMethod(nameof(Point.Sum))!;
                table.Position = 0;
                slot = 0;
                while (tokens.ReadInt32() != sum.MetadataToken) {
                    slot++;
                }
                MethodInfo structRedirect = typeof(World).Assembly.GetType("FOnline.PatchPointRedirects", true)!
                    .GetMethods(BindingFlags.Static | BindingFlags.NonPublic)
                    .Single(m => m.ReturnType == typeof(int) && m.GetParameters().Select(p => p.ParameterType).SequenceEqual(new[] { typeof(Point).MakeByRefType(), typeof(int), typeof(int), typeof(RuntimeMethodHandle) }));
                Console.WriteLine("STRUCT FALLBACK " + structRedirect.Invoke(null, new object[] { new Point { X = 5 }, 2, slot, sum.MethodHandle }));
                return;
            }
            if (kind == "refusals") {
                string Errors(string source) => string.Join(" | ", Compile(source).Errors);
                Console.WriteLine("MISSING " + Errors("public static class F { [ReplacesMethod(typeof(World), \"Nope\")] public static int Nope(int v) => v; }"));
                Console.WriteLine("SIGNATURE " + Errors("public static class F { [ReplacesMethod(typeof(World), nameof(World.Twice))] public static long Twice(int v) => v; }"));
                Console.WriteLine("EXCLUDED " + Errors("public static class F { [ReplacesMethod(typeof(World), nameof(World.Excluded))] public static int Excluded(int v) => v; }"));
                Console.WriteLine("INSTANCE " + Errors("public class F { [ReplacesMethod(typeof(World), nameof(World.Twice))] public int Twice(int v) => v; }"));
                Console.WriteLine("EMPTY " + Errors("public static class F { public static int Other(int v) => v; }"));
                Console.WriteLine("SYNTAX " + Errors("public static class F {\n  public static int Other(int v) => missing;\n}"));
                return;
            }
            if (kind == "foreign") {
                int token = typeof(World).GetMethod(nameof(World.Twice))!.MetadataToken;
                AssemblyBuilder foreignAssembly = AssemblyBuilder.DefineDynamicAssembly(new AssemblyName("Foreign"), AssemblyBuilderAccess.Run);
                AppDomain.CurrentDomain.AssemblyResolve += (_, args) => new AssemblyName(args.Name).Name == "Foreign" ? foreignAssembly : null;
                ModuleBuilder module = foreignAssembly.DefineDynamicModule("Foreign");
                TypeBuilder owner = module.DefineType("ForeignTarget", TypeAttributes.Public);
                for (int row = 1; row <= (token & 0x00FFFFFF); row++) {
                    MethodBuilder method = owner.DefineMethod(row == (token & 0x00FFFFFF) ? "Twice" : "Dummy" + row,
                        MethodAttributes.Public | MethodAttributes.Static, typeof(int), new[] { typeof(int) });
                    ILGenerator il = method.GetILGenerator();
                    il.Emit(OpCodes.Ldarg_0);
                    il.Emit(OpCodes.Ret);
                }
                Type foreign = owner.CreateType()!;
                TypeBuilder fix = module.DefineType("ForeignFix", TypeAttributes.Public);
                MethodBuilder replacement = fix.DefineMethod("Replace", MethodAttributes.Public | MethodAttributes.Static, typeof(int), new[] { typeof(int) });
                replacement.SetCustomAttribute(new CustomAttributeBuilder(typeof(ReplacesMethodAttribute).GetConstructors()[0], new object[] { foreign, "Twice" }));
                ILGenerator body = replacement.GetILGenerator();
                body.Emit(OpCodes.Ldarg_0);
                body.Emit(OpCodes.Ret);
                MethodInfo methodInfo = fix.CreateType()!.GetMethod("Replace")!;
                DynamicPatchManifest.Functions = new[] { new ScriptPatchFunction(methodInfo, methodInfo.MethodHandle.GetFunctionPointer()) };
                string refusal = Failure(() => ScriptPatches.Apply(typeof(Checks).Assembly));
                ScriptPatches.RevertAll();
                Console.WriteLine("FOREIGN " + refusal + " collision=" + (foreign.GetMethod("Twice")!.MetadataToken == token));
                return;
            }
            if (kind == "disabled") {
                DynamicCompileResult result = Compile("public static class F { [ReplacesMethod(typeof(World), nameof(World.Twice))] public static int Twice(int v) => v; }");
                Assembly patch = AssemblyLoadContext.Default.LoadFromStream(new MemoryStream(result.Image));
                Console.WriteLine("DISABLED " + Failure(() => ScriptPatches.Apply(patch)) + " twice=" + World.Twice(2));
                return;
            }

            throw new InvalidOperationException("Unknown probe kind " + kind);
        }
    }
}
"""


def run(command, **kwargs):
    result = subprocess.run(command, capture_output=True, text=True, encoding="utf-8", errors="replace", timeout=600, **kwargs)
    assert result.returncode == 0, result.stdout + result.stderr
    return result.stdout


@pytest.fixture(scope="module")
def patch_probe(tmp_path_factory):
    dotnet = shutil.which("dotnet")
    if dotnet is None:
        pytest.skip("dotnet SDK is required for patch point tests")

    root = tmp_path_factory.mktemp("managed-patch-points")
    probe_dir = root / "probe"
    probe_dir.mkdir()

    for name in CORE_FILES:
        shutil.copyfile(MANAGED / "CoreScripts" / name, probe_dir / name)

    (probe_dir / "Probe.csproj").write_text(PROBE_PROJECT.format(compiler=COMPILER_PROJECT), encoding="utf-8")
    (probe_dir / "Scripts.cs").write_text(SCRIPTS_SOURCE, encoding="utf-8")
    (probe_dir / "Checks.cs").write_text(CHECKS_SOURCE, encoding="utf-8")
    out = root / "bin"
    run([dotnet, "build", str(probe_dir / "Probe.csproj"), "-c", "Release", "--nologo", "-v:minimal", "-o", str(out)])
    weaver_out = root / "weaver"
    run([dotnet, "build", str(WEAVER_PROJECT), "-c", "Release", "--nologo", "-v:minimal", "-o", str(weaver_out)])

    assembly = out / "Probe.Scripts.dll"
    first = run([dotnet, str(weaver_out / "FOnline.PatchPointWeaver.dll"), str(assembly)])
    second = run([dotnet, str(weaver_out / "FOnline.PatchPointWeaver.dll"), str(assembly)])
    return dotnet, assembly, first, second


def run_probe(patch_probe, kind, env=None):
    dotnet, assembly, _, _ = patch_probe
    return run([dotnet, str(assembly), kind], env={**os.environ, **(env or {})})


def test_weaver_reports_its_points_and_leaves_a_woven_assembly_alone(patch_probe):
    _, _, first, second = patch_probe
    assert "patch points" in first and "small methods kept inlinable" in first
    assert "already carries patch points" in second


def test_eligible_methods_get_points_and_the_rest_do_not(patch_probe):
    assert "POINTS available=True twice=True excluded=False generic=False ctor=False engine=False" in run_probe(patch_probe, "points")


def test_a_redirect_handed_an_empty_slot_runs_the_original_method(patch_probe):
    output = run_probe(patch_probe, "fallback")
    assert "FALLBACK 10 10 0" in output
    assert "STRUCT FALLBACK 7" in output


def test_patches_replace_every_call_path_and_revert_cleanly(patch_probe):
    output = run_probe(patch_probe, "apply")
    assert "BEFORE 9 50 secret:a 6 6 1 -1 1 4" in output
    assert "AFTER 13 12 60 fixed:a 600 4 2 00 True-3 -2 7 40 targets=11 applied=True" in output
    assert "REVERTED 9 50 secret:a 6 6 applied=False count=0" in output


def test_a_later_patch_of_a_method_wins_and_reverting_restores_the_earlier(patch_probe):
    assert "LAYERS 20 10 30 2 applied=False" in run_probe(patch_probe, "layers")


def test_readonly_byref_returns_keep_their_calling_convention(patch_probe):
    output = run_probe(patch_probe, "readonly")
    assert "READONLY BEFORE 7" in output
    assert "READONLY AFTER 42" in output
    assert "READONLY REVERTED 7" in output


def test_explicit_property_accessors_can_be_patched(patch_probe):
    output = run_probe(patch_probe, "accessor")
    assert "ACCESSOR 42" in output
    assert "ACCESSOR REVERTED 9" in output


def test_replacement_names_can_be_escaped_keywords(patch_probe):
    assert "KEYWORD 15" in run_probe(patch_probe, "keyword")


def test_an_inlined_loop_observes_a_patch_from_another_thread(patch_probe):
    assert "CONCURRENT True" in run_probe(patch_probe, "concurrent", {"DOTNET_TieredCompilation": "0"})


def test_runtime_rejects_a_foreign_method_with_a_colliding_metadata_token(patch_probe):
    output = run_probe(patch_probe, "foreign")
    assert "FOREIGN ArgumentException:Method has no patch point: ForeignTarget::Twice" in output
    assert "collision=True" in output


def test_the_compiler_refuses_what_cannot_be_patched(patch_probe):
    output = run_probe(patch_probe, "refusals")
    assert "MISSING patch(1," in output and "FOPATCH002: no method Probe.World.Nope" in output
    assert "SIGNATURE patch(1," in output and "has the replacement's signature" in output
    assert "EXCLUDED patch(1," in output and "has no patch point" in output
    assert "INSTANCE patch(1," in output and "must be a static non-generic method" in output
    assert "EMPTY patch(1,1): error FOPATCH001: patch has no [ReplacesMethod] method" in output
    assert "SYNTAX patch(2," in output and "CS0103" in output


def test_a_process_with_patches_disabled_refuses_them(patch_probe):
    output = run_probe(patch_probe, "disabled", {"PROBE_PATCHES_DISABLED": "1"})
    assert "DISABLED InvalidOperationException:Script patches are disabled on this side (ManagedScript.ServerPatchesEnabled / ManagedScript.ClientPatchesEnabled) twice=4" in output
