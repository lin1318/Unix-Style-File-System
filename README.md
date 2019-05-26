

# Unix-Style File System

This is an inode-based Unix-Style file system.



## Attributes 

- Allocate **16MB** space in memory as the storage for my file system
- The file system is divided into **16384 blocks** and the size of each block is **1KB**
- The inode support **10 direct block address, and one indirect block address**
- Using **random strings** to fill the files you created.
- All codes are written by **C++**



## Functionality and Usage

1. Use "format" to format the file system. 
   - i.e. format
3. Use "createFile \<fileName\> \<fileSize\> " to create file with its name and size(in KB)
   - i.e. createFile  /dir/a.txt 10
3. Use "deleteFile \<filename\> " to delete a file
   - i.e. deleteFile /dir/a.txt
4. Use "createDir \<dirName\>" to create a directory
   - i.e.  createDir /dir/sub1
5. Use "deleteDir \<dirName\> " to delete a directory 
   - i.e. deleteDir /dir/sub1
6. Use "changeDir \<dirName\> " to change the current working path
   - i.e. changeDir /dir
7. Use "dir" to list all the files and sub-directories under current working directory and list some attributes of  them
   - i.e. dir
8. Use "cp \<sourceFileName\> \<destinationFileName\> " to copy a file from source file to destination file. If the destination file does not exist, it will create one same as the source file.
   - i.e. cp file1 file2
9. Use "cat fileName" to show the content of the file.
   - i.e. cat file

## License

Copyright (c) 2019 Yangzhou Lin

Licensed under the [MIT License](https://opensource.org/licenses/MIT).
