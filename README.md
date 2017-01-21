uDEFLATE
========
This is a minimalistic library written with embedded software in mind. The
library only handles DEFLATE decomrpession and makes no use of dynamic data but
keeps it all on the stack instead.

As a size comparison this is the resulting size of the library functions on
different platforms:
Compiling on x86_64 with gcc 4.8.4 and -Os sums up to 1462 bytes of code.
Compiling on arm with gcc 4.9.3 and -Os sums up to 1628 bytes of code.
Compiling on arm with gcc 4.9.3, -Os and -mthumb sums up to 1012 bytes of code.
