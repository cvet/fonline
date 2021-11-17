:man_page: bson_md5_append

bson_md5_append()
=================

Deprecated
----------
All MD5 APIs are deprecated in libbson.

Synopsis
--------

.. code-block:: c

  void
  bson_md5_append (bson_md5_t *pms,
                   const uint8_t *data,
                   uint32_t nbytes) BSON_GNUC_DEPRECATED;

Parameters
----------

* ``pms``: A :symbol:`bson_md5_t`.
* ``data``: A memory region to feed to the md5 algorithm.
* ``nbytes``: The length of ``data`` in bytes.

Description
-----------

Feeds more data into the MD5 algorithm.

This function is deprecated and should not be used in new code.
