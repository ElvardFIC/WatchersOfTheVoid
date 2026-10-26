#pragma once
#include <functional>
namespace coop::net {class Session;}
namespace coop::llama_trigger {
// True after one-shot admission; state/audio sampling remains in llama_sync.
bool TryRemote(net::Session& session,void* actor,const std::function<bool()>& current);
// The native check uses camera zero even when its overlap was a remote puppet.
bool AllowLocal(net::Session& session,void* actor,const std::function<bool()>& current);
// The predicate is retained: it must own its source references/key/epoch by value.
void QueueNativeMotion(net::Session& session,void* actor,const std::function<bool()>& sourceCurrent);
void TickMotion(net::Session& session);
bool BlocksNative(void* actor);
void Reset();
}
