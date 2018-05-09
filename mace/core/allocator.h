// Copyright 2018 Xiaomi, Inc.  All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef MACE_CORE_ALLOCATOR_H_
#define MACE_CORE_ALLOCATOR_H_

#include <stdlib.h>
#include <map>
#include <limits>
#include <vector>
#include <cstring>

#include "mace/core/macros.h"
#include "mace/core/registry.h"
#include "mace/core/types.h"
#include "mace/public/mace.h"

namespace mace {

#if defined(__hexagon__)
constexpr size_t kMaceAlignment = 128;
#elif defined(__ANDROID__)
// 16 bytes = 128 bits = 32 * 4 (Neon)
constexpr size_t kMaceAlignment = 16;
#else
// 32 bytes = 256 bits (AVX512)
constexpr size_t kMaceAlignment = 32;
#endif

class Allocator {
 public:
  Allocator() {}
  virtual ~Allocator() noexcept {}
  virtual void *New(size_t nbytes) const = 0;
  virtual void *NewImage(const std::vector<size_t> &image_shape,
                         const DataType dt) const = 0;
  virtual void Delete(void *data) const = 0;
  virtual void DeleteImage(void *data) const = 0;
  virtual void *Map(void *buffer, size_t offset, size_t nbytes) const = 0;
  virtual void *MapImage(void *buffer,
                         const std::vector<size_t> &image_shape,
                         std::vector<size_t> *mapped_image_pitch) const = 0;
  virtual void Unmap(void *buffer, void *mapper_ptr) const = 0;
  virtual bool OnHost() const = 0;

  template <typename T>
  T *New(size_t num_elements) {
    if (num_elements > (std::numeric_limits<size_t>::max() / sizeof(T))) {
      return nullptr;
    }
    void *p = New(sizeof(T) * num_elements);
    T *typed_p = reinterpret_cast<T *>(p);
    return typed_p;
  }
};

class CPUAllocator : public Allocator {
 public:
  ~CPUAllocator() override {}
  void *New(size_t nbytes) const override {
    VLOG(3) << "Allocate CPU buffer: " << nbytes;
    void *data = nullptr;
#if defined(__ANDROID__) || defined(__hexagon__)
    data = memalign(kMaceAlignment, nbytes);
#else
    MACE_CHECK(posix_memalign(&data, kMaceAlignment, nbytes) == 0);
#endif
    MACE_CHECK_NOTNULL(data);
    // TODO(heliangliang) This should be avoided sometimes
    memset(data, 0, nbytes);
    return data;
  }

  void *NewImage(const std::vector<size_t> &shape,
                 const DataType dt) const override {
    MACE_UNUSED(shape);
    MACE_UNUSED(dt);
    LOG(FATAL) << "Allocate CPU image";
    return nullptr;
  }

  void Delete(void *data) const override {
    VLOG(3) << "Free CPU buffer";
    free(data);
  }
  void DeleteImage(void *data) const override {
    LOG(FATAL) << "Free CPU image";
    free(data);
  };
  void *Map(void *buffer, size_t offset, size_t nbytes) const override {
    MACE_UNUSED(nbytes);
    return reinterpret_cast<char*>(buffer) + offset;
  }
  void *MapImage(void *buffer,
                 const std::vector<size_t> &image_shape,
                 std::vector<size_t> *mapped_image_pitch) const override {
    MACE_UNUSED(image_shape);
    MACE_UNUSED(mapped_image_pitch);
    return buffer;
  }
  void Unmap(void *buffer, void *mapper_ptr) const override {
    MACE_UNUSED(buffer);
    MACE_UNUSED(mapper_ptr);
  }
  bool OnHost() const override { return true; }
};

std::map<int32_t, Allocator *> *gAllocatorRegistry();

Allocator *GetDeviceAllocator(DeviceType type);

struct AllocatorRegisterer {
  explicit AllocatorRegisterer(DeviceType type, Allocator *alloc) {
    if (gAllocatorRegistry()->count(type)) {
      LOG(ERROR) << "Allocator for device type " << type
                 << " registered twice. This should not happen."
                 << gAllocatorRegistry()->count(type);
      std::exit(1);
    }
    gAllocatorRegistry()->emplace(type, alloc);
  }
};

#define MACE_REGISTER_ALLOCATOR(type, alloc)                                  \
  namespace {                                                                 \
  static AllocatorRegisterer MACE_ANONYMOUS_VARIABLE(Allocator)(type, alloc); \
  }

}  // namespace mace

#endif  // MACE_CORE_ALLOCATOR_H_
