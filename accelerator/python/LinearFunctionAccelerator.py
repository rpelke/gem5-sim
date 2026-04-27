from m5.objects.Device import BasicPioDevice
from m5.objects.Gic import ArmInterruptPin
from m5.params import *


# MMIO accelerator computing y_i = a * x_i + b for 4 int32 lanes.
class LinearFunctionAccelerator(BasicPioDevice):
    type = "LinearFunctionAccelerator"
    cxx_class = "gem5::LinearFunctionAccelerator"
    cxx_header = "linear_function_accelerator.hh"

    pio_size = Param.Addr(
        0x40,
        "Size of the MMIO window: 64-bit control register, 64-bit IRQ clear register, plus 128-bit input and output vectors",
    )
    interrupt = Param.ArmInterruptPin("Interrupt to use for this device")
