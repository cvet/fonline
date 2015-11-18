/**

\page doc_datatypes Data types

Note that the host application may add types specific to that application, refer to the application's manual for more information.

 - \subpage doc_builtin_types
 - \subpage doc_addon_types



\page doc_builtin_types Built-in types

 - \subpage doc_datatypes_primitives 
 - \subpage doc_datatypes_obj
 - \subpage doc_datatypes_funcptr
 - \subpage doc_datatypes_auto

 
 
\page doc_addon_types Add-on types
 
 - \subpage doc_datatypes_strings
 - \subpage doc_datatypes_arrays
 - \subpage doc_datatypes_dictionary
 - \subpage doc_datatypes_ref
 - \subpage doc_datatypes_weakref


 
 
\page doc_datatypes_primitives Primitives

\section void void

<code>void</code> is not really a data type, more like lack of data type. It can only be used to tell the compiler that a function doesn't return any data.



\section bool bool

<code>bool</code> is a boolean type with only two
possible values: <code>true</code> or
<code>false</code>. The keywords
<code>true</code> and
<code>false</code> are constants of type
<code>bool</code> that can be used as such in expressions.


\section int Integer numbers

<table border=0 cellspacing=0 cellpadding=0>
<tr><td width=100><b>type</b></td><td width=200><b>min value</b></td><td><b>max value</b></td></tr>
<tr><td><code>int8  </code></td><td>                      -128</td><td>                       127</td></tr>
<tr><td><code>int16 </code></td><td>                   -32,768</td><td>                    32,767</td></tr>
<tr><td><code>int   </code></td><td>            -2,147,483,648</td><td>             2,147,483,647</td></tr>
<tr><td><code>int64 </code></td><td>-9,223,372,036,854,775,808</td><td> 9,223,372,036,854,775,807</td></tr>
<tr><td><code>uint8 </code></td><td>                         0</td><td>                       255</td></tr>
<tr><td><code>uint16</code></td><td>                         0</td><td>                    65,535</td></tr>
<tr><td><code>uint  </code></td><td>                         0</td><td>             4,294,967,295</td></tr>
<tr><td><code>uint64</code></td><td>                         0</td><td>18,446,744,073,709,551,615</td></tr>
</table>

As the scripting engine has been optimized for 32 bit datatypes, using the smaller variants is only recommended for accessing application specified variables. For local variables it is better to use the 32 bit variant.

<code>int32</code> is an alias for <code>int</code>, and <code>uint32</code> is an alias for <code>uint</code>.




\section real Real numbers

<table border=0 cellspacing=0 cellpadding=0>
<tr><td width=100><b>type</b></td><td width=230><b>range of values</b></td><td width=230><b>smallest positive value</b></td> <td><b>maximum digits</b></td></tr>
<tr><td><code>float </code></td> <td>+/- 3.402823466e+38      </td> <td>1.175494351e-38      </td> <td>6 </td></tr>
<tr><td><code>double</code></td> <td>+/- 1.79769313486231e+308</td> <td>2.22507385850720e-308</td> <td>15</td></tr>
</table>

\note These numbers assume the platform uses the IEEE 754 to represent floating point numbers in the CPU

Rounding errors may occur if more digits than the maximum number of digits are used.

<b>Curiousity</b>: Real numbers may also have the additional values of positive and negative 0 or 
infinite, and NaN (Not-a-Number). For <code>float</code> NaN is represented by the 32 bit data word 0x7fc00000.








\page doc_datatypes_arrays Arrays

\note Arrays are only available in the scripts if the application \ref doc_addon_array "registers the support for them". 
The syntax for using arrays may differ for the application you're working with so consult the application's manual
for more details.

It is possible to declare array variables with the array identifier followed by the type of the 
elements within angle brackets. 

Example:

<pre>
  array<int> a, b, c;
  array<Foo\@> d;
</pre>

<code>a</code>, <code>b</code>, and <code>c</code> are now arrays of integers, and <code>d</code>
is an array of handles to objects of the Foo type.

When declaring arrays it is possible to define the initial size of the array by passing the length as
a parameter to the constructor. The elements can also be individually initialized by specifying an 
initialization list. Example:

<pre>
  array<int> a;           // A zero-length array of integers
  array<int> b(3);        // An array of integers with 3 elements
  array<int> c(3, 1);     // An array of integers with 3 elements, all set to 1 by default
  array<int> d = {5,6,7}; // An array of integers with 3 elements with specific values
</pre>

Multidimensional arrays are supported as arrays of arrays, for example:

<pre>
  array<array<int>> a;                     // An empty array of arrays of integers
  array<array<int>> b = {{1,2},{3,4}}      // A 2 by 2 array with initialized values
  array<array<int>> c(10, array<int>(10)); // A 10 by 10 array of integers with uninitialized values
</pre>

Each element in the array is accessed with the indexing operator. The indices are zero based, i.e. the
range of valid indices are from 0 to length - 1.

<pre>
  a[0] = some_value;
</pre>

When the array stores \ref doc_script_handle "handles" the elements are assigned using the \ref handle "handle assignment".

<pre>
  // Declare an array with initial length 1
  array<Foo\@> arr(1);
  
  // Set the first element to point to a new instance of Foo
  \@arr[0] = Foo();
</pre>



\section doc_datatypes_arrays_addon Supporting array object and functions

The array object supports a number of operators and has several class methods to facilitate the manipulation of strings.

The array object is a \ref doc_datatypes_obj "reference type" even if the elements are not, so it's possible
to use handles to the array object when passing it around to avoid costly copies.

\subsection doc_datatypes_array_addon_ops Operators

 - =       assignment
 - []      index operator
 - ==, !=  equality

\subsection doc_datatypes_array_addon_mthd Methods

  - uint length() const
  - void resize(uint)
  - void reverse()
  - void insertAt(uint index, const T& in)
  - void insertLast(const T& in)
  - void removeAt(uint index)
  - void removeLast()
  - void sortAsc()
  - void sortAsc(uint startAt, uint count)
  - void sortDesc()
  - void sortDesc(uint startAt, uint count)
  - int  find(const T& in)
  - int  find(uint startAt, const T& in)
  - int  findByRef(const T& in)
  - int  findByRef(uint startAt, const T& in)

The T represents the type of the array elements.
  
Script example:

<pre>
  int main()
  {
    array<int> arr = {1,2,3}; // 1,2,3
    arr.insertLast(0);        // 1,2,3,0
    arr.insertAt(2,4);        // 1,2,4,3,0
    arr.removeAt(1);          // 1,4,3,0

    arr.sortAsc();            // 0,1,3,4
  
    int sum = 0;
    for( uint n = 0; n < arr.length(); n++ )
      sum += arr[n];
      
    return sum;
  }
</pre>





\page doc_datatypes_dictionary dictionary

\note Dictionaries are only available in the scripts if the application \ref doc_addon_dict "registers the support for them". 
The syntax for using dictionaries may differ for the application you're working with so consult the application's manual
for more details.

The dictionary stores key-value pairs, where the key is a string, and the value can be of any type. Key-value
pairs can be added or removed dynamically, making the dictionary a good general purpose container object.

<pre>
  obj object;
  obj \@handle;
  
  // Initialize with a list
  dictionary dict = {{'one', 1}, {'object', object}, {'handle', \@handle}};
  
  // Examine and access the values through get or set methods ...
  if( dict.exists('one') )
  {
    // get returns true if the stored type is compatible with the requested type
    bool isValid = dict.get('handle', \@handle);
    if( isValid )
    {
      dict.delete('object');
      dict.set('value', 1);
    }
  }
  
  // ... or through index operators
  int val = int(dict['value']);
  dict['value'] = val + 1;
  \@handle = cast<obj>(dict['handle']);
  if( handle is null )
    \@dict['handle'] = object;
  
  // Delete everything with a single call
  dict.deleteAll();
</pre>

\section doc_datatypes_dictionary_addon Supporting dictionary object and functions

The dictionary object is a \ref doc_datatypes_obj "reference type", so it's possible
to use handles to the dictionary object when passing it around to avoid costly copies.

\subsection doc_datatypes_dictionary_addon_ops Operators

 - =       assignment
 - []      index operator
 
The assignment operator performs a shallow copy of the content.

The index operator takes a string for the key, and returns a reference to the value.
If the key/value pair doesn't exist, it will be inserted with a null value.

\subsection doc_datatypes_dictionary_addon_mthd Methods

 - void set(const string &in key, ? &in value)
 - void set(const string &in key, int64 &in value)
 - void set(const string &in key, double &in value)
 - bool get(const string &in key, ? &out value) const
 - bool get(const string &in key, int64 &out value) const
 - bool get(const string &in key, double &out value) const
 - array<string> \@getKeys() const
 - bool exists(const string &in key) const
 - void delete(const string &in key)
 - void deleteAll()
 - bool isEmpty() const
 - uint getSize() const







\page doc_datatypes_obj Objects and handles

\section objects Objects

There are two forms of objects, reference types and value types. 

Value types behave much like the primitive types, in that they are allocated on the stack and deallocated 
when the variable goes out of scope. Only the application can register these types, so you need to check
with the application's documentation for more information about the registered types.

Reference types are allocated on the memory heap, and may outlive the initial variable that allocates
them if another reference to the instance is kept. All \ref doc_script_class "script declared classes" are reference types. 
\ref doc_global_interface "Interfaces" are a special form of reference types, that cannot be instanciated, but can be used to access
the objects that implement the interfaces without knowing exactly what type of object it is.

<pre>
  obj o;      // An object is instanciated
  o = obj();  // A temporary instance is created whose 
              // value is assigned to the variable
</pre>



\section handles Object handles

Object handles are a special type that can be used to hold references to other objects. When calling methods or 
accessing properties on a variable that is an object handle you will be accessing the actual object that the 
handle references, just as if it was an alias. Note that unless initialized with the handle of an object, the 
handle is <code>null</code>.

<pre>
  obj o;
  obj@ a;           // a is initialized to null
  obj@ b = \@o;      // b holds a reference to o

  b.ModifyMe();     // The method modifies the original object

  if( a is null )   // Verify if the object points to an object
  {
    \@a = \@b;        // Make a hold a reference to the same object as b
  }
</pre>

Not all types allow a handle to be taken. Neither of the primitive types can have handles, and there may exist some object types that do not allow handles. Which objects allow handles or not, are up to the application that registers them.

Object handle and array type modifiers can be combined to form handles to arrays, or arrays of handles, etc.

\see \ref doc_script_handle











\page doc_datatypes_strings Strings

\note Strings are only available in the scripts if the application \ref doc_addon_std_string "registers the support for them". 
The syntax for using strings may differ for the application you're working with so consult the application's manual
for more details.

Strings hold an array of bytes or 16bit words depending on the application settings. 
Normally they are used to store text but can really store any kind of binary data.

There are two types of string constants supported in the AngelScript
language, the normal quoted string, and the documentation strings,
called heredoc strings.

The normal strings are written between double quotation marks (<code>"</code>) or single quotation marks (<code>'</code>).
Inside the constant strings some escape sequences can be used to write exact
byte values that might not be possible to write in your normal editor.


<table cellspacing=0 cellpadding=0 border=0>
<tr><td width=100 valign=top><b>sequence</b></td>
<td valign=top width=100><b>value</b></td>
<td valign=top><b>description</b></td></tr>

<tr><td width=80 valign=top><code>\\0</code>&nbsp;  </td>
<td valign=top width=50>0</td>
<td valign=top>null character</td></tr>
<tr><td width=80 valign=top><code>\\\\</code>&nbsp;  </td>
<td valign=top width=50>92</td>
<td valign=top>back-slash</td></tr>
<tr><td width=80 valign=top><code>\\'</code>&nbsp;  </td>
<td valign=top width=50>39</td>
<td valign=top>single quotation mark (apostrophe)</td></tr>
<tr><td width=80 valign=top><code>\\"</code>&nbsp;  </td>
<td valign=top width=50>34</td>
<td valign=top>double quotation mark</td></tr>
<tr><td width=80 valign=top><code>\\n</code>&nbsp;  </td>
<td valign=top width=50>10</td>
<td valign=top>new line feed</td></tr>
<tr><td width=80 valign=top><code>\\r</code>&nbsp;  </td>
<td valign=top width=50>13</td>
<td valign=top>carriage return</td></tr>
<tr><td width=80 valign=top><code>\\t</code>&nbsp;  </td>
<td valign=top width=50>9</td>
<td valign=top>tab character</td></tr>
<tr><td width=80 valign=top><code>\\xFFFF</code>&nbsp;</td>
<td valign=top width=50>0xFFFF</td>
<td valign=top>FFFF should be exchanged for a 1 to 4 digit hexadecimal number representing the value wanted. If the application uses 8bit strings then only values up to 255 is accepted.</td></tr>
<tr><td width=80 valign=top><code>\\uFFFF</code>&nbsp;</td>
<td valign=top width=50>0xFFFF</td>
<td valign=top>FFFF should be exchanged for the hexadecimal number representing the unicode code point</td></tr>
<tr><td width=80 valign=top><code>\\UFFFFFFFF</code>&nbsp;</td>
<td valign=top width=50>0xFFFFFFFF</td>
<td valign=top>FFFFFFFF should be exchanged for the hexadecimal number representing the unicode code point</td></tr>
</table>


<pre>
  string str1 = "This is a string with \"escape sequences\" .";
  string str2 = 'If single quotes are used then double quotes can be included without "escape sequences".';
</pre>


The heredoc strings are designed for inclusion of large portions of text
without processing of escape sequences. A heredoc string is surrounded by
triple double-quotation marks (<code>"""</code>), and can span multiple lines
of code. If the characters following the start of the string until the first
linebreak only contains white space, it is automatically removed by the
compiler. Likewise if the characters following the last line break until the
end of the string only contains white space this is also removed.


<pre>
  string str = """
  This is some text without "escape sequences". This is some text.
  This is some text. This is some text. This is some text. This is
  some text. This is some text. This is some text. This is some
  text. This is some text. This is some text. This is some text.
  This is some text.
  """;
</pre>

If more than one string constants are written in sequence with only whitespace or
comments between them the compiler will concatenate them into one constant.

<pre>
  string str = "First line.\n"
               "Second line.\n"
               "Third line.\n";
</pre>

The escape sequences \\u and \\U will add the specified unicode code point as a
UTF-8 or UTF-16 encoded sequence depending on the application settings. Only valid unicode 5.1 
code points are accepted, i.e. code points between U+D800 and U+DFFF (reserved for surrogate pairs) 
or above U+10FFFF are not accepted.

\section doc_datatypes_strings_addon Supporting string object and functions

The string object supports a number of operators, and has several class methods and supporting 
global functions to facilitate the manipulation of strings.

\subsection doc_datatypes_strings_addon_ops Operators

 - =            assignment
 - +, +=        concatenation
 - ==, !=       equality
 - <, >, <=, >= comparison
 - []           index operator

Concatenation or assignment with primitives is allowed, and will then do
a default transformation of the primitive to a string.
 
\subsection doc_datatypes_strings_addon_mthd Methods

 - uint           length() const
 - void           resize(uint)
 - bool           isEmpty() const
 - string         substr(uint start = 0, int count = -1) const
 - int            findFirst(const string &in str, uint start = 0) const
 - int            findLast(const string &in str, int start = -1) const
 - array<string>@ split(const string &in delimiter) const

\subsection doc_datatypes_strings_addon_funcs Functions

 - string join(const array<string> &in arr, const string &in delimiter)
 - int64  parseInt(const string &in, uint base = 10, uint &out byteCount = 0)
 - double parseFloat(const string &in, uint &out byteCount = 0)
 - string formatInt(int64 val, const string &in options = '', uint width = 0)
 - string formatUInt(uint64 val, const string &in options = '', uint width = 0)
 - string formatFloat(double val, const string &in options = '', uint width = 0, uint precision = 0)

The format functions takes a string that defines how the number should be formatted. The string
is a combination of the following characters:

  - l = left justify
  - 0 = pad with zeroes
  - + = always include the sign, even if positive
  - space = add a space in case of positive number
  - h = hexadecimal integer small letters
  - H = hexadecimal integer capital letters
  - e = exponent character with small e
  - E = exponent character with capital E

Examples:

<pre>
  // Left justify number in string with 10 characters
  string justified = formatInt(number, 'l', 10);
  
  // Create hexadecimal representation with capital letters, right justified
  string hex = formatInt(number, 'H', 10);
  
  // Right justified, padded with zeroes and two digits after decimal separator
  string num = formatFloat(number, '0', 8, 2);
</pre>






\page doc_datatypes_ref ref

\note <code>ref</code> is only available in the scripts if the application \ref doc_addon_handle "registers the support for it". 

The <code>ref</code> type works like a generic object handle. Normally a \ref handles "handle" can only refer to 
objects of a specific type or those related to it, however not all object types are related, and this is
where <code>ref</code> comes in. Being completely generic it can refer to any object type (as long as it is a \ref doc_datatypes_obj "reference type").

<pre>
  // Two unrelated types
  class car {}
  class banana {}

  // A function that take the ref type as argument can work on both types
  void func(ref \@handle)
  {
    // Cast the handle to the expected type and check which cast work
    car \@c = cast<car>(handle);
    banana \@b = cast<banana>(handle);
    if( c !is null )
      print('The handle refers to a car\\n');
    else if( b !is null )
      print('The handle refers to a banana\\n');
    else if( handle !is null )
      print('The handle refers to a different object\\n');
    else
      print('The handle is null\\n');
  }

  void main()
  {
    // Assigning a ref handle works the same way as ordinary handles
    ref \@r = car();
    func(r);
    \@r = banana();
    func(r);
  }
</pre>

\section doc_datatypes_ref_addon Supporting ref object

The ref object supports only a few operators as it is just a place holder for handles.

\subsection doc_datatypes_ref_addon_ops Operators

 - \@=          handle assignment
 - is, !is      identity operator
 - cast<type>   cast operator



 
 
\page doc_datatypes_weakref weakref

\note <code>weakref</code> is only available in the scripts if the application \ref doc_addon_weakref "registers the support for it". 

An object handle will keep the object it refers to alive as long as the handle itself exists. A <code>weakref</code> object
can be used in place of the handle where the reference to the object is needed but the object shouldn't be kept alive.

<pre>
  class MyClass {}
  MyClass \@obj1 = MyClass();
  
  // Keep a weakref to the object
  weakref<MyClass> r1(obj1);
  
  // Keep a weakref to a readonly object
  const_weakref<MyClass> r2(obj1);
  
  // As long as there is a strong reference to the object, 
  // the weakref will be able to return a handle to the object
  MyClass \@obj2 = r1.get();
  assert( obj2 !is null );
  
  // After all strong references are removed the
  // weakref will only return null
  \@obj1 = null;
  \@obj2 = null;
  
  const MyClass \@obj3 = r2.get();
  assert( obj3 is null );
</pre>

\section doc_datatypes_weakref_addon Supporting weakref object



\subsection doc_datatypes_weakref_addon_ops Operators

 - \@=          handle assignment
 - is, !is      identity operator
 - cast<type>   cast operator

\subsection doc_datatypes_array_addon_mthd Methods

 - T@ get() const



 
 
 

\page doc_datatypes_funcptr Function handles

A function handle is a data type that can be dynamically set to point to a global function that has
a matching function signature as that defined by the variable declaration. Function handles are commonly
used for callbacks, i.e. where a piece of code must be able to call back to some code based on some 
conditions, but the code that needs to be called is not known at compile time.

To use function handles it is first necessary to \ref doc_global_funcdef "define the function signature" 
that will be used at the global scope. Once that is done the variables can be declared using that definition.

Here's an example that shows the syntax for using function handles

<pre>
  // Define a function signature for the function handle
  funcdef bool CALLBACK(int, int);

  // An example function that shows how to use this
  void main()
  {
    // Declare a function handle, and set it 
    // to point to the myCompare function.
    CALLBACK \@func = \@myCompare;

    // The function handle can be compared with the 'is' operator
    if( func is null )
    {
      print("The function handle is null\n");
      return;
    }

    // Call the function through the handle, just as if it was a normal function
    if( func(1, 2) )
    {
      print("The function returned true\n");
    }
    else
    {
      print("The function returned false\n");
    }
  }

  // This function matches the CALLBACK definition, since it has 
  // the same return type and parameter types.
  bool myCompare(int a, int b)
  {
    return a > b;
  }
</pre>

\section doc_datatypes_delegate Delegates

It is also possible to take function handles to class methods, but in this case the class method
must be bound to the object instance that will be used for the call. To do this binding is called
creating a delegate, and is done by performing a construct call for the declared function definition
passing the class method as the argument.

<pre>
  class A
  {
    bool Cmp(int a, int b)
    {
       count++;
       return a > b;
    }
    int count = 0;
  }
  
  void main()
  {
    A a;
    
    // Create the delegate for the A::Cmp class method
    CALLBACK \@func = CALLBACK(a.Cmp);
    
    // Call the delegate normally as if it was a global function
    if( func(1,2) )
    {
      print("The function returned true\n");
    }
    else
    {
      print("The function returned false\n");
    }
    
    printf("The number of comparisons performed is "+a.count+"\n");
  }
</pre>









\page doc_datatypes_auto Auto declarations

It is possible to use 'auto' as the data type of an assignment-style variable declaration.

The appropriate type for the variable(s) will be automatically determined.

<pre>
auto i = 18;         // i will be an integer
auto f = 18 + 5.f;   // the type of f resolves to float
auto anObject = getLongObjectTypeNameById(id); // avoids redundancy for long type names
</pre>

Auto can be qualified with const to force a constant value:

<pre>
const auto i = 2;  // i will be typed as 'const int'
</pre>


If receiving object references or objects by value, auto will make
a local object copy by default. To force a handle type, add '@'.

<pre>
obj getObject() {
    return obj();
}

auto value = getObject();    // auto is typed 'obj', and makes a local copy
auto@ handle = getObject();  // auto is typed 'obj@', and refers to the returned obj
</pre>

The '@' specifier is not necessary if the value already resolves to a handle:

<pre>
obj@ getObject() {
    return obj();
}

auto value = getObject();   // auto is already typed 'obj@', because of the return type of getObject()
auto@ value = getObject();  // this is still allowed if you want to be more explicit, but not needed
</pre>

Auto handles can not be used to declare class members, since their resolution is dependent on the constructor.

*/
