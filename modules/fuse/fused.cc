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

#include "fuse/fused.h"

#include <limits>
#include <unordered_map>

#include "boost/algorithm/string/predicate.hpp"

#include "basic/ds/array.h"
#include "basic/ds/dataframe.h"
#include "client/client.h"
#include "client/ds/blob.h"
#include "client/ds/i_object.h"
#include "common/util/logging.h"
#include "common/util/uuid.h"
#include "fuse/adaptors/formats.h"

namespace vineyard {

namespace fuse {

struct fs::fs_state_t fs::state {};

static std::string name_from_path(std::string const& path,
                                  std::string const& suffix) {
  return path.substr(
      1, path.length() - suffix.length() /* .arrow/.parquet/.h5 */ - 1);
}

static std::string name_from_path(std::string const& path) {
  if (boost::algorithm::ends_with(path, ".arrow")) {
    return name_from_path(path, ".arrow");
  } else if (boost::algorithm::ends_with(path, ".csv")) {
#if defined(WITH_CSV)
    return name_from_path(path, ".csv");
#endif
  } else if (boost::algorithm::ends_with(path, ".parquet")) {
#if defined(WITH_PARQUET)
    return name_from_path(path, ".parquet");
#endif
  }
  if (boost::algorithm::ends_with(path, ".h5")) {
#if defined(WITH_HDF5)
    return name_from_path(path, ".h5");
#endif
  } else {
    return "";
  }
}

static inline bool ends_with(std::string const& value,
                             std::string const& ending) {
  if (ending.size() > value.size())
    return false;
  return std::equal(ending.rbegin(), ending.rend(), value.rbegin());
}

static std::shared_ptr<arrow::Buffer> generate_fuse_view(
    std::string const& path, std::shared_ptr<Object> object) {
  if (ends_with(path, ".arrow")) {
    if (auto dataframe =
            std::dynamic_pointer_cast<vineyard::DataFrame>(object)) {
      return fuse::arrow_view(dataframe);
    } else if (auto recordbatch =
                   std::dynamic_pointer_cast<vineyard::RecordBatch>(object)) {
      return fuse::arrow_view(recordbatch);
    } else if (auto table =
                   std::dynamic_pointer_cast<vineyard::Table>(object)) {
      return fuse::arrow_view(table);
    }
  } else if (ends_with(path, ".csv")) {
#if defined(WITH_CSV)
    if (auto dataframe =
            std::dynamic_pointer_cast<vineyard::DataFrame>(object)) {
      return fuse::csv_view(dataframe);
    } else if (auto recordbatch =
                   std::dynamic_pointer_cast<vineyard::RecordBatch>(object)) {
      return fuse::csv_view(recordbatch);
    } else if (auto table =
                   std::dynamic_pointer_cast<vineyard::Table>(object)) {
      return fuse::csv_view(table);
    }
#endif
  } else if (ends_with(path, ".parquet")) {
#if defined(WITH_PARQUET)
    if (auto dataframe =
            std::dynamic_pointer_cast<vineyard::DataFrame>(object)) {
      return fuse::parquet_view(dataframe);
    } else if (auto recordbatch =
                   std::dynamic_pointer_cast<vineyard::RecordBatch>(object)) {
      return fuse::parquet_view(recordbatch);
    } else if (auto table =
                   std::dynamic_pointer_cast<vineyard::Table>(object)) {
      return fuse::parquet_view(table);
    }
#endif
  } else if (ends_with(path, ".h5")) {
#if defined(WITH_HDF5)
    if (auto tensor = std::dynamic_pointer_cast<vineyard::ITensor>(object)) {
      return fuse::hdf5_view(tensor);
    }
#endif
  }

  VINEYARD_ASSERT(
      false, "Unsupported vineyard data type: " + object->meta().GetTypeName());
  return nullptr;
}

static void build_from_fuse_view(Client* client, std::string const& path,
                                 std::shared_ptr<arrow::BufferBuilder> buffer) {
  if (ends_with(path, ".arrow")) {
    fuse::from_arrow_view(client, name_from_path(path, ".arrow"), buffer);
  } else if (ends_with(path, ".csv")) {
    fuse::from_csv_view(client, name_from_path(path, ".csv"), buffer);
  } else if (ends_with(path, ".parquet")) {
    fuse::from_parquet_view(client, name_from_path(path, ".parquet"), buffer);
  } else if (ends_with(path, ".h5")) {
    fuse::from_hdf5_view(client, name_from_path(path, ".h5"), buffer);
  } else {
    VINEYARD_ASSERT(false, "Unsupported vineyard file type: " + path);
  }
}

int fs::fuse_getattr(const char* path, struct stat* stbuf,
                     struct fuse_file_info*) {
  VLOG(2) << "fuse: getattr on " << path;
  std::lock_guard<std::mutex> guard(state.mtx_);

  memset(stbuf, 0, sizeof(struct stat));
  if (strcmp(path, "/") == 0) {
    stbuf->st_mode = S_IFDIR | 0755;
    stbuf->st_nlink = 2;
    return 0;
  }

  stbuf->st_mode = S_IFREG | 0444;
  stbuf->st_nlink = 1;

  {
    auto iter = state.views.find(path);
    if (iter != state.views.end()) {
      stbuf->st_size = iter->second->size();
      return 0;
    }
  }
  {
    auto iter = state.mutable_views.find(path);
    if (iter != state.mutable_views.end()) {
      stbuf->st_size = iter->second->length();
      return 0;
    }
  }

  {
    std::string path_string(path);
    std::string target_name = name_from_path(path_string);
    if (target_name.empty()) {
      return -ENOENT;
    }
    ObjectID target = InvalidObjectID();
    auto status = state.client->GetName(target_name, target);
    if (!status.ok()) {
      return -ENOENT;
    }
    bool exists = false;
    VINEYARD_CHECK_OK(state.client->Exists(target, exists));
    if (!exists) {
      return -ENOENT;
    }

    auto buffer = generate_fuse_view(path, state.client->GetObject(target));
    state.views.emplace(path, buffer);
    stbuf->st_size = buffer->size();
    return 0;
  }
}

int fs::fuse_open(const char* path, struct fuse_file_info* fi) {
  VLOG(2) << "fuse: open " << path << " with mode " << fi->flags;

  if ((fi->flags & O_ACCMODE) != O_RDONLY) {
    return -EACCES;
  }
  std::lock_guard<std::mutex> guard(state.mtx_);

  std::string path_string(path);
  ObjectID target = InvalidObjectID();
  std::string target_name = name_from_path(path_string);
  if (target_name.empty()) {
    return -ENOENT;
  }
  VINEYARD_CHECK_OK(state.client->GetName(target_name, target));

  auto object = state.client->GetObject(target);
  if (object == nullptr) {
    return -ENOENT;
  }
  auto loc = state.views.find(path_string);
  if (loc == state.views.end()) {
    auto buffer = generate_fuse_view(path, state.client->GetObject(target));
    state.views[path_string] = buffer;
  }
  // bypass kernel's page cache to avoid knowing the size in `getattr`.
  //
  // see also:
  // https://stackoverflow.com/questions/46267972/fuse-avoid-calculating-size-in-getattr
  fi->direct_io = 1;
  return 0;
}

int fs::fuse_read(const char* path, char* buf, size_t size, off_t offset,
                  struct fuse_file_info* fi) {
  VLOG(2) << "fuse: read " << path << " from " << offset << ", expect " << size
          << " bytes";
  std::unordered_map<std::string,
                     std::shared_ptr<arrow::Buffer>>::const_iterator loc;
  {
    std::lock_guard<std::mutex> guard(state.mtx_);
    loc = state.views.find(path);
  }
  if (loc == state.views.end()) {
    return -ENOENT;
  }
  auto buffer = loc->second;
  if (offset >= buffer->size()) {
    return 0;
  } else {
    size_t slice = size;
    if (slice > static_cast<size_t>(buffer->size() - offset)) {
      slice = buffer->size() - offset;
    }
    memcpy(buf, buffer->data() + offset, slice);
    return slice;
  }
}

int fs::fuse_write(const char* path, const char* buf, size_t size, off_t offset,
                   struct fuse_file_info* fi) {
  VLOG(2) << "fuse: write " << path << " from " << offset << ", expect " << size
          << " bytes";

  auto loc = state.mutable_views.find(path);
  if (loc == state.mutable_views.end()) {
    return -ENOENT;
  }
  auto buffer = loc->second;
  if (static_cast<int64_t>(offset + size) >= buffer->capacity()) {
    VINEYARD_CHECK_OK(buffer->Reserve(offset + size));
  }
  memcpy(buffer->mutable_data() + offset, buf, size);
  if (static_cast<int64_t>(offset + size) > buffer->length()) {
    buffer->UnsafeAdvance((offset + size) - buffer->length());
  }
  return size;
}

int fs::fuse_statfs(const char* path, struct statvfs*) {
  VLOG(2) << "fuse: statfs " << path;
  return 0;
}

int fs::fuse_flush(const char* path, struct fuse_file_info*) {
  VLOG(2) << "fuse: flush " << path;
  return 0;
}

int fs::fuse_release(const char* path, struct fuse_file_info*) {
  VLOG(2) << "fuse: release " << path;

  {
      // TODO: ref count should be used
      //
      // auto loc = state.views.find(path);
      // if (loc != state.views.end()) {
      //   state.views.erase(loc);
      //   return 0;
      // }
  }

  {
    auto loc = state.mutable_views.find(path);
    if (loc != state.mutable_views.end()) {
      build_from_fuse_view(state.client.get(), loc->first, loc->second);
      loc->second->Reset();
      state.mutable_views.erase(loc);
      return 0;
    }
  }

  return -ENOENT;
}

int fs::fuse_getxattr(const char* path, const char* name, char*, size_t) {
  VLOG(2) << "fuse: getxattr " << path << ": name";
  return 0;
}

int fs::fuse_opendir(const char* path, struct fuse_file_info* info) {
  VLOG(2) << "fuse: opendir " << path;
  return 0;
}

int fs::fuse_readdir(const char* path, void* buf, fuse_fill_dir_t filler,
                     off_t offset, struct fuse_file_info* fi,
                     enum fuse_readdir_flags flags) {
  VLOG(2) << "fuse: readdir " << path;

  if (strcmp(path, "/") != 0) {
    return -ENOENT;
  }

  filler(buf, ".", NULL, 0, fuse_fill_dir_flags::FUSE_FILL_DIR_PLUS);
  filler(buf, "..", NULL, 0, fuse_fill_dir_flags::FUSE_FILL_DIR_PLUS);

  std::vector<std::string> supported_types{
      "vineyard::DataFrame",
      "vineyard::RecordBatch",
      "vineyard::Table",
      "vineyard::Tensor*",
  };

  std::unordered_map<ObjectID, json> metas{};
  for (auto const& type : supported_types) {
    VINEYARD_CHECK_OK(state.client->ListData(
        type, false, std::numeric_limits<size_t>::max(), metas));
  }

  for (auto const& item : metas) {
    // std::string base = ObjectIDToString(item.first) + ".arrow";
    // filler(buf, base.c_str(), NULL, 0,
    // fuse_fill_dir_flags::FUSE_FILL_DIR_PLUS);

    // only filling those objects with names.
    if (!item.second.contains("__name")) {
      continue;
    }
    std::string name = item.second["__name"].get<std::string>();
    std::string type = item.second["typename"].get<std::string>();
    if (type == "vineyard::DataFrame" || type == "vineyard::Table" ||
        type == "vineyard::RecordBatch") {
      std::string parquet_name = name + ".parquet";
      std::string arrow_name = name + ".arrow";
      filler(buf, parquet_name.c_str(), NULL, 0,
             fuse_fill_dir_flags::FUSE_FILL_DIR_PLUS);
      filler(buf, arrow_name.c_str(), NULL, 0,
             fuse_fill_dir_flags::FUSE_FILL_DIR_PLUS);
    } else if (type.find_first_of("vineyard::Tensor") == 0) {
      std::string hdf5_name = name + ".h5";
      filler(buf, hdf5_name.c_str(), NULL, 0,
             fuse_fill_dir_flags::FUSE_FILL_DIR_PLUS);
    }
  }
  return 0;
}

void* fs::fuse_init(struct fuse_conn_info* conn, struct fuse_config* cfg) {
  VLOG(2) << "fuse: initfs with vineyard socket " << state.vineyard_socket;

  state.client.reset(new vineyard::Client());
  VINEYARD_CHECK_OK(state.client->Connect(state.vineyard_socket));

  fuse_apply_conn_info_opts(state.conn_opts, conn);

  cfg->kernel_cache = 0;
  return NULL;
}

void fs::fuse_destroy(void* private_data) {
  VLOG(2) << "fuse: destroy";

  state.views.clear();
  state.client->Disconnect();
}

int fs::fuse_access(const char* path, int mode) {
  VLOG(2) << "fuse: access " << path << " with mode " << mode;

  return mode;
}

int fs::fuse_create(const char* path, mode_t mode, struct fuse_file_info*) {
  VLOG(2) << "fuse: create " << path << " with mode " << mode;
  if (state.mutable_views.find(path) != state.mutable_views.end()) {
    LOG(ERROR) << "fuse: create: file already exists" << path;
    return EEXIST;
  }
  state.mutable_views.emplace(
      path, std::shared_ptr<arrow::BufferBuilder>(new arrow::BufferBuilder()));
  return 0;
}

}  // namespace fuse

}  // namespace vineyard
