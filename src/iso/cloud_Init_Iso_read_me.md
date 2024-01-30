This documentation serves as a supplementary material to the cloud-init-iso.cpp code and comments. 

# ISO 9660 with Joliet Extension
ISO 9660 is a file format that organize all the file names and contents from a directory hierarchy into one standardized, platform independent file system structure. Furthermore, the whole file system is stored in one single binary file (commonly know as an ISO image). ISO 9660 was introduced at 1988, therefore it has some limitations like file/directory name length, character set, directory depth and system compatibility in the modern times.

Joliet extension is an enhancement of ISO 9660 and it is developed by Microsoft, it extends the ISO 9660 standard to better support different systems and it overcomes the limitations of ISO 9660, one of them is using Unicode to extend the name length.

With the preliminary knowledge, the whole reading file function comes down to understanding how the ISO file data is organized and navigating the structure to retrieve the file name and data.

# cloud-init-config.iso
cloud-init-config.iso file is typically created as ISO 9660 with Joliet extension. In addition to that, cloud-init-config.iso file has a property that it only has the root directory and it has one flat list of files. That can be very advantageous when it comes to navigating the file structure.

The data areas components in the file includes volume Descriptor, path table, directory and file records and file data. The first three have both ISO 9660 part and Joliet part and they hold the same information in slightly different formats. As we mentioned before, Joliet is an enhancement of ISO 9660 and it contains all the information we need, the existence of ISO 9660 data areas is merely for very old operating systems. So as a result, we can just focus on the Joliet data area to navigate to files records and file data. In the context of cloud-init-config.iso, which is essentially a flat list files resides in one directory, the navigation can be greatly simplified. Before we delve into specifics, let us have a look of the roles of each relevant component and their data layout.

# cloud-init-config.iso components

## Joliet volume descriptor

Joliet volume descriptor is the header of the data area, it contains information like the location of path table and the Joliet root directory record.

The below table shows the data layout, the 0th byte indicates the volume descriptor type, that holds the value 2 in the case of Joliet volume descriptor.
The identifier comes after that and it takes the 2nd-6th bytes, it is normally assigned with value "CD001". These first two parts can be used to verify if the found data block is indeed Joliet volume descriptor. At 156-190 bytes, we have the root directory data, accessing that can take us to the location of it.

| `Part` | Type| Identifier| ...| root dir record| 
|:--- |:---|:---|:---|:---|
| `location(byte index)` | 0 | 1-6 | ... | 156-190 |

## Path table

Path table is the ISO 9660 standard's way of quickly looking up directories. It is not relevant in the context of cloud-init-config.iso, since the root directory is the only directory and it can be located directly via the Joliet volume descriptor.

## Joliet root directory record

It contains the information about the root directory itself, such as its size, location. The role of it in the cloud-init-config.iso navigation is to help us to locate the file records. Since the file records are placed right after the root directory record and root parent directory record, we can simply stride over the two data blocks.

| `Part` | data block size | current location| ...|
|:--- |:---|:---|:---|
| `location(byte index)` | 0 | 2-10 (lsb_msb)| ...|

## Joliet file record

It contains metadata about a specific file, includes the file name and file data location, etc. The below table shows the detailed data layout. The 0th byte is the current file record size, it can be used to locate the next file record start position because file records are contiguous in memory. 2nd-10th bytes are for file data location and 10th-18th bytes are for the size of extent. At the end, we have the file name info which are file name length and file name. With these metadata, we can extract file name and file data effortlessly.

| `Part` | data block size | file data location | size of extent | ... | file name length | file name |
|:--- |:--- |:---|:---|:---|:---|:---|
| `location(byte index)` | 0  | 2-10 (lsb_msb)| 10-18 (lsb_msb)| ... | 32 | 33 - (33 + file name length)|


# The navigation strategy
