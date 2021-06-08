bits 64
section .text

global SyscallLogString
SyscallLogString:
    ; EAXに設定する番号とコールされる機能の対応はOS作者の自由
    mov eax, 0x80000000
    ; SyscallLogStringには本来不要だが、
    ; syscallはRIPをRCXに保存してしまうので、第4引数レジスタとしてRCXが使えない
    ; よって、R10に退避しておく（Linuxと同じ対処法）
    mov r10, rcx
    syscall
    ret
