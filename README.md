## rsi - reverse shell improver

Automates the boring part after catching a reverse shell: pty upgrade via python3/python2/script, TERM and window size fixup, then hands you back a fully interactive TTY.

Works with any listener - nc, ncat..

## Install

Download the latest binary from releases and drop it in your PATH:

```bash
chmod +x rsi
sudo mv rsi /usr/bin/
```
