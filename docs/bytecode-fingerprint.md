# Bytecode fingerprints

## Purpose

A game update can keep every class and function name the mod uses and still change what a function
does. Resolving names by string cannot see that. A fingerprint can: the mod renders a game function's
compiled Blueprint bytecode as canonical text and hashes it, so the same function on two builds of the
game compares equal when its logic is the same and differs when it is not. This page is the
specification of that text. The mod and the port tooling each render it, one from the running game and
one from the cooked assets, and the two renderings are compared function by function.

## How it works

### Reading the bytecode

A Blueprint function's script is the byte array `UStruct::Script`. The mod walks it expression by
expression with a decoder of every UE4.27 Kismet opcode, laid out as the engine keeps it in memory: an
object or property pointer takes 8 bytes and a name (`FScriptName`) 12. The operand order of each
opcode follows UAssetAPI's expression readers (MIT). A decoder that mis-sized one operand would not land
on the script's end; on both game builds it has read every function of every loaded Blueprint class to
its end with no problem `[V]` (21,021 functions on 0.9.0n, ***REMOVED*** in a world).

### The canonical text

1. **Line 1 is the signature**: `sig(` then the function's parameters (the properties flagged
   `CPF_Parm`) in declaration order, each `name:flags:type`, then `)`. The flags are letters in the
   order C (const), P (parameter), O (out), R (return), F (reference). The type is the property class
   (`objectproperty`), followed by `:<type object>` for an object, class, interface, struct, enum or
   delegate property, `<inner>` for an array or set and `<key,value>` for a map. There is no size: a
   native struct's cooked size is not its run-time one (`FHitResult` reads 144 bytes in the asset and
   136 in the game `[V]`), and the type already names the struct. A native function has no script, so
   its text is this line alone.
2. **Statements in flow order**, one per line as `<label> <expression>`. From the entry, a straight run
   is followed; an unconditional `EX_Jump` is layout glue, followed and never written. Every other code
   offset -- a conditional branch, a pushed execution flow, a latent action's resume point -- is
   deferred, last in first out, in operand order. A run that reaches a statement already written ends
   with ` >@<label>`. Labels are indices in this order, and every code offset is written `@<label>`
   (`@?` if it is not a statement). A recook that re-lays a function out without changing it therefore
   writes the same text.
3. **An expression** is `Name(operand,...)`, the opcode's name without `EX_`, or `Name` alone when it
   has no operand. Operands are `o:<object path>` (the object's outer chain without its package,
   joined by `.`), `p:<owner path>:<field chain>` for a property, `l<k>:<base>` for a frame's own
   property (rule 4), `n:<name>`, the integers `i:` (int32) `b:` (byte) `h:` (uint16) `l:` (int64) `u:`
   (uint64) `B:` (bool), `f:` and `d:` floats as their bits in hex, `s:"..."` a string, `t:<kind>` a
   text literal's kind, `k:<conversion>` a primitive cast's, and `[...]` a list. A null object or
   property is `o:-` or `p:-`. Dropped, as derived from the layout: `EX_Context`'s skip offset,
   `EX_Skip`'s offset, `EX_SwitchValue`'s goto offsets and the element counts of container constants.
4. **A frame's own property** -- a parameter, or a slot the compiler made (`CallFunc_*`, `K2Node_*`,
   `Temp_*`) -- is written `l<k>:<base>`: `k` by first appearance in the whole text, `base` its name
   with every `_<digits>` part taken out. The compiler numbers its slots across a whole event graph and
   a plugin node numbers inside the name (`K2Node_LowEntry_LocalVariable_Value__12_Object`), so an edit
   elsewhere renumbers both.
5. **Text.** Names, paths and types are folded to printable ASCII and lowercased: a name renders in the
   casing first registered in the process, which the game cannot know from the asset. A string keeps
   its case; any character outside printable ASCII becomes `?`, one per UTF-16 unit.
6. **Events.** An event function is a stub that calls its class's event graph
   (`ExecuteUbergraph_<class>`) at an entry offset. Its text writes that entry as `IntConst(@U)` and
   continues with `== <event graph path> ==` and the event graph's statements in flow order from that
   entry, labels restarting, frame numbering shared with the stub. An `ExecuteUbergraph_*` function
   itself is written as `== entry <stub> ==` and its flow from each stub's entry, the stubs of its class
   in name order, frame numbering restarting for each.
7. **Fingerprints** are FNV-1a 64 over the UTF-8 text: one over the signature line (a change means a
   hook reads wrong parameters) and one over the rest (a change means the logic drifted).

### Agreement

The text is rendered twice: by the mod from the running game, and by the port tooling from the cooked
assets (UAssetAPI's expression tree). A difference between the two is a defect in one of the readers.
Over every loaded Blueprint function both builds agree `[V]`: 20,678 identical on 0.9.0n (three differ,
all one class whose name two packages carry, the tooling having read the other package) and 22,331 on
***REMOVED***.

### The capture

`[dev] fingerprint_capture` writes, once gameplay is loaded, every function of every loaded Blueprint
class with its fingerprint and text to `multivoid-fingerprints-capture.txt` beside the game's exe:
`### <path> <fingerprint> <ok|native|problem>` then the text. It is about a second of game-thread work.

## Who owns what

`ue_wrap/core/script_bytecode` decodes; `ue_wrap/core/script_fingerprint` orders, renders the signature
and hashes; `harness/fingerprint_census` runs the capture once gameplay is loaded. All of it reads; none
of it changes the game.

## Wire messages

None: a fingerprint is a local check of the game the mod runs on.

## Late join

Not applicable: every peer checks its own game when its world loads.

## Known limits

- Two statements that swap places inside one straight run read as a change although nothing depends on
  ***REMOVED***
  It errs toward reporting.
- A native function is compared by its signature; its body is not readable.
- Only loaded classes are read in the game; the port tooling reads every class of the pak.

## Code map

| File | What |
|---|---|
| `src/votv-coop/src/ue_wrap/core/script_bytecode.cpp` | the decoder: the opcode table, one expression scanned or rendered, a script's statements |
| `src/votv-coop/src/ue_wrap/core/script_fingerprint.cpp` | flow order, the signature line, the fingerprints |
| `src/votv-coop/src/harness/fingerprint_census.cpp` | the capture |
