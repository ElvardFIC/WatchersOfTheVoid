// harness/fingerprint_census_detail.h -- PRIVATE seam between the census (fingerprint_census.cpp) and its [dev]
// capture (fingerprint_capture.cpp): what both read. Not public API.

#pragma once

#include "ue_wrap/facts/function_row.h"

#include <functional>
#include <string>

namespace harness::fingerprint_census::detail {

// A row's key, "class.fn" lowercase ("*" for a row with no class).
std::string Key(const ue_wrap::facts::Function& row);

// The name of the class a RuntimeClass row's site last resolved it on; empty before that.
std::wstring WitnessName(const ue_wrap::facts::Function& row);

// Every function the any-class rows have met that is still live. Game thread.
void ForEachLiveSubject(const std::function<void(void*)>& fn);

// [dev] fingerprint_capture: write the capture file, once the first pass has met the loaded classes. Game thread.
void Capture();

}  // namespace harness::fingerprint_census::detail
