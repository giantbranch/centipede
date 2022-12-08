// Copyright 2022 The Centipede Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// Instrumentation callbacks for SanitizerCoverage (sancov).
// https://clang.llvm.org/docs/SanitizerCoverage.html

#include <cstdint>

#include "./feature.h"
#include "./runner.h"

namespace centipede {
  void RunnerSancov(){}  // to be referenced in runner.cc
}  // namespace centipede

using centipede::state;
using centipede::tls;

// Tracing data flow.
// The instrumentation is provided by
// https://clang.llvm.org/docs/SanitizerCoverage.html#tracing-data-flow.
// For every load we get the address of the load. We can also get the caller PC.
// If the load address in
// [main_object_start_address, main_object_start_address + main_object_size),
// it is likely a global.
// We form a feature from a pair of {caller_pc, address_of_load}.
// The rationale here is that loading from a global address unique for the
// given PC is an interesting enough behavior that it warrants its own feature.
//
// Downsides:
// * The instrumentation is expensive, it can easily add 2x slowdown.
// * This creates plenty of features, easily 10x compared to control flow,
//   and bloats the corpus. But this is also what we want to achieve here.

// NOTE: In addition to `always_inline`, also use `inline`, because some
// compilers require both to actually enforce inlining, e.g. GCC:
// https://gcc.gnu.org/onlinedocs/gcc/Common-Function-Attributes.html.
#define ENFORCE_INLINE __attribute__((always_inline)) inline

// Use this attribute for functions that must not be instrumented even if
// the runner is built with sanitizers (asan, etc).
#define NO_SANITIZE __attribute__((no_sanitize("all")))

// NOTE: Enforce inlining so that `__builtin_return_address` works.
ENFORCE_INLINE static void TraceLoad(void *addr) {
  if (!state.run_time_flags.use_dataflow_features) return;
  auto caller_pc = reinterpret_cast<uintptr_t>(__builtin_return_address(0));
  auto load_addr = reinterpret_cast<uintptr_t>(addr);
  auto pc_offset = caller_pc - state.main_object_start_address;
  if (pc_offset >= state.main_object_size) return;  // PC outside of main obj.
  auto addr_offset = load_addr - state.main_object_start_address;
  if (addr_offset >= state.main_object_size) return;  // Not a global address.
  state.data_flow_feature_set.set(centipede::ConvertPcPairToNumber(
      pc_offset, addr_offset, state.main_object_size));
}

// NOTE: Enforce inlining so that `__builtin_return_address` works.
ENFORCE_INLINE static void TraceCmp(uint64_t Arg1, uint64_t Arg2) {
  if (!state.run_time_flags.use_cmp_features) return;
  auto caller_pc = reinterpret_cast<uintptr_t>(__builtin_return_address(0));
  auto pc_offset = caller_pc - state.main_object_start_address;
  uintptr_t hash =
      centipede::Hash64Bits(pc_offset) ^ tls.path_ring_buffer.hash();
  state.cmp_feature_set.set(
      centipede::ConvertContextAndArgPairToNumber(Arg1, Arg2, hash));
}

//------------------------------------------------------------------------------
// Implementations of the external sanitizer coverage hooks.
//------------------------------------------------------------------------------

extern "C" {
NO_SANITIZE void __sanitizer_cov_load1(uint8_t *addr) { TraceLoad(addr); }
NO_SANITIZE void __sanitizer_cov_load2(uint16_t *addr) { TraceLoad(addr); }
NO_SANITIZE void __sanitizer_cov_load4(uint32_t *addr) { TraceLoad(addr); }
NO_SANITIZE void __sanitizer_cov_load8(uint64_t *addr) { TraceLoad(addr); }
NO_SANITIZE void __sanitizer_cov_load16(__uint128_t *addr) { TraceLoad(addr); }

NO_SANITIZE
void __sanitizer_cov_trace_const_cmp1(uint8_t Arg1, uint8_t Arg2) {
  TraceCmp(Arg1, Arg2);
}
NO_SANITIZE
void __sanitizer_cov_trace_const_cmp2(uint16_t Arg1, uint16_t Arg2) {
  TraceCmp(Arg1, Arg2);
  if (Arg1 != Arg2 && state.run_time_flags.use_auto_dictionary)
    tls.cmp_trace2.Capture(Arg1, Arg2);
}
NO_SANITIZE
void __sanitizer_cov_trace_const_cmp4(uint32_t Arg1, uint32_t Arg2) {
  TraceCmp(Arg1, Arg2);
  if (Arg1 != Arg2 && state.run_time_flags.use_auto_dictionary)
    tls.cmp_trace4.Capture(Arg1, Arg2);
}
NO_SANITIZE
void __sanitizer_cov_trace_const_cmp8(uint64_t Arg1, uint64_t Arg2) {
  TraceCmp(Arg1, Arg2);
  if (Arg1 != Arg2 && state.run_time_flags.use_auto_dictionary)
    tls.cmp_trace8.Capture(Arg1, Arg2);
}
NO_SANITIZE
void __sanitizer_cov_trace_cmp1(uint8_t Arg1, uint8_t Arg2) {
  TraceCmp(Arg1, Arg2);
}
NO_SANITIZE
void __sanitizer_cov_trace_cmp2(uint16_t Arg1, uint16_t Arg2) {
  TraceCmp(Arg1, Arg2);
  if (Arg1 != Arg2 && state.run_time_flags.use_auto_dictionary)
    tls.cmp_trace2.Capture(Arg1, Arg2);
}
NO_SANITIZE
void __sanitizer_cov_trace_cmp4(uint32_t Arg1, uint32_t Arg2) {
  TraceCmp(Arg1, Arg2);
  if (Arg1 != Arg2 && state.run_time_flags.use_auto_dictionary)
    tls.cmp_trace4.Capture(Arg1, Arg2);
}
NO_SANITIZE
void __sanitizer_cov_trace_cmp8(uint64_t Arg1, uint64_t Arg2) {
  TraceCmp(Arg1, Arg2);
  if (Arg1 != Arg2 && state.run_time_flags.use_auto_dictionary)
    tls.cmp_trace8.Capture(Arg1, Arg2);
}
// TODO(kcc): [impl] handle switch.
NO_SANITIZE
void __sanitizer_cov_trace_switch(uint64_t Val, uint64_t *Cases) {}

// https://clang.llvm.org/docs/SanitizerCoverage.html#pc-table
// This function is called at the DSO init time.
void __sanitizer_cov_pcs_init(const uintptr_t *beg, const uintptr_t *end) {
  state.pcs_beg = beg;
  state.pcs_end = end;
}

// https://clang.llvm.org/docs/SanitizerCoverage.html#tracing-control-flow
// This function is called at the DSO init time.
void __sanitizer_cov_cfs_init(const uintptr_t *beg, const uintptr_t *end) {
  state.cfs_beg = beg;
  state.cfs_end = end;
}

// TODO(kcc): [impl] actually implement this callback.
// See https://clang.llvm.org/docs/SanitizerCoverage.html#tracing-pcs.
// This instrumentation is redundant if other instrumentation
// (e.g. trace-pc-guard) is available, but GCC as of 2022-04 only supports
// this variant.
void __sanitizer_cov_trace_pc() {}

// This function is called at the DSO init time.
void __sanitizer_cov_trace_pc_guard_init(uint32_t *start, uint32_t *stop) {
  state.pc_guard_start = start;
  state.pc_guard_stop = stop;
}

// This function is called on every instrumented edge.
NO_SANITIZE
void __sanitizer_cov_trace_pc_guard(uint32_t *guard) {
  // `guard` is in [pc_guard_start, pc_guard_stop), which gives us the offset.
  uintptr_t offset = guard - state.pc_guard_start;

  // counter or pc features.
  if (state.run_time_flags.use_counter_features) {
    state.counter_array.Increment(offset);
  } else if (state.run_time_flags.use_pc_features) {
    state.pc_feature_set.set(offset);
  }

  // path features.
  if (auto path_level = state.run_time_flags.path_level) {
    uintptr_t hash = tls.path_ring_buffer.push(offset, path_level);
    state.path_feature_set.set(hash);
  }
}

}  // extern "C"
