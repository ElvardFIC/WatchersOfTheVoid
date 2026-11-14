#pragma once
namespace coop::net {class Session;}
namespace coop::llama_timers {
void Install(net::Session* session);
void Tick();
void OnDisconnect();
}
