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

        section .note.GNU-stack noalloc noexec nowrite progbits
