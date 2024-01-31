This documentation serves as supplementary material to the cloud_init_iso.cpp code and comments. 

# ISO 9660 with Joliet Extension

ISO 9660 is a file format that organizes all the file names and contents from a directory hierarchy into one standardized, platform-independent file system structure. Furthermore, the whole file system is stored in one single binary file (commonly known as an ISO image). ISO 9660 was introduced in 1988, therefore it has some limitations like file/directory name length, character set, directory depth, and system compatibility in the modern era.

Joliet extension is an enhancement of ISO 9660 and it is developed by Microsoft, it extends the ISO 9660 standard to better support different systems and it overcomes the limitations of ISO 9660, notably by employing Unicode to extend the file and directory name lengths. 

With this foundational understanding, the process of reading an ISO file essentially boils down to comprehending the file's structural organization and navigating this layout to retrieve the file names and their corresponding data.

# cloud-init-config.iso

cloud-init-config.iso file is typically created as ISO 9660 with Joliet extension. In addition to that, cloud-init-config.iso file has the property that it only contains a root directory with a simple, flat list of files. That can be very advantageous for navigating the file system.

In terms of its composition, the cloud-init-config.iso file includes volume descriptors, path tables, directory and file records, and file data. The first three have both ISO 9660 part and Joliet part and they hold the same information in slightly different formats. As we mentioned before, Joliet is an enhancement of ISO 9660 standard, providing all necessary information, the existence of ISO 9660 data areas is mainly for compatibility with older operating systems. Therefore, we can just focus on the Joliet data area to access file records and file data. In the context of cloud-init-config.iso, which is essentially a flat list of files residing in one directory, the navigation can be greatly simplified. Let's now examine the roles of each relevant component and their data layout.

# cloud-init-config.iso components

## Joliet volume descriptor

Joliet volume descriptor is the header of the data area, it contains information like the location of the path table and the Joliet root directory record.

The below table shows the data layout, the 0th byte indicates the volume descriptor type, that holds the value 2 in the case of Joliet volume descriptor.
The identifier comes after that and it takes the 2nd-6th bytes, it is normally assigned with the value "CD001". These first two parts can be used to verify if the found data block is indeed Joliet volume descriptor. At 156-190 bytes, we have the root directory data, accessing that can take us to its location.

| `Part` | Type| Identifier| ...| root dir record|
|:--- |:---|:---|:---|:---|
| `location(byte index)` | 0 | 1-6 | ... | 156-190 |

## Path table

Path table is the ISO 9660 standard's way of quickly looking up directories. It is not relevant in the context of cloud-init-config.iso, since the root directory is the only directory and it can be located directly via the Joliet volume descriptor.

## Joliet root directory record

It contains information about the root directory itself, such as its size, and location. The role of it in the cloud-init-config.iso navigation is to help us locate the file records. Since the file records are placed right after the root directory record and root parent directory record, we can simply stride over the two data blocks to reach the file records.

| `Part` | data block size | current location| ...|
|:--- |:---|:---|:---|
| `location(byte index)` | 0 | 2-10 (lsb_msb)| ...|

## Joliet file record

It contains metadata about a specific file, including the file name and file data location, etc. The below table shows the detailed data layout. The 0th byte is the current file record size, it can be used to locate the next file record start position because file records are contiguous in memory. 2nd-10th bytes are for file data location and 10th-18th bytes are for the size of extent. Finally, the file name information is provided at the end. With these metadata, retrieving the file name and file data becomes a straightforward task.

| `Part` | data block size | file data location | size of extent | ... | file name length | file name |
|:--- |:--- |:---|:---|:---|:---|:---|
| `location(byte index)` | 0  | 2-10 (lsb_msb)| 10-18 (lsb_msb)| ... | 32 | 33 - (33 + file name length)|

For detailed information on the order of each component in memory, please consult the top comment in the cloud_init_iso.cpp file. The full data layout of each component is in each struct definition. 

# The navigation strategy

With the preliminary knowledge of the relation between the Joliet components and their specific data layout, we can easily figure out the overall navigation strategy.

Start from Joliet Volume descriptor and access the root directory data area. This allows us to quickly move to the exact location of the root directory record. From there, we can jump over the root directory record and root parent directory record part and reach the file records. Upon reaching this point, we can iterate through these file records and extract the file name and the file data.
