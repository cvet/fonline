Object Lifecycle
================

This page documents the order of creation and destruction for libmongoc's main struct types.

Clients and pools
-----------------

Call :symbol:`mongoc_init()` once, before calling any other libmongoc functions, and call :symbol:`mongoc_cleanup()` once before your program exits.

A program that uses libmongoc from multiple threads should create a :symbol:`mongoc_client_pool_t` with :symbol:`mongoc_client_pool_new()`. Each thread acquires a :symbol:`mongoc_client_t` from the pool with :symbol:`mongoc_client_pool_pop()` and returns it with :symbol:`mongoc_client_pool_push()` when the thread is finished using it. To destroy the pool, first return all clients, then call :symbol:`mongoc_client_pool_destroy()`.

If your program uses libmongoc from only one thread, create a :symbol:`mongoc_client_t` directly with :symbol:`mongoc_client_new()` or :symbol:`mongoc_client_new_from_uri()`. Destroy it with :symbol:`mongoc_client_destroy()`.

Databases, collections, and related objects
-------------------------------------------

You can create a :symbol:`mongoc_database_t` or :symbol:`mongoc_collection_t` from a :symbol:`mongoc_client_t`, and create a :symbol:`mongoc_cursor_t` or :symbol:`mongoc_bulk_operation_t` from a :symbol:`mongoc_collection_t`.

Each of these objects must be destroyed before the client they were created from, but their lifetimes are otherwise independent.

GridFS objects
--------------

You can create a :symbol:`mongoc_gridfs_t` from a :symbol:`mongoc_client_t`, create a :symbol:`mongoc_gridfs_file_t` or :symbol:`mongoc_gridfs_file_list_t` from a :symbol:`mongoc_gridfs_t`, create a :symbol:`mongoc_gridfs_file_t` from a :symbol:`mongoc_gridfs_file_list_t`, and create a :symbol:`mongoc_stream_t` from a :symbol:`mongoc_gridfs_file_t`.

Each of these objects depends on the object it was created from. Always destroy GridFS objects in the reverse of the order they were created. The sole exception is that a :symbol:`mongoc_gridfs_file_t` need not be destroyed before the :symbol:`mongoc_gridfs_file_list_t` it was created from.

GridFS bucket objects
---------------------

Create :symbol:`mongoc_gridfs_bucket_t` with a :symbol:`mongoc_database_t` derived from a :symbol:`mongoc_client_t`. The :symbol:`mongoc_database_t` is independent from the :symbol:`mongoc_gridfs_bucket_t`. But the :symbol:`mongoc_client_t` must outlive the :symbol:`mongoc_gridfs_bucket_t`.

A :symbol:`mongoc_stream_t` may be created from the :symbol:`mongoc_gridfs_bucket_t`. The :symbol:`mongoc_gridfs_bucket_t` must outlive the :symbol:`mongoc_stream_t`.

Sessions
--------

.. include:: includes/session-lifecycle.txt

Client Side Encryption
----------------------

When configuring a :symbol:`mongoc_client_t` for automatic encryption via :symbol:`mongoc_client_enable_auto_encryption()`, if a separate key vault client is set in the options (via :symbol:`mongoc_auto_encryption_opts_set_keyvault_client()`) the key vault client must outlive the encrypted client.

When configuring a :symbol:`mongoc_client_pool_t` for automatic encryption via :symbol:`mongoc_client_pool_enable_auto_encryption()`, if a separate key vault client pool is set in the options (via :symbol:`mongoc_auto_encryption_opts_set_keyvault_client_pool()`) the key vault client pool must outlive the encrypted client pool.

When creating a :symbol:`mongoc_client_encryption_t`, the configured key vault client (set via :symbol:`mongoc_client_encryption_opts_set_keyvault_client()`) must outlive the :symbol:`mongoc_client_encryption_t`.