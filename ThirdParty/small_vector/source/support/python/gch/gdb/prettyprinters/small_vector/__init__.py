import gdb

def register_printers(obj):
  from .prettyprinter import gch_small_vector_printer
  gdb.printing.register_pretty_printer(obj, gch_small_vector_printer)

register_printers(None)
