
wolfssl_lib:
  Build wolfssl_lib.lib

test:
  Get missing files 
  - create DUMMY project
  - copy all files under DUMMY project except DUMMY.*
  - uncomment "Use SIM I/O" lines in resetprg.c
  - set heap size in sbrk.h
  - set stack size in stacksct.h
  Build test wolfCrypt

