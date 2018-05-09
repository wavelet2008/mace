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

#ifndef MACE_CORE_RUNTIME_OPENCL_OPENCL_RUNTIME_H_
#define MACE_CORE_RUNTIME_OPENCL_OPENCL_RUNTIME_H_

#include <map>
#include <memory>
#include <mutex>  // NOLINT(build/c++11)
#include <set>
#include <string>
#include <vector>

#include "mace/core/future.h"
#include "mace/core/runtime/opencl/cl2_header.h"
#include "mace/core/runtime/opencl/opencl_wrapper.h"
#include "mace/public/mace_runtime.h"
#include "mace/utils/string_util.h"
#include "mace/utils/timer.h"

namespace mace {

enum GPUType {
  QUALCOMM_ADRENO,
  MALI,
  PowerVR,
  UNKNOWN,
};


const std::string OpenCLErrorToString(cl_int error);

#define MACE_CHECK_CL_SUCCESS(error) \
  MACE_CHECK(error == CL_SUCCESS) << "error: " << OpenCLErrorToString(error)

class OpenCLProfilingTimer : public Timer {
 public:
  explicit OpenCLProfilingTimer(const cl::Event *event)
      : event_(event), accumulated_micros_(0) {}
  void StartTiming() override;
  void StopTiming() override;
  void AccumulateTiming() override;
  void ClearTiming() override;
  double ElapsedMicros() override;
  double AccumulatedMicros() override;

 private:
  const cl::Event *event_;
  double start_nanos_;
  double stop_nanos_;
  double accumulated_micros_;
};

class OpenCLRuntime {
 public:
  static OpenCLRuntime *Global();
  static void Configure(GPUPerfHint, GPUPriorityHint);

  cl::Context &context();
  cl::Device &device();
  cl::CommandQueue &command_queue();
  GPUType gpu_type() const;
  const std::string platform_info() const;
  uint64_t device_global_mem_cache_size() const;
  uint32_t device_compute_units() const;

  void GetCallStats(const cl::Event &event, CallStats *stats);
  uint64_t GetDeviceMaxWorkGroupSize();
  uint64_t GetKernelMaxWorkGroupSize(const cl::Kernel &kernel);
  uint64_t GetKernelWaveSize(const cl::Kernel &kernel);
  bool IsNonUniformWorkgroupsSupported() const;
  bool IsOutOfRangeCheckEnabled() const;
  bool is_profiling_enabled() const;

  cl::Kernel BuildKernel(const std::string &program_name,
                         const std::string &kernel_name,
                         const std::set<std::string> &build_options);

  void SaveBuiltCLProgram();

 private:
  OpenCLRuntime();
  ~OpenCLRuntime();
  OpenCLRuntime(const OpenCLRuntime &) = delete;
  OpenCLRuntime &operator=(const OpenCLRuntime &) = delete;

  void BuildProgram(const std::string &program_file_name,
                    const std::string &binary_file_name,
                    const std::string &build_options,
                    cl::Program *program);
  bool BuildProgramFromBinary(
      const std::string &built_program_key,
      const std::string &build_options_str,
      cl::Program *program);
  bool BuildProgramFromCache(
      const std::string &built_program_key,
      const std::string &build_options_str,
      cl::Program *program);
  void BuildProgramFromSource(
      const std::string &program_name,
      const std::string &built_program_key,
      const std::string &build_options_str,
      cl::Program *program);
  const std::string ParseDeviceVersion(const std::string &device_version);

 private:
  std::unique_ptr<KVStorage> storage_;
  bool is_profiling_enabled_;
  // All OpenCL object must be a pointer and manually deleted before unloading
  // OpenCL library.
  std::shared_ptr<cl::Context> context_;
  std::shared_ptr<cl::Device> device_;
  std::shared_ptr<cl::CommandQueue> command_queue_;
  std::map<std::string, cl::Program> built_program_map_;
  std::mutex program_build_mutex_;
  std::string platform_info_;
  std::string opencl_version_;
  bool out_of_range_check_;
  uint64_t device_gloabl_mem_cache_size_;
  uint32_t device_compute_units_;
  GPUType gpu_type_;

  static GPUPerfHint kGPUPerfHint;
  static GPUPriorityHint kGPUPriorityHint;
};

}  // namespace mace

#endif  // MACE_CORE_RUNTIME_OPENCL_OPENCL_RUNTIME_H_
