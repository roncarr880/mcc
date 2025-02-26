# mcc
C Cross compiler

Output modules for TMS32C26 dsp,  8085, 16F and 18F PIC devices, 68HC11.
Written in Mix Power C.  This is a MSDOS program, no Windows.  Nowadays use Dosbox to run.
Code generated is basically char variables for the 8 bit processors and 16 bit for the TMS dsp processor.

The compiler is a one pass recursive descending parser.   A few of the output modules have a a peephole optimizer.

A programming model is used that is different for each output module. For example the 8085 module is register based.  The HC11 is stack based.
The model generally refers to how function argument(s) and local variables are implemented.  This limits the number of locals that can be declared.
The 16F pics do not have a data stack, all variables are global or static.  The 18F uses the same model.

A rich variety of variables can be declared including arrays, structures, string literal, etc.  Sometimes some assembly code will be needed to implement
features that are not basic to the target processor.  Example: mult and divide is not implemented for most.

The zip file is a snapshot of all at a certain time point.  External files ( example : mc11out.c ) are improvements and would need to be compiled and 
linked to be up to date.


