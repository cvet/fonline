:man_page: mongoc_gridfs_bucket_upload_from_stream_with_id

mongoc_gridfs_bucket_upload_from_stream_with_id()
=================================================

Synopsis
--------

.. code-block:: c

   bool
   mongoc_gridfs_bucket_upload_from_stream_with_id (mongoc_gridfs_bucket_t *bucket,
                                                    const bson_value_t *file_id,
                                                    const char *filename,
                                                    mongoc_stream_t *source,
                                                    const bson_t *opts,
                                                    bson_error_t *error);

Parameters
----------

* ``bucket``: A :symbol:`mongoc_gridfs_bucket_t`.
* ``file_id``: A :symbol:`bson_value_t` specifying the id of the created file.
* ``filename``: The name of the file to create.
* ``source``: A :symbol:`mongoc_stream_t` used as the source of the data to upload.
* ``opts``: A :symbol:`bson_t` or ``NULL``.
* ``error``: A :symbol:`bson_error_t` to receive any error or ``NULL``.

.. include:: includes/gridfs-bucket-upload-opts.txt

Description
-----------

Reads from the ``source`` stream and writes to a new file in GridFS.
To have libmongoc generate an id, use :symbol:`mongoc_gridfs_bucket_upload_from_stream()`.

Reads from the ``source`` stream using :symbol:`mongoc_stream_read()` until the return value indicates end-of-file.
The ``source`` stream is not closed after calling :symbol:`mongoc_gridfs_bucket_upload_from_stream()`; call :symbol:`mongoc_stream_close()` after.

Returns
-------

True if the operation succeeded. False otherwise and sets ``error``.

.. seealso::

  | :symbol:`mongoc_stream_file_new` and :symbol:`mongoc_stream_file_new_for_path`, which can be used to create a source stream from a file.

