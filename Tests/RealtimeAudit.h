#pragma once
#include <cstdint>
namespace realtimeAudit {
struct Counts { std::uint64_t allocations {}, frees {}, locks {}, waits {}, io {}; };
extern thread_local bool active;
extern thread_local Counts counts;
int installImportHooks();
bool selfTest();
struct Guard { Guard() { counts={};active=true; } ~Guard() { active=false; } };
}
