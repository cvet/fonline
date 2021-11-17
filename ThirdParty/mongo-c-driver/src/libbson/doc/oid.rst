:man_page: bson_oid

ObjectIDs
=========

Libbson provides a simple way to generate ObjectIDs. It can be used in a single-threaded or multi-threaded manner depending on your requirements.

The :symbol:`bson_oid_t` structure represents an ``ObjectID`` in MongoDB. It is a 96-bit identifier.

Composition
-----------

* 4 bytes : The UNIX timestamp in big-endian format.
* 5 bytes : A random number.
* 3 bytes : A 24-bit monotonic counter incrementing from ``rand()`` in big-endian.

Sorting ObjectIDs
-----------------

The typical way to sort in C is using ``qsort()``. Therefore, Libbson provides a ``qsort()`` compatible callback function named :symbol:`bson_oid_compare()`. It returns ``less than 1``, ``greater than 1``, or ``0`` depending on the equality of two :symbol:`bson_oid_t` structures.

Comparing Object IDs
--------------------

If you simply want to compare two :symbol:`bson_oid_t` structures for equality, use :symbol:`bson_oid_equal()`.

Generating
----------

To generate a :symbol:`bson_oid_t`, you may use the following.

.. code-block:: c

  bson_oid_t oid;

  bson_oid_init (&oid, NULL);

Parsing ObjectID Strings
------------------------

You can also parse a string containing a :symbol:`bson_oid_t`. The input string *MUST* be 24 characters or more in length.

.. code-block:: c

  bson_oid_t oid;

  bson_oid_init_from_string (&oid, "123456789012345678901234");

.. code-block:: c

  bson_oid_t oid;

  bson_oid_init_from_string_unsafe (&oid, "123456789012345678901234");

Hashing ObjectIDs
-----------------

If you need to store items in a hashtable, you may want to use the :symbol:`bson_oid_t` as the key. Libbson provides a hash function for just this purpose. It is based on DJB hash.

.. code-block:: c

  unsigned hash;

  hash = bson_oid_hash (oid);

Fetching ObjectID Creation Time
-------------------------------

You can easily fetch the time that a :symbol:`bson_oid_t` was generated using :symbol:`bson_oid_get_time_t()`.

.. code-block:: c

  time_t t;

  t = bson_oid_get_time_t (oid);
  printf ("The OID was generated at %u\n", (unsigned) t);

