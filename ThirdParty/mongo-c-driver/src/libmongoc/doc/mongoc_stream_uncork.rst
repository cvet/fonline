:man_page: mongoc_stream_uncork

mongoc_stream_uncork()
======================

Synopsis
--------

.. code-block:: c

  int
  mongoc_stream_uncork (mongoc_stream_t *stream);

Parameters
----------

* ``stream``: A :symbol:`mongoc_stream_t`.

This function shall allow a previously corked socket to pass bytes to the underlying socket.

.. note::

  Not all streams implement this function. Buffering generally works better.

Returns
-------

``0`` on success, ``-1`` on failure and ``errno`` is set.

.. seealso::

  | :doc:`mongoc_stream_buffered_new() <mongoc_stream_buffered_new>`.

  | :doc:`mongoc_stream_cork() <mongoc_stream_cork>`.

