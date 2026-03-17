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

Arrays can also be created and initialized within expressions as \ref anonobj "anonymous objects". 

<pre>
  // Call a function that expects an array of integers as input
  foo({1,2,3,4});
  
  // If the function has multiple overloads supporting different types with 
  // initialization lists it is necessary to explicitly inform the array type
  foo2(array<int> = {1,2,3,4});
</pre>

\section doc_datatypes_arrays_addon Supporting array object and functions

The array object supports a number of operators and has several class methods to facilitate the manipulation of strings.

The array object is a \ref doc_datatypes_obj "reference type" even if the elements are not, so it's possible
to use handles to the array object when passing it around to avoid costly copies.

\subsection doc_datatypes_array_addon_ops Operators

<b>=       assignment</b>

The assignment operator performs a shallow copy of the content.
 
<b>[]      index operator</b>

The index operator returns the reference of an element allowing it to be 
inspected or modified. If the index is out of range, then an exception will be raised.
 
<b>==, !=  equality</b>
 
Performs a value comparison on each of the elements in the two arrays 
and returns true if all match the used operator.

\subsection doc_datatypes_array_addon_mthd Methods

<b>uint length() const</b>
  
Returns the length of the array.
  
<b>void resize(uint)</b>
 
Sets the new length of the array.
 
<b>void reverse()</b>

Reverses the order of the elements in the array.
 
<b>void insertAt(uint index, const T& in value)</b><br>
<b>void insertAt(uint index, const array<T>& arr)</b><br>

Inserts a new element, or another array of elements, into the array at the specified index. 
 
<b>void insertLast(const T& in)</b>

Appends an element at the end of the array.
 
<b>void removeAt(uint index)</b>

Removes the element at the specified index.
 
<b>void removeLast()</b>

Removes the last element of the array.

<b>void removeRange(uint start, uint count)</b>

Removes \a count elements starting from \a start.

<b>void sortAsc()</b><br>
<b>void sortAsc(uint startAt, uint count)</b><br>

Sorts the elements in the array in ascending order. For object types, this will use the type's opCmp method.

The second variant will sort only the elements starting at index \a startAt and the following \a count elements.
 
<b>void sortDesc()</b><br>
<b>void sortDesc(uint startAt, uint count)</b><br>

These does the same thing as sortAsc except sorts the elements in descending order.

<b>void sort(const less &in compareFunc, uint startAt = 0, uint count = uint(-1))</b><br>

This method takes as input a callback function to use for comparing two elements when sorting the array.

The callback function should take as parameters two references of the same type of the array elements and it 
should return a bool. The return value should be true if the first argument should be placed before the second 
argument.

<pre>
  array<int> arr = {3,2,1};
  arr.sort(function(a,b) { return a < b; });
</pre>

The example shows how to use the sort method with a callback to an \ref doc_script_anonfunc "anonymous function".

<b>int  find(const T& in)</b><br>
<b>int  find(uint startAt, const T& in)</b><br>

These will return the index of the first element that has the same value as the wanted value.

For object types, this will use the type's opEquals or opCmp method to compare the value. 
For arrays of handles any null handle will be skipped.

If no match is found the methods will return a negative value.
 
<b>int  findByRef(const T& in)</b><br>
<b>int  findByRef(uint startAt, const T& in)</b><br>

These will search for a matching address. These are especially useful for arrays of handles where
specific instances of objects are desired, and not just objects that happen to have equal value.

If no match is found the methods will return a negative value.

\subsection doc_datatypes_array_addon_example Script example
  
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
</pre>

Dictionary values can also be accessed or added by using the index operator.
  
<pre>
  // Read and modify an integer value
  int val = int(dict['value']);
  dict['value'] = val + 1;
  
  // Read and modify a handle to an object instance
  \@handle = cast<obj>(dict['handle']);
  if( handle is null )
    \@dict['handle'] = object;
</pre>
  
Dictionaries can also be created and initialized within expressions as \ref anonobj "anonymous objects". 

<pre>
  // Call a function that expects a dictionary as input and no other overloads
  // In this case it is possible to inform the initialization list without explicitly giving the type
  foo({{'a', 1},{'b', 2}});
  
  // Call a function where there are multiple overloads expecting different
  // In this case it is necessary to explicitly define the type of the initialization list
  foo2(dictionary = {{'a', 1},{'b', 2}});
</pre>

Dictionaries of dictionaries are created using \ref anonobj "anonymous objects" as well.

<pre>
  dictionary d2 = {{'a', dictionary = {{'aa', 1}, {'ab', 2}}}, 
                   {'b', dictionary = {{'ba', 1}, {'bb', 2}}}};
</pre>


\section doc_datatypes_dictionary_addon Supporting dictionary object and functions

The dictionary object is a \ref doc_datatypes_obj "reference type", so it's possible
to use handles to the dictionary object when passing it around to avoid costly copies.

\subsection doc_datatypes_dictionary_addon_ops Operators

<b>=       assignment</b><br>

The assignment operator performs a shallow copy of the content.

<b>[]      index operator</b><br>
 
The index operator takes a string for the key, and returns a reference to the \ref doc_datatypes_dictionaryValue_addon "value".
If the key/value pair doesn't exist it will be inserted with a null value.

\subsection doc_datatypes_dictionary_addon_mthd Methods

<b>void set(const string &in key, ? &in value)</b><br>
<b>void set(const string &in key, int64 &in value)</b><br>
<b>void set(const string &in key, double &in value)</b><br>

Sets a key/value pair in the dictionary. If the key already exists, the value will be changed.

<b>bool get(const string &in key, ? &out value) const</b><br>
<b>bool get(const string &in key, int64 &out value) const</b><br>
<b>bool get(const string &in key, double &out value) const</b><br>
 
Retrieves the value corresponding to the key. The methods return false if the key is not 
found, and in this case the value will maintain its default value based on the type.
 
<b>array<string> \@getKeys() const</b><br>
 
This method returns an array with all of the existing keys in the dictionary. 
The order of the keys in the array is undefined.
 
<b>bool exists(const string &in key) const</b><br>
 
Returns true if the key exists in the dictionary.
 
<b>bool delete(const string &in key)</b><br>

Removes the key and the corresponding value from the dictionary. Returns false if the key wasn't found.
 
<b>void deleteAll()</b><br>

Removes all entries in the dictionary.
 
<b>bool isEmpty() const</b><br>

Returns true if the dictionary doesn't hold any entries.

<b>uint getSize() const</b><br>

Returns the number of keys in the dictionary.



\section doc_datatypes_dictionaryValue_addon Supporting dictionaryValue object and functions

The dictionaryValue type is how the \ref doc_datatypes_dictionary_addon "dictionary" object stores the 
values. When accessing the values through the dictionary index operator a reference to a dictionaryValue is returned.

The dictionaryValue type itself is a value type, i.e. no handles to it can be held, but it can hold handles
to other objects as well as values of any type.

\subsection doc_datatypes_dictionaryValue_addon_ops Operators

<b>=       assignment</b><br>

The value assignment operator should be used to copy a value into the dictionaryValue.

<b>\@=     handle assignment</b><br>

The handle assignment operator should be used to set the dictionaryValue to refer to an object instance.

<b>cast<type>  cast operator</b><br>

The cast operator is used to dynamically cast the handle held in the dictionaryValue to the desired type. 
If the dictionaryValue doesn't hold a handle, or the handle is not compatible with the desired type, the 
cast operator will return a null handle.

<b>type()      conversion operator</b><br>

The conversion operator is used to return a new value of the desired type. If no value conversion is found,
an uninitialized value of the desired type is returned.





\page doc_datatypes_obj Objects and handles

\section objects Objects

There are two forms of objects, reference types and value types. 

Value types behave much like the primitive types, in that they are allocated on the stack and deallocated 
when the variable goes out of scope. Only the application can register these types, so you need to check
with the application's documentation for more information about the registered types.

Reference types are allocated on the memory heap, and may outlive the initial variable that allocates
them if another reference to the instance is kept. All \ref doc_script_class "script declared classes" are reference types. 
\ref doc_global_interface "Interfaces" are a special form of reference types, that cannot be instantiated, but can be used to access
the objects that implement the interfaces without knowing exactly what type of object it is.

<pre>
  obj o;      // An object is instantiated
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

<b>=            assignment</b><br>

The assignment operator copies the content of the right hand string into the left hand string. 

Assignment of primitive types is allowed, which will do a default transformation of the primitive to a string.

<b>+, +=        concatenation</b><br>

The concatenation operator appends the content of the right hand string to the end of the left hand string.

Concatenation of primitives types is allowed, which will do a default transformation of the primitive to a string.
 
<b>==, !=       equality</b><br>
 
Compares the content of the two strings.
 
<b><, >, <=, >= comparison</b><br>

Compares the content of the two strings. The comparison is done on the byte values in the strings, which 
may not correspond to alphabetical comparisons for some languages.

<b>[]           index operator</b><br>

The index operator gives access to a single byte in the string.
 
\subsection doc_datatypes_strings_addon_mthd Methods

<b>uint           length() const</b><br>

Returns the length of the string.
 
<b>void           resize(uint)</b><br>

Sets the length of the string.
 
<b>bool           isEmpty() const</b><br>

Returns true if the string is empty, i.e. the length is zero.
 
<b>string         substr(uint start = 0, int count = -1) const</b><br>

Returns a string with the content starting at \a start and the number of bytes given by count. The default arguments will return the whole string as the new string.

<b>void insert(uint pos, const string &in other)</b><br>

Inserts another string \a other at position \a pos in the original string. 

<b>void erase(uint pos, int count = -1)</b><br>

Erases a range of characters from the string, starting at position \a pos and counting \a count characters.

<b>int            findFirst(const string &in str, uint start = 0) const</b><br>

Find the first occurrence of the value \a str in the string, starting at \a start. If no occurrence is found a negative value will be returned.
 
<b>int            findLast(const string &in str, int start = -1) const</b><br>

Find the last occurrence of the value \a str in the string. If \a start is informed the search will begin at that position, i.e. any potential occurrence after that position will not be searched. If no occurrence is found a negative value will be returned.
 
<b>int            findFirstOf(const string &in chars, int start = 0) const</b><br>
<b>int            findFirstNotOf(const string &in chars, int start = 0) const</b><br>
<b>int            findLastOf(const string &in chars, int start = -1) const</b><br>
<b>int            findLastNotOf(const string &in chars, int start = -1) const</b><br>

The first variant finds the first character in the string that matches on of the characters in \a chars, starting at \a start. If no occurrence is found a negative value will be returned.

The second variant finds the first character that doesn't match any of those in \a chars. The third and last variant are the same except they start the search from the end of the string.

\note These functions work on the individual bytes in the strings. They do not attempt to understand encoded characters, e.g. UTF-8 encoded characters that can take up to 4 bytes.

<b>array<string>@ split(const string &in delimiter) const</b><br>

Splits the string in smaller strings where the delimiter is found.
 
\subsection doc_datatypes_strings_addon_funcs Functions

<b>string join(const array<string> &in arr, const string &in delimiter)</b><br>

Concatenates the strings in the array into a large string, separated by the delimiter.
 
<b>int64  parseInt(const string &in str, uint base = 10, uint &out byteCount = 0)</b><br>
<b>uint64 parseUInt(const string &in str, uint base = 10, uint &out byteCount = 0)</b><br>

Parses the string for an integer value. The \a base can be 10 or 16 to support decimal numbers or 
hexadecimal numbers. If \a byteCount is provided it will be set to the number of bytes that were 
considered as part of the integer value.

<b>double parseFloat(const string &in, uint &out byteCount = 0)</b><br>

Parses the string for a floating point value. If \a byteCount is provided it will be set to the 
number of bytes that were considered as part of the value.

<b>string formatInt(int64 val, const string &in options = '', uint width = 0)</b><br>
<b>string formatUInt(uint64 val, const string &in options = '', uint width = 0)</b><br>
<b>string formatFloat(double val, const string &in options = '', uint width = 0, uint precision = 0)</b><br>

The format functions takes a string that defines how the number should be formatted. The string
is a combination of the following characters:

  - l = left justify
  - 0 = pad with zeroes
  - + = always include the sign, even if positive
  - space = add a space in case of positive number
  - h = hexadecimal integer small letters (not valid for formatFloat)
  - H = hexadecimal integer capital letters (not valid for formatFloat)
  - e = exponent character with small e (only valid for formatFloat)
  - E = exponent character with capital E (only valid for formatFloat)

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

<b>\@=          handle assignment</b><br>
 
The handle assignment operator is used to set the object that the referred to by the ref type.
 
<b>is, !is      identity operator</b><br>
 
The identity operators are used to compare the address of the object referred to by the ref type.
 
<b>cast<type>   cast operator</b><br>

The cast operator is used to perform a dynamic cast to the desired type. 
If the type is not compatible with the object referred to by the ref type this will return null.


 
 
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

<b>\@=          handle assignment</b><br>
 
The handle assignment operator is used to set the object that the referred to by the ref type.

<b>=            value assignment</b><br>

The value assignment operator is used when one weakref object is copied to another.

<b>is, !is      identity operator</b><br>
 
The identity operators are used to compare the address of the object referred to by the ref type.

<b>cast<type>   implicit cast operator</b><br>

The implicit cast operator is used to cast the weak ref type to strong reference of the type.
If the object referred to by the weakref is already dead this operator will return null.


 
\subsection doc_datatypes_array_addon_mthd Methods

<b>T@ get() const</b><br>

This does the exact same thing as the implicit cast operator. It is just a more explicit way of 
writing it.

 
 
 

\page doc_datatypes_funcptr Function handles

A function handle is a data type that can be dynamically set to point to a global function that has
a matching function signature as that defined by the variable declaration. Function handles are commonly
used for callbacks, i.e. where a piece of code must be able to call back to some code based on some 
conditions, but the code that needs to be called is not known at compile time.

To use function handles it is first necessary to \ref doc_global_funcdef "define the function signature" 
that will be used at the global scope or as a member of a class. Once that is done the variables can be 
declared using that definition.

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
