# AnySwitch guest runtime

The native glue that lets translated code run on the host.

Remill's lifted functions have the shape:

```cpp
Memory* trace(State* state, uint64_t pc, Memory* memory);
```

and call `__remill_*` intrinsics for anything the semantics cannot express
natively. Those intrinsics are undefined in every Remill bitcode file —
providing them is the integrator's job, and this is that layer.

## What lives here

| Piece | Role |
|---|---|
| `Memory` | Opaque to translated code; Remill never dereferences it. We define it. |
| `GuestMemory` | Flat, bounds-checked guest address space |
| `__remill_read/write_memory_*` | Guest memory access |
| `__remill_compare_*` | Comparison hooks; identity by default |
| `__remill_function_return` and friends | Hypercall seam; updates the architectural PC on return |

## What this is not

There is no interpreter, no JIT, and no dispatch loop. The guest code was
translated ahead of time; this only supplies the register file, the memory
model, and the boundary crossed when the guest does something the host must
service (a syscall, a call into a system library).

## Register offsets

Measured against `remill/Arch/AArch64/Runtime/State.h`:

```
sizeof(State) = 1200
State.gpr     = 536
GPR.x0        = 8     -> State.X0 = 544
GPR.sp        = 504   -> State.SP = 1040
GPR.pc        = 520   -> State.PC = 1056
```

Note that Remill's runtime headers define these types in the **global**
namespace, not under `remill::`.

## Testing

`core/relinker/tests/run_exec_tests.py` hand-builds `.nro` fixtures,
translates them, links them against this runtime, executes them and asserts
the guest's X0 result. That is the test proving the translation is
semantically correct rather than merely emitting bytes.
