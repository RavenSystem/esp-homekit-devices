#########################################
# Default FatFs parameters
#########################################

# This option switches read-only configuration. (0:Read/Write or 1:Read-only)
# Read-only configuration removes writing API functions, f_write(), f_sync(),
# f_unlink(), f_mkdir(), f_chmod(), f_rename(), f_truncate(), f_getfree()
# and optional writing functions as well.
FATFS_FS_READONLY ?= 0

# This option switches string functions, f_gets(), f_putc(), f_puts() and
# f_printf().
#   0: Disable string functions.
#   1: Enable without LF-CRLF conversion.
#   2: Enable with LF-CRLF conversion.
FATFS_USE_STRFUNC ?= 1

# This option switches filtered directory read functions, f_findfirst() and
# f_findnext(). (0:Disable, 1:Enable 2:Enable with matching altname[] too)
FATFS_USE_FIND ?= 2

# This option switches f_mkfs() function. (0:Disable or 1:Enable)
FATFS_USE_MKFS ?= 1

# This option switches fast seek function. (0:Disable or 1:Enable)
FATFS_USE_FASTSEEK ?= 1

# This option switches f_expand function. (0:Disable or 1:Enable)
FATFS_USE_EXPAND ?= 1

# This option switches attribute manipulation functions, f_chmod() and f_utime().
# (0:Disable or 1:Enable) Also _FS_READONLY needs to be 0 to enable this option.
FATFS_USE_CHMOD ?= 1

# This option switches volume label functions, f_getlabel() and f_setlabel().
# (0:Disable or 1:Enable)
FATFS_USE_LABEL ?= 1

# This option switches f_forward() function. (0:Disable or 1:Enable)
FATFS_USE_FORWARD ?= 1

# This option specifies the OEM code page to be used on the target system.
# Incorrect setting of the code page can cause a file open failure.
#   437 - U.S.
#   720 - Arabic
#   737 - Greek
#   771 - KBL
#   775 - Baltic
#   850 - Latin 1
#   852 - Latin 2
#   855 - Cyrillic
#   857 - Turkish
#   860 - Portuguese
#   861 - Icelandic
#   862 - Hebrew
#   863 - Canadian French
#   864 - Arabic
#   865 - Nordic
#   866 - Russian
#   869 - Greek 2
#   932 - Japanese (DBCS)
#   936 - Simplified Chinese (DBCS)
#   949 - Korean (DBCS)
#   950 - Traditional Chinese (DBCS)
#     0 - Include all code pages above and configured by f_setcp()
FATFS_CODE_PAGE ?= 437

# The FATFS_USE_LFN switches the support of long file name (LFN).
#   0: Disable support of LFN. FATFS_MAX_LFN has no effect.
#   1: Enable LFN with static working buffer on the BSS. Always NOT thread-safe.
#   2: Enable LFN with dynamic working buffer on the STACK.
#   3: Enable LFN with dynamic working buffer on the HEAP.
# The working buffer occupies (FATFS_MAX_LFN + 1) * 2 bytes and
# additional 608 bytes at exFAT enabled. FATFS_MAX_LFN can be in range from 12 to 255.
# It should be set 255 to support full featured LFN operations.
# When use stack for the working buffer, take care on stack overflow.
FATFS_USE_LFN ?= 3
FATFS_MAX_LFN ?= 255

# This option switches character encoding on the API. (0:ANSI/OEM or 1:UTF-16)
# To use Unicode string for the path name, enable LFN and set FATFS_LFN_UNICODE = 1.
# This option also affects behavior of string I/O functions.
FATFS_LFN_UNICODE ?= 0

# When FATFS_LFN_UNICODE == 1, this option selects the character encoding ON THE FILE to
# be read/written via string I/O functions, f_gets(), f_putc(), f_puts and f_printf().
#   0: ANSI/OEM
#   1: UTF-16LE
#   2: UTF-16BE
#   3: UTF-8
# This option has no effect when FATFS_LFN_UNICODE == 0.
FATFS_STRF_ENCODE ?= 3

# This option configures support of relative path.
#   0: Disable relative path and remove related functions.
#   1: Enable relative path. f_chdir() and f_chdrive() are available.
#   2: f_getcwd() function is available in addition to 1.
FATFS_FS_RPATH ?= 2

# This option switches support of exFAT file system. (0:Disable or 1:Enable)
# When enable exFAT, also LFN needs to be enabled. (FATFS_USE_LFN >= 1)
FATFS_FS_EXFAT ?= 1

# The option FATFS_FS_NORTC switches timestamp functiton. If the system does not have
# any RTC function or valid timestamp is not needed, set FATFS_FS_NORTC = 1 to disable
# the timestamp function. All objects modified by FatFs will have a fixed timestamp
# defined by FATFS_NORTC_MON, FATFS_NORTC_MDAY and _NORTC_YEAR in local time.
# To enable timestamp function (FATFS_FS_NORTC = 0), get_fattime() function need to be
# added to the project to get current time form real-time clock. FATFS_NORTC_MON,
# FATFS_NORTC_MDAY and FATFS_NORTC_YEAR have no effect. 
# These options have no effect at read-only configuration (FATFS_FS_READONLY = 1).
FATFS_FS_NORTC ?= 1
FATFS_NORTC_MON ?= 1
FATFS_NORTC_MDAY ?= 1
FATFS_NORTC_YEAR ?= 2017

# The option FATFS_FS_LOCK switches file lock function to control duplicated file open
# and illegal operation to open objects. This option must be 0 when FATFS_FS_READONLY
# is 1.
# 0:  Disable file lock function. To avoid volume corruption, application program
#     should avoid illegal open, remove and rename to the open objects.
# >0: Enable file lock function. The value defines how many files/sub-directories
#     can be opened simultaneously under file lock control. Note that the file
#     lock control is independent of re-entrancy. */
FATFS_FS_LOCK ?= 64

# Timeout (system ticks) when requesting grant to access the volume
FATFS_FS_TIMEOUT ?= 1000
