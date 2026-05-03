/**

\page doc_adv_var_type The variable parameter type

The application can register functions that take a reference to a variable
type, which means that the function can receive a reference to a variable of
any type. This is useful when making generic containers.


When a function is registered with this special parameter type, the
function will receive both the reference and an extra argument with the \ref doc_typeid "type id" 
of the variable type. The reference refers to the actual value that the
caller sent, i.e. if the expression is an object handle then the reference
will refer to the handle, not the actual object. 

\code
// An example usage with a native function
engine->RegisterGlobalFunction("void func_c(?&in)", asFUNCTION(func_c), asCALL_CDECL);

void func_c(void *ref, int typeId)
{
    // Do something with the reference

    // The type of the reference is determined through the type id
}

// An example usage with a generic function
engine->RegisterGlobalFunction("void func_g(?&in)", asFUNCTION(func_g), asCALL_GENERIC);

void func_g(asIScriptGeneric *gen)
{
    void *ref = gen->GetArgAddress(0);
    int typeId = gen->GetArgTypeId(0);

    func_c(ref, typeId);
}
\endcode

The variable type can also be used with <code>out</code> references, but not
with <code>inout</code> references. Currently it can only be used with global
functions, object constructors, and object methods. It cannot be used with
other behaviours and operators.

The variable type is not available within scripts, so it can only be used
to register application functions.

\see \ref doc_addon_any and \ref doc_addon_dict for examples. \ref doc_typeid for information on how to interpret the type id.


\section doc_adv_var_type_1 Variable conversion operators

The variable parameter type can also be used in special versions of \ref doc_script_class_conv "the opConv and opCast"
operator overloads. This is especially useful for generic container types that need to
be able to hold any type of content.

 - void opCast(?&out)
 - void opConv(?&out)

\see \ref doc_addon_handle and \ref doc_addon_dict for examples




\page doc_adv_variadic Variadic arguments

Functions that take variadic arguments can be registered with the engine, though the function has to be implemented with the \ref doc_generic "generic calling convention".

When a function is registered as taking variadic arguments, the compiler will push an extra hidden argument on the stack holding the number of arguments passed to the function. 
This is used by the \ref asIScriptGeneric interface to know how many arguments there are. The application doesn't have to explicitly read this hidden argument, 
but can just use the \ref asIScriptGeneric::GetArgCount "GetArgCount" method normally. The argument type for all the arguments passed to the variadic will be the same, which is 
the type that the application registered for it. 

A common option for the argument is <tt>const ?&in ...</tt> for taking a list of \ref doc_adv_var_type "variable argument types" as input, or <tt>?&out ...</tt> for taking a list of output arguments of variable types. 
But it is perfectly fine to specify a single type, e.g. <tt>int ...</tt>, if that is all that is required.

\code
    r = engine->RegisterGlobalFunction("string format(const string&in fmt, const ?&in ...)", asFUNCTION(StringFormat), asCALL_GENERIC); assert(r >= 0);
    r = engine->RegisterGlobalFunction("uint scan(const string&in str, ?&out ...)", asFUNCTION(StringScan), asCALL_GENERIC); assert(r >= 0);
\endcode

\see The implementation of <tt>format</tt> and <tt>scan</tt> in the \ref doc_addon_std_string "string add-on"

*/
