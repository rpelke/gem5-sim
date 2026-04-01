from m5.objects.Device import BasicPioDevice
from m5.params import *


# Minimal memory-mapped test device exposing a single 32-bit register.
class DummyMmio(BasicPioDevice):
    type = "DummyMmio"
    cxx_class = "gem5::DummyMmio"
    cxx_header = "dummy_mmio.hh"

    pio_size = Param.Addr(0x4, "Size of the single 32-bit MMIO register")
