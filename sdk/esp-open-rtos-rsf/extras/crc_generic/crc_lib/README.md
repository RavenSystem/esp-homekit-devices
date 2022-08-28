# crc_generic_lib
A library to compute all crc with multiple options and choices.
Create for the project esp-open-rtos but portable.


# Todo
Lookup table generation improvements (for Not multiple of 8 bits).
Error management when initialisation and selection algorithm.
Add specific algorithm(s) for each crc standard ( example: MAXIM ).
Add more generic algorithm (1024 lookup table for example).
Add a way to use hardware calculation functions. 

# Problem
Some algorithm don't work Properly at 64 bits and when order is not multiple of 8 bits.


# Source
This library is based on : CRC tester v1.3 written on 4th of February 2003 by Sven Reifegerste (zorc/reflex).
Library tested and checked with: http://crccalc.com/.
Possible algorithm to add example: http://create.stephan-brumme.com/crc32/.
