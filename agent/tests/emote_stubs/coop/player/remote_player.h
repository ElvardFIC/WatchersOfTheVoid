#pragma once
namespace coop {struct RemotePlayer {void* body=nullptr;bool eligible=true;bool CanPlayEmote()const{return eligible;}void* actor()const{return body;}};}
