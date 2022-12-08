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

#ifndef THIRD_PARTY_CENTIPEDE_CONTROL_FLOW_H_
#define THIRD_PARTY_CENTIPEDE_CONTROL_FLOW_H_

#include <cstddef>
#include <cstdint>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "./coverage.h"

namespace centipede {

class ControlFlowGraph {
 public:
  // Reads form __sancov_cfs section. On error it crashes, if the section is not
  // there, the graph_ will be empty.
  void ReadFromCfTable(const Coverage::CFTable &cf_table);
  // Returns the vector of successor PCs for the given basic block PC.
  const std::vector<uintptr_t> &GetSuccessors(uintptr_t basic_block) const;

  // Returns the number of cfg entries.
  size_t size() const { return graph_.size(); }
  // Checks if basic_block is in cfg.
  bool exists(const uintptr_t basic_block) const {
    return graph_.contains(basic_block);
  }

 private:
  // A map with PC as the keys and vector of PCs as value.
  absl::flat_hash_map<uintptr_t, std::vector<uintptr_t>> graph_;
};

}  // namespace centipede
#endif  // THIRD_PARTY_CENTIPEDE_CONTROL_FLOW_H_
