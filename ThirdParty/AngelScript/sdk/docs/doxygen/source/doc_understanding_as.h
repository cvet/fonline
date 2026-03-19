/**

\page doc_understanding_as Understanding AngelScript

In order to successfully use AngelScript it is important to 
understand some differences between AngelScript and C++. While
the library has been written so that it can be embedded within
the application with as little change to the application functions
and classes as possible, there are some care that must be taken
when passing objects between AngelScript and C++.

 - \subpage doc_versions
 - \subpage doc_module
 - \subpage doc_as_vs_cpp_types
 - \subpage doc_obj_handle
 - \subpage doc_memory
 - \subpage doc_typeid
 - \subpage doc_callconv

\todo Add article on sandboxing





\page doc_typeid Structure of the typeid

The typeid used in several functions of the AngelScript interface is a 32bit signed integer.
Negative values are not valid typeids, and functions returning a typeid can thus use negative
values to \ref asERetCodes "indicate errors".

The value contained in the typeid is composed of two parts, the lower bits indicated by \ref asTYPEID_MASK_SEQNBR
is a sequence that will increment with each new type registered by the application or declared in scripts. The
higher bits form a bit mask indicating the category of the type. The bits in the bit mask have 
the following meaning:

 - bit \ref asTYPEID_APPOBJECT : The type is an application registered object
 - bit \ref asTYPEID_SCRIPTOBJECT : The type is a script declared type, i.e. object can be cast to \ref asIScriptObject
 - bit \ref asTYPEID_TEMPLATE : The type is a template, and cannot be instantiated
 - bit \ref asTYPEID_HANDLETOCONST : The type is a handle to a const object, i.e. the referenced object must not be modified
 - bit \ref asTYPEID_OBJHANDLE : The type is an object handle, i.e. the value must be dereferenced to get the address of the actual object
 - bit 32: The typeid is invalid, and the number indicates an error from \ref asERetCodes

To identify if a typeid is to a primitive check that the bits in \ref asTYPEID_MASK_OBJECT are zero. This will be true
for all built-in primitive types and for enums.

The built-in primitive types have pre-defined ids as seen in the enum \ref asETypeIdFlags, and can be directly compared 
for their value, e.g. the typeid for a 32bit signed integer is \ref asTYPEID_INT32.

To get further information about the type, e.g. the exact object type, then use \ref asIScriptEngine::GetTypeInfoById which
will return a \ref asITypeInfo that can then be used to get all details about the type.




\page doc_callconv Calling convention

The internal calling convention used by AngelScript in the virtual machine is quite simple.

All function arguments are pushed on the stack from last to first. All arguments are aligned to 4 byte boundaries. References are either 4 bytes or 8 bytes depending on the CPU architecture.
Primitives passed by value will be pushed as-is. Objects passed by value will have a reference to the object pushed on the stack while the object itself is stored on the heap. The called 
function is responsible for cleaning up the object and freeing the memory. For object handles, the calling function will increase the refcount as the arguments are prepared and the handle 
is pushed on the stack, the ownership of the handle is then handed over to the called function that must release it before returning.

A variable argument type, ?, will have the typeid pushed on the stack before the actual argument value.

If the function is a variadic, ..., i.e. accepts any number of arguments, then the number of actual arguments is pushed on the stack after the first function argument.

If the function returns an object by value, then a hidden argument with the address where the returned object must be initialized is pushed on the stack.
The returned object is owned by the called function until the control is returned to the caller, i.e. if an exception happens between the object is initialized 
and the control is returned to the caller, the exception handler will clean up the returned object as part of the called functions call frame.

If the function is a class method, then the address of the object instance is pushed on the stack as the last argument, thus allowing the called function to
always rely on the this pointer to be on position 0 of the stack frame.

Here's a representation of the stack for an object method that returns an object by value and has variadic function arguments:

<table>
<tr>
<td>argN</td><td>...</td><td>arg2</td><td>arg1</td><td>var cnt = N</td><td>ret obj ptr</td><td>this obj ptr</td><td>local variables</td><td>space for preparing next function call</td>
</tr>
</table>

*/
