%ifidn __?OUTPUT_FORMAT?__, win32
        %define f_asm _f_asm
%endif

        section .text
        global f_asm

f_asm:
        push ebp
        mov ebp, esp
        mov eax, [ebp+8]
        neg eax
        sbb eax, eax
        neg eax
        pop ebp
        ret

%ifidn __?OUTPUT_FORMAT?__, elf32
        section .note.GNU-stack noalloc noexec nowrite progbits
%endif
