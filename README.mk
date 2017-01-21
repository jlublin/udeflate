uDEFLATE
========
This is a minimalistic library written with embedded software in mind. The
library only handles DEFLATE decomrpession and makes no use of dynamic data but
keeps it all on the stack instead.

Compiling on x86_64 with gcc 4.8.4 and -Os sums up to 1388 bytes of code.
Compiling on arm with gcc 4.9.3 and -Os sums up to 1556 bytes of code.
Compiling on arm with gcc 4.9.3, -Os and -mthumb sums up to 968 bytes of code.
