from m5.objects.Device import BasicPioDevice
from m5.params import *


# Minimal memory-mapped test device exposing a single 64-bit register.
class DummyMmio(BasicPioDevice):
    type = "DummyMmio"
    cxx_class = "gem5::DummyMmio"
    cxx_header = "dummy_mmio.hh"

    pio_size = Param.Addr(0x8, "Size of the single 64-bit MMIO register")
