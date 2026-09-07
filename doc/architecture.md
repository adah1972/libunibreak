libunibreak Implementation Architecture
=======================================

This document describes how libunibreak is implemented.  It is aimed at
maintainers and contributors who want to understand the code, fix bugs,
or add support for new rules or Unicode versions.  It complements the
Doxygen comments in the source (which document individual functions and
data structures) by describing the overall structure and the mapping
between Unicode rules and code.

The library implements three Unicode segmentation algorithms:

- **Line breaking** (UAX #14), the most complex of the three
- **Word breaking** (UAX #29)
- **Grapheme cluster breaking** (UAX #29)

All three share the same overall design: no dynamic allocation, output
written to a caller-provided buffer, and a UTF-8/16/32 "triad" of public
entry points that delegate to one generic internal function.

---

## 1. Overview

### 1.1 Public API shape

Each module exposes `set_<mod>breaks_utf8/16/32` entry points plus an
`init_<mod>` no-op.  Line breaking additionally exposes
`set_linebreaks_utf8/16_per_code_point` and `is_line_breakable`.

All entry points forward to a single generic function that receives the
string as `const void *` plus a function pointer:

```c
typedef utf32_t (*get_next_char_t)(const void *, size_t, size_t *);
```

The `get_next_char_t` implementations (`ub_get_next_char_utf8`,
`ub_get_next_char_utf16`, `ub_get_next_char_utf32`) decode one code
point and advance the index.  The generic core (`set_linebreaks`,
`set_wordbreaks`, `set_graphemebreaks`) is therefore encoding-agnostic.

The installed (public) headers are `unibreakbase.h`, `unibreakdef.h`,
`linebreak.h`, `wordbreak.h`, and `graphemebreak.h`.  Other header files
are internal and are not installed.

### 1.2 Output modes

Line breaking supports two output modes, selected by
`enum BreakOutputType`:

- `LBOT_PER_CODE_UNIT` — one result per UTF-8/16 code unit; multi-unit
  code points are marked `LINEBREAK_INSIDEACHAR`.
- `LBOT_PER_CODE_POINT` — one result per code point.

Word and grapheme breaking always work per code unit, filling the
in-between code units of a multi-unit code point with
`WORDBREAK_INSIDEACHAR` / `GRAPHEMEBREAK_INSIDEACHAR`.

### 1.3 Property lookup

Characters are classified by consulting generated range tables
(`static const struct { utf32_t start, end; enum X prop; }[]` arrays)
through `ub_bsearch`, a binary search over those sorted ranges.  The BMP
line-breaking classes use a flat O(1) array (`lb_prop_bmp`) instead, as
an optimization.

---

## 2. Shared infrastructure

### 2.1 `unibreakbase.h` / `unibreakbase.c`

Defines the UTF types (`utf8_t`, `utf16_t`, `utf32_t`), the library
version (`UNIBREAK_VERSION`, `unibreak_version`), and
`UNIBREAK_UTF_TYPES_DEFINED` so that embedding applications can share
the typedefs.

### 2.2 `unibreakdef.h` / `unibreakdef.c`

Private shared definitions:

- `EOS` (`0xFFFFFFFF`) — end-of-string sentinel.
- `get_next_char_t` and the three `ub_get_next_char_utf*` decoders.
- `ub_bsearch` — generic binary search over `{start, end, ...}` range
  tables.
- `ARRAY_LEN(x)`.
- A `bool` fallback for MSVC older than VS2013.

### 2.3 Generated `*data.c` files

Each module's generated data file defines `static const` arrays and is
`#include`d directly into the module's `.c` file (it is not compiled
separately):

| File                     | Defines                              | Included by      |
|--------------------------|--------------------------------------|------------------|
| `linebreakdata.c`        | `lb_prop_bmp`, `lb_prop_supplementary` | `linebreak.c`  |
| `linebreakauxdata.c`     | `lb_prop_pi_qu`, `lb_prop_pf_qu`, `lb_prop_sa_cm`, `lb_prop_potential_emoji` | `linebreak.c` |
| `wordbreakdata.c`        | `wb_prop_default`                    | `wordbreak.c`    |
| `graphemebreakdata.c`    | `gb_prop_default`                    | `graphemebreak.c` |
| `indicconjunctbreakdata.c` | `incb_prop`                        | `graphemebreak.c` |
| `eastasianwidthdata.c`   | `eaw_prop`                           | `eastasianwidthdef.c` |
| `emojidata.c`            | `ep_prop`                            | `emojidef.c`     |

### 2.4 Auxiliary property lookups

Some line-breaking rules need properties beyond the Line_Break property:

- `ub_get_char_eaw_class` / `ub_is_op_east_asian` (East Asian Width, in
  `eastasianwidthdef.c`).
- `ub_is_extended_pictographic` (`emojidef.c`).

---

## 3. Data pipeline

Generated data is produced by Python scripts in `src/` and wired into
`src/Makefile.am`.  Each `make <name>` target downloads (via `wget`) a
Unicode data file and runs the corresponding generator:

| Target                    | Generator                        | Source file(s)                                            |
|---------------------------|----------------------------------|-----------------------------------------------------------|
| `linebreakdata`           | `generate_linebreakdata.py`      | `LineBreak.txt`                                           |
| `linebreakauxdata`        | `generate_linebreak_aux.py`      | `DerivedGeneralCategory.txt`, `LineBreak.txt`, `emoji-data.txt` |
| `wordbreakdata`           | `generate_word_break.py`         | `WordBreakProperty.txt`                                   |
| `graphemebreakdata`       | `generate_grapheme_break.py`     | `GraphemeBreakProperty.txt`                               |
| `eastasianwidthdata`      | `generate_east_asian_width.py`   | `EastAsianWidth.txt`, `LineBreak.txt`                     |
| `indicconjunctbreakdata`  | `generate_indic_conjunct_break.py` | `DerivedCoreProperties.txt`                             |
| `emojidata`               | `generate_extended_pictographic.py` | `emoji-data.txt`                                       |
| `update-test`             | (none)                           | `LineBreakTest.txt`, `WordBreakTest.txt`, `GraphemeBreakTest.txt` |

`unicode_data_property.py` is shared helper code.

`linebreakauxdata.c` is special: it holds tables derived from
General_Category that several rules need:

- `QU ∩ Pi` / `QU ∩ Pf` (LB15a/15b, LB19/19a quotation handling).
- `SA ∩ (Mn | Mc)` (LB1: SA resolves to CM for combining marks).
- `Extended_Pictographic ∩ Cn` (LB30b "potential emoji").

### 3.1 Fast-path thresholds

Two functions early-out on a codepoint threshold to avoid a binary
search for the common case:

- `is_east_asian` returns `false` for `ch < 0x1100`.
- `is_potential_emoji` returns `false` for `ch < 0x1F000`.

These thresholds are derived from the generated data and **must be
re-checked whenever the data is regenerated** (the comments in
`linebreak.c` note this).

---

## 4. Line breaking (UAX #14)

Line breaking is the most involved module.  Unlike word/grapheme
breaking, which are pure state machines, line breaking combines a
**pair table** (an encoding of the pair-level rules), a set of
**explicit post-table rules**, and a **lookahead/fixup mechanism** for
rules that need to see one character ahead.

### 4.1 Data structures

#### `enum LineBreakClass` (`linebreakdef.h`)

All line break classes.  The values up to and including `LBP_CB` are the
classes that participate in the pair table; the rest (`AI`, `BK`, `CJ`,
`CR`, `LF`, `NL`, `SA`, `SG`, `SP`, `XX`) are handled outside it.

#### `struct LineBreakContext` (`linebreakdef.h`)

State carried across characters, in two groups:

- **Hot state** (touched on every character): the property pointer
  `lbpLang`, `posLast`, the current/next/last classes (`lbcCur`,
  `lbcNew`, `lbcLast`), the LB25 state `eLb25`, the pending-rule tag
  `ePending`, the RI counter `cLb30aRI`, and a set of `bool` flags
  (`fLb8aZwj`, `fLb21aHebrew`, `fLb20aWordInit`, `fLb28aPrevAksara`,
  `fLb28aAkVi`, plus quotation/EA flags `fQuPiInitial`, `fPrevQuPi`,
  `fPrevQuPf`, `fPrevEA`, `fQuPrevEA`, `fPrevPotentialEmoji`, and the
  language flags `fLangCjk`/`fLangStrict`).
- **Lookahead-fixup state** (rarely used): `posPending`,
  `cPendingOrigBrk`, `fPendingRevert`, and the LB25 fixup pair
  `posLb25Fixup`/`fLb25Mark`.

#### `enum Lb25State` / `enum PendingRule`

`Lb25State` is the LB25 numeric-expression state machine.  `PendingRule`
identifies which lookahead rule currently has a tentative break pending
(see §4.4).

### 4.2 Processing flow

```
set_linebreaks(s, len, lang, outputType, brks, get_next_char)
    lb_init_break_context(&ctx, firstCh, lang)
    loop over remaining chars:
        ctx.posLast = posLast
        brks[posLast] = lb_process_next_char(&ctx, ch)
        apply LB25 fixup  (posLb25Fixup/fLb25Mark -> NOBREAK)
        apply pending revert (fPendingRevert -> cPendingOrigBrk)
    revert any unresolved pending break
    write the final break (MUSTBREAK or INDETERMINATE)
```

`lb_process_next_char` does, per character:

1. **LB9/LB10** — fold combining marks/ZWJ into `lbcLast`.
2. Resolve the class of the new character (`resolve_lb_class`).
3. `resolve_pending_break` — resolve the lookahead rule that was pending
   from the previous character.
4. `get_lb_result_simple` — the mandatory rules (early exit).
5. `get_lb_result_lookup` — the pair table + explicit rules (for the
   `LINEBREAK_UNDEFINED` case).
6. Update the quotation/ZWJ/EA state flags.

`get_lb_result_lookup` is split into three parts:

1. **Pair table** — `baTable[lbcCur - 1][lbcNew - 1]`, with a special
   early return for combining marks (`CMI_BRK`/`CMP_BRK` when
   `lbcLast != SP`).
2. **`get_lb_result_decision`** — the explicit rules, applied in
   ascending UAX #14 order with early returns (first match wins).
3. **`update_lb_state`** — unconditional per-character state updates.

The last thing `get_lb_result_lookup` does is `lbcCur = lbcNew`.

### 4.3 Rule-by-function map

This table maps each UAX #14 rule (or sub-rule) to where it is
implemented.  "Pair table" means the rule is encoded in `baTable`
(see the comment above `baTable` for the manual adjustments).

| Rule  | Description (abbreviated) | Implementation |
|-------|---------------------------|----------------|
| LB1   | SA → CM (GC Mn/Mc) or AL | `resolve_lb_class` via `is_sa_cm` |
| LB2   | `sot ÷` | implicit in `lb_init_break_context`/`treat_first_char` |
| LB3   | `! eot` | implicit in `set_linebreaks` end-of-string |
| LB4   | `BK !` | `get_lb_result_simple` |
| LB5   | `CR × LF`, `CR !`, `LF !`, `NL !` | `get_lb_result_simple` (+ `treat_first_char`) |
| LB6   | `× (BK\|CR\|LF\|NL)` | `get_lb_result_simple` |
| LB7   | `× SP`, `× ZW` | SP in `get_lb_result_simple`; ZW in pair table |
| LB8   | `ZW SP* ÷` | pair table |
| LB8a  | `ZWJ ×` | decision via `fLb8aZwj` (set in `lb_process_next_char`) |
| LB9   | `× (CM\|ZWJ)` | `lb_process_next_char` |
| LB10  | treat remaining CM/ZWJ as AL | `lb_process_next_char` |
| LB11  | `× WJ`, `WJ ×` | pair table |
| LB12  | `GL ×` | pair table |
| LB12a | `[^SP BA HY HH] × GL` | pair table |
| LB13  | `× (CL\|CP\|EX\|IS\|SY)` | pair table |
| LB14  | `OP SP* ×` | pair table |
| LB15  | `QU SP* × OP` | pair table |
| LB15a | no break after initial Pi&QU | decision via `fQuPiInitial` |
| LB15b | no break before final Pf&QU | lookahead `PENDING_LB15B` + `is_lb15b_word_like` |
| LB15c | `SP ÷ IS NU` | lookahead `PENDING_LB15C` |
| LB15d | `× IS` | pair table |
| LB16  | `(CL\|CP) SP* × NS` | pair table |
| LB17  | `B2 SP* × B2` | pair table |
| LB18  | `SP ÷` | pair table |
| LB19  | `× QU`, `QU ×` | pair table |
| LB19a | EA quotation breaks | decision (after Pf&QU) + lookahead `PENDING_LB19A` (before Pi&QU) |
| LB20  | `÷ CB`, `CB ÷` | pair table (CB added manually) |
| LB20a | no break after word-initial hyphen | decision via `fLb20aWordInit` |
| LB21  | `× (BA\|HY\|NS\|HH)`, `BB ×` | pair table |
| LB21a | `HL (HY\|HH) × [^HL]` | decision via `fLb21aHebrew` |
| LB21b | `SY × HL` | pair table |
| LB22  | `IN` rules | pair table |
| LB23  | `(AL\|HL) × NU`, `NU × (AL\|HL)`, `× (AL\|HL\|NU)` | pair table |
| LB23a | `PR × (ID\|EB\|EM)`, `(ID\|EB\|EM) × PO` | pair table |
| LB24  | `PR ×`, `PO ×` | pair table |
| LB25  | numeric expressions | decision (state-machine effects) + `lb25_transition` + `posLb25Fixup`/`fLb25Mark` |
| LB26  | Hangul JL/JV/JT/H2/H3 | pair table |
| LB27  | Hangul × PO/PR, PR × Hangul | pair table |
| LB28  | `(AL\|HL) × (AL\|HL)` | pair table |
| LB28a | Brahmic syllables | see below |
| LB29  | `IS × (AL\|HL)` | pair table |
| LB30  | `(AL\|HL\|NU) × [OP non-EA]`, `CP × (AL\|HL\|NU)` | decision |
| LB30a | RI pairing | decision via `cLb30aRI` |
| LB30b | `EB × EM`, potential-emoji `× EM` | pair table (EB×EM) + decision via `fPrevPotentialEmoji` |
| LB31  | `÷` everywhere else | default (final pair-table result) |

There is also a non-UAX "programmer" tailoring in the decision function:
no break between `++` and between `-` and `` ` `` (for "C++" and some
code expressions).

**LB28a** is implemented entirely in code, not in the pair table, because
it must treat U+25CC DOTTED CIRCLE as aksara-like even though its
Line_Break class is AL (so it cannot appear in the pair table):

- Sub-rules 1–3 (`AP × (AK|◌|AS)`, `(AK|◌|AS) × (VF|VI)`,
  `(AK|◌|AS) VI × (AK|◌)`) are early returns in
  `get_lb_result_decision`, using `lb28a_is_aksara` and the
  `fLb28aPrevAksara`/`fLb28aAkVi` flags (set in `update_lb_state`).
- Sub-rule 4 (`(AK|◌|AS) × (AK|◌|AS) VF`) needs one-character lookahead,
  so it arms `PENDING_LB28A4`.

The `baTable` still holds the new AK/AP/AS/VF/VI/HH classes, but only
with their base `IND_BRK`/`DIR_BRK` values; LB28a's no-break decisions
are all made in code.  U+25CC DOTTED CIRCLE is handled purely by the
inline `lb28a_is_aksara` check.

### 4.4 LB25 (numeric expressions) in detail

LB25 implements the "Example 7" tailoring for numeric expressions:

    (PR | PO) × (OP | HY)? NU
    (OP | HY) × NU
    NU × (NU | SY | IS)
    NU (NU | SY | IS)* × (NU | SY | IS | CL | CP)
    NU (NU | SY | IS)* (CL | CP)? × (PO | PR)

These replace the simple pair-based LB25 of earlier UAX #14 revisions.

**State machine.**  `enum Lb25State` tracks the left side of the next
break:

| State           | Meaning                       |
|-----------------|-------------------------------|
| `LB25_NONE`     | not in a numeric expression   |
| `LB25_PREFIX`   | saw PR or PO                  |
| `LB25_PREFIXOP` | saw (PR\|PO) then OP or HY    |
| `LB25_NUM`      | inside NU (NU\|SY\|IS)*       |
| `LB25_NUMCLOSE` | saw NU (NU\|SY\|IS)* (CL\|CP) |

A dedicated state for a standalone OP/HY (not preceded by PR/PO) is not
needed: `OP × NU` is already `PRH_BRK` (LB14) and `HY × NU` is `IND_BRK`,
so neither needs an override.

The transition is a pure function (`lb25_transition`); its break effect
and fixup arming are applied in `get_lb_result_decision`:

| State    | Incoming `lbcNew` | New state | Effect                |
|----------|-------------------|-----------|-----------------------|
| NONE     | PR, PO            | PREFIX    | —                     |
| NONE     | NU                | NUM       | —                     |
| PREFIX   | OP, HY            | PREFIXOP  | record `posLb25Fixup` |
| PREFIX   | NU                | NUM       | —                     |
| PREFIXOP | NU                | NUM       | set `fLb25Mark`       |
| PREFIXOP | (else)            | (restart) | clear `posLb25Fixup`  |
| NUM      | NU, SY, IS        | NUM       | `brk = NOBREAK`       |
| NUM      | CL, CP            | NUMCLOSE  | —                     |
| NUM      | PO, PR            | PREFIX    | `brk = NOBREAK`       |
| NUMCLOSE | PO, PR            | PREFIX    | `brk = NOBREAK`       |
| (any)    | (else)            | (restart) | —                     |

"(restart)" means re-check for a new start: PR/PO → PREFIX, NU → NUM,
otherwise NONE.

**Pair-table override.**  LB25 does not change `baTable`; it overrides
the result at runtime.  A small bitmap (or an equivalent explicit
condition, selected by the `UB_LB25_OPT_HACK` optimization) forces
`ALLOWBREAK` for the pairs that require extended NU context, after which
the state machine may restore them to `NOBREAK`:

| Pair                       | Table   | Override                             |
|----------------------------|---------|--------------------------------------|
| CL×PR, CL×PO, CP×PR, CP×PO | IND_BRK | ALLOWBREAK (needs NU..CL/CP context) |
| PR×OP, PO×OP               | IND_BRK | ALLOWBREAK (needs NU right context)  |
| SY×NU                      | IND_BRK | ALLOWBREAK (needs NU left context)   |

All other relevant pairs keep their table value; the state machine only
*strengthens* the result to `NOBREAK`, never weakens it to `ALLOWBREAK`.

**Fixup.**  Only the `(PR|PO) × (OP|HY) NU` sub-rule needs a fixup.  On
PREFIX → PREFIXOP the ALLOWBREAK is written normally and the position is
recorded in `posLb25Fixup`; on PREFIXOP → NUM, `fLb25Mark` is set and
`set_linebreaks` patches that position to `NOBREAK`.  The maximum fixup
depth is one position.

### 4.5 Lookahead / fixup mechanism

Four rules need to know the *next* character before the break before the
current one can be finalized:

- LB15b, LB15c, LB19a (quotation/decimal rules), and LB28a sub-rule 4.

Because the algorithm processes characters one at a time, these rules
write a **tentative** break and arm a pending record:

- `ePending` — which rule is pending (`PENDING_LB15B/15C/19A/28A4`);
- `posPending` — the output position of the tentative break;
- `cPendingOrigBrk` — the break value to restore if the lookahead fails.

On the next character, `resolve_pending_break` inspects the new
character's class and either confirms the tentative break or sets
`fPendingRevert`.  `set_linebreaks` then applies the revert by writing
`cPendingOrigBrk` back to `brks[posPending]`.

Only one rule can be pending at a time: their trigger conditions are
mutually exclusive (Pf&QU, IS-after-SP, Pi&QU, aksara pairs).

LB25 uses a *separate* fixup pair (`posLb25Fixup`/`fLb25Mark`) because
it is entangled with the LB25 state machine and reverts to a fixed
`NOBREAK` rather than a captured original value.

### 4.6 Language-specific customization

The `lang` parameter (an ISO 639-1 code, optionally with a `-strict`
suffix) drives three things:

1. **Per-language class overrides** (`linebreakdef.c`).
   `lb_prop_lang_map` maps a language prefix to a small
   `LineBreakProperties` array that overrides the default Line_Break
   class of a few codepoints — almost all quotation marks (whose default
   class is the ambiguous `QU`).  Languages with data: `en`, `de`, `es`,
   `fr`, `ru`, and `zh`.

   For example, Chinese reclassifies U+2018/2019/201C/201D as `OP`/`CL`
   (opening/closing).  This is still needed despite the new LB19a rule:
   LB19a only disambiguates a `Pi`/`Pf` `QU` when it is *surrounded by
   East Asian characters*, so a quotation of non-EA text (e.g. English
   quoted inside Chinese) is left ambiguous without the override.

2. **CJK resolution** (`is_lang_cjk`).  For `zh`/`ja`/`ko`, `AI`
   resolves to `ID` instead of `AL`, so ambiguous-width characters act
   as ideographic in East Asian text.  (`ja` and `ko` get this treatment
   but no per-language class overrides.)

3. **Strict mode** (`resolve_lb_class`).  For `CJ` (conditional Japanese
   starter, small kana), normal mode resolves to `ID` and `-strict` to
   `NS` — the UAX #14 "Conditional Japanese Starter" tailoring.

`get_char_lb_class_lang` consults the language override first, then
falls back to the default data for codepoints not listed there.

---

## 5. Word breaking (UAX #29)

`set_wordbreaks` is a pure state machine over the input, using:

- `wbcLast` — the class of the previous character.
- `wbcSeqStart` — the class that started the current sequence (used to
  defer the decision for a run of characters, e.g. `Numeric MidNum
  Numeric`).
- `riCounter` — parity for WB15/16 (Regional Indicator pairs).

`set_brks_to` fills a range of the output with a single break type,
deferring the write until the sequence is known — this is what allows
rules like WB6/7/11/12 (which depend on what follows a mid-letter/mid-num
character) to work without lookahead.

The rules are applied in a `switch (wbcCur)` with the following mapping:

| Rules | Condition / effect |
|-------|--------------------|
| WB3c | `ZWJ × Extended_Pictographic` (checked before the switch, regardless of the pictograph's class) |
| WB3, WB3a/b | CR/LF/newline handling |
| WB3d | `WSegSpace × WSegSpace` |
| WB4 | `× (Extend\|Format\|ZWJ)` |
| WB5/6/7 | ALetter / Hebrew letter contexts |
| WB7a/b/c | single/double quote after Hebrew letter |
| WB8–12 | numeric contexts (`Numeric`, `MidNum`, `MidNumLet`, `MidLetter`, `Single_Quote`) |
| WB13/13a/13b | Katakana / ExtendNumLet |
| WB15/16 | Regional Indicator pairs |
| WB2 | `sot ÷` (final flush) |

Word breaking uses `WBP_Undefined` as the "start of text" sentinel for
`wbcSeqStart`.

---

## 6. Grapheme breaking (UAX #29)

`set_graphemebreaks` applies the extended grapheme cluster rules as an
`if`/`else if` chain (GB1/GB2 through GB999), the first match winning:

| Rules | Condition |
|-------|-----------|
| GB1/GB2 | `sot ÷`, `÷ eot` |
| GB3 | `CR × LF` |
| GB4/GB5 | control characters |
| GB6 | `L × (L\|V\|LV\|LVT)` |
| GB7 | `(LV\|V) × (V\|T)` |
| GB8 | `(LVT\|T) × T` |
| GB9 | `× (Extend\|ZWJ\|Virama)` |
| GB9a | `× SpacingMark` |
| GB9b | `Prepend ×` |
| GB9c | Indic Conjunct Break (`InCB`) |
| GB11 | `Extended_Pictographic Extend* ZWJ × Extended_Pictographic` |
| GB12/GB13 | Regional Indicator pairs |
| GB999 | `÷` (fallback) |

Three pieces of state support the rules:

- `rule11Detector` — recognizes the `ExtPict Extend* ZWJ` pattern for
  GB11 (values 0–3).
- `rule9cStage` — a small state machine (`R9C_*`) for GB9c, driven by
  the InCB class (`InCB_None`/`InCB_Linker`/`InCB_Consonant`).
- `evenRegionalIndicators` — parity for GB12/GB13.

`UNIBREAK_LAZY_INCB` (default 0) controls whether the InCB lookup is
skipped for characters that cannot be relevant.  It is disabled because
Unicode 16+ assigns `InCB=Consonant` to consonants whose
Grapheme_Cluster_Break is `Other`, which the lazy assumption would miss.

---

## 7. How to add a new rule

When a new Unicode version adds or changes a rule, decide which
mechanism it needs:

1. **Simple pair rule** — the break depends only on `lbcCur × lbcNew`.
   Encode it in `baTable` (a `PRH_BRK`/`DIR_BRK`/`IND_BRK` cell).
   Update the `baTable` comment's "manual adjustments" list.

2. **One-character lookback / a single flag** — the break depends on the
   previous character's class.  Add a `bool`/counter field to
   `LineBreakContext`, test it in `get_lb_result_decision` (early
   return), and update it in `update_lb_state`.  Examples: `fLb8aZwj`,
   `fLb21aHebrew`, `cLb30aRI`.

3. **Extended run context** — the break depends on an unbounded run of
   preceding characters.  Add a state-machine `enum` and a transition
   function, mirroring `Lb25State`/`lb25_transition`.  Example: LB25.

4. **One-character lookahead** — the break before the current character
   depends on the *next* character.  Add a `PendingRule` value, arm it
   in `get_lb_result_decision` (recording `posPending` and
   `cPendingOrigBrk`), and add the resolution logic to
   `resolve_pending_break`.  Examples: LB15b/15c/19a, LB28a sub-rule 4.

5. **New character class** — add the value to `enum LineBreakClass`
   (inside the pair-table block if it participates in the table), widen
   `baTable`, and regenerate the data.  Mind the `lbcCur <= LBP_CB`
   asserts and any ordering-dependent indexes (e.g. the LB25 `allow[]`
   optimization).

General rules:

- Keep `get_lb_result_decision` rules in **ascending UAX #14 order**
  (first match wins).  Rules with an early return (LB8a through LB30b)
  come before the one-character lookahead arms only because their
  trigger conditions are disjoint from the arms' — no single boundary
  can match both, so the early-returning rule decides and the arm never
  fires.
- One-character lookahead rules (LB15b/15c/19a, LB28a sub-rule 4) are
  armed **last**, after all other rules: their outcome depends on the
  next character, so they cannot decide when the boundary is reached.
  The arm records the value that the ordinary rules have already
  settled in `cPendingOrigBrk`, returns a tentative override, and
  `resolve_pending_break` either confirms the override or restores the
  recorded value one character later.  Placing the arms last guarantees
  that the recorded fallback reflects every ordinary rule; a
  lower-priority rule must therefore *seed* `brk` (pair table, LB25)
  rather than early-return before an arm, which would steal the
  boundary from the lookahead rule.
- State that must advance on every character goes in `update_lb_state`,
  not in the early-returning decision function.
- If the rule needs a new property, add a table to the appropriate
  `*data.c` generator and re-check the `is_east_asian` /
  `is_potential_emoji` thresholds.
- Re-run `make check`; add only immaterial skips to `test_skips.h`.

---

## 8. Test harness

`src/tests.c` is a self-contained harness (no external framework).  It
reads the official Unicode conformance files
(`LineBreakTest.txt`, `WordBreakTest.txt`, `GraphemeBreakTest.txt`),
runs the `set_*breaks_utf32` function on each test string, and compares
the result.  `make check` builds and runs it three times (once per
module).

`src/test_skips.h` lists test line numbers to skip.  The policy is to
skip a test only when its failure is immaterial (e.g. "XX treated as
AL"); since the Unicode 17.0 update the list is empty.
