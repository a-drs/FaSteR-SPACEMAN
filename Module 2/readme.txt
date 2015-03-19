This module was concerned with ensuring that xfs_spaceman accounted for AGFL blocks in the XFS file system.

The files worked on are: (File is in kernel space)
1) fs/xfs/libxfs/xfs_alloc.c

Description:
We added new logic and subsequent functions which will walk the free extents in AGFL and account for these AGFL blocks, during file system scanning.
The previous logic walked the free space B+trees to account for free blocks. However the blocks that are indexed by the AGFL cannot be found by walking these B+trees, 
hence we had to create logic to access the free list as a circular list and subsequently account for these extents.



