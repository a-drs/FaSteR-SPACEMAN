Code files related to creating the XFS specific ioctl XFS_IOC_FIEMAPFS for xfs_spaceman, in order to replace an existing genric file based ioctl FS_IOC_FIEMAPFS


The following files were worked on in kernel space
1) fs/ioctl.c
2) include/linux/fs.h
3) fs/xfs/xfs_ioctl.c
4) fs/xfs/xfs_fs.h


The following file were worked on in user space
1) include/xfs_fs.h
2) spaceman/freesp.c


Descriptions

1) The command and command argument type definitions in the ioctl need to be shared across the file system (in kernel space) and the application (in user space). Hence the ioctl definition for XFS_IOC_FIEMAPFS is in the folowing header files
-> include/linux/fs.h
-> fs/xfs/xfs_fs.h
-> include/xfs_fs.h

2) The ioctl will be called from userspace i.e from file spaceman/freesp.c. The ioctl call will be intercepted in fs/xfs/xfs_ioctl.c instead of fs/ioctl.c. The free space mapping task will now be done in kernel space and sent back to userspace in the form of populated fiemap structures. 
  
