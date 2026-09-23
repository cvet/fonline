namespace FOnline.PatchPointWeaver;

using System;
using System.Collections.Generic;
using System.Collections.Immutable;
using System.IO;
using System.Linq;
using System.Reflection.Metadata.Ecma335;
using System.Reflection.PortableExecutable;
using Mono.Cecil;
using Mono.Cecil.Cil;
using Mono.Cecil.Rocks;
using MethodAttributes = Mono.Cecil.MethodAttributes;
using MethodBody = Mono.Cecil.Cil.MethodBody;
using MethodImplAttributes = Mono.Cecil.MethodImplAttributes;
using TypeAttributes = Mono.Cecil.TypeAttributes;
using Srm = System.Reflection.Metadata;

// Gives every eligible method of a script assembly a patch point that ScriptPatches can redirect to a replacement:
//
//     ldsfld  PatchPointSlots::Active     // false while no patch is applied
//     brtrue  CHECK
//   BODY: <original body>
//   CHECK: ldc.i4 <slot>; call PatchPointRedirects::Slot; brfalse BODY
//          <arguments>; ldc.i4 <slot>; ldtoken <method>; call PatchPointRedirects::R<n>(..., slot, self); ret
//
// No value crosses a branch and no argument lives across a call, so Mono keeps the check out of callee-saved registers
// and the method prologue. The calli lives in a redirect shared by every method with the same erased signature,
// because Mono refuses to inline a method that contains an indirect call
internal static class Program
{
    // Must match ScriptPatches.TableResourceName
    private const string TableResourceName = "FOnline.PatchPoints.Table";
    private const string EngineNamespace = "FOnline";
    private const string SlotsTypeName = "FOnline.PatchPointSlots";
    private const string SlotsFieldName = "Slots";
    private const string ActiveFieldName = "Active";
    private const string RedirectsTypeName = "PatchPointRedirects";
    private const string NoPatchPointAttributeName = "FOnline.NoPatchPointAttribute";

    // INLINE_LENGTH_LIMIT of mono/mini/method-to-ir.c: a method this small is inlined unless its IL grows past it
    private const int InlineLengthLimit = 20;

    private static int Main(string[] args)
    {
        if (args.Length != 1) {
            Console.Error.WriteLine("Usage: FOnline.PatchPointWeaver <assembly>");
            return 2;
        }

        try {
            Weave(args[0]);
            return 0;
        }
        catch (Exception ex) {
            Console.Error.WriteLine("PatchPointWeaver: error: " + ex);
            return 1;
        }
    }

    private static void Weave(string path)
    {
        DefaultAssemblyResolver resolver = new DefaultAssemblyResolver();
        resolver.AddSearchDirectory(Path.GetDirectoryName(Path.GetFullPath(path)) ?? ".");
        (ModuleDefinition module, bool withSymbols) = ReadModule(path, resolver);

        using (module)
        {
            // The build runs this after every compile step, including the ones that found the assembly up to date
            if (module.Resources.Any(resource => resource.Name == TableResourceName)) {
                Console.WriteLine("PatchPointWeaver: " + module.Name + " already carries patch points");
                return;
            }

            FieldDefinition slots = FindSlotsField(module, SlotsFieldName, MetadataType.IntPtr);
            FieldDefinition active = FindSlotsField(module, ActiveFieldName, MetadataType.Boolean);
            List<MethodDefinition> methods = CollectMethods(module);
            TypeDefinition redirects =
                new TypeDefinition(EngineNamespace,
                                   RedirectsTypeName,
                                   TypeAttributes.NotPublic | TypeAttributes.Abstract | TypeAttributes.Sealed |
                                       TypeAttributes.BeforeFieldInit | TypeAttributes.Class,
                                   module.TypeSystem.Object);
            module.Types.Add(redirects);
            Dictionary<string, MethodDefinition> redirectsBySignature =
                new Dictionary<string, MethodDefinition>(StringComparer.Ordinal);
            MethodDefinition slotReader = MakeSlotReader(module, redirects, slots);
            int inlinable = 0;

            for (int slot = 0; slot < methods.Count; slot++) {
                MethodDefinition method = methods[slot];
                bool small = method.Body.CodeSize <= InlineLengthLimit && !IsStateMachineKickoff(method);
                MethodDefinition redirect = GetRedirect(module, redirects, redirectsBySignature, slotReader, method);
                InsertPatchPoint(method, active, slotReader, slot, redirect);

                if (small && (method.ImplAttributes & MethodImplAttributes.NoInlining) == 0) {
                    method.ImplAttributes |= MethodImplAttributes.AggressiveInlining;
                    inlinable++;
                }
            }

            // Tokens are known only once Cecil has laid the tables out, so the module is written twice; the symbol
            // writer names its output after the file, so both writes go to files beside the assembly
            WriterParameters writer = MakeWriterParameters(withSymbols);
            string draftPath = path + ".patchpoints-draft";
            string wovenPath = path + ".patchpoints";

            try {
                module.Write(draftPath, writer);

                int[] tokens = methods.Select(method => method.MetadataToken.ToInt32()).ToArray();
                byte[] table = new byte[tokens.Length * sizeof(int)];
                Buffer.BlockCopy(tokens, 0, table, 0, table.Length);
                module.Resources.Add(
                    new EmbeddedResource(TableResourceName, ManifestResourceAttributes.Private, table));

                module.Write(wovenPath, writer);
                VerifyTable(File.ReadAllBytes(wovenPath), methods, tokens);
                File.Move(wovenPath, path, true);
            }
            finally {
                File.Delete(draftPath);
                File.Delete(wovenPath);
            }

            Console.WriteLine("PatchPointWeaver: " + module.Name + ": " + methods.Count + " patch points, " +
                              redirectsBySignature.Count + " redirects, " + inlinable +
                              " small methods kept inlinable");
        }
    }

    private static (ModuleDefinition Module, bool WithSymbols) ReadModule(string path, IAssemblyResolver resolver)
    {
        try {
            return (ModuleDefinition.ReadModule(
                        path,
                        new ReaderParameters { ReadSymbols = true, InMemory = true, AssemblyResolver = resolver }),
                    true);
        }
        catch (SymbolsNotFoundException) {
            return (ModuleDefinition.ReadModule(
                        path,
                        new ReaderParameters { ReadSymbols = false, InMemory = true, AssemblyResolver = resolver }),
                    false);
        }
    }

    private static WriterParameters MakeWriterParameters(bool withSymbols)
    {
        return withSymbols ? new WriterParameters { WriteSymbols = true,
                                                    SymbolWriterProvider = new EmbeddedPortablePdbWriterProvider() }
                           : new WriterParameters();
    }

    private static FieldDefinition FindSlotsField(ModuleDefinition module, string name, MetadataType fieldType)
    {
        TypeDefinition type =
            module.GetType(SlotsTypeName) ??
            throw new InvalidOperationException(
                "Script assembly does not compile CoreScripts/ScriptPatches.cs: " + SlotsTypeName + " is missing");
        FieldDefinition field = type.Fields.FirstOrDefault(field => field.Name == name && field.IsStatic) ??
                                throw new InvalidOperationException(SlotsTypeName + "." + name + " is missing");

        if (field.FieldType.MetadataType != fieldType) {
            throw new InvalidOperationException(SlotsTypeName + "." + name + " must be " + fieldType);
        }

        return field;
    }

    // Script methods outside the engine namespace, in declaration order; the slot index is the order in this list
    private static List<MethodDefinition> CollectMethods(ModuleDefinition module)
    {
        List<MethodDefinition> methods = new List<MethodDefinition>();

        foreach (TypeDefinition type in module.GetTypes()) {
            if (IsEngineType(type) || IsGeneratedOrGeneric(type)) {
                continue;
            }

            foreach (MethodDefinition method in type.Methods) {
                if (IsEligible(method)) {
                    methods.Add(method);
                }
            }
        }

        return methods;
    }

    private static bool IsEngineType(TypeDefinition type)
    {
        TypeDefinition top = type;

        while (top.DeclaringType != null) {
            top = top.DeclaringType;
        }

        return top.Namespace == EngineNamespace ||
               top.Namespace.StartsWith(EngineNamespace + ".", StringComparison.Ordinal);
    }

    private static bool IsGeneratedOrGeneric(TypeDefinition type)
    {
        for (TypeDefinition? current = type; current != null; current = current.DeclaringType) {
            if (current.Name.StartsWith('<') || current.HasGenericParameters ||
                HasAttribute(current, "System.Runtime.CompilerServices.CompilerGeneratedAttribute")) {
                return true;
            }
        }

        return false;
    }

    // Constructors, generic methods and compiler-generated bodies (lambdas, state machines, auto-properties) are left
    // alone: a lambda or a state machine is replaced together with the method that creates it
    private static bool IsEligible(MethodDefinition method)
    {
        return method.HasBody && !method.IsConstructor && !method.HasGenericParameters && !method.IsAbstract &&
               method.CallingConvention != MethodCallingConvention.VarArg && !method.Name.StartsWith('<') &&
               !HasAttribute(method, "System.Runtime.CompilerServices.CompilerGeneratedAttribute") &&
               !HasAttribute(method, NoPatchPointAttributeName);
    }

    private static bool IsStateMachineKickoff(MethodDefinition method)
    {
        return HasAttribute(method, "System.Runtime.CompilerServices.AsyncStateMachineAttribute") ||
               HasAttribute(method, "System.Runtime.CompilerServices.IteratorStateMachineAttribute") ||
               HasAttribute(method, "System.Runtime.CompilerServices.AsyncIteratorStateMachineAttribute");
    }

    private static bool HasAttribute(ICustomAttributeProvider provider, string fullName)
    {
        return provider.HasCustomAttributes &&
               provider.CustomAttributes.Any(attribute => attribute.AttributeType.FullName == fullName);
    }

    private static void InsertPatchPoint(MethodDefinition method, FieldDefinition active, MethodDefinition slotReader,
                                         int slot, MethodDefinition redirect)
    {
        MethodBody body = method.Body;
        body.SimplifyMacros();
        ILProcessor il = body.GetILProcessor();
        Instruction first = body.Instructions[0];
        Instruction bodyStart = first;

        // A branch lands outside every protected region, so a body that opens with one gets a label before it
        if (body.ExceptionHandlers.Any(handler => handler.TryStart == first || handler.HandlerStart == first ||
                                                  handler.FilterStart == first)) {
            bodyStart = il.Create(OpCodes.Nop);
            il.InsertBefore(first, bodyStart);
        }

        Instruction check = il.Create(OpCodes.Ldc_I4, slot);
        il.InsertBefore(bodyStart, il.Create(OpCodes.Ldsfld, active));
        il.InsertBefore(bodyStart, il.Create(OpCodes.Brtrue, check));

        List<Instruction> cold = new List<Instruction> {
            check,
            il.Create(OpCodes.Call, slotReader),
            il.Create(OpCodes.Brfalse, bodyStart),
        };

        if (method.HasThis) {
            cold.Add(il.Create(OpCodes.Ldarg, body.ThisParameter));
        }

        foreach (ParameterDefinition parameter in method.Parameters) {
            cold.Add(il.Create(OpCodes.Ldarg, parameter));
        }

        cold.Add(il.Create(OpCodes.Ldc_I4, slot));
        cold.Add(il.Create(OpCodes.Ldtoken, method));
        cold.Add(il.Create(OpCodes.Call, redirect));

        if (NeedsCast(method.ReturnType, redirect.ReturnType)) {
            cold.Add(il.Create(OpCodes.Castclass, method.ReturnType));
        }

        cold.Add(il.Create(OpCodes.Ret));

        foreach (Instruction instruction in cold) {
            il.Append(instruction);
        }

        // A handler that ran to the end of the method now ends where the appended code begins
        foreach (ExceptionHandler handler in body.ExceptionHandlers) {
            handler.TryEnd ??= check;
            handler.HandlerEnd ??= check;
        }

        body.OptimizeMacros();
    }

    // Slots[slot] of the native table, inlined into every caller: the JIT folds the offset into one indexed load
    private static MethodDefinition MakeSlotReader(ModuleDefinition module, TypeDefinition redirects,
                                                   FieldDefinition slots)
    {
        MethodDefinition reader =
            new MethodDefinition("Slot",
                                 MethodAttributes.Assembly | MethodAttributes.Static | MethodAttributes.HideBySig,
                                 module.TypeSystem.IntPtr);
        reader.ImplAttributes |= MethodImplAttributes.AggressiveInlining;
        ParameterDefinition slot = new ParameterDefinition("slot", ParameterAttributes.None, module.TypeSystem.Int32);
        reader.Parameters.Add(slot);

        ILProcessor il = reader.Body.GetILProcessor();
        il.Emit(OpCodes.Ldsfld, slots);
        il.Emit(OpCodes.Ldarg, slot);
        il.Emit(OpCodes.Conv_I);
        il.Emit(OpCodes.Sizeof, module.TypeSystem.IntPtr);
        il.Emit(OpCodes.Mul);
        il.Emit(OpCodes.Add);
        il.Emit(OpCodes.Ldind_I);
        il.Emit(OpCodes.Ret);
        reader.Body.OptimizeMacros();
        redirects.Methods.Add(reader);
        return reader;
    }

    // Reference types are erased to object in a redirect, which the calling convention passes the same way, so
    // methods that differ only in class types share one
    private static MethodDefinition GetRedirect(ModuleDefinition module, TypeDefinition redirects,
                                                Dictionary<string, MethodDefinition> bySignature,
                                                MethodDefinition slotReader, MethodDefinition method)
    {
        List<TypeReference> parameters = new List<TypeReference>();

        if (method.HasThis) {
            parameters.Add(method.DeclaringType.IsValueType ? new ByReferenceType(method.DeclaringType)
                                                            : module.TypeSystem.Object);
        }

        foreach (ParameterDefinition parameter in method.Parameters) {
            parameters.Add(Erase(module, parameter.ParameterType));
        }

        TypeReference returnType = Erase(module, method.ReturnType);
        string key = returnType.FullName + "(" + string.Join(",", parameters.Select(type => type.FullName)) + ")";

        if (bySignature.TryGetValue(key, out MethodDefinition? existing)) {
            return existing;
        }

        MethodDefinition redirect =
            new MethodDefinition("R" + bySignature.Count,
                                 MethodAttributes.Assembly | MethodAttributes.Static | MethodAttributes.HideBySig,
                                 returnType);
        redirect.ImplAttributes |= MethodImplAttributes.NoInlining;
        CallSite site =
            new CallSite(returnType) { CallingConvention = MethodCallingConvention.Default, HasThis = false };

        for (int i = 0; i < parameters.Count; i++) {
            redirect.Parameters.Add(new ParameterDefinition("arg" + i, ParameterAttributes.None, parameters[i]));
            site.Parameters.Add(new ParameterDefinition(parameters[i]));
        }

        TypeReference handleType =
            new TypeReference("System", "RuntimeMethodHandle", module, module.TypeSystem.CoreLibrary, true);
        MethodReference getFunctionPointer =
            new MethodReference("GetFunctionPointer", module.TypeSystem.IntPtr, handleType) { HasThis = true };
        ParameterDefinition slot = new ParameterDefinition("slot", ParameterAttributes.None, module.TypeSystem.Int32);
        ParameterDefinition self = new ParameterDefinition("self", ParameterAttributes.None, handleType);
        redirect.Parameters.Add(slot);
        redirect.Parameters.Add(self);
        VariableDefinition function = new VariableDefinition(module.TypeSystem.IntPtr);
        redirect.Body.Variables.Add(function);
        redirect.Body.InitLocals = true;

        // An empty slot means a revert raced the method's check, and calling the method again finds its body. Only a
        // multithreaded runtime races so, and there the handle yields native code that calli accepts
        ILProcessor il = redirect.Body.GetILProcessor();
        Instruction call = il.Create(OpCodes.Nop);
        il.Emit(OpCodes.Ldarg, slot);
        il.Emit(OpCodes.Call, slotReader);
        il.Emit(OpCodes.Stloc, function);
        il.Emit(OpCodes.Ldloc, function);
        il.Emit(OpCodes.Brtrue, call);
        il.Emit(OpCodes.Ldarga, self);
        il.Emit(OpCodes.Call, getFunctionPointer);
        il.Emit(OpCodes.Stloc, function);
        il.Append(call);

        for (int i = 0; i < parameters.Count; i++) {
            il.Emit(OpCodes.Ldarg, redirect.Parameters[i]);
        }

        il.Emit(OpCodes.Ldloc, function);
        il.Emit(OpCodes.Calli, site);
        il.Emit(OpCodes.Ret);
        redirect.Body.OptimizeMacros();
        redirects.Methods.Add(redirect);
        bySignature.Add(key, redirect);
        return redirect;
    }

    private static TypeReference Erase(ModuleDefinition module, TypeReference type)
    {
        if (type is IModifierType || type.IsByReference || type.IsPointer || type.IsFunctionPointer ||
            type.IsGenericParameter || type.IsValueType || type.IsPrimitive || type.MetadataType == MetadataType.Void) {
            return type;
        }

        return module.TypeSystem.Object;
    }

    private static bool NeedsCast(TypeReference methodReturn, TypeReference redirectReturn)
    {
        return methodReturn.FullName != redirectReturn.FullName && redirectReturn.MetadataType == MetadataType.Object &&
               methodReturn.MetadataType != MetadataType.Object;
    }

    // Guards the second write: each slot must still name the method it was computed for
    private static void VerifyTable(byte[] image, List<MethodDefinition> methods, int[] tokens)
    {
        using PEReader pe = new PEReader(ImmutableArray.Create(image));
        Srm.MetadataReader metadata = Srm.PEReaderExtensions.GetMetadataReader(pe);

        for (int slot = 0; slot < methods.Count; slot++) {
            Srm.MethodDefinitionHandle handle = MetadataTokens.MethodDefinitionHandle(tokens[slot] & 0x00FFFFFF);
            Srm.MethodDefinition row = metadata.GetMethodDefinition(handle);
            Srm.TypeDefinition owner = metadata.GetTypeDefinition(row.GetDeclaringType());

            if (metadata.GetString(row.Name) != methods[slot].Name ||
                metadata.GetString(owner.Name) != methods[slot].DeclaringType.Name) {
                throw new InvalidOperationException("Patch point table lost its methods on write: slot " + slot + " " +
                                                    methods[slot].FullName);
            }
        }
    }
}
