/**

\page doc_serialization Serialization

To serialize scripts the application needs to be able to iterate through all variables, and objects that have any relation to the 
script and store their content in a way that would allow it to be restored at a later time. For simple values this is trivial, but 
for objects and handles it becomes more complex as it is necessary to keep track of references between objects and internal structure 
of both classes defined by scripts and application registered types.

Iterating through the variables and members of objects is already described in the article about \ref doc_adv_reflection "reflection", 
so this article will focus on the storing and restoring of values.

Look at the source code for the \ref doc_addon_serializer add-on for a sample implementation of serialization.

\see \ref doc_adv_dynamic_build_hot

\section doc_serialization_modules Serialization of modules

To serialize the script modules you'll need to enumerate all the global variables and \ref doc_serialization_vars "serialize each of them".

When deserializing the module, first compile it normally either from \ref doc_compile_script "source script" or 
by \ref doc_adv_precompile "loading a pre-compiled byte code", except you first need to turn off initialization of 
global variables by \ref asIScriptEngine::SetEngineProperty "turning off the engine property asEP_INIT_GLOBAL_VARS_AFTER_BUILD".

\section doc_serialization_vars Serialization of global variables

To serialize a global variable you'll need the name and namespace to use as the key for the variable, and then the type id and address 
of the variable. You get these with the methods \ref asIScriptModule::GetGlobalVar and \ref asIScriptModule::GetAddressOfGlobalVar. If 
the type id is for a primitive type, then the value can be stored as is. If it is a handle or reference you'll need to serialize the 
reference itself and the object it refers to. If the type id is an object type then \ref doc_serialization_objects "serialize the object" 
and its content.

To deserialize the global variable, you'll use the name and namespace to look it up, and then use GetAddressOfGlobalVar to get the 
address of the memory that needs to be restored with the serialized content.

\section doc_serialization_objects Serialization of objects

To serialize script objects you'll use the \ref asIScriptObject interface to iterate over the members to store the content. Remember that objects can hold 
references to other objects and even to itself so it is important to keep track of object instances already serialized and just store a reference if 
the same object comes up again.

When deserializing the script objects you should first allocate the memory using \ref asIScriptEngine::CreateUninitializedScriptObject so that the constructor 
is not executed, and then iterate over the members to restore their content.

For application registered types you obviously need to provide your own implementation as the script engine doesn't know the full content of the types 
and thus cannot provide an interface for serialization.

\section doc_serialization_contexts Serialization of contexts

Serialization of a script context involves storing the full call stack with all the function calls, local variables, registers, etc. To do this you'll
use the \ref asIScriptContext interface. 

 - Use \ref asIScriptContext::GetCallstackSize "GetCallstackSize" to get the size of the call stack
 - For each call stack entry do:
   - Use \ref asIScriptContext::GetCallStateRegisters "GetCallStateRegisters" to store the registers, i.e. program pointer, stack pointer, etc.
   - Use \ref asIScriptContext::GetVarCount "GetVarCount", \ref asIScriptContext::GetVar "GetVar", and \ref asIScriptContext::GetAddressOfVar "GetAddressOfVar" to store all local variables, including unnamed temporary variables
   - Use \ref asIScriptContext::GetArgsOnStackCount "GetArgsOnStackCount" and \ref asIScriptContext::GetArgOnStack "GetArgOnStack" to store values pushed on the stack for a subsequent function call
 - Use \ref asIScriptContext::GetStateRegisters "GetStateRegisters" to store additional context registers   

To deserialize a context follow these steps:

 - Call \ref asIScriptContext::StartDeserialization "StartDeserialization" to tell the context that a deserialization will be done
 - For each call stack entry previously stored do:
   - Call \ref asIScriptContext::PushFunction "PushFunction" to reserve space for a call stack entry
   - Call \ref asIScriptContext::SetCallStateRegisters "SetCallStateRegisters" to restore the registers
   - Use \ref asIScriptContext::GetVar "GetVar" and \ref asIScriptContext::GetAddressOfVar "GetAddressOfVar" to restore all local variables
   - Use \ref asIScriptContext::GetArgOnStack "GetArgOnStack" to restore values pushed on the stack
 - Call \ref asIScriptContext::SetStateRegisters "SetStateRegisters" to restore additional context registers
 - Call \ref asIScriptContext::FinishDeserialization "FinishDeserialization" to conclude the serialization and allow the execution to resume

\subsection Limitations

The following are some limitations with serialization of contexts:

 - The serialization is platform dependent, i.e. it is not possible to serialize a context on a 32bit platform and then deserialize it on a 64bit platform or vice versa
 - Attempting to deserialize a context after \ref doc_adv_dynamic_build_hot "hot reloading" modified scripts has undefined behavior, and will most likely cause crashes




*/
