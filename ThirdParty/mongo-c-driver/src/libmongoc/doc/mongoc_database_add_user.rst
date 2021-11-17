:man_page: mongoc_database_add_user

mongoc_database_add_user()
==========================

Synopsis
--------

.. code-block:: c

  bool
  mongoc_database_add_user (mongoc_database_t *database,
                            const char *username,
                            const char *password,
                            const bson_t *roles,
                            const bson_t *custom_data,
                            bson_error_t *error);

Parameters
----------

* ``database``: A :symbol:`mongoc_database_t`.
* ``username``: The name of the user.
* ``password``: The cleartext password for the user.
* ``roles``: An optional :symbol:`bson:bson_t` for roles.
* ``custom_data``: A optional :symbol:`bson:bson_t` for extra data.
* ``error``: A location for a :symbol:`bson_error_t <errors>` or ``NULL``.

This function shall create a new user with access to ``database``.

.. warning::

  Do not call this function without TLS.

Errors
------

Errors are returned through the ``error`` parameter and can include socket or other server side failures.

Returns
-------

Returns ``true`` if the user was successfully added. Returns ``false`` and sets ``error`` if there are invalid arguments or a server or network error.

