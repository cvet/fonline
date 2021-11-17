:man_page: bson_iter_init_find_case

bson_iter_init_find_case()
==========================

Synopsis
--------

.. code-block:: c

  bool
  bson_iter_init_find_case (bson_iter_t *iter,
                            const bson_t *bson,
                            const char *key);

Parameters
----------

* ``iter``: A :symbol:`bson_iter_t`.
* ``bson``: A :symbol:`bson_t`.
* ``key``: A key to locate after initializing the iter.

Description
-----------

This function is identical to ``bson_iter_init() && bson_iter_find_case()``.

.. only:: html

  .. include:: includes/seealso/iter-init.txt
