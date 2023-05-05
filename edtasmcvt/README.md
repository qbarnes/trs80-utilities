Utility for converting original TRS-80 EDTASM format files to plain
text.

Sample use:
```
   $ edtasmcvt -cs ASMPGM.ESC asmpkg.txt
```

Option `-c` will convert the file to using tabs after line numbers
instead of spaces.

Option `-s` will strip line numbers making it compatible with later
Z-80 assembly utilities like `zmac`.
