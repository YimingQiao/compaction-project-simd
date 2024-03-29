#include "base.h"

namespace simd_compaction {

void Vector::Reference(Vector &other) {
  assert(type_ == other.type_);
  data_ = other.data_;
}

DataChunk::DataChunk(const vector<AttributeType> &types) : count_(0), types_(types), selection_vector_(kBlockSize) {
  for (auto &type : types) data_.emplace_back(type);
  for (size_t i = 0; i < kBlockSize; ++i) selection_vector_[i] = i;
}

void DataChunk::Append(DataChunk &chunk, size_t num, size_t offset) {
  assert(types_.size() == chunk.types_.size());
  assert(count_ + num <= kBlockSize);

  for (size_t i = 0; i < types_.size(); ++i) {
    assert(types_[i] == chunk.types_[i]);
    for (size_t j = 0; j < num; ++j) {
      auto r_idx = chunk.selection_vector_[j + offset];
      data_[i].GetValue(count_ + j) = chunk.data_[i].GetValue(r_idx);
    }
  }
  count_ += num;
}

void DataChunk::AppendTuple(vector<Attribute> &tuple) {
  for (size_t i = 0; i < types_.size(); ++i) {
    auto &col = data_[i];
    col.GetValue(count_) = tuple[i];
  }
  ++count_;
}

void DataChunk::Slice(DataChunk &other, vector<uint32_t> &selection_vector, size_t count) {
  assert(other.data_.size() <= data_.size());
  this->count_ = count;
  for (size_t c = 0; c < other.data_.size(); ++c) data_[c].Reference(other.data_[c]);

  for (size_t i = 0; i < count; ++i) {
    auto new_idx = selection_vector[i];
    auto key_idx = other.selection_vector_[new_idx];
    selection_vector_[i] = key_idx;
  }
}

void DataChunk::SIMDSlice(DataChunk &other, vector<uint32_t> &selection_vector, size_t count) {
  assert(other.data_.size() <= data_.size());
  this->count_ = count;
  for (size_t c = 0; c < other.data_.size(); ++c) data_[c].Reference(other.data_[c]);

  uint32_t tail = count & 15;
  for (size_t i = 0; i < count - tail; i += 16) {
    __m512i new_ids = _mm512_loadu_epi32(selection_vector.data() + i);
    __m512i key_ids = _mm512_i32gather_epi32(new_ids, other.selection_vector_.data(), 4);
    _mm512_storeu_epi32(selection_vector_.data() + i, key_ids);
  }

  if (tail) {
    for (size_t i = count - tail; i < count; ++i) {
      auto new_idx = selection_vector[i];
      auto key_idx = other.selection_vector_[new_idx];
      selection_vector_[i] = key_idx;
    }
  }
}
}// namespace simd_compaction
