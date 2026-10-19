#pragma once
#include "coop/net/protocol.h"

namespace coop::service_panel_rules {
inline bool StateValid(uint8_t family,const uint8_t s[16]) {
    if (family==1) {
        if ((s[0]|s[1])&0xf0) return false;
        for (int i=2;i<16;++i) if (s[i]) return false;
        return true;
    }
    if (family!=2 || s[0]!=6 || (s[1]!=4 && s[1]!=9 && s[1]!=16 && s[1]!=25) ||
        s[2]>1 || s[3]>1 || s[14]>180 || s[15]>90) return false;
    for (int i=4;i<10;++i) if (s[i]>2) return false;
    const unsigned lights=unsigned(s[10])|(unsigned(s[11])<<8)|(unsigned(s[12])<<16)|(unsigned(s[13])<<24);
    return lights<(1u<<s[1]);
}
inline bool Valid(const net::ServicePanelPayload& p) {
    if (p.family<1 || p.family>2) return false;
    if (p.op==0) return !p.seq && StateValid(p.family,p.state);
    if (!p.seq) return false;
    if (p.family==1) {
        if (p.op==5) return p.index<4;
        if (p.op==6) return p.index<4 && p.coverKey.len<=31 && (p.coverKey.len || p.coverEid);
        if (p.op!=1 || !p.action || !StateValid(1,p.before) || !StateValid(1,p.state)) return false;
        const uint8_t changed=static_cast<uint8_t>((p.before[0]^p.state[0])|((p.before[1]^p.state[1])<<4));
        // Removal has its own host operation; a delta must name exactly its edits.
        return changed==p.action && (p.action&15)==0; // covers are removed/returned only on host
    }
    if (p.op==2) return (p.action==1 && p.index<25) || ((p.action==2 || p.action==3) && p.index==0);
    if (p.op==4) return p.index<6;
    if (p.op!=3 || p.index>=6 || !StateValid(2,p.before) || !StateValid(2,p.state)) return false;
    if (p.before[4+p.index]!=0 || p.state[4+p.index]!=1) return false;
    for (int i=4;i<10;++i) if (i!=4+p.index && p.before[i]!=p.state[i]) return false;
    return true;
}
// Compare and set each independent bit, so one peer's screwdriver does not erase
// another peer's work on a different cover. Refused returns need an item refund.
inline uint8_t MergeDesk(const net::ServicePanelPayload& p,uint8_t state[16]) {
    uint8_t refunds=0;
    for (int n=0;n<2;++n) for (int bit=0;bit<4;++bit) {
        const uint8_t mask=static_cast<uint8_t>(1u<<bit);
        if (!(p.action&(mask<<(n*4)))) continue;
        if ((state[n]^p.before[n])&mask) {
            if (n==0) refunds|=mask;
        } else state[n]=static_cast<uint8_t>((state[n]&~mask)|(p.state[n]&mask));
    }
    return refunds;
}
}
