This modules was concerned with adding support for partial defragmentation in existing xfs_fsr utility.

The code files changed were
1) fsr/xfs_fsr.c

Description
xfs_fsr in its current capacity failed to perform partail defragementation when sufficent free space was not availabale. By adding an option -u to differentiate our logic from the existing performance, we are able to force defragementation of parts of the file, hence improving system organization and performance.
The code will create a cumulative extent data structure and then perform a selction of a contiguous group of extents after comparision against max free space. On an average 14% improvement in space organziation was seen and resulted in an average 63% improvement in file location search performance.
