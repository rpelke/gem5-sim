# gem5-sim

## Build & Install
1. Clone the repository including submodules:
    ```bash
    git clone --recursive git@gitlab.ice.rwth-aachen.de:pelke/gem5-sim.git
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

    If you want to also build the `dummy_mmio` device, you can use:
    ```bash
    # ${PWD} points to the repositories working directory
    export EXTRAS_DIR=${PWD}/accelerator
    cd gem5
    scons build/ARM/gem5.opt EXTRAS=${EXTRAS_DIR} -j`nproc`
    ```

1. Build the software (Linux image, etc.): Follow the steps in this [README.md](images/README.md).

1. Start the VP:
    ```bash
    ./start_vp.bash
    ```

*   (Optional)
    To attach the (out-of-tree) [`DummyMmio`](accelerator/python/DummyMmio.py) device to the bus, use:
    ```bash
    ./start_vp.bash --dummy-mmio
    ```

    If not specified otherwise, the device is attached by default at address `0x1c150000`.

    To test the `DummyMmio` device, you can use `devmem`.
    `devmem` is a small Linux userspace tool which can directly read and write memory-mapped physical addresses. This makes it useful for quickly testing MMIO devices without writing a kernel driver first.

    To write `0xcaffee` to the device register from Linux, use:
    ```bash
    devmem 0x1c150000 32 0xcaffee
    ```

    To read the register value back, use:
    ```bash
    devmem 0x1c150000 32
    ```

    You should get the following output:

    ![devmem example](docs/images/devmem.png)

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

- Convert the binary device tree `system.dtb` into the readable source file `system.dts`:
    ```bash
    dtc -I dtb -O dts -o system.dts system.dtb
    ```

- Convert the device tree source `system.dts` back into the binary `system.dtb`:
    ```bash
    dtc -I dts -O dtb -o system.dtb system.dts
    ```

- Connect to the gem5 terminal interface exposed on TCP port `3456` to interact with the Linux system, for example to run commands normally inside the guest:
    ```bash
    telnet localhost 3456
    ```
