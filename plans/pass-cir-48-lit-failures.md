# Plan: Pass The 48 CIR CodeGen/IR Lit Failures

## Objective

Make this gate pass in `/Users/revofusion/Projects/llvm`:

```bash
ninja -C build clang split-file FileCheck
build/bin/llvm-lit -s clang/test/CIR/CodeGen clang/test/CIR/IR
```

Current evidence after building `split-file`:

- Command: `build/bin/llvm-lit -sv clang/test/CIR/CodeGen clang/test/CIR/IR > /tmp/cir48-lit-after-splitfile-verbose.log 2>&1`
- Result: 258 discovered, 210 passed, 48 failed.
- Scope: all 48 failures are in `clang/test/CIR/CodeGen`; `clang/test/CIR/IR` is clean in this slice.
- No assertion failures, verifier failures, crashes, or `split-file` lookup failures remain.

## Non-Negotiable Decisions

1. Do production fixes before any mass FileCheck updates. Source fixes can change output and invalidate refreshed checks.
2. Do not bless FileCheck drift until the emitted CIR/LLVM is compared against the test intent and, where applicable, OGCG output.
3. `nonzeroinit-struct.cpp` is not a stale negative test. Current CIR emits `#cir.zero` for a data member pointer record, while OGCG emits `i64 -1`; that is unsound for Itanium data member pointers.
4. Bitfield constant fixes must preserve byte-exact initialized storage and then return the destination memory type accepted by `convertTypeForMem`.
5. `forward-decls.cpp` needs an explicit semantic decision before checks are updated: either incomplete recursive record references are the intended canonical CIR, or record completion is broken.

## Phase 0: Reproduce Cleanly

Run:

```bash
cd /Users/revofusion/Projects/llvm
ninja -C build clang split-file FileCheck
build/bin/llvm-lit -sv clang/test/CIR/CodeGen clang/test/CIR/IR
```

Do not inspect or edit generated files under `build/tools/clang/test/.../Output` except as debugging evidence. Test edits belong under `clang/test/CIR/CodeGen`.

## Phase 1: Restore Member-Pointer Zero-Initialization Soundness

Files to inspect and likely edit:

- `clang/lib/CIR/CodeGen/CIRGenTypes.cpp`
- `clang/lib/CIR/CodeGen/CIRGenCXXABI.h`
- `clang/lib/CIR/CodeGen/CIRGenCXXABI.cpp`
- `clang/lib/CIR/CodeGen/CIRGenItaniumCXXABI.cpp`
- `clang/lib/CIR/CodeGen/CIRGenExprConstant.cpp`
- `clang/test/CIR/CodeGen/nonzeroinit-struct.cpp`

Invariant: CIR must not treat Itanium data member pointers as zero-initializable. A null data member pointer is `-1`, matching OGCG.

Patch shape:

- Add ABI-owned zero-initializable knowledge for member pointers, mirroring classic `CodeGenTypes::isZeroInitializable(QualType)` and `ItaniumCXXABI::isZeroInitializable(const MemberPointerType *)`.
- Make `CIRGenTypes::isZeroInitializable(QualType)` ask the CIR C++ ABI for `MemberPointerType`.
- Then either:
  - implement correct null member-pointer constant emission for this global initializer, or
  - keep/fail with the existing precise NYI until correct member-pointer constants are implemented.

Acceptance for this phase:

```bash
build/bin/llvm-lit -sv clang/test/CIR/CodeGen/nonzeroinit-struct.cpp
```

The test may pass as a positive test only if CIR represents the null data member pointer correctly, not as `#cir.zero`.

## Phase 2: Fix Bitfield Constant Memory Retargeting

Files to inspect and likely edit:

- `clang/lib/CIR/CodeGen/CIRGenExprConstant.cpp`
- `clang/lib/CIR/CodeGen/CIRGenRecordLayoutBuilder.cpp`
- `clang/test/CIR/CodeGen/constant-inits.cpp`
- `clang/test/CIR/CodeGen/struct-init.cpp`

Failing diagnostics:

- `struct-init.cpp`: byte anonymous record to `BitfieldStruct {!u32i}`
- `constant-inits.cpp`: byte anonymous records to named/padded bitfield storage records

Invariant: `ConstantEmitter::emitForMemory` may retarget same-size aggregate constants only when layout, byte offsets, padding, and endian interpretation preserve the exact initialized memory image.

Patch shape:

- Extend `retargetAggregateConstant` / `retargetLayoutIdenticalConstant` so byte-shaped record constants can be rewrapped into a destination record with same total allocation size.
- Support destination fields that are wider integer storage units, such as four source `!u8i` bytes becoming one `!u32i`, using target endianness.
- Support destination padded forms such as `!u16i, !array<!u8i x 2>` when the source is two byte constants plus zero/undef padding.
- Reject ambiguous or overlapping non-zero source members; keep an NYI rather than guessing.

Acceptance for this phase:

```bash
build/bin/llvm-lit -sv \
  clang/test/CIR/CodeGen/constant-inits.cpp \
  clang/test/CIR/CodeGen/struct-init.cpp
```

Only update CHECK lines after generated CIR and `-emit-llvm` output match OGCG memory values.

## Phase 3: Decide And Fix Recursive Forward-Decl Output

File:

- `clang/test/CIR/CodeGen/forward-decls.cpp`

Current state after `split-file` is built:

- The helper runs.
- `CHECK1` through `CHECK4` pass.
- `CHECK5` fails because output starts with `!rec_A = !cir.record<struct "A" incomplete>` instead of the old fully expanded recursive type expectations.

Decision needed before edit:

- If incomplete forward records are now the canonical CIR shape for complex recursive graphs, refresh `CHECK5` to assert the incomplete handle plus the function body semantics.
- If complete recursive type propagation is still required, fix the producer in record type completion, not the test.

Acceptance:

```bash
build/bin/llvm-lit -sv clang/test/CIR/CodeGen/forward-decls.cpp
```

## Phase 4: Per-File FileCheck Refresh With Semantic Review

After Phases 1-3, rerun the 48-test gate and update this table. Each remaining FileCheck failure needs a short local decision: semantic regression, intended output drift, or test brittleness.

| File | Initial bucket | Required action |
| --- | --- | --- |
| `address-space.c` | Signature/arg spelling drift | Refresh checks only after address-space types match intent. |
| `agg-expr-lvalue.c` | Aggregate/string init shape drift | Verify element stores versus constant aggregate expectation. |
| `array.cpp` | FileCheck drift | Compare CIR array init/access semantics before refresh. |
| `assign-operator.cpp` | C++ expression drift | Verify assignment operator call/copy semantics before refresh. |
| `basic.c` | Signature/return-value drift | Avoid brittle `%argN`; verify simple function semantics. |
| `cast.cpp` | Cast spelling drift | Confirm cast kind and source/destination types. |
| `class.cpp` | C++ record/member access drift | Refresh arg numbering only if member access is unchanged. |
| `cleanup.cpp` | Cleanup output drift | Verify cleanup runs on the intended paths before refresh. |
| `cmp.cpp` | Label/check prefix drift | Confirm comparison ops and bool materialization. |
| `complex-mul-div.cpp` | Complex op output drift | Verify combined/element ops against LLVM output. |
| `compound_literal.cpp` | Aggregate init drift | Verify storage and lifetime semantics. |
| `constant-inits.cpp` | Production bitfield retargeting | Fix Phase 2, then refresh checks. |
| `coro-task.cpp` | Coroutine output drift | Confirm coroutine frame/lifetime semantics before refresh. |
| `count-of.c` | Expression output drift | Verify `countof` lowering and types. |
| `ctor-alias.cpp` | Alias attribute spelling/order drift | Check alias target and linkage, then refresh. |
| `ctor.cpp` | Constructor output drift | Verify constructor calls and object initialization. |
| `cxx-special-member-attr.cpp` | Special-member arg numbering/order drift | Preserve `special_member` attributes; avoid `%argN` brittleness. |
| `dtor-alias.cpp` | Alias attribute spelling/order drift | Check alias target and destructor call semantics. |
| `embed-expr.c` | Output drift | Verify embed expression payload and storage. |
| `empty-union.cpp` | LLVM output drift | Compare CIR-LLVM and OGCG for union storage. |
| `finegrain-bitfield-access.cpp` | Bitfield access check drift | Verify access-unit types and offsets. |
| `forward-decls.cpp` | Recursive type decision | Complete Phase 3 first. |
| `global-init.cpp` | Global init drift | Compare CIR constants and LLVM globals. |
| `goto.cpp` | CFG/label drift | Verify branches and labels, not exact block numbering. |
| `if.cpp` | CFG/arg numbering drift | Verify branch structure and returns. |
| `inline-attributes.cpp` | Signature/function attr drift | Preserve intended inline/no-inline attributes. |
| `kr-func-promote.c` | K&R arg spelling drift | Verify promotions and function signatures. |
| `label-values.c` | Indirect branch order drift | Verify label address list and block targets. |
| `label.c` | Label/branch drift | Verify label ops and control flow. |
| `lambda-static-invoker.cpp` | Lambda invoker drift | Verify static invoker signature and call target. |
| `lambda.cpp` | Lambda output drift | Verify captures, call operators, and conversions. |
| `loop.cpp` | CFG drift | Verify loop blocks, condition, and updates. |
| `no-prototype.c` | no-proto signature/call drift | Verify old-style function semantics and bitcasts. |
| `nonzeroinit-struct.cpp` | Soundness bug | Complete Phase 1 first. |
| `paren-init-list.cpp` | Aggregate init strategy drift | Current output stores fields instead of constant record; decide intended form. |
| `pointer-to-member-func.cpp` | Member-pointer output drift | Verify ABI representation and call path. |
| `record-zero-init-padding.c` | Padding constant drift | Verify zero padding bytes and LLVM output. |
| `size-of-vla.cpp` | VLA size expression drift | Verify size calculation and temporary storage. |
| `statement-exprs.c` | CFG/value drift | Verify GNU statement expression result path. |
| `struct-init.cpp` | Production bitfield retargeting | Fix Phase 2, then refresh checks. |
| `struct.c` | Struct access drift | Verify layout and field offsets. |
| `try-catch-tmp.cpp` | Cleanup/EH drift | Verify cleanup and exception path semantics. |
| `var_arg.c` | Variadic arg numbering drift | Verify `va_start`, `va_arg`, `va_end` operands. |
| `variable-decomposition.cpp` | Decomposition output drift | Verify bindings and storage. |
| `vector-ext.cpp` | Vector op/check drift | Verify ext-vector element ops. |
| `vector.cpp` | Vector op/check drift | Verify vector constants, loads, stores, and casts. |
| `virtual-function-calls.cpp` | Vtable/call drift | Verify virtual dispatch path and this adjustments. |
| `volatile.cpp` | Volatile op drift | Verify volatile loads/stores are preserved. |

FileCheck policy:

- Prefer regex captures for SSA values and `%argN`.
- Keep checks specific for semantic facts: op kind, type, linkage, attributes, record layout, branch target shape, cleanup order.
- Do not remove a check unless its asserted behavior is no longer part of the intended CIR contract and the replacement checks the new contract.

## Phase 5: Verification Ladder

After each production phase:

```bash
ninja -C build clang
build/bin/llvm-lit -sv <focused failing tests>
```

Before any final commit:

```bash
ninja -C build clang split-file FileCheck
build/bin/llvm-lit -s clang/test/CIR/CodeGen clang/test/CIR/IR
build/bin/llvm-lit -s clang/test/CIR
python3 /tmp/broad.py base/
```

Required pass condition for the user's 48-failure gate:

- `clang/test/CIR/CodeGen clang/test/CIR/IR`: 258 tests, 258 passed, 0 failed.

Broader CIR status may still show failures outside CodeGen+IR; report them separately and do not conflate them with this 48-test objective.

## Suggested Commit Structure

1. Member-pointer zero-initialization soundness.
2. Bitfield constant memory retargeting.
3. Forward-declaration recursive record decision/fix.
4. Mechanical FileCheck refresh, split into smaller commits if the diff is large.

Each commit should include the focused lit command in its message or notes, and the final commit should include the full CodeGen+IR gate result.
