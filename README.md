# DASE - Disk Auditor and Secure Eraser
Disk Auditor and Secure Eraser (DASE) is a bootable USB tool designed to help users securely erase data from their storage devices while also providing auditing capabilities to ensure that the data has been properly removed.

**DASE IS DESIGNED TO FULLY ERASE DATA.** Wiping permanently destroys all data on the selected disks.

This tool is particularly useful for individuals and organizations that need to comply with data protection regulations and want to ensure that sensitive information is not recoverable after deletion.

DASE is composed of a simple command-line interface that allows for managing disks to erase, providing useful data about the disks, and performing secure erasure operations.

## Motivation
Currently, there are limited tools available in the market that provide both secure erasure and disk auditing capabilities in a single bootable USB image. Not only that, but the current existing tools are either paid, being too expensive, or most of the support and development already stopped. This causes a problem where use cases that require other methods, than the ones for HDDs, are practically non-existent. This is a problem, since overwriting data on SSDs, both NVMe and SATA, not only is not safe, but also can cause heavy damage to the device itself.
More importantly, the existing tools are developed on top of Linux distributions that add a layer of complexity and unnecessary dependencies that make the tools harder to use and slower.

DASE aims to fill this gap by building a lightweight tool, based on GNU-EFI programming, that can run a bootable USB image without the need for a full operating system. This allows for a more streamlined and efficient approach to secure erasure and disk auditing. Moreover, DASE is currently designed to support both HDDs and SSDs, automatically detecting the type of disk and applying the appropriate erasure method. This ensures that users can securely erase data from their storage devices without risking damage to the hardware.
Also, by removing the dependency on a full operating system, DASE can be more easily maintained and updated, ensuring that it remains a reliable tool for secure erasure and disk auditing.

## Building and Running DASE
To build and run DASE, follow these steps:
- If there is no pre-built binary available, you can build the project from source. The following steps outline the process:
1. Clone the DASE repository from GitHub.
2. Build the project using the provided build scripts or makefiles. Use the ./build.sh script. Note that docker is required.
3. Run the built binary from a bootable USB image. Use software such as Rufus to create a bootable USB drive with the DASE binary.

- If a pre-built binary is available, you can download it from the DASE GitHub repository. Once downloaded, create a bootable USB drive using software such as Rufus and run the binary directly from the USB drive.

## System Design
As said before, DASE is designed to be a lightweight tool that can run from a bootable USB image without the need for a full operating system. The tool is built using GNU-EFI programming, which allows it to run directly on the hardware without the need for an underlying operating system.
DASE is accompanied by a simple command-line interface that allows to manage and wipe the disks. Writing a few lines such as:
```
DASE> listdisks
```
will list all the disks connected to the system, while:
```
DASE> help
```
will provide a list of available commands and their usage.

## Features
- **Secure Erasure**: DASE uses advanced algorithms to securely erase data from storage devices, making it unrecoverable.
- **Disk Auditing**: The tool provides detailed information about the disks, including their size, partitions, and file systems, allowing users to audit the state of their storage devices before and after erasure.
- **Bootable USB**: DASE can be run from a bootable USB drive, making it easy to use on different systems without the need for installation.
- **Multi-x86_64 Architecture Support**: DASE is designed to work on both 32-bit and 64-bit systems, ensuring compatibility with a wide range of hardware.
- **User-Friendly Interface**: The command-line interface is designed to be intuitive and easy to use. The help command provides enough information to get started with the tool.

## Security Features and Current Implementation
DASE is designed with security in mind, implementing several features to ensure that data is securely erased:
- HDD: DASE overwrites data on HDDs with zeros, and verifies the erasure by reading back the data to ensure that it has been properly removed. This is a standard method for securely erasing data on HDDs, and is widely accepted as a reliable approach.
- SSD: DASE uses the ATA Secure Erase command to securely erase data on SSDs. The approach is to use SSDs Sanitize and other secure erase implementations, which are designed to ensure that data is completely removed from the device. This method is considered to be the most effective way to securely erase data on SSDs, as it takes into account the unique characteristics of these devices.
- RAID, USB and unknown types: DASE refuses to perform any erasure operation on RAID (unless set to IT/Passthrough mode), USB and unknown types of disks. This is a precautionary measure to prevent accidental data loss or damage to the device. Users are advised to use other tools that are specifically designed for these types of disks.
- Serial Number and Model Verification: DASE verifies the serial number and model of the disk before performing any erasure operation. This lets users ensure that the correct disk is being erased, and prevents accidental data loss on other devices.
- Every wipe is verified: DASE verifies the erasure operation by reading back the data from the disk to ensure that it has been properly removed. This provides an additional layer of assurance that the data has been securely erased.
- Every wipe requires user confirmation: Before performing any erasure operation, DASE prompts the user for confirmation to ensure that they are aware of the consequences of the operation. This helps prevent accidental data loss and ensures that users are making informed decisions about their data.

DASE also has several utility features that enhance its usability and functionality:
- **Disk Listing**: The tool can list all connected disks, providing information about their size, partitions, and file systems. This allows users to easily identify the disks they want to erase and verify their state before and after the erasure process.
- **FAT32 test**: DASE includes a FAT32 test feature that allows users to read the contents of a FAT32-formatted disk. This helps the user ensure that the data inside the disk should be erased, and provides an additional layer of verification before performing the secure erasure operation.

## Limitations
While DASE is a powerful tool for secure erasure and disk auditing, it does have some limitations that users should be aware of:
- **Limited Disk Support**: DASE currently supports only NVMe SSDs and HDDs. It does not support other types of storage devices, such as SATA SSDs, USB drives, or RAID configurations.
- **x86_64 Architecture Only**: DASE is designed to work only on x86_64 architecture systems. It does not support other architectures, such as ARM or PowerPC.
- **Single-Namespace NVMe SSDs Only**: DASE currently supports only single-namespace NVMe SSDs. It does not support multi-namespace NVMe SSDs, which may limit its usability for some users.
- **Slow Verification Process**: The verification process for erasure operations can be slow, especially for large disks. This may result in longer wait times for users who need to verify the erasure of their data.

## Future Work
While DASE is a current solo project from a MSc student in a free time, the goal is to allow the project to grow and become a community-driven open-source project. 

### Future Functionalities
The future work for DASE functionalities includes:
- **Enhanced Disk Support**: Expanding support for SATA SSDs safely.
- **Graphical User Interface (GUI)**: Developing a TUI (Text-based User Interface) to make the tool more accessible to users who prefer a visual interface over command-line interactions.
- **File-System**: Despite the needed complexity to implement a file-system support for such as NTFS, ext4 or exFAT, this will allow the user to check the contents of the disk before any erasure operation, providing an additional layer of auditing and verification. This also removes the need for a second tool or operating system to check the contents of the disk before erasure.
- **PDF Generation**: Adding the ability to generate PDF reports of the disk auditing and erasure operations for documentation and compliance purposes.
- **Secure Boot**: Implementing secure boot capabilities to ensure that the tool can only be run on authorized systems, preventing unauthorized access and tampering.
- **Full NIST and IEEE Compliance**: Implementing secure erasure methods that comply with NIST and IEEE 2883 standards to ensure that data is irrecoverable and meets regulatory requirements. While the current implementation of DASE provides secure erasure capabilities, achieving full compliance with these standards will require additional development and testing to ensure that the tool meets the necessary criteria for data destruction.

### Community Involvement and Contributions
The future work for DASE community involvement includes:
- **Community Contributions**: Encouraging contributions from the open-source community to enhance the tool's capabilities, fix bugs, and add new features based on user feedback.
- **Learn GNU-EFI**: Despite the current lack of documentation and resources for GNU-EFI programming, the goal is to learn and master this technology to improve DASE's functionality and performance. While I still am learning, this project could also integrate other students or developers that are interested in learning GNU-EFI programming, providing a collaborative environment for skill development and knowledge sharing.