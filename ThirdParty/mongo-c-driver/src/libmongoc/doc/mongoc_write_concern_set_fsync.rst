:man_page: mongoc_write_concern_set_fsync

mongoc_write_concern_set_fsync()
================================

Synopsis
--------

.. code-block:: c

  void
  mongoc_write_concern_set_fsync (mongoc_write_concern_t *write_concern,
                                  bool fsync_);

Parameters
----------

* ``write_concern``: A :symbol:`mongoc_write_concern_t`.
* ``fsync_``: A boolean.

Description
-----------

Sets if a fsync must be performed before indicating write success.

Beginning in version 1.9.0, this function can now alter the write concern after
it has been used in an operation. Previously, using the struct with an operation
would mark it as "frozen" and calling this function would log a warning instead
instead of altering the write concern.

Deprecated
----------

.. warning::

  The ``fsync`` write concern is deprecated.

Please use :symbol:`mongoc_write_concern_set_journal()` instead.
