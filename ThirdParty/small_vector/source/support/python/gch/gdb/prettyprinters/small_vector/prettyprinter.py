import gdb

class GCHSmallVectorPrinter(object):
  """Print a gch::small_vector"""

  class _iterator(object):
    def __init__ (self, begin, size):
      self.curr = begin
      self.size = size
      self.count = 0

    def __iter__(self):
      return self

    def __next__(self):
      if self.count == self.size:
        raise StopIteration

      val = self.curr.dereference()
      idx = self.count

      self.curr = self.curr + 1
      self.count = self.count + 1

      return f'[{idx}]', val

  def __init__(self, val):
    self.val = val
    self.base = val['m_data']
    self.data_base = self.base.cast(self.base.type.fields()[0].type)

  def children(self):
    begin = self.data_base['m_data_ptr']
    size = int(self.data_base['m_size'])
    return self._iterator(begin, size)

  def to_string(self):
    size = int(self.data_base['m_size'])
    capacity = int(self.data_base['m_capacity'])
    return f'small_vector of length {size}, capacity {capacity}'

  def display_hint(self):
    return 'array'

class GCHSmallVectorIteratorPrinter(object):
  """Print a gch::small_vector::iterator"""

  def __init__(self, val):
    self.val = val

  def to_string(self):
    if not self.val['m_ptr']:
      return 'non-dereferenceable iterator for gch::small_vector'
    return str(self.val['m_ptr'].dereference())


def build_printer():
  pp = gdb.printing.RegexpCollectionPrettyPrinter('gch::small_vector')

  pp.add_printer('gch::small_vector',
                 '^gch::small_vector<.*>$',
                 GCHSmallVectorPrinter)

  pp.add_printer('gch::small_vector::iterator',
                 '^gch::small_vector_iterator<.*>$',
                 GCHSmallVectorIteratorPrinter)
  return pp

gch_small_vector_printer = build_printer()
