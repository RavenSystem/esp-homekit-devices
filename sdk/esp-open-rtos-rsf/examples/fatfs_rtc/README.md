This example shows how to use real-time clock (e.g. ds1307)
with FatFs for real timestamps on the filesystem objects.

1. Set `FATFS_FS_NORTC` to 0 (it's 1 by default) in application makefile.
2. Define function `uint32_t get_fattime()` which will return current time in
timestamp format:

 Bits   | Date part
 -------|----------
 0..4   | Second / 2 (0..29)
 5..10  | Minute (0..59)
 11..15 | Hour (0..23)
 16..20 | Day (1..31)
 21..24 | Month (1..12)
 25..31 | Year origin from 1980 (0..127)
