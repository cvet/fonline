:man_page: mongoc_apm_topology_changed_get_topology_id

mongoc_apm_topology_changed_get_topology_id()
=============================================

Synopsis
--------

.. code-block:: c

  void
  mongoc_apm_topology_changed_get_topology_id (
     const mongoc_apm_topology_changed_t *event, bson_oid_t *topology_id);

Returns this event's topology id.

Parameters
----------

* ``event``: A :symbol:`mongoc_apm_topology_changed_t`.
* ``topology_id``: A :symbol:`bson:bson_oid_t` to receive the event's topology_id.

Returns
-------

The event's topology id.

.. seealso::

  | :doc:`Introduction to Application Performance Monitoring <application-performance-monitoring>`

