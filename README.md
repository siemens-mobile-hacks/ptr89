# USAGE
```
Usage: ptr89 [arguments]

Global options:
  -h, --help               show this help
  -f, --file FILE          fullflash file [required]
  -b, --base HEX           fullflash base address [default: A0000000]
  -a, --align N            search align [default: 1]
  -V, --verbose            enable debug
  -J, --json               output as JSON

Find patterns:
  -p, --pattern STRING     pattern to search

Find patterns from functions.ini:
  --from-ini FILE          path to functions.ini
```

# Pattern syntax

### Simple byte maks
```bash
# Exact match one byte
B5

# Match ANY one byte
??

# Match one byte by half-byte mask
# ? is wildcard for any 4bit part of byte
2?
?2

# Match one byte by bit mask
# . is wildcard for bit
[1111....]
```

### Pattern separators
You can use spaces or commas to separate different bytes or groups.

All these patterns are equivalent:
```bash
# With commas
??,B5,??,B0,??,1C,??,20,??,43,??,99,??,4D,??,90,??,94,??,1C,??,92,??,91,??,68,??,26,??,23,??,21,??,A2,??,48,??,47

# With spaces
?? B5 ?? B0 ?? 1C ?? 20 ?? 43 ?? 99 ?? 4D ?? 90 ?? 94 ?? 1C ?? 92 ?? 91 ?? 68 ?? 26 ?? 23 ?? 21 ?? A2 ?? 48 ?? 47

# With spaces by 2 bytes groups
??B5 ??B0 ??1C ??20 ??43 ??99 ??4D ??90 ??94 ??1C ??92 ??91 ??68 ??26 ??23 ??21 ??A2 ??48 ??47

# Without any separators
??B5??B0??1C??20??43??99??4D??90??94??1C??92??91??68??26??23??21??A2??48??47
```

### Offset corrector

### Decode as reference
Emulating ARM/THUMB `LDR Rd, [PC, #offset]` instruction found by the pattern.

Syntax:
```bash
&( subPattern )
&( subPattern ) + valueCorrector
&( subPattern ) - valueCorrector
```

For example:
```bash
&( ??,48,??,47,??,B5,??,B0,??,1C,??,D1,??,20 ) + 0x4
```

Steps:
1. Pattern `??,48,??,47,??,B5,??,B0,??,1C,??,D1,??,20` was found at 0xA093BA58
2. Emulating LDR on 0xA093BA58:
```
A093BA58: 37 48           ; ldr r0, [pc, #0xdc]
; Emulation: PC + 0xDC = 0xA093BB38
```
3. Decoding pointer at 0xA093BB38
```
A093BB38: 10 97 E6 A8
```

3. Result is `0xA8E69710 + 0x4 = 0xA8E69714`

### Decode as pointer
