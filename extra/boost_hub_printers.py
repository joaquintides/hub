# Copyright 2026 Joaquin M Lopez Munoz.
# Distributed under the Boost Software License, Version 1.0.
# (See accompanying file LICENSE_1_0.txt or copy at
# http://www.boost.org/LICENSE_1_0.txt)

import gdb.printing
import gdb.xmethod

class BoostContainerHubHelpers:
  def countr_zero(n):
    for i in range(64):
      if (n & (1 << i)) != 0: return i
    return 64

  # Fancy pointer support: fancy pointer's visualizer is assumed to provide
  # a static boost_to_address(fancy_ptr) method returning a gdb.Value with the
  # equivalent raw pointer.

  def to_address(ptr):
    if ptr.type.strip_typedefs().code == gdb.TYPE_CODE_PTR: # strip_typedefs() needed for some unknown reason
      return ptr
    try:
      return type(gdb.default_visualizer(ptr)).boost_to_address(ptr)
    except AttributeError:
      raise gdb.error(
        f"{ptr.type.strip_typedefs().name}'s visualizer must have a static boost_to_address(ptr) method "
         "for proper visualization")

class BoostContainerHubPrinter:
  def __init__(self, val):
    self.N = 64
    self.blist = val["blist"]
    self.num_blocks = int(val["num_blocks"])
    self.size_ = int(val["size_"])
    # block_list derives from block
    self.block_type = next(f for f in self.blist.type.fields() if f.is_base_class).type

  def to_string(self):
    return f"boost::container::hub with {{size = {self.size_}, capacity = {self.num_blocks * self.N}}}"

  def display_hint(self):
    return "array"

  def children(self):
    def generator():
      pbb = BoostContainerHubHelpers.to_address(self.blist["next"])
      mask = int(pbb.dereference()["mask"])
      count = 0
      while count != self.size_:
        n = BoostContainerHubHelpers.countr_zero(mask)
        pb = pbb.cast(self.block_type.pointer())
        pd = BoostContainerHubHelpers.to_address(pb.dereference()["data_"])
        yield f"[{count}]", (pd + n).dereference()
        mask &= mask - 1
        if mask == 0:
          pbb = BoostContainerHubHelpers.to_address(pbb["next"])
          mask = int(pbb.dereference()["mask"])
        count += 1
        
    return generator()

class BoostContainerHubIteratorPrinter:
  def __init__(self, val):
    # TODO: ::block typedef may have been stripped away by compiler
    block_type = gdb.lookup_type(f"{val.type.strip_typedefs().name}::block")
    self.pb = BoostContainerHubHelpers.to_address(val["pbb"]).cast(block_type.pointer())
    if self.pb != 0:
      self.px = BoostContainerHubHelpers.to_address(self.pb.dereference()["data_"]) + int(val["n"])
    
  def to_string(self):
    if self.pb == 0:
      return "iterator = { invalid }"
    elif self.px == 0:
      return "iterator = { end iterator }"
    else:
      return f"iterator = {{ {self.px.dereference()} }}"

def boost_container_hub_build_pretty_printers():
  pp = gdb.printing.RegexpCollectionPrettyPrinter("boost_container_hub")
  add_template_printer = lambda name, printer: pp.add_printer(name, f"^{name}<.*>$", printer)

  add_template_printer("boost::container::hub", BoostContainerHubPrinter)
  add_template_printer("boost::container::hub_detail::iterator", BoostContainerHubIteratorPrinter)

  return pp

gdb.printing.register_pretty_printer(gdb.current_objfile(), boost_container_hub_build_pretty_printers())
