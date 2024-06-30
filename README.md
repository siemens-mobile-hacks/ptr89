# Ptr89

There is yet another ARM/THUMB pattern finder.

Main features:
- Compatible with Smelter patterns syntax.
- Compatible with Ghidra SRE patterns syntax.
- Enhanced patterns syntax:
	- Nested patterns for LDR.
	- Half-byte patterns.
	- Bitmask patterns.
 - JSON output.

The name was chosen in respect to [Viktor89](https://patches.kibab.com/user.php5?action=view_profile&id=4205), who is greatest patch porter in the Siemens Mobile modding scene.

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

### Find patterns
```bash
$ ptr89 -f EL71v45.bin -p "F0B5061C0C1C151C85B068461122??49??????????E0207869466A460009085C307021780134"
Pattern: 'F0B5061C0C1C151C85B068461122??49??????????E0207869466A460009085C307021780134'
Found 1 matches:
  A058BB98: A058BB99 (offset)

Search done in 72 ms
```

```bash
$ ptr89 -f EL71v45.bin -p "??2800D0F5E6704780B508F0??E980BD80B5+1" -p "??B589B006A901A80522??????????49051C"
Pattern: '??2800D0F5E6704780B508F0??E980BD80B5+1'
Found 1 matches:
  A0092F93: A0092F93 (offset)

Pattern: '??B589B006A901A80522??????????49051C'
Found 1 matches:
  A05C4B38: A05C4B39 (offset)

Search done in 143 ms
```

### Convert patterns.ini to swilib.vkp
```
ptr89 -f EL71v45.bin --from-ini ELKA.ini > swilib.vkp
```

# Pattern syntax

Syntax is fully compatible with WinHex, Smelter and Ghidra SRE patterns.

## Simple byte maks
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

## Pattern separators
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

## Offset corrector
Apply some correction value to the found offset.

Syntax:
```bash
pattern + offsetCorrector
pattern - offsetCorrector
```

For example:
```bash
# offsetCorrector value is always in HEX.
# For e.g.: 20 and 0x20 are equal.
B801C4E10200A0E30000C1E5B601D4E11040BDE8??????EA + 20
```

Steps:
1. Pattern `B801C4E10200A0E30000C1E5B601D4E11040BDE8??????EA` found at 0xA009B780
2. Result is `0xA009B780 + 0x20 = 0xA009B7A0`

## Decode as pointer
Decoding a pointer value from the bytes found by the pattern.

Syntax:
```bash
*( subPattern )
*( subPattern ) + valueCorrector
*( subPattern ) - valueCorrector
```

For example:
```bash
# valueCorrector value is always in HEX.
# For e.g.: 20 and 0x20 are equal.
*(B801C4E10200A0E30000C1E5B601D4E11040BDE8??????EA + 20) + 2
```

Steps:
1. Pattern `B801C4E10200A0E30000C1E5B601D4E11040BDE8??????EA+20` found at 0xA009B7A0
2. Decoding bytes as pointer at 0xA009B7A0:
	```asm
	A009B7A0: CC 5B D9 A8 ; 0xA8D95BCC
	```
3. Result is `0xA8D95BCC + 0x2 = 0xA8D95BCE`

## Decode as reference
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
1. Pattern `??,48,??,47,??,B5,??,B0,??,1C,??,D1,??,20` found at 0xA093BA58
2. Emulating LDR on 0xA093BA58:
	```asm
	A093BA58: 37 48           ; ldr r0, [pc, #0xdc]
	; Emulation: PC + 0xDC = 0xA093BB38
	```
3. Decoding pointer at 0xA093BB38
	```asm
	A093BB38: 10 97 E6 A8 ; 0xA8E69710
	```

3. Result is `0xA8E69710 + 0x4 = 0xA8E69714`

## Nested patterns for branches
Follow the branch and checking it for a pattern.

Syntax:
```bash
# ARM B/BL/BLX or THUMB BL/BLX (4 bytes instruction)
{ subPattern }
_BLF(subPattern) # alias for { }

# THUMB B (2 bytes instruction)
[ subPattern ]
```

Supported instructions:
```
# ARM
B #offset
BL #offset
BLX #offset
LDR PC, [PC, #offset]

# THUMB
BL #offset
BLX #offset
```

For example:
```bash
?? 1C ?? 48 ?? B5 ?? 68    { ?? 1C ?? 68 ?? 68 ?? 2B ?? D0 ?? 68 [ ?? 23 [ ?? B5 ?? 1C ?? 6E ] ] 47 }    BD + 0x1
```

Steps:
1. Pattern `?? 1C ?? 48 ?? B5 ?? 68    ?? ?? ?? ??    BD + 0x1` found at 0xA0978822
    ```asm
    A0978822: 01 1C           ADD        R1,R0,#0x0
	A0978824: 62 48           LDR        R0,[DAT_A09789B0]
	A0978826: 80 B5           PUSH       {R7,LR}
	A0978828: 00 68           LDR        R0,[R0,#0x0]=>DAT_A8DBE3F0
	A097882A: 9E F0 F6 FD     BL         FUN_A0A1741A ; <--- see this
	A097882E: 80 BD           POP        {R7,PC}
	```
3. Emulating BL at 0xA097882A (+8)
	```asm
	A097882A: 9E F0 F6 FD ; BL #0xA0A1741A
	```
4. Checking pattern `?? 1C ?? 68 ?? 68 ?? 2B ?? D0 ?? 68     ?? ??     47` at 0xA0A1741A
	```asm
	A0A1741A: 0A 1C           ADD        R2,R1,#0x0
	A0A1741C: 01 68           LDR        R1,[R0,#0x0]
	A0A1741E: 8B 68           LDR        R3,[R1,#0x8]
	A0A17420: 00 2B           CMP        R3,#0x0
	A0A17422: 01 D0           BEQ        LAB_A0A17428
	A0A17424: C9 68           LDR        R1,[R1,#0xC]
	A0A17426: F6 E7           B          FUN_A0A17416 ; <--- see this
	A0A17428: 70 47           BX         LR
	```
6. Emulating BL at 0xA0A17426 (+12)
	```asm
	A0A17426: F6 E7 ; B #0xA0A17416
	```
7. Checking pattern `?? 23    ?? ??` at 0xA0A17416
	```asm
	A0A17416: 01 23           MOV        R3,#0x1
	A0A17418: D7 E7           B          LAB_A0A173CA ; <--- see this
	```
9. Emulating BL at 0xA0A17418 (+2)
	```asm
	A0A17418: D7 E7 ; B #0xA0A173CA
	```
10. Checking pattern `?? B5 ?? 1C ?? 6E` at 0xA0A173CA
	```asm
	A0A173CA: F8 B5           PUSH       {R3,R4,R5,R6,R7,LR}
	A0A173CC: 04 1C           ADD        R4,R0,#0x0
	A0A173CE: 80 6E           LDR        R0,[R0,#0x68]
	A0A173D0: 0D 1C           ADD        R5,R1,#0x0
	A0A173D2: 16 1C           ADD        R6,R2,#0x0
	```
11. Pattern result is `0xA0978822 + 0x1 = 0xA0978823`

## Nested patterns for references
Follow the reference and checking it for a pattern.

Syntax:
```bash
# ARM LDR (4 bytes instruction)
LDR{ subPattern }

# THUMB LDR (2 bytes instruction)
LDR[ subPattern ]
```

Supported instructions:
```
# ARM/THUMB
LDR Rd, [PC, #offset]
```

For example:
```bash
LDR{ 436f70797269676874204d47432032303034 } 1e ff 2f e1
```

Steps:
1. Pattern `?? ?? ?? ??    1e ff 2f e1` found at 0xA00A0B1C
3. Emulating LDR at 0xA00A0B1C (+0)
   ```asm
   A00A0B1C: 00 00 9F E5  LDR R0, [PC, #+0x0] ; 0xA00A0B24
   ; Emulation: PC + 0x0 = 0xA00A0B24
   ```
4. Decoding pointer at 0xA00A0B24
	```asm
	A00A0B24: 1D 34 0A A0 ; 0xA00A341D
	```
5. Checking pattern `436f70797269676874204d47432032303034` at 0xA00A341D
	```asm
 	A00A341D: ds "Copyright MGC 2004 - Nucleus PLUS - Integrator RVCT v. 1.15"
 	```
6. Result is: `0xA0A63270`

## Automatic THUMB bit
If the found address points to a THUMB `PUSH { ... }` instruction, +1 will be added to the result.

Example:
```
??,06,??,0E,??,38,??,30,??,78,??,42,??,D0,??,29,??,D1,??,42,??,D0,??,20,??,47
```

Steps:
1. Pattern `F0B5061C0C1C151C85B068461122??49??????????E0207869466A460009085C307021780134` found at 0xA058BB98
	```asm
	A058BB98 F0 B5           PUSH       {R4,R5,R6,R7,LR} ; <-- see this
	A058BB9A 06 1C           ADD        R6,R0,#0x0
	A058BB9C 0C 1C           ADD        R4,R1,#0x0
	A058BB9E 15 1C           ADD        R5,R2,#0x0
	A058BBA0 85 B0           SUB        SP,#0x14
	A058BBA2 68 46           MOV        R0,SP
	A058BBA4 11 22           MOV        R2,#0x11
	```
2. Result is: `0xA058BB98 | 1 = 0xA058BB99`

## Stub value
Usually used in `patterns.ini` for stub entries.
```bash
# Pattern result is 0xA8000000
< A8000000 >
```
