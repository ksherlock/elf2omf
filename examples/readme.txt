
hello - a simple shell application.

S16 and EXEs have a dedicated direct page (256 bytes in this case).


fish_nda - a new desk accessory.

NDAs, CDAs, etc don't have a dedicated direct page so we set up space
for the direct page/pseudo registers on the stack.  However, this means
that tiny (direct page) variables are not supported.


Since the toolbox calling convention is not yet supported, we need 
to use stubs or inline assembly.

