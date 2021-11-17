:man_page: mongoc_apm_command_failed_get_request_id

mongoc_apm_command_failed_get_request_id()
==========================================

Synopsis
--------

.. code-block:: c

  int64_t
  mongoc_apm_command_failed_get_request_id (
     const mongoc_apm_command_failed_t *event);

Returns this event's wire-protocol request id. Use this number to correlate client-side events with server log messages.

Parameters
----------

* ``event``: A :symbol:`mongoc_apm_command_failed_t`.

Returns
-------

The event's request id.

.. seealso::

  | :doc:`Introduction to Application Performance Monitoring <application-performance-monitoring>`

