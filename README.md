# Ptr89

There is yet another ARM/THUMB/C166 pattern finder.

[JavaScript module](JS.md).

Main features:
- Compatible with [Smelter](https://web.archive.org/web/20090414122112/http://avkiev.kiev.ua/Siemens/Smelter/Smelter.htm) patterns syntax.
- Compatible with Ghidra SRE patterns syntax.
- Enhanced patterns syntax:
	- Nested patterns for LDR.
	- Half-byte patterns.
	- Bitmask patterns.
 - JSON output.

The name was chosen in respect to [Viktor89](https://patches.kibab.com/user.php5?action=view_profile&id=4205), who is greatest patch porter in the Siemens Mobile modding scene.

# DOWNLOAD
- Windows: download .exe in [Releases](https://github.com/siemens-mobile-hacks/ptr89/releases).
- ArchLinux: `yay -S ptr89-git`
- OSX: `brew install siemens-mobile-hacks/tap/ptr89`
- Ubuntu 24.04/Debian 13+: download .deb in [Releases](https://github.com/siemens-mobile-hacks/ptr89/releases).
- Build from sources:
	```bash
 	git clone https://github.com/siemens-mobile-hacks/ptr89
 	cd ptr89
	git submodule init
 	git submodule update

	# Ubuntu 24.04/Debian 13+
	fakeroot debian/rules binary

	# OSX, Linux, Unix, MinGW
	cmake -B build -DCMAKE_BUILD_TYPE=Release
	cmake --build build
	cmake --install build

	# Windows
	cmake -B build
	cmake --build build --config Release
	```

# USAGE
```
Usage: ptr89 [arguments]

Global options:
  -h, --help               show this help
  -f, --file FILE          fullflash file [required]
  -b, --base HEX           fullflash base address [default: A0000000 for arm, auto for c166]
  -A, --arch ARCH          architecture: arm or c166 [default: arm]
  -a, --align N            search align [default: 1]
  -V, --verbose            enable debug
  -J, --json               output as JSON
      --show-bytes         show all bytes for long results

Find patterns:
  -p, --pattern STRING     pattern to search
  -n, --limit NUMBER       limit results count [default 100]

Find xrefs:
  -x, --xref HEX           address to search
  -n, --limit NUMBER       limit results count [default 100]

Find patterns from functions.ini:
  --from-ini FILE          path to functions.ini

Prettify pattern:
  --prettify STRING        pattern
```

### C166 fullflash

Select C166 with `-A c166` or `--arch c166`. If `--base` is omitted, the image is placed at the top of the 16 MiB C166 address space:

```
base = 16 * 1024 * 1024 - fullflash_size
```

For example, a 14 MiB M55 fullflash gets base `0x200000`. An explicit `--base` always overrides this calculation.

```bash
# F0 C8        MOV R12, R8
# F0 D9        MOV R13, R9
# DA 25 72 93  CALLS 0x25, 0x9372 -> 0x259372
# F0 C8        MOV R12, R8
# F0 D9        MOV R13, R9
# DA 9A 32 93  CALLS 0x9A, 0x9332 -> 0x9A9332
$ ptr89 -f M55_v91.bin -A c166 -p '&BL(F0C8F0D9DA257293F0C8F0D9DA9A3293 + C)'
Pattern: '&BL(F0C8F0D9DA257293F0C8F0D9DA9A3293 + C)'
Found 1 branch:
  ADDRESS   OFFSET    BYTES
  009A9332  00059254  DA 9A 32 93
```

### Find x-refs
```bash
$ ptr89 -f EL71v45.bin -x A04CA048
Xrefs to 0xA04CA048
Found 3 xrefs:
  OFFSET    XREF      KIND       BYTES
  00CF63A4  A0CF63A4  branch     CC F2 1A E9
  00FC25DC  A0FC25DC  branch     04 F0 1F E5
  00FC25E0  A0FC25E0  pointer    48 A0 4C A0

Search done in 1612 ms
```

### Find patterns
```bash
$ ptr89 -f EL71v45.bin -p "F0B5061C0C1C151C85B068461122??49??????????E0207869466A460009085C307021780134"
Pattern: 'F0B5061C0C1C151C85B068461122??49??????????E0207869466A460009085C307021780134'
Found 1 address:
  ADDRESS   OFFSET    BYTES
  A058BB99  0058BB98  F0 B5 06 1C 0C 1C 15 1C 85 B0 68 46 11 22 0A 49 …

Search done in 72 ms
```

```bash
$ ptr89 -f EL71v45.bin -p "??2800D0F5E6704780B508F0??E980BD80B5+1" -p "??B589B006A901A80522??????????49051C"
Pattern: '??2800D0F5E6704780B508F0??E980BD80B5+1'
Found 1 address:
  ADDRESS   OFFSET    BYTES
  A0092F93  00092F93  28 00 D0 F5 E6 70 47 80 B5 08 F0 58 E9 80 BD 80 …

Pattern: '??B589B006A901A80522??????????49051C'
Found 1 address:
  ADDRESS   OFFSET    BYTES
  A05C4B39  005C4B38  F0 B5 89 B0 06 A9 01 A8 05 22 3B F7 5E FF 54 49 …

Search done in 143 ms
```

### JSON output

`-J` or `--json` returns full, untruncated bytes without `elapsed`. Search and
x-ref results intentionally use different structures:

```json
{
  "patterns": [
    {
      "pattern": "&BL(F0C8F0D9DA257293F0C8F0D9DA9A3293 + C)",
      "type": "branch",
      "results": [
        { "address": 10130226, "offset": 365140, "bytes": "DA9A3293" }
      ]
    }
  ]
}
```

```json
{
  "target": 2689376328,
  "xrefs": [
    {
      "xref": 2697946020,
      "offset": 13591460,
      "type": "branch",
      "bytes": "CCF21AE9"
    }
  ]
}
```

For a fixed address, `offset` and `bytes` are omitted. `--from-ini` returns a
`functions` array with one `result` object or `null` for each entry.

### Convert patterns.ini to swilib.vkp
```
ptr89 -f EL71v45.bin --from-ini ELKA.ini > swilib.vkp
```

# Pattern syntax

Syntax is fully compatible with WinHex, Smelter and Ghidra SRE patterns.

## Simple byte masks
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
1. Pattern `B801C4E10200A0E30000C1E5B601D4E11040BDE8??????EA` found at 0xA01A39D4
2. Result is `0xA01A39D4 + 0x20 = 0xA01A39F4`

## Decode as pointer
Decoding a pointer value from the bytes found by the pattern.

With `--arch c166`, a four-byte code pointer is decoded as a little-endian 16-bit segment offset followed by an 8-bit segment number. The padding byte is ignored.

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
1. Pattern `B801C4E10200A0E30000C1E5B601D4E11040BDE8??????EA+20` found at 0xA01A39F4
2. Decoding bytes as pointer at 0xA01A39F4:
	```asm
	A01A39F4: B8 37 D8 A8 ; 0xA8D837B8
	```
3. Result is `0xA8D837B8 + 0x2 = 0xA8D837BA`

## Decode as reference
Emulating ARM/THUMB `LDR Rd, [PC, #offset]` instruction found by the pattern.

The `&(...)` operator remains ARM-specific. C166 register-pair references are supported by the `LDR{...}` nested form described below.

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
4. Result is `0xA8E69710 + 0x4 = 0xA8E69714`

## Decode as BL address
Emulating ARM/THUMB `B/BL/BLX` or `LDR PC, [PC, #offset]` and C166 branch instructions found by the pattern.

Syntax:
```bash
&BL( subPattern )
&BL( subPattern ) + valueCorrector
&BL( subPattern ) - valueCorrector
```

For example:
```bash
&BL( ?? ?? ?? [1111101.] 00 00 5? E3 ?? ?? 9F 05 08 40 B? 08 ?? ?? ?? 0A 08 80 B? E8 )
```

Steps:
1. Pattern `?? ?? ?? [1111101.] 00 00 5? E3 ?? ?? 9F 05 08 40 B? 08 ?? ?? ?? 0A 08 80 B? E8` found at 0xA06A09A4
2. Emulating BL on 0xA06A09A4:
	```asm
	A06A09A4: 7B D6 E7 FA      ; BLX #0xA0096398
	```

3. Result is `0xA0096398 | 1 = 0xA0096399`

## Nested patterns for branches
Follow the branch and checking it for a pattern.

Syntax:
```bash
# ARM B/BL/BLX, THUMB BL/BLX, or C166 CALLA/CALLS/PCALL/JMPA/JMPS/JB/JNB/JBC/JNBS
{ subPattern }
_BLF(subPattern) # alias for { }

# THUMB B or a 2-byte C166 CALLR/JMPR
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

# C166 (4 bytes)
CALLA cc, caddr
CALLS seg, caddr
PCALL reg, caddr
JMPA cc, caddr
JMPS seg, caddr
JB bitaddr, rel
JNB bitaddr, rel
JBC bitaddr, rel
JNBS bitaddr, rel

# C166 (2 bytes)
CALLR rel
JMPR cc, rel
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
# ARM LDR or the first MOV of a C166 pointer pair (4-byte instruction)
LDR{ subPattern }

# THUMB LDR (2-byte instruction; not supported for C166)
LDR[ subPattern ]
```

Supported instructions:
```
# ARM/THUMB
LDR Rd, [PC, #offset]

# C166 large model: huge/code pointer
MOV Rn, #SOF(target)
MOV Rn+1, #SEG(target)

# C166 large model: far data pointer
MOV Rn, #POF(target)
MOV Rn+1, #PAG(target)
```

The two C166 `MOV` instructions must be adjacent and `Rn` must be even. `LDR{...}` occupies the first four-byte `MOV`; the following `SEG`/`PAG` load is inspected to reconstruct the linear address. When the raw operands are valid both as a far data pointer and as a huge/code pointer, the nested pattern is checked at both addresses.

ARM:
```bash
LDR{ 436f70797269676874204d47432032303034 } 1e ff 2f e1
```

C166:
```bash
# M55 at 0x207680:
# E6 FC 8C 75  MOV R12, #0x758C = SOF(0x20758C)
# E6 FD 20 00  MOV R13, #0x20   = SEG(0x20758C)
# Target at 0x20758C:
# E6 00 07 00  MOV DPP0, #7
LDR{ E6000700 } E6FD2000
```

Steps for ARM:
1. Pattern `?? ?? ?? ??    1e ff 2f e1` found at 0xA00A0B1C
2. Emulating LDR at 0xA00A0B1C (+0)
   ```asm
   A00A0B1C: 00 00 9F E5  LDR R0, [PC, #+0x0] ; 0xA00A0B24
   ; Emulation: PC + 0x0 = 0xA00A0B24
   ```
3. Decoding pointer at 0xA00A0B24
	```asm
	A00A0B24: 1D 34 0A A0 ; 0xA00A341D
	```
4. Checking pattern `436f70797269676874204d47432032303034` at 0xA00A341D
	```asm
 	A00A341D: ds "Copyright MGC 2004 - Nucleus PLUS - Integrator RVCT v. 1.15"
 	```
5. Result is: `0xA00A0B1C`

## Automatic THUMB bit
If the found address points to a THUMB `PUSH { ... }` instruction, +1 will be added to the result.

Example:
```
F0B5061C0C1C151C85B068461122??49??????????E0207869466A460009085C307021780134
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

## Fixed address
Usually used in `patterns.ini` for entries whose address is already known.
```bash
# Address is 0xA8000000
< A8000000 >
```
