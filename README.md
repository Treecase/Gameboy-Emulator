

GBEmu
=====

A [Gameboy](https://en.wikipedia.org/wiki/Gameboy) emulator.  


The default controls are:  

    W - D-pad Up  
    S - D-pad Down  
    A - D-pad Left  
    D - D-pad Right  
  
    N - B button  
    M - A button  
  
    Space - Start  
    Right Alt - Select  
  
    Escape - Quit  
    Plus - Scale up  
    Minus - Scale down  
    F - Show the full 256x256 screen instead of the usual 160x144 one  

(For now, controls can only be remapped by manually changing the bindings in low.c)


Command-Line Options are:  

    -d/--disassemble    Print out a disassembly instead of running the ROM  
    -b/--bios           Start the emulator in BIOS mode (the Nintendo logo scroll)  
  
    --log[=[!][zmdc]]   Enable debug logging. This can be controlled at a finer grain by passing  
                        --log=[!][zmdc]. The letters specify the module to enable; z=Z80.c, m=mem.c, d=display.c, c=cpu.c.  
                        The ! means to NOT enable logging for the next module letter (eg. !z means do not enable Z80.c logging)  
                        Z80.c logs the framerate, mem.c logs memory accesses, bank switches, etc., display.c logs scanline and  
                        frame drawing information, and cpu.c logs instructions and interrupts.  
  
    --break LOC         Set a breakpoint at $LOC. Note that the CPU has to hit this location exactly for the breakpoint to trigger.  
  
    -h/--help           Exactly what you think.  


