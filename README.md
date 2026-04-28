# gem5-sim

This repository contains an example of a gem5-based full-system simulator (aarch64) including a custom memory-mapped accelerator.
The linear function accelerator (short LFA) is a dummy example on how to integrate a custom memory-mapped device into a gem5-based simulation.

The corresponding Linux image (buildroot based) and accelerator driver can be found in the [gem5_sw](https://github.com/rpelke/gem5_sw) repository.

## Build & Install

1. Clone the repository including submodules:

    ```bash
    git clone --recursive git@github.com:rpelke/gem5-sim.git
    ```

1. Build gem5:

    ```bash
    python3 -m venv .venv
    source .venv/bin/activate
    pip install -r requirements.txt
    pip install -r gem5/requirements.txt
    ./gem5/util/pre-commit-install.sh
    ./build_gem5.bash
    ```

    If you want to rebuild gem5 later on, you can use:

    ```bash
    cd gem5
    scons build/ARM/gem5.opt -j`nproc`
    ```

1. Build the software (Linux image, etc.): Follow the steps in this [README.md](images/README.md).

1. Start the VP:

    ```bash
    ./start_vp.bash
    ```

## Debugging

1. Set the environment variables for the GDB script:

    ```bash
    # ${PWD} points to the repositories working directory
    export LINUX_KERNEL_ELF=${PWD}/images/system/binaries/vmlinux
    # Insert path to 'gem5_sw' repository
    export BUILDROOT_SRC=<path-to-gem5_sw>/BUILD/buildroot
    ```

1. Start GDB:

    ```bash
    gdb-multiarch -x gdb-config
    (gdb) connect
    ```

## Some Useful Commands

* Convert the binary device tree `system.dtb` into the readable source file `system.dts`:

    ```bash
    dtc -I dtb -O dts -o system.dts system.dtb
    ```

* Convert the device tree source `system.dts` back into the binary `system.dtb`:

    ```bash
    dtc -I dts -O dtb -o system.dtb system.dts
    ```

* Connect to the gem5 terminal interface exposed on TCP port `3456` to interact with the Linux system, for example to run commands normally inside the guest:

    ```bash
    telnet localhost 3456
    ```

## Host-VP File Sharing via VirtIO 9p

Use this setup to share files between host and VP without rebuilding or restarting the VP for every small file change (for example during kernel driver development).

1. Install `diod` on the host: `diod` provides the 9p file server backend used by gem5's `VirtIO9PDiod` device.

1. Start the VP with 9p attached: 9p is optional and disabled by default. Enable it explicitly with `--attach-9p`.

    ```bash
    ./start_vp.bash --attach-9p
    ```

1. Mount the shared folder inside the VP:

    ```bash
    # Host folder
    export HOST_9P_SHARE_DIR=<absolute_path_to_m5out/9p/share-folder>

    # VP folder
    mkdir -p /root/share9p

    # Mount
    mount -t 9p -o trans=virtio,version=9p2000.L,aname=${HOST_9P_SHARE_DIR} gem5 /root/share9p

    # Unmount
    umount /root/share9p
    ```

## Dummy-Example: Linear Function Accelerator (LFA)

### Build & Install the LFA

* Set environment variable pointing to the LFA implementation:

    ```bash
    # ${PWD} points to the repositories working directory
    export EXTRAS_DIR=${PWD}/accelerator
    ```

* If you want to build the [accelerator](accelerator/src/linear_function_accelerator.cc), you can use:

    ```bash
    cd gem5
    scons build/ARM/gem5.opt EXTRAS=${EXTRAS_DIR} -j`nproc`
    ```

### Test the LFA implementation

1. To attach the (out-of-tree) [`LinearFunctionAccelerator`](accelerator/python/LinearFunctionAccelerator.py) device to the bus, use:

    ```bash
    ./start_vp.bash --linear-function-accelerator
    ```

    If not specified otherwise, the device is attached at address `0x1c150000`.
    The MMIO register layout and behavior are documented in [`accelerator/README.md`](accelerator/README.md).

1. To test LinearFunctionAccelerator with `devmem`, execute the script `./test_linear_function_accelerator.sh` inside the VP.
You can find it under `/root` after login.
The source code can be seen in the [test_linear_function_accelerator.sh](https://github.com/rpelke/gem5_sw/tree/main/overlay/root/test_linear_function_accelerator.sh) file.

### Testing the LFA Driver

1. Make sure the driver is built in the [gem5_sw](https://github.com/rpelke/gem5_sw) repository.

1. Start the VP with the `--linear-function-accelerator` flag:

    ```bash
    ./start_vp.bash --linear-function-accelerator
    ```

1. Load the driver module:

    ```bash
    modprobe lfa
    ```

    (To unload the module, use `rmmod lfa`.)

1. List registered kernel drivers containing "lfa" in name:

    ```bash
    cat /proc/devices | grep lfa
    ```

    You should get `245 lfa`.

1. Check the device class in sysfs:

    ```bash
    ls /sys/class
    ```

    You should see `lfa` somewhere.

    This directory is part of **sysfs** and contains metadata about your device.
    You can inspect the device instance with
    `ls /sys/class/lfa/`.

1. Check the device node:

    ```bash
    ls /dev
    ```

    You should see `lfa0` somewhere.
    This is the **character device file** created via `udev`.
    It is the interface used by user-space applications to interact with the driver (e.g., via `open`, `read`, `write`, `ioctl`).

1. Test the `lfa` driver inside the VP. You can find it under `/root` after login.
    The source code can be seen in the [test_lfa_driver.c](https://github.com/rpelke/gem5_sw/tree/main/lib/test_lfa_driver.c) file.

    ```bash
    ./test_lfa_driver
    ```
