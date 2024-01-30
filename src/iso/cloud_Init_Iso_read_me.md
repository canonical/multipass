This documentation serves as a supplementary material to the cloud-init-iso.cpp code and comments. 

# ISO 9660 with Joliet Extension
ISO 9660 is a file format that organize all the file names and contents from a directory hierarchy into one standardized, platform independent file system structure. Furthermore, the whole file system is stored in one single binary file (commonly know as an ISO image). ISO 9660 was introduced at 1988, therefore it has some limitations like file/directory name length, character set, directory depth and system compatibility in the modern times.

Joliet extension is an enhancement of ISO 9660 and it is developed by Microsoft, it extends the ISO 9660 standard to better support different systems and it overcomes the limitations of ISO 9660, one of them is using Unicode to extend the name length.

With the preliminary knowledge, the whole reading file function comes down to understanding how the ISO file data is organized and navigating the structure to retrieve the file name and data.

# cloud-init-config.iso
cloud-init-config.iso file is typically created as ISO 9660 with Joliet extension. In addition to that, cloud-init-config.iso file has a property that it only has the root directory and it has one flat list of files. That can be very advantageous when it comes to navigating the file structure.

The data areas components in the file includes volume Descriptor, path table, directory and file records and file data. The first three have both ISO 9660 part and Joliet part and they hold the same information in slightly different formats. As we mentioned before, Joliet is an enhancement of ISO 9660 and it contains all the information we need, the existence of ISO 9660 data areas is merely for very old operating systems. So as a result, we can just focus on the Joliet data area to navigate to files records and file data. In the context of cloud-init-config.iso, which is essentially a flat list files resides in one directory, the navigation can be greatly simplified. Before we delve into specifics, let us have a look of the roles of each relevant component and their data layout.

# cloud-init-config.iso components


# The navigation strategy
