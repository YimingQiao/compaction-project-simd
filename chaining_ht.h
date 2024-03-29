//===----------------------------------------------------------------------===//
//
//                         SIMD Compaction
//
// hash_table
//
//
//===----------------------------------------------------------------------===//

#pragma once

#include <atomic>
#include <functional>
#include <list>
#include <unordered_map>
#include <utility>

#include "base.h"
#include "hash_functions.h"
#include "profiler.h"

namespace simd_compaction {

class HashTable;

using Tuple = vector<Attribute>;
using Key = int64_t;

class ScanStructure {
 public:
  explicit ScanStructure(size_t count, vector<uint32_t> bucket_sel_vector, vector<list<Key> *> buckets,
                         vector<uint32_t> &key_sel_vector, HashTable *ht)
      : count_(count), buckets_(std::move(buckets)), bucket_sel_vector_(std::move(bucket_sel_vector)),
        key_sel_vector_(key_sel_vector), ht_(ht) {
    iterators_.resize(kBlockSize);
    iterator_ends_.resize(kBlockSize);
    for (size_t i = 0; i < count; ++i) {
      auto idx = bucket_sel_vector_[i];
      iterators_[idx] = buckets_[idx]->begin();
      iterator_ends_[idx] = buckets_[idx]->end();
    }
  }

  size_t Next(Vector &join_key, DataChunk &input, DataChunk &result);

  size_t InOneNext(Vector &join_key, DataChunk &input, DataChunk &result);

  inline bool HasNext() const { return HasBucket() || HasBuffer(); }

 private:
  size_t count_;
  vector<list<Key> *> buckets_;
  vector<uint32_t> bucket_sel_vector_;
  vector<uint32_t> &key_sel_vector_;
  vector<list<Key>::iterator> iterators_;
  vector<list<Key>::iterator> iterator_ends_;
  HashTable *ht_;

  // buffer
  unique_ptr<DataChunk> buffer_;

  inline bool HasBucket() const { return count_ > 0; }

  inline bool HasBuffer() const { return buffer_ != nullptr && buffer_->count_ > 0; }

  size_t ScanInnerJoin(Vector &join_key, vector<uint32_t> &result_vector);

  inline void AdvancePointers();

  inline void GatherResult(vector<Vector *> &cols, vector<uint32_t> &result_vector, size_t count);

  // ----------------------------  SIMD  ----------------------------
 public:
  size_t SIMDNext(Vector &join_key, DataChunk &input, DataChunk &result, bool compact_mode = true);

  size_t SIMDInOneNext(Vector &join_key, DataChunk &input, DataChunk &result, bool compact_mode = false);

 private:
  size_t SIMDScanInnerJoin(Vector &join_key, vector<uint32_t> &result_vector);

  inline void SIMDAdvancePointers();

  inline void SIMDGatherResult(vector<Vector *> &cols, vector<uint32_t> &result_vector, size_t count);
};

class HashTable {
 public:
  HashTable(size_t n_rhs_tuples, size_t chunk_factor);

  ScanStructure Probe(Vector &join_key, size_t count, vector<uint32_t> &sel_vec);

  ScanStructure SIMDProbe(Vector &join_key, size_t count, vector<uint32_t> &sel_vec);

 private:
  size_t n_buckets_;
  vector<unique_ptr<list<Key>>> linked_lists_;

  // we use & mask to replace % n.
  uint64_t SCALAR_BUCKET_MASK;
  __m512i SIMD_BUCKET_MASK;
};
}// namespace simd_compaction
