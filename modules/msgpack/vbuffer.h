/** Copyright 2020-2021 Alibaba Group Holding Limited.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/

#ifndef MODULES_MSGPACK_VBUFFER_H_
#define MODULES_MSGPACK_VBUFFER_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "basic/ds/array.vineyard.h"
#include "client/client.h"
#include "client/ds/blob.h"
#include "client/ds/i_object.h"
#include "common/util/uuid.h"

namespace vineyard {

namespace msgpack {

class Buffer {
 public:
  Buffer(uint8_t* buffer, size_t size, bool owned = false)
      : buffer_(buffer), size_(size), owned_(owned) {}

  Buffer(const uint8_t* buffer, size_t size, bool owned = false)
      : buffer_(const_cast<uint8_t*>(buffer)), size_(size), owned_(owned) {}

  explicit Buffer(std::string const& buffer) {
    buffer_ = static_cast<uint8_t*>(malloc(buffer.size()));
    size_ = buffer.size();
    owned_ = true;
    memcpy(buffer_, buffer.data(), buffer.size());
  }

  template <typename T>
  explicit Buffer(std::vector<T> const& buffer) {
    size_t elements_size = buffer.size() * sizeof(T);
    buffer_ = static_cast<uint8_t*>(malloc(elements_size));
    size_ = elements_size;
    owned_ = true;
    memcpy(buffer_, buffer.data(), elements_size);
  }

  Buffer(const Buffer& other) {
    buffer_ = other.buffer_;
    size_ = other.size_;
    owned_ = false;
  }

  Buffer(Buffer&& other) {
    buffer_ = other.buffer_;
    size_ = other.size_;
    if (other.owned_) {
      owned_ = true;
    }
    other.buffer_ = nullptr;
    other.size_ = 0;
    other.owned_ = false;
  }

  Buffer& operator=(const Buffer& other) {
    buffer_ = other.buffer_;
    size_ = other.size_;
    owned_ = false;
    return *this;
  }

  Buffer& operator=(Buffer&& other) {
    buffer_ = other.buffer_;
    size_ = other.size_;
    if (other.owned_) {
      owned_ = true;
    }
    other.buffer_ = nullptr;
    other.size_ = 0;
    other.owned_ = false;
    return *this;
  }

  ~Buffer() {
    if (owned_ && buffer_ != nullptr) {
      free(buffer_);
      buffer_ = nullptr;
    }
  }

  size_t size() const { return size_; }

  uint8_t* ptr() const { return buffer_; }

 private:
  uint8_t* buffer_ = nullptr;
  size_t size_;
  bool owned_;
};

class VBuffer {
 public:
  VBuffer() { offsets_.emplace_back(0); }

  void Append(Buffer const& buffer) {
    buffers_.emplace_back(buffer);
    size_ += buffer.size();
    offsets_.emplace_back(size_);
  }

 private:
  std::vector<size_t> offsets_;
  std::vector<Buffer> buffers_;
  size_t size_ = 0;
};

}  // namespace msgpack

}  // namespace vineyard

#endif  // MODULES_MSGPACK_VBUFFER_H_
