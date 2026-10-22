#include "coop/net/protocol.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <limits>
int main() {
    using namespace coop::net;
    assert(QuantizeUnitFraction(0.f)==0);
    assert(QuantizeUnitFraction(-1.f)==0);
    assert(QuantizeUnitFraction(std::numeric_limits<float>::quiet_NaN())==0);
    assert(QuantizeUnitFraction(-std::numeric_limits<float>::infinity())==0);
    assert(QuantizeUnitFraction(std::numeric_limits<float>::infinity())==255);
    assert(QuantizeUnitFraction(std::numeric_limits<float>::denorm_min())==1);
    assert(QuantizeUnitFraction(.00001f)==1);
    assert(QuantizeUnitFraction(.5f)==128);
    assert(QuantizeUnitFraction(1.f)==255);
    for(int i=0;i<256;++i)
        assert(QuantizeUnitFraction(DequantizeUnitFraction(static_cast<uint8_t>(i)))==i);
    puts("PASS production vitals: tiny positive health, NaN, infinities and all 256 round trips");
}
