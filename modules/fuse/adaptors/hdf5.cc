/** Copyright 2020-2022 Alibaba Group Holding Limited.

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

#if defined(WITH_HDF5)

#include <memory>

#include <hdf5.h>

#include <H5LTpublic.h>

#include <highfive/H5Easy.hpp>
#include <highfive/H5PropertyList.hpp>

#include "basic/ds/dataframe.h"
#include "basic/ds/tensor.h"
#include "client/client.h"

namespace H5Easy {

namespace detail {

template <class T>
struct is_vineyard_tensor : std::false_type {};
template <class T>
struct is_vineyard_tensor<vineyard::Tensor<T>> : std::true_type {};

template <typename T>
struct io_impl<T, typename std::enable_if<is_vineyard_tensor<T>::value>::type> {
  inline static std::vector<size_t> shape(std::shared_ptr<T> const& data) {
    auto sh = data->shape();
    return std::vector<size_t>(sh.cbegin(), sh.cend());
  }

  inline static DataSet dump(File& file, const std::string& path,
                             std::shared_ptr<T> const& data,
                             const DumpOptions& options) {
    using value_type = typename std::decay_t<T>::value_t;
    DataSet dataset = initDataset<value_type>(file, path, shape(data), options);
    dataset.write_raw(data->data());
    if (options.flush()) {
      file.flush();
    }
    return dataset;
  }

  inline static T load(vineyard::Client& client, std::string const& name,
                       const File& file, const std::string& path) {
    using value_type = typename std::decay_t<T>::value_t;
    DataSet dataset = file.getDataSet(path);
    std::vector<size_t> dims = dataset.getDimensions();
    std::vector<int64_t> shape{dims.cbegin(), dims.cend()};
    vineyard::TensorBuilder<value_type> builder(client, shape);
    dataset.read(builder.data());
    auto tensor = builder.Seal(client);
    VINEYARD_CHECK_OK(client.Persist(tensor->id()));
    VINEYARD_CHECK_OK(client.PutName(tensor->id(), name));
    return *std::dynamic_pointer_cast<T>(tensor);
  }
};

}  // namespace detail
}  // namespace H5Easy

namespace vineyard {
namespace fuse {

namespace detail {

class h5_memory_access : public HighFive::FileAccessProps {
 public:
  h5_memory_access() : HighFive::FileAccessProps() {
    _initializeIfNeeded();
    auto result = H5Pset_fapl_core(getId(), 1 << (20 + 9), false);
    VINEYARD_ASSERT(result >= 0, "H5Pset_fapl_core failed");
  }
};

std::shared_ptr<arrow::Buffer> get_buffer_from_h5file(hid_t id) {
  ssize_t sz = H5Fget_file_image(id, nullptr, 0);
  auto buffer = arrow::AllocateBuffer(sz).ValueOrDie();
  H5Fget_file_image(id, buffer->mutable_data(), sz);
  return buffer;
}

std::shared_ptr<arrow::Buffer> get_buffer_from_h5file(HighFive::File& hf) {
  return get_buffer_from_h5file(hf.getId());
}

}  // namespace detail

std::shared_ptr<arrow::Buffer> hdf5_view(
    std::shared_ptr<vineyard::ITensor>& tensor) {
  const detail::h5_memory_access& props = detail::h5_memory_access();
  HighFive::File file = HighFive::File("/dummy.h5", H5F_ACC_TRUNC, props);

  if (auto ts = std::dynamic_pointer_cast<Tensor<int32_t>>(tensor)) {
    H5Easy::detail::io_impl<Tensor<int32_t>>::dump(file, "data", ts,
                                                   H5Easy::DumpOptions());
  } else if (auto ts = std::dynamic_pointer_cast<Tensor<uint32_t>>(tensor)) {
    H5Easy::detail::io_impl<Tensor<uint32_t>>::dump(file, "data", ts,
                                                    H5Easy::DumpOptions());
  } else if (auto ts = std::dynamic_pointer_cast<Tensor<int64_t>>(tensor)) {
    H5Easy::detail::io_impl<Tensor<int64_t>>::dump(file, "data", ts,
                                                   H5Easy::DumpOptions());
  } else if (auto ts = std::dynamic_pointer_cast<Tensor<uint64_t>>(tensor)) {
    H5Easy::detail::io_impl<Tensor<uint64_t>>::dump(file, "data", ts,
                                                    H5Easy::DumpOptions());
  } else if (auto ts = std::dynamic_pointer_cast<Tensor<float>>(tensor)) {
    H5Easy::detail::io_impl<Tensor<float>>::dump(file, "data", ts,
                                                 H5Easy::DumpOptions());
  } else if (auto ts = std::dynamic_pointer_cast<Tensor<double>>(tensor)) {
    H5Easy::detail::io_impl<Tensor<double>>::dump(file, "data", ts,
                                                  H5Easy::DumpOptions());
  } else {
    VINEYARD_ASSERT(false,
                    "Unsupported tensor type: " + tensor->meta().GetTypeName());
  }
  return detail::get_buffer_from_h5file(file);
}

static void from_hdf5_view(Client* client, std::string const& name,
                           const void* data, size_t size) {
  hid_t file_id = H5LTopen_file_image(const_cast<void*>(data), size,
                                      H5LT_FILE_IMAGE_DONT_RELEASE);
  hid_t root_id = H5Gopen(file_id, "/", H5P_DEFAULT);
  HighFive::File file(root_id);
  H5Easy::detail::io_impl<Tensor<double>>::load(*client, name, file, "data");
}

void from_hdf5_view(Client* client, std::string const& name,
                    std::shared_ptr<arrow::BufferBuilder> buffer) {
  from_hdf5_view(client, name, buffer->mutable_data(), buffer->length());
}

void from_hdf5_view(Client* client, std::string const& name,
                    std::shared_ptr<arrow::Buffer> buffer) {
  from_hdf5_view(client, name, buffer->data(), buffer->size());
}

}  // namespace fuse
}  // namespace vineyard

#endif  // WITH_HDF5
