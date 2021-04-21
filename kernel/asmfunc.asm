; asmfunc.asm
;
; System V AMD64 Calling Convention
; Registers: RDI, RSI, RDX, RCX, R8, R9

bits 64
section .text

global IoOut32  ; void IoOut32(uint16_t addr, uint32_t data);
IoOut32:
    mov dx, di    ; dx = addr
    mov eax, esi  ; eax = data
    out dx, eax
    ret

global IoIn32  ; uint32_t IoIn32(uint16_t addr);
IoIn32:
    mov dx, di    ; dx = addr
    in eax, dx
    ret

global GetCS  ; uint16_t GetCS(void);
GetCS:
    xor eax, eax  ; also clears upper 32 bits of rax
    mov ax, cs
    ret

global LoadIDT  ; void LoadIDT(uint16_t limit, uint64_t offset);
LoadIDT:
    push rbp
    mov rbp, rsp
    sub rsp, 10
    mov [rsp], di  ; limit
    mov [rsp + 2], rsi  ; offset
    lidt [rsp]
    mov rsp, rbp
    pop rbp
    ret

global LoadGDT  ; void LoadGDT(uint16_t limit, uint64_t offset);
LoadGDT:
    push rbp
    mov rbp, rsp
    sub rsp, 10
    mov [rsp], di  ; limit
    mov [rsp + 2], rsi  ; ofset
    lgdt [rsp]  ; load GDT : GDTRレジスタにGDTを登録
    mov rsp, rbp
    pop rbp
    ret

global SetCSSS  ; void SetCSSS(uint16_t cs, uint16_t ss);
SetCSSS:
    push rbp
    mov rbp, rsp
    mov ss, si
    mov rax, .next
    push rdi  ; CS
    push rax  ; RIP
    o64 retf
.next:
    mov rsp, rbp
    pop rbp
    ret

global SetDSAll  ; void SetDSAll(uint16_t value);
SetDSAll:
    ; x86-64 64bitモードでは以下4つのセグメントレジスタは特に使用されない
    ; ので呼び出し元では引数に0を指定する（ヌルディスクリプタ）
    mov ds, di
    mov es, di
    mov fs, di
    mov gs, di
    ret

global SetCR3  ; void SetCR3(uint64_t value);
SetCR3:
    mov cr3, rdi
    ret

extern g_kernel_main_stack
extern KernelMainNewStack

global KernelMain
KernelMain:
    ; スタック領域をUEFI管理のものからOS管理のものへと移動
    mov rsp, g_kernel_main_stack + 1024 * 1024
    call KernelMainNewStack
.fin:
    ; 万が一KernelMainNewStack()から抜けてしまった場合に備え、無限ループさせる
    ; 新たなスタック領域にはKernelMain()からの戻り先アドレスがないため
    hlt
    jmp .fin
