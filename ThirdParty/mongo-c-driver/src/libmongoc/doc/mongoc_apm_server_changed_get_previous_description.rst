:man_page: mongoc_apm_server_changed_get_previous_description

mongoc_apm_server_changed_get_previous_description()
====================================================

Synopsis
--------

.. code-block:: c

  const mongoc_server_description_t *
  mongoc_apm_server_changed_get_previous_description (
     const mongoc_apm_server_changed_t *event);

Returns this event's previous description. The data is only valid in the scope of the callback that receives this event; copy it if it will be accessed after the callback returns.

Parameters
----------

* ``event``: A :symbol:`mongoc_apm_server_changed_t`.

Returns
-------

A :symbol:`mongoc_server_description_t` that should not be modified or freed.

.. seealso::

  | :doc:`Introduction to Application Performance Monitoring <application-performance-monitoring>`

