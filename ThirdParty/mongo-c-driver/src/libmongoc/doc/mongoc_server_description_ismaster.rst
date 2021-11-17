:man_page: mongoc_server_description_ismaster

mongoc_server_description_ismaster()
====================================

Synopsis
--------

.. code-block:: c

  const bson_t *
  mongoc_server_description_ismaster (
     const mongoc_server_description_t *description);

Deprecated
----------

.. warning::

  This function is deprecated and should not be used in new code.

Please use :doc:`mongoc_server_description_hello_response() <mongoc_server_description_hello_response>` instead.

Parameters
----------

* ``description``: A :symbol:`mongoc_server_description_t`.

Description
-----------

The client or client pool periodically runs a `"hello" <https://docs.mongodb.org/manual/reference/command/hello/>`_ command on each server, to update its view of the MongoDB deployment. Use :symbol:`mongoc_client_get_server_descriptions()` and ``mongoc_server_description_hello_response()`` to get the most recent "hello" response.

Returns
-------

A reference to a BSON document, owned by the server description. The document is empty if the driver is not connected to the server.

