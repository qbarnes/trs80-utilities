This utility for analyzing a TRS-80 CMD file (run-time executable)
for it to be correctly constructed.  If an output file is given, the
utility will write a new CMD file stripped of any trailing junk
characters.

```
stripcmd [-q] [{cmd_file|-} [out_file]]

    -q        Run quietly (repeat for more quiet)
```

Example output:

```
$ stripcmd RHINO.DVR RHINO2.DVR
Filename = "RHINO   10/10/10"
Load address == 0xc000 (len == 0xfe)
Load address == 0xc0fe (len == 0xfe)
Load address == 0xc1fc (len == 0xfe)
Load address == 0xc2fa (len == 0x91)
Transfer address == 0x402d
Found 159 extraneous bytes at end of file.
CMD file looks good!

$ ls -l RHINO.DVR RHINO2.DVR
-rw-r--r--. 1 qbarnes qbarnes  945 May  4 14:46 RHINO2.DVR
-rw-r--r--. 1 qbarnes qbarnes 1104 May  2 14:22 RHINO.DVR

$ sh -c 'expr 1104 - 945'
159
```

The `stripcmd` utility will read `RHINO.DVR` reporting on what it
finds in the file.  In this case four loadable blocks with a
transfer address of 0x402d.  It found 159 extraneous bytes at end
of file.  (Some TRS-80 DOSes did not keep accurate end-of-file
markers.)  It wrote out `RHINO2.DVR` without those extraneous bytes.
