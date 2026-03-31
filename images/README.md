## Images - gem5 Software

1. To generate the software (kernel image etc.), please follow the steps in [https://github.com/rpelke/gem5_sw](https://github.com/rpelke/gem5_sw).

1. Copy the content of the resulting `image` folder into this repositories `image` folder.

1. After that, this folder should have the following structure:

    ```text
    images/
    ├── .gitignore
    ├── README.md
    ├── aarch64-buildroot-linux-gnu_sdk-buildroot.tar.gz
    └── system/
        ├── system.dtb
        └── binaries/
            ├── boot.arm
            ├── boot.arm64
            ├── rootfs.ext2
            └── vmlinux
    ```
