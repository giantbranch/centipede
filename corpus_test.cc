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

#include "./corpus.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "googletest/include/gtest/gtest.h"
#include "./control_flow.h"
#include "./coverage.h"
#include "./defs.h"
#include "./feature.h"
#include "./util.h"

namespace centipede {
namespace {

TEST(FeatureSet, ComputeWeight) {
  FeatureSet feature_set(10);

  auto W = [&](const FeatureVec &features) -> uint32_t {
    return feature_set.ComputeWeight(features);
  };

  feature_set.IncrementFrequencies({1, 2, 3});
  EXPECT_EQ(W({1}), W({2}));
  EXPECT_EQ(W({1}), W({3}));
  EXPECT_DEATH(W({4}), "");

  feature_set.IncrementFrequencies({1, 2});
  EXPECT_GT(W({3}), W({2}));
  EXPECT_GT(W({3}), W({1}));
  EXPECT_GT(W({3, 1}), W({2, 1}));
  EXPECT_GT(W({3, 2}), W({2}));

  feature_set.IncrementFrequencies({1});
  EXPECT_GT(W({3}), W({2}));
  EXPECT_GT(W({2}), W({1}));
  EXPECT_GT(W({3, 2}), W({3, 1}));
}

TEST(FeatureSet, ComputeWeightWithDifferentDomains) {
  FeatureSet feature_set(10);
  // Increment the feature frequencies such that the domain #1 is the rarest and
  // the domain #3 is the most frequent.
  auto f1 = feature_domains::k8bitCounters.begin();
  auto f2 = feature_domains::kCMP.begin();
  auto f3 = feature_domains::kBoundedPath.begin();
  feature_set.IncrementFrequencies(
      {/* one feature from domain #1 */ f1,
       /* two features from domain #2 */ f2, f2 + 1,
       /* three features from domain #3 */ f3, f3 + 1, f3 + 2});

  auto weight = [&](const FeatureVec &features) -> uint32_t {
    return feature_set.ComputeWeight(features);
  };

  // Test that features from a less frequent domain have more weight.
  EXPECT_GT(weight({f1}), weight({f2}));
  EXPECT_GT(weight({f2}), weight({f3}));
}

TEST(FeatureSet, CountUnseenAndPruneFrequentFeatures_IncrementFrequencies) {
  size_t frequency_threshold = 3;
  FeatureSet feature_set(frequency_threshold);
  FeatureVec features;
  // Shorthand for CountUnseenAndPruneFrequentFeatures.
  auto CountUnseenAndPrune = [&]() -> size_t {
    return feature_set.CountUnseenAndPruneFrequentFeatures(features);
  };
  // Shorthand for IncrementFrequencies.
  auto Increment = [&](const FeatureVec &features) {
    feature_set.IncrementFrequencies(features);
  };

  // CountUnseenAndPrune on the empty set.
  features = {10, 20};
  EXPECT_EQ(CountUnseenAndPrune(), 2);
  EXPECT_EQ(feature_set.size(), 0);
  EXPECT_EQ(features, FeatureVec({10, 20}));

  // Add {10} for the first time.
  features = {10, 20};
  Increment({10});
  EXPECT_EQ(CountUnseenAndPrune(), 1);
  EXPECT_EQ(feature_set.size(), 1);
  EXPECT_EQ(features, FeatureVec({10, 20}));

  // Add {10} for the second time.
  features = {10, 20};
  Increment({10});
  EXPECT_EQ(CountUnseenAndPrune(), 1);
  EXPECT_EQ(feature_set.size(), 1);
  EXPECT_EQ(features, FeatureVec({10, 20}));

  // Add {10} for the third time. {10} becomes "frequent", prune removes it.
  features = {10, 20};
  Increment({10});
  EXPECT_EQ(CountUnseenAndPrune(), 1);
  EXPECT_EQ(feature_set.size(), 1);
  EXPECT_EQ(features, FeatureVec({20}));

  // Add {30} for the first time. {10, 20} still gets pruned to {20}.
  features = {10, 20};
  Increment({30});
  EXPECT_EQ(CountUnseenAndPrune(), 1);
  EXPECT_EQ(feature_set.size(), 2);
  EXPECT_EQ(features, FeatureVec({20}));

  // {10, 20, 30} => {20, 30}; 1 unseen.
  features = {10, 20, 30};
  EXPECT_EQ(CountUnseenAndPrune(), 1);
  EXPECT_EQ(feature_set.size(), 2);
  EXPECT_EQ(features, FeatureVec({20, 30}));

  // {10, 20, 30} => {20}; 1 unseen.
  features = {10, 20, 30};
  Increment({30});
  Increment({30});
  EXPECT_EQ(CountUnseenAndPrune(), 1);
  EXPECT_EQ(feature_set.size(), 2);
  EXPECT_EQ(features, FeatureVec({20}));

  // {10, 20, 30} => {20}; 0 unseen.
  features = {10, 20, 30};
  Increment({20});
  Increment({20});
  EXPECT_EQ(CountUnseenAndPrune(), 0);
  EXPECT_EQ(feature_set.size(), 3);
  EXPECT_EQ(features, FeatureVec({20}));

  // {10, 20, 30} => {}; 0 unseen.
  features = {10, 20, 30};
  Increment({20});
  EXPECT_EQ(CountUnseenAndPrune(), 0);
  EXPECT_EQ(feature_set.size(), 3);
  EXPECT_EQ(features, FeatureVec({}));
}

TEST(Corpus, GetCmpArgs) {
  PCTable pc_table(100);
  CFTable cf_table(100);
  CoverageFrontier coverage_frontier({pc_table, {}, cf_table, {}, {}});
  FeatureSet fs(3);
  Corpus corpus;
  ByteArray cmp_args{2, 0, 1, 2, 3};
  FeatureVec features1 = {10, 20, 30};
  fs.IncrementFrequencies(features1);
  corpus.Add({1}, features1, cmp_args, fs, coverage_frontier);
  EXPECT_EQ(corpus.NumActive(), 1);
  EXPECT_EQ(corpus.GetCmpArgs(0), cmp_args);
}

TEST(Corpus, PrintStats) {
  PCTable pc_table(100);
  CFTable cf_table(100);
  CoverageFrontier coverage_frontier({pc_table, {}, cf_table, {}, {}});
  FeatureSet fs(3);
  Corpus corpus;
  FeatureVec features1 = {10, 20, 30};
  FeatureVec features2 = {20, 40};
  fs.IncrementFrequencies(features1);
  corpus.Add({1, 2, 3}, features1, {}, fs, coverage_frontier);
  fs.IncrementFrequencies(features2);
  corpus.Add({4, 5}, features2, {}, fs, coverage_frontier);
  std::ostringstream os;
  corpus.PrintStats(os, fs);
  EXPECT_EQ(os.str(),
            "{ \"corpus_stats\": [\n"
            "  {\"size\": 3, \"frequencies\": [1, 2, 1]},\n"
            "  {\"size\": 2, \"frequencies\": [2, 1]}]}\n");
}

TEST(Corpus, Prune) {
  // Prune will remove an input if all of its features appear at least 3 times.
  PCTable pc_table(100);
  CFTable cf_table(100);
  CoverageFrontier coverage_frontier({pc_table, {}, cf_table, {}, {}});
  FeatureSet fs(3);
  Corpus corpus;
  Rng rng(0);
  size_t max_corpus_size = 1000;

  auto Add = [&](const CorpusRecord &record) {
    fs.IncrementFrequencies(record.features);
    corpus.Add(record.data, record.features, {}, fs, coverage_frontier);
  };

  auto VerifyActiveInputs = [&](std::vector<ByteArray> expected_inputs) {
    std::vector<ByteArray> observed_inputs;
    for (size_t i = 0, n = corpus.NumActive(); i < n; i++) {
      observed_inputs.push_back(corpus.Get(i));
    }
    std::sort(observed_inputs.begin(), observed_inputs.end());
    std::sort(expected_inputs.begin(), expected_inputs.end());
    EXPECT_EQ(observed_inputs, expected_inputs);
  };

  Add({{0}, {20, 40}});
  Add({{1}, {20, 30}});
  Add({{2}, {30, 40}});
  Add({{3}, {40, 50}});
  Add({{4}, {10, 20}});

  // Prune. Features 20 and 40 are frequent => input {0} will be removed.
  EXPECT_EQ(corpus.NumActive(), 5);
  EXPECT_EQ(corpus.Prune(fs, coverage_frontier, max_corpus_size, rng), 1);
  EXPECT_EQ(corpus.NumActive(), 4);
  EXPECT_EQ(corpus.NumTotal(), 5);
  VerifyActiveInputs({{1}, {2}, {3}, {4}});

  Add({{5}, {30, 60}});
  EXPECT_EQ(corpus.NumTotal(), 6);
  // Prune. Feature 30 is now frequent => inputs {1} and {2} will be removed.
  EXPECT_EQ(corpus.NumActive(), 5);
  EXPECT_EQ(corpus.Prune(fs, coverage_frontier, max_corpus_size, rng), 2);
  EXPECT_EQ(corpus.NumActive(), 3);
  VerifyActiveInputs({{3}, {4}, {5}});

  // Test with smaller max_corpus_size values.
  EXPECT_EQ(corpus.Prune(fs, coverage_frontier, 3, rng), 0);
  EXPECT_EQ(corpus.NumActive(), 3);
  EXPECT_EQ(corpus.Prune(fs, coverage_frontier, 2, rng), 1);
  EXPECT_EQ(corpus.NumActive(), 2);
  EXPECT_EQ(corpus.Prune(fs, coverage_frontier, 1, rng), 1);
  EXPECT_EQ(corpus.NumActive(), 1);
  EXPECT_DEATH(corpus.Prune(fs, coverage_frontier, 0, rng),
               "max_corpus_size");  // CHECK-fail.
  EXPECT_EQ(corpus.NumTotal(), 6);
}

// Regression test for a crash in Corpus::Prune().
TEST(Corpus, PruneRegressionTest1) {
  PCTable pc_table(100);
  CFTable cf_table(100);
  CoverageFrontier coverage_frontier({pc_table, {}, cf_table, {}, {}});
  FeatureSet fs(2);
  Corpus corpus;
  Rng rng(0);
  size_t max_corpus_size = 1000;

  auto Add = [&](const CorpusRecord &record) {
    fs.IncrementFrequencies(record.features);
    corpus.Add(record.data, record.features, {}, fs, coverage_frontier);
  };

  Add({{1}, {10, 20}});
  Add({{2}, {10}});
  corpus.Prune(fs, coverage_frontier, max_corpus_size, rng);
}

TEST(WeightedDistribution, WeightedDistribution) {
  std::vector<uint32_t> freq;
  WeightedDistribution wd;
  const int kNumIter = 10000;

  auto set_weights = [&](const std::vector<uint32_t> &weights) {
    wd.clear();
    for (auto weight : weights) {
      wd.AddWeight(weight);
    }
  };

  auto compute_freq = [&]() {
    freq.clear();
    freq.resize(wd.size());
    // We use numbers in [0, kNumIter) instead of random numbers
    // for simplicity.
    for (int i = 0; i < kNumIter; i++) {
      freq[wd.RandomIndex(i)]++;
    }
  };

  set_weights({1, 1});
  compute_freq();
  EXPECT_EQ(freq[0], kNumIter / 2);
  EXPECT_EQ(freq[1], kNumIter / 2);

  set_weights({1, 2});
  compute_freq();
  EXPECT_GT(freq[0], kNumIter / 4);
  EXPECT_LT(freq[0], kNumIter / 2);
  EXPECT_GT(freq[1], kNumIter / 2);

  set_weights({10, 100, 1});
  compute_freq();
  EXPECT_LT(9 * freq[2], freq[0]);
  EXPECT_LT(9 * freq[0], freq[1]);

  set_weights({0, 1, 2});
  compute_freq();
  EXPECT_EQ(freq[0], 0);
  EXPECT_GT(freq[2], freq[1]);

  set_weights({2, 1, 0});
  compute_freq();
  EXPECT_EQ(freq[2], 0);
  EXPECT_GT(freq[0], freq[1]);

  // Test ChangeWeight
  set_weights({1, 2, 3, 4, 5});
  compute_freq();
  EXPECT_GT(freq[4], freq[3]);
  EXPECT_GT(freq[3], freq[2]);
  EXPECT_GT(freq[2], freq[1]);
  EXPECT_GT(freq[1], freq[0]);

  wd.ChangeWeight(2, 1);
  // Calling RandomIndex() after ChangeWeight() w/o calling
  // RecomputeInternalState() should crash.
  EXPECT_DEATH(compute_freq(), "");
  wd.RecomputeInternalState();
  // Weights: {1, 2, 1, 4, 5}
  compute_freq();
  EXPECT_GT(freq[4], freq[3]);
  EXPECT_GT(freq[3], freq[2]);
  EXPECT_LT(freq[2], freq[1]);
  EXPECT_GT(freq[1], freq[0]);

  // Weights: {1, 2, 1, 0, 5}
  wd.ChangeWeight(3, 0);
  wd.RecomputeInternalState();
  compute_freq();
  EXPECT_GT(freq[4], freq[1]);
  EXPECT_GT(freq[1], freq[0]);
  EXPECT_GT(freq[1], freq[2]);
  EXPECT_EQ(freq[3], 0);

  // Test PopBack().
  wd.PopBack();
  // Weights: {1, 2, 1, 0} after PopBack().
  EXPECT_EQ(wd.size(), 4);
  EXPECT_GT(freq[1], freq[0]);
  EXPECT_GT(freq[1], freq[2]);
  EXPECT_EQ(freq[3], 0);

  // Stress test. If the algorithm is too slow, we may be able to catch it as a
  // timeout.
  wd.clear();
  for (int i = 1; i < 100000; i++) {
    wd.AddWeight(i);
  }
  compute_freq();
}

// TODO(navidem): based on comment from sergeygs -- This is becoming difficult
// to maintain: various bits of the input data are stored in independent arrays,
// other bits are dynamically initialized, and the matching expected results are
// listed in two long chains of EXPECT's. I think it should be doable to
// refactor this to use something like a TestCase struct tying all that
// together, then iterate over test_cases once to populate pc_table etc, and a
// second time to e.g. EXPECT_EQ(frontier.PcIndexIsFrontier(i),
// test_cases[i].expected_is_frontier).
TEST(CoverageFrontier, Compute) {
  // Function [0, 1): Fully covered.
  // Function [1, 2): Not covered.
  // Function [2, 4): Partially covered => has one frontier.
  // Function [4, 6): Not covered.
  // Function [6, 9): Partially covered => has one frontier.
  // Function [9, 12): Fully covered.
  // Function [12, 19): Partially covered => has two frontiers.
  PCTable pc_table{{0, PCInfo::kFuncEntry},  // Covered.
                   {1, PCInfo::kFuncEntry},
                   {2, PCInfo::kFuncEntry},  // Covered.
                   {3, 0},
                   {4, PCInfo::kFuncEntry},
                   {5, 0},
                   {6, PCInfo::kFuncEntry},  // Covered.
                   {7, 0},                   // Covered.
                   {8, 0},
                   {9, PCInfo::kFuncEntry},   // Covered.
                   {10, 0},                   // Covered.
                   {11, 0},                   // Covered.
                   {12, PCInfo::kFuncEntry},  // Covered.
                   {13, 0},                   // Covered.
                   {14, 0},                   // Covered.
                   {15, 0},
                   {16, 0},  // Covered.
                   {17, 0},  // Covered.
                   {18, 0}};
  CFTable cf_table{
      0, 0, 9, 0,               // 0 calls 9.
      1, 0, 6, 0,               // 1 calls 6.
      2, 3, 0, 0,               // 2 calls 4 in bb 3.
      3, 0, 4, 0,               // This bb calls 4.
      4, 5, 0, 0,               // 4 calls 9 in bb 5.
      5, 0, 9, 0,               // This bb calls 9.
      6, 7, 8, 0, 0,            // 6 calls 2 and makes indirect call in bb 8.
      7, 0, 0, 8, 0, 2, -1, 0,  // This bb calls 2 and makes an indirect
                                // call.
      9, 10, 0, 0,              // 9 calls no one.
      10, 11, 0, 0, 11, 0, 0, 12, 13, 14, 0, 0,  // 12 call 9 and 99 in bb
                                                 // 15, and calls 4 in
                                                 // bb 18.
      13, 15, 16, 0, 0, 14, 17, 18, 0, 0, 15, 0, 9, 99,
      0,                                    // This bb calls 9 and 99.
      16, 13, 0, 0, 17, 0, 0, 18, 0, 4, 0,  // This bb calls 4.
  };

  ControlFlowGraph cfg;
  cfg.InitializeControlFlowGraph(cf_table, pc_table);
  CallGraph call_graph;
  call_graph.InitializeCallGraph(cf_table, pc_table);
  BinaryInfo bin_info = {pc_table, {}, cf_table, cfg, call_graph};
  CoverageFrontier frontier(bin_info);

  FeatureVec pcs(pc_table.size());
  for (size_t i = 0; i < pc_table.size(); i++) {
    pcs[i] = feature_domains::k8bitCounters.ConvertToMe(
        Convert8bitCounterToNumber(i, /*counter_value*/ 1));
  }

  FeatureSet fs(100);
  Corpus corpus;

  auto Add = [&](feature_t feature) {
    fs.IncrementFrequencies({feature});
    corpus.Add({42}, {feature}, {}, fs, frontier);
  };

  // Add PC-based features.
  for (size_t idx : {0, 2, 6, 7, 9, 10, 11, 12, 13, 14, 16, 17}) {
    Add(pcs[idx]);
  }
  // add some non-pc features.
  for (size_t x : {1, 2, 3, 4}) {
    Add(feature_domains::kUnknown.ConvertToMe(x));
  }

  // Compute and check the frontier.
  EXPECT_EQ(frontier.Compute(corpus), 3);
  EXPECT_EQ(frontier.NumFunctionsInFrontier(), 3);
  EXPECT_FALSE(frontier.PcIndexIsFrontier(0));
  EXPECT_FALSE(frontier.PcIndexIsFrontier(1));
  EXPECT_TRUE(frontier.PcIndexIsFrontier(2));
  EXPECT_FALSE(frontier.PcIndexIsFrontier(3));
  EXPECT_FALSE(frontier.PcIndexIsFrontier(4));
  EXPECT_FALSE(frontier.PcIndexIsFrontier(5));
  EXPECT_TRUE(frontier.PcIndexIsFrontier(6));
  EXPECT_FALSE(frontier.PcIndexIsFrontier(7));
  EXPECT_FALSE(frontier.PcIndexIsFrontier(8));
  EXPECT_FALSE(frontier.PcIndexIsFrontier(9));
  EXPECT_FALSE(frontier.PcIndexIsFrontier(10));
  EXPECT_FALSE(frontier.PcIndexIsFrontier(11));
  EXPECT_FALSE(frontier.PcIndexIsFrontier(12));
  EXPECT_TRUE(frontier.PcIndexIsFrontier(13));
  EXPECT_TRUE(frontier.PcIndexIsFrontier(14));
  EXPECT_FALSE(frontier.PcIndexIsFrontier(15));
  EXPECT_FALSE(frontier.PcIndexIsFrontier(16));
  EXPECT_FALSE(frontier.PcIndexIsFrontier(17));
  EXPECT_FALSE(frontier.PcIndexIsFrontier(18));

  // Check frontier weight.
  EXPECT_EQ(frontier.FrontierWeight(0), 0);
  EXPECT_EQ(frontier.FrontierWeight(1), 0);
  EXPECT_EQ(frontier.FrontierWeight(2), 153);
  EXPECT_EQ(frontier.FrontierWeight(3), 0);
  EXPECT_EQ(frontier.FrontierWeight(4), 0);
  EXPECT_EQ(frontier.FrontierWeight(5), 0);
  EXPECT_EQ(frontier.FrontierWeight(6), 230);
  EXPECT_EQ(frontier.FrontierWeight(7), 0);
  EXPECT_EQ(frontier.FrontierWeight(8), 0);
  EXPECT_EQ(frontier.FrontierWeight(9), 0);
  EXPECT_EQ(frontier.FrontierWeight(10), 0);
  EXPECT_EQ(frontier.FrontierWeight(11), 0);
  EXPECT_EQ(frontier.FrontierWeight(12), 0);
  EXPECT_EQ(frontier.FrontierWeight(13), 25);
  EXPECT_EQ(frontier.FrontierWeight(14), 153);
  EXPECT_EQ(frontier.FrontierWeight(15), 0);
  EXPECT_EQ(frontier.FrontierWeight(16), 0);
  EXPECT_EQ(frontier.FrontierWeight(17), 0);
  EXPECT_EQ(frontier.FrontierWeight(18), 0);
}

TEST(CoverageFrontierDeath, InvalidIndexToFrontier) {
  PCTable pc_table = {{0, PCInfo::kFuncEntry}, {1, 0}};
  CFTable cf_table = {
      0, 1, 0, 0, 1, 0, 0,
  };

  ControlFlowGraph cfg;
  cfg.InitializeControlFlowGraph(cf_table, pc_table);
  CallGraph call_graph;
  call_graph.InitializeCallGraph(cf_table, pc_table);

  BinaryInfo bin_info = {pc_table, {}, cf_table, cfg, call_graph};
  CoverageFrontier frontier(bin_info);

  Corpus corpus;
  frontier.Compute(corpus);
  // Check with a non-existent idx.
  // TODO(navidem): enable the following test once CHECK is used in
  // PcIndexIsFrontier. EXPECT_DEATH(frontier.PcIndexIsFrontier(666), "");
  EXPECT_DEATH(frontier.FrontierWeight(666), "");
}

}  // namespace
}  // namespace centipede
