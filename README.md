# mikan-os

[WIP]  
[ゼロからのOS自作入門](https://zero.osdev.jp/)の写経  

## 開発環境

- Windows10 Pro ver.2004
- WSL2, Ubuntu 20.04
- Qemu 4.2.1
- VcXsrv 64.1.20.9.0

## セットアップ

- 開発ツールのインストール

```bash
$ ./scripts/setup.sh
```

- ``.bashrc``に以下を追記 (VcXsrvのための設定)

```bash
export DISPLAY=$(awk '/nameserver / {print $2; exit}' /etc/resolv.conf 2>/dev/null):0
```