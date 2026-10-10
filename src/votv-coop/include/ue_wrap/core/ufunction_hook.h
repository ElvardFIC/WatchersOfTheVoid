// ue_wrap/core/ufunction_hook.h -- patch a native UFunction's Func pointer to catch a call our
// ProcessEvent detour can NEVER see.
//
// Engine-wrapper layer (principle 7): no gameplay or network logic. Our only MinHook seam is
// UObject::ProcessEvent, the OUTER entry to the Blueprint VM. A BP-internal call -- EX_CallMath,
// EX_FinalFunction, EX_VirtualFunction, EX_Local*Function -- routes through UFunction::Invoke to
// (Context->*Func)(Stack, Result), one layer BELOW the detour, and never re-enters ProcessEvent, so
// a ProcessEvent observer on such a callee registers and never fires. The chipPile grab and the
// clump re-pile spawn are exactly this.
//
// SCOPE, AND IT IS LOAD-BEARING: those routes funnel through `Func` only when the callee is NATIVE.
// Both dispatch handlers branch on FUNC_Native, and a SCRIPT (bytecode) callee goes through
// ProcessScriptFunction to ProcessInternal, never reading Func at all. Per-function detail:
// docs/coop-dispatch-visibility.md.

#pragma once

#include "ue_wrap/core/script_gate.h"

#include <cstdint>

namespace ue_wrap::ufunction_hook {

// Post-native observer. `context` is the dispatch Context -- for a member call the object the
// function runs ON, such as the DYING actor for K2_DestroyActor; for a static GameplayStatics call
// the world-context-ish caller. `sourceObject` is FFrame::Object, the actor whose BYTECODE is
// executing, so the caller: the re-piling clump that issued a spawn. `spawnedResult` is *Result,
// the native function's RESULT_PARAM -- for BeginDeferred the new actor, and possibly null on a
// failed spawn. Fires AFTER the original Func returns, so `spawnedResult` is populated. Raw
// UObject*s.
//
// THREADING: native UFunction dispatch, UWorld::SpawnActor among it, is GAME-THREAD only, so this
// runs on the game thread -- but DEEP inside an engine call, so it MUST be cheap and MUST NOT
// throw. The facility SEH-wraps it as a crash backstop, the same firewall contract the ProcessEvent
// observers have.
using PostNativeCallback = void(*)(void* context, void* sourceObject, void* spawnedResult);

// The frame the call a callback (post or pre) is reporting executes in: for a call from bytecode
// that is the calling Blueprint function (`function`) and its storage (`locals`), where each of its
// locals and parameters sits at its Offset_Internal (reflection::FindPropertyOffset on the
// function); for a call through ProcessEvent it is the called function and its parameters, so check
// `function` before reading. An ubergraph's locals are the actor's persistent frame, which is how a
// spawn issued inside a Blueprint loop names the loop's current element. Both null outside any
// callback; code a callback runs sees that callback's frame. Game thread.
struct CallerFrame {
    void*    function;
    uint8_t* locals;
};
CallerFrame CurrentCallerFrame();

// The result storage of the call a post callback is reporting, the native function's RESULT_PARAM.
// The caller reads it once the callback returns, so a callback that writes a value of the
// function's return type there answers the call with that value: how a seam gives a read a
// Blueprint makes of an object the answer that object stands in for. Null outside any callback
// and for a function with no return value. Game thread.
void* CurrentResult();

// Patch `ufunction`'s native Func with a transparent forwarder that forwards to the original Func
// (which steps the params off the bytecode stream, runs the implementation and writes *Result),
// then reads FFrame::Object and *Result and invokes `cb` with both. No engine layout leaks upward.
// Idempotent per (ufunction, cb). Returns false if the arguments are null, the table is full, or
// the Func slot reads null -- a wrong offset for this build, where refusing beats corrupting the
// UFunction. There is no unpatch: a Func patch replaces an observation scheme wholesale once
// proven. `armed` false installs the patch disarmed (SetArmed). Game thread.
//
// The scope rule above is about the DISPATCH ROUTE, not the callee list. Script overrides ARE
// patched here and DO fire -- puppet_spawn's BlueprintUpdateAnimation, save_indicator_suppress's
// saveAnim and addHint -- because their dispatch is ProcessEvent, whose Invoke also reads Func.
// What does not work is a script function called via EX_Local*: the patch INSTALLS, since Func is
// ProcessInternal and non-null and passes the guard, it LOGS "patched", and it NEVER FIRES. That
// class belongs to the script-body gate (ue_wrap/core/script_gate.h), which sees every script body
// with its arguments and can refuse it.
bool InstallPostHook(void* ufunction, PostNativeCallback cb, bool armed = true);
// The row form (ue_wrap/facts/function_row.h): the patch is installed on a resolved row, which records it.
bool InstallPostHook(const facts::Resolved& fn, PostNativeCallback cb, bool armed = true);

// Arm or disarm an installed hook. Disarmed, the forwarder pays one load and a branch after the
// original and never calls back: the shape for a seam that matters only while a call of the
// consumer's own runs, on a native every Blueprint calls all the time. False when (ufunction, cb)
// is not installed. Game thread.
bool SetArmed(void* ufunction, PostNativeCallback cb, bool armed);

// Pre-native interceptor: the call's parameters, evaluated, before the original Func runs, and a
// verdict. `parms` holds each parameter at its Offset_Internal (reflection::FindParamOffset on the
// function); `context` and `sourceObject` are PostNativeCallback's, and CurrentCallerFrame names the
// calling frame while it runs. Cancel refuses the call: the original never runs, and the call's
// parameters are consumed from the caller's bytecode as the original's own steps consume them. Same
// threading and firewall contract as the post callback.
using PreNativeCallback = script_gate::Verdict (*)(void* context, void* sourceObject, const uint8_t* parms);

// Patch `ufunction`'s native Func with a forwarder that, armed, reads the call's parameters before the
// original does. A native thunk steps them off the caller's bytecode into its own locals, one expression each
// through the VM's exec-handler table, so a post forwarder never sees them, and UE4SS's native pre-hook reads
// the frame's Locals (LuaMod.cpp:128-200), which hold them only on the ProcessEvent path. This one steps them
// through the same table (script_gate::ExecHandlers) into a frame of its own. Run rewinds the caller's code
// pointer and forwards, so the original steps them again: the engine's own shape for a native it calls both
// remotely and locally (UObject::CallFunction). A ProcessEvent call's parameters are read from its frame.
// Refused at install, logged: a script function, a return value, an out or reference parameter, a parameter
// the engine must destroy, a frame past this one's, no exec-handler table. Off the game thread it forwards
// untouched. A pre hook stays the outermost patch on its function in either install order, so a post hook
// never reports a call its pre hook refused. `armed` defaults to false, the shape being a seam armed only
// while a call of the consumer's own runs. Idempotent per (ufunction, cb). Game thread.
bool InstallPreHook(void* ufunction, PreNativeCallback cb, bool armed = false);
bool InstallPreHook(const facts::Resolved& fn, PreNativeCallback cb, bool armed = false);

// Every installed patch, by function and kind. For coop/dev/dependency_dump; game thread.
using HookVisitor = void (*)(void* ctx, void* ufunction, bool pre);
void DebugForEachHook(HookVisitor visit, void* ctx);

// Arm or disarm an installed pre hook, as SetArmed does a post hook. Game thread.
bool SetArmed(void* ufunction, PreNativeCallback cb, bool armed);

}  // namespace ue_wrap::ufunction_hook
