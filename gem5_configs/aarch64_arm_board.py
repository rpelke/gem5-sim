import argparse
import os
from pathlib import Path

import m5
import m5.defines
from m5.objects import *
from m5.util import *

from gem5.components.boards.abstract_board import AbstractBoard
from gem5.components.boards.arm_board import ArmBoard
from gem5.components.cachehierarchies.classic.no_cache import NoCache
from gem5.components.cachehierarchies.classic.private_l1_private_l2_cache_hierarchy import (
    PrivateL1PrivateL2CacheHierarchy,
)
from gem5.components.memory import (
    DualChannelDDR3_1600,
    DualChannelDDR4_2400,
    SingleChannelDDR3_1600,
    SingleChannelDDR4_2400,
)
from gem5.components.processors.cpu_types import (
    CPUTypes,
    get_cpu_type_from_str,
)
from gem5.components.processors.simple_processor import SimpleProcessor
from gem5.isas import ISA
from gem5.resources.resource import (
    DiskImageResource,
    KernelResource,
)
from gem5.simulate.simulator import Simulator

SCRIPT_DIR = os.path.dirname(os.path.realpath(__file__))
REPO_ROOT = os.path.abspath(os.path.join(SCRIPT_DIR, ".."))
m5.util.addToPath(os.path.join(REPO_ROOT, "gem5", "configs"))

from common import SysPaths

default_kernel = "vmlinux.arm64"
default_disk = "linaro-minimal-aarch64.img"
default_root_device = "/dev/vda1"


class LocalArmBoard(ArmBoard):
    def __init__(self, *args, dtb_path=None, **kwargs):
        self._external_dtb_path = dtb_path
        super().__init__(*args, **kwargs)

    def _get_dtb_filename(self) -> str:
        if self._external_dtb_path is not None:
            return self._external_dtb_path
        return super()._get_dtb_filename()

    def _pre_instantiate(self, full_system=None):
        root = AbstractBoard._pre_instantiate(self, full_system=full_system)

        self.pci_devices = self._pci_devices

        dtb_filename = self._get_dtb_filename()
        self.workload.dtb_filename = dtb_filename

        self.realview.setupBootLoader(self, dtb_filename, self._bootloader)

        if self._external_dtb_path is None:
            self.generateDtb(dtb_filename)

        return root


def resolve_binary(path_or_name: str) -> str:
    if os.path.isfile(path_or_name):
        return os.path.abspath(path_or_name)
    return SysPaths.binary(path_or_name)


def resolve_disk(path_or_name: str) -> str:
    if os.path.isfile(path_or_name):
        return os.path.abspath(path_or_name)
    return SysPaths.disk(path_or_name)


def build_memory(mem_type: str, mem_channels: int, mem_size: str):
    if mem_channels not in (1, 2):
        raise ValueError("--mem-channels must be 1 or 2 for this config.")

    memory_builders = {
        ("DDR3_1600_8x8", 1): SingleChannelDDR3_1600,
        ("DDR3_1600_8x8", 2): DualChannelDDR3_1600,
        ("DDR4_2400_8x8", 1): SingleChannelDDR4_2400,
        ("DDR4_2400_8x8", 2): DualChannelDDR4_2400,
    }

    try:
        builder = memory_builders[(mem_type, mem_channels)]
    except KeyError as exc:
        raise ValueError(
            f"Unsupported memory setup: {mem_type} with {mem_channels} channels."
        ) from exc

    return builder(size=mem_size)


def parse_cpu_type(cpu: str) -> CPUTypes:
    if cpu == "hpi":
        print("Warning: mapping legacy CPU type 'hpi' to stdlib 'timing'.")
        return CPUTypes.TIMING
    return get_cpu_type_from_str(cpu)


def build_cache_hierarchy(cpu_type: CPUTypes):
    if cpu_type == CPUTypes.ATOMIC:
        return NoCache()

    return PrivateL1PrivateL2CacheHierarchy(
        l1d_size="32KiB",
        l1i_size="48KiB",
        l2_size="1MiB",
    )


def build_kernel_args(args):
    return [
        "earlycon=pl011,0x1c090000",
        "console=ttyAMA0",
        "lpj=19988480",
        "norandmaps",
        f"root={args.root_device}",
        "rw",
        f"mem={args.mem_size}",
    ]


def get_bootloader_paths() -> list[str]:
    return [
        resolve_binary("boot.arm64"),
        resolve_binary("boot.arm"),
    ]


def attach_linear_function_accelerator(board, args) -> None:
    if not args.linear_function_accelerator:
        return

    lfa_irq_spi = 41

    try:
        from m5.objects import LinearFunctionAccelerator
    except ImportError as exc:
        raise RuntimeError(
            "LinearFunctionAccelerator is not available in this gem5 build. "
            "Rebuild gem5 with EXTRAS pointing to the accelerator directory."
        ) from exc

    board.linear_function_accelerator = LinearFunctionAccelerator(
        pio_addr=args.linear_function_accelerator_addr,
        pio_size=args.linear_function_accelerator_size,
        pio_latency=args.linear_function_accelerator_latency,
        interrupt=ArmSPI(num=lfa_irq_spi),
    )
    board.linear_function_accelerator.pio = board.get_io_bus().mem_side_ports
    print(
        "Attached LinearFunctionAccelerator at "
        f"{args.linear_function_accelerator_addr:#x} "
        f"size={args.linear_function_accelerator_size:#x} "
        f"latency={args.linear_function_accelerator_latency} "
        f"irq_spi={lfa_irq_spi}"
    )


def attach_9p(parent):
    viopci = PciVirtIO()
    viopci.vio = VirtIO9PDiod()
    viopci.vio.tag = "gem5"
    viodir = os.path.realpath(os.path.join(m5.options.outdir, "9p"))
    viopci.vio.root = os.path.join(viodir, "share")
    viopci.vio.socketPath = os.path.join(viodir, "socket")
    os.makedirs(viopci.vio.root, exist_ok=True)
    if os.path.exists(viopci.vio.socketPath):
        os.remove(viopci.vio.socketPath)
    parent.viopci = viopci
    parent.attachPciDevice(viopci)


def create_board(args):
    if args.script and not os.path.isfile(args.script):
        raise FileNotFoundError(f"Bootscript does not exist: {args.script}")

    if args.dtb and not os.path.isfile(args.dtb):
        raise FileNotFoundError(f"DTB does not exist: {args.dtb}")

    if args.mem_ranks is not None:
        print("Warning: --mem-ranks is ignored by this ArmBoard-based config.")

    cpu_type = parse_cpu_type(args.cpu)
    processor = SimpleProcessor(
        cpu_type=cpu_type,
        num_cores=args.num_cores,
        isa=ISA.ARM,
    )
    memory = build_memory(args.mem_type, args.mem_channels, args.mem_size)
    cache_hierarchy = build_cache_hierarchy(cpu_type)

    release = ArmDefaultRelease()
    release.remove(ArmExtension("FEAT_PMULL"))

    board = LocalArmBoard(
        clk_freq=args.cpu_freq,
        processor=processor,
        memory=memory,
        cache_hierarchy=cache_hierarchy,
        release=release,
        platform=VExpress_GEM5_V1(),
        dtb_path=os.path.abspath(args.dtb) if args.dtb else None,
    )

    kernel = KernelResource(
        local_path=resolve_binary(args.kernel),
        architecture=ISA.ARM,
    )
    disk = DiskImageResource(local_path=resolve_disk(args.disk_image))

    board.set_kernel_disk_workload(
        kernel=kernel,
        disk_image=disk,
        readfile=args.script if args.script else None,
        kernel_args=build_kernel_args(args),
        checkpoint=Path(args.restore) if args.restore else None,
    )

    if args.initrd:
        board.workload.initrd_filename = os.path.abspath(args.initrd)

    board._bootloader = get_bootloader_paths()
    attach_linear_function_accelerator(board, args)

    if args.attach_9p:
        attach_9p(board.realview)

    return board


def main():
    parser = argparse.ArgumentParser()

    parser.add_argument("--dtb", type=str, default=None, help="DTB file to load")
    parser.add_argument(
        "--kernel", type=str, default=default_kernel, help="Linux kernel"
    )
    parser.add_argument(
        "--initrd",
        type=str,
        default=None,
        help="initrd/initramfs file to load",
    )
    parser.add_argument(
        "--disk-image",
        type=str,
        default=default_disk,
        help="Disk to instantiate",
    )
    parser.add_argument(
        "--root-device",
        type=str,
        default=default_root_device,
        help=f"OS device name for root partition (default: {default_root_device})",
    )
    parser.add_argument(
        "--script", type=str, default="", help="Linux bootscript"
    )
    parser.add_argument(
        "--cpu",
        type=str,
        choices=["atomic", "timing", "minor", "o3", "hpi"],
        default="atomic",
        help="CPU model to use",
    )
    parser.add_argument("--cpu-freq", type=str, default="4GHz")
    parser.add_argument(
        "--num-cores", type=int, default=1, help="Number of CPU cores"
    )
    parser.add_argument(
        "--mem-type",
        type=str,
        default="DDR3_1600_8x8",
        choices=["DDR3_1600_8x8", "DDR4_2400_8x8"],
        help="Memory type to use",
    )
    parser.add_argument(
        "--mem-channels", type=int, default=1, help="Number of memory channels"
    )
    parser.add_argument(
        "--mem-ranks",
        type=int,
        default=None,
        help="Ignored compatibility option.",
    )
    parser.add_argument(
        "--mem-size",
        type=str,
        default="2GiB",
        help="Specify the physical memory size",
    )
    parser.add_argument("--restore", type=str, default=None)
    parser.add_argument(
        "--linear-function-accelerator",
        action="store_true",
        help="Attach the out-of-tree LinearFunctionAccelerator to the board I/O bus",
    )
    parser.add_argument(
        "--linear-function-accelerator-addr",
        type=lambda value: int(value, 0),
        default=0x1C150000,
        help="MMIO base address for LinearFunctionAccelerator",
    )
    parser.add_argument(
        "--linear-function-accelerator-size",
        type=lambda value: int(value, 0),
        default=0x40,
        help="MMIO window size for LinearFunctionAccelerator",
    )
    parser.add_argument(
        "--linear-function-accelerator-latency",
        type=str,
        default="10ns",
        help="PIO latency for LinearFunctionAccelerator",
    )
    parser.add_argument(
        "--attach-9p",
        action="store_true",
        help="Attach the VirtIO 9p device to the board PCI bus",
    )

    args = parser.parse_args()

    simulator = Simulator(board=create_board(args))
    simulator.run()


if __name__ == "__m5_main__":
    main()
