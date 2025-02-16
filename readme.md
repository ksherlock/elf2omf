
# ELF2OMF

An omf linker for [Calypsi](https://www.calypsi.cc/) [65816 compiler](https://github.com/hth313/Calypsi-tool-chains/releases).

Links the 65816 elf object files into an OMF executable. You'll need to review the calypsi manual and review the generated assembly if you want to actually build a IIgs application.  

```
usage elf2omf [flags] file...
Flags:
 -h               show usage
 -v               be verbose
 -X               inhibit ExpressLoad segment
 -C               inhibit SUPER records
 -S size          specify stack segment size
 -1               generate version 1 OMF File
 -o file          specify outfile name
 -t xx[:xxxx]     specify file type
```

## stack

You can specify the stack size with the `-S` flag or a bss section named "stack". Any direct page components (registers, tiny, ztiny) will be stored at the start and `.sectionStart stack`, `.sectionSize stack` will be adjusted to compensate.
