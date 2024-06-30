# Ptr89

There is yet another ARM/THUMB pattern finder.

Main features:
- Compatible with Smelter patterns syntax.
- Compatible with Ghidra SRE patterns syntax.
- Enhanced patterns syntax:
	- Nested patterns for LDR.
	- Half-byte patterns.
	- Bitmask patterns.

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
1. Pattern `B801C4E10200A0E30000C1E5B601D4E11040BDE8??????EA` was found at 0xA009B780
2. Result is `0xA009B780 + 0x20 = 0xA009B7A0`

### Decode as pointer
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
1. Pattern `B801C4E10200A0E30000C1E5B601D4E11040BDE8??????EA+20` was found at 0xA009B7A0
2. Decoding bytes as pointer at 0xA009B7A0:
	```asm
	A009B7A0: CC 5B D9 A8 ; 0xA8D95BCC
	```
3. Result is `0xA8D95BCC + 0x2 = 0xA8D95BCE`

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
	```asm
	A093BA58: 37 48           ; ldr r0, [pc, #0xdc]
	; Emulation: PC + 0xDC = 0xA093BB38
	```
3. Decoding pointer at 0xA093BB38
	```asm
	A093BB38: 10 97 E6 A8 ; 0xA8E69710
	```

3. Result is `0xA8E69710 + 0x4 = 0xA8E69714`

### Nested patterns for branches
Follow the branch and checking it for a pattern.

Syntax:
```bash
# ARM B/BL/BLX or THUMB BL/BLX (4 bytes instruction)
{ subPattern }

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
1. Pattern `?? 1C ?? 48 ?? B5 ?? 68    ?? ?? ?? ??    BD + 0x1` was found at 0xA0978822
2. Emulating BL at 0xA097882A (+8)
	```asm
	A097882A: 9E F0 F6 FD ; BL #0xA0A1741A
	```
3. Checking pattern `?? 1C ?? 68 ?? 68 ?? 2B ?? D0 ?? 68     ?? ??     47` at 0xA0A1741A
4. Emulating BL at 0xA0A17426 (+12)
	```asm
	A0A17426: F6 E7 ; B #0xA0A17416
	```
5. Checking pattern `?? 23    ?? ??` at 0xA0A1741A
6. Emulating BL at 0xA0A17418 (+2)
	```asm
	A0A17418: D7 E7 ; B #0xA0A173CA
	```
7. Checking pattern `?? B5 ?? 1C ?? 6E` at 0xA0A173CA
8. Pattern result is `0xA0978822 + 0x1 = 0xA0978823`

### Nested patterns for references
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
00 [00101...] ?? D1 LDR [ 7F 16 65 62 73 64 E3 ] ?? [0001110.] ?? [00100...] ?? [111100..] ?? [11.0....] ?? [00101...] ?? D0 02 DF ?? [0001110.] ?? [0001110.] ?? [00100...] 01 20 ?? [111100..] ?? [11.0....] ?? [11100...] 03 DF ?? [0001110.] ?? [0001110.] ?? [00100...] 01 20 ?? [111100..] ?? [11.0....]
```

Steps:
1. Pattern `00 [00101...] ?? D1     ?? ??      ?? [0001110.] ?? [00100...] ?? [111100..] ?? [11.0....] ?? [00101...] ?? D0 02 DF ?? [0001110.] ?? [0001110.] ?? [00100...] 01 20 ?? [111100..] ?? [11.0....] ?? [11100...] 03 DF ?? [0001110.] ?? [0001110.] ?? [00100...] 01 20 ?? [111100..] ?? [11.0....]` was found at 0xA0A63270
3. Emulating LDR at 0xA0A63274 (+4)
   ```asm
   A0A63274: 6F 4C ; LDR R4, [PC, #0x1BC]
   ; Emulation: PC + 0x1BC = 0xA0A63434
   ```
4. Decoding pointer at 0xA0A63434
	```asm
	A0A63434: D6 99 C6 A0 ; 0xA0C699D6
	```
5. Checking pattern `7F 16 65 62 73 64 E3` at 0xA0C699D6
6. Result is: `0xA0A63270`

### Stub value
Usually used in `patterns.ini` for stub entries.
```bash
# Pattern result is 0xA8000000
< A8000000 >
```
