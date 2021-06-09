bits 64
section .text

%macro define_syscall 2
global Syscall%1
Syscall%1:
    ; EAXに設定する番号とコールされる機能の対応はOS作者の自由
    mov rax, %2
    ; syscallはRIPをRCXに保存してしまうので、第4引数レジスタとしてRCXが使えない
    ; よって、R10に退避しておく（Linuxと同じ対処法）
    mov r10, rcx
    syscall
    ret
%endmacro

define_syscall LogString, 0x80000000
define_syscall PutString, 0x80000001
define_syscall Exit,      0x80000002
define_syscall OpenWindow, 0x80000003
define_syscall WinWriteString, 0x80000004
