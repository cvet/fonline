:man_page: mongoc_apm_server_heartbeat_failed_get_duration

mongoc_apm_server_heartbeat_failed_get_duration()
=================================================

Synopsis
--------

.. code-block:: c

  int64_t
  mongoc_apm_server_heartbeat_failed_get_duration (
     const mongoc_apm_server_heartbeat_failed_t *event);

Returns this event's duration in microseconds.

Parameters
----------

* ``event``: A :symbol:`mongoc_apm_server_heartbeat_failed_t`.

Returns
-------

The event's duration.

.. seealso::

  | :doc:`Introduction to Application Performance Monitoring <application-performance-monitoring>`

