:man_page: mongoc_write_concern_set_wmajority

mongoc_write_concern_set_wmajority()
====================================

Synopsis
--------

.. code-block:: c

  void
  mongoc_write_concern_set_wmajority (mongoc_write_concern_t *write_concern,
                                      int32_t wtimeout_msec);

Parameters
----------

* ``write_concern``: A :symbol:`mongoc_write_concern_t`.
* ``wtimeout_msec``: A positive ``int32_t`` or zero. If you need to set wtimeout with an ``int64_t``, use :symbol:`mongoc_write_concern_set_wtimeout_int64`.

Description
-----------

Sets if the write must have been propagated to a majority of nodes before indicating write success.

The timeout specifies how long, in milliseconds, the server should wait before indicating that the write has failed. This is not the same as a socket timeout. A value of zero may be used to indicate no timeout.

Beginning in version 1.9.0, this function can now alter the write concern after
it has been used in an operation. Previously, using the struct with an operation
would mark it as "frozen" and calling this function would log a warning instead
instead of altering the write concern.
