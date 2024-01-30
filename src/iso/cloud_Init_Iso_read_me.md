This documentation serves as a supplementary material to the cloud-init-iso.cpp code and comments. 

# ISO 9660 with Joliet Extension
ISO 9660 is a file format that organize all the file names and contents from a directory hierarchy into one standardized, platform independent file system structure. Furthermore, the whole file system is stored in one single binary file (commonly know as an ISO image). ISO 9660 was introduced at 1988, therefore it has some limitations like file/directory name length, character set, directory depth and system compatibility in the modern times.

Joliet extension is an enhancement of ISO 9660 and it is developed by Microsoft, it extends the ISO 9660 standard to better support different systems and it overcomes the limitations of ISO 9660, one of them is using Unicode to extend the name length.

With the preliminary knowledge, the whole reading file function comes down to understanding how the ISO file data is organized and navigating the structure to retrieve the file name and data.

# cloud-init-config.iso


# cloud-init-config.iso components


# The navigation strategy
