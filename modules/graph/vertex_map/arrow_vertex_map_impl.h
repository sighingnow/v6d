/** Copyright 2020-2023 Alibaba Group Holding Limited.

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

#ifndef MODULES_GRAPH_VERTEX_MAP_ARROW_VERTEX_MAP_IMPL_H_
#define MODULES_GRAPH_VERTEX_MAP_ARROW_VERTEX_MAP_IMPL_H_

#include <algorithm>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "basic/ds/arrow.h"
#include "basic/ds/hashmap.h"
#include "client/client.h"
#include "common/util/typename.h"

#include "graph/fragment/property_graph_types.h"
#include "graph/utils/thread_group.h"
#include "graph/vertex_map/arrow_vertex_map.h"

namespace vineyard {

template <typename OID_T, typename VID_T>
void ArrowVertexMap<OID_T, VID_T>::Construct(const vineyard::ObjectMeta& meta) {
  this->meta_ = meta;
  this->id_ = meta.GetId();

  this->fnum_ = meta.GetKeyValue<fid_t>("fnum");
  this->label_num_ = meta.GetKeyValue<label_id_t>("label_num");

  id_parser_.Init(fnum_, label_num_);
  size_t nbytes = 0, local_oid_total = 0;
  size_t o2g_total_bytes = 0, o2g_size = 0, o2g_bucket_count = 0;
  o2g_.resize(fnum_);
  oid_arrays_.resize(fnum_);
  for (fid_t i = 0; i < fnum_; ++i) {
    o2g_[i].resize(label_num_);
    oid_arrays_[i].resize(label_num_);
    for (label_id_t j = 0; j < label_num_; ++j) {
      o2g_[i][j].Construct(meta.GetMemberMeta("o2g_" + std::to_string(i) + "_" +
                                              std::to_string(j)));

      typename InternalType<oid_t>::vineyard_array_type array;
      array.Construct(meta.GetMemberMeta("oid_arrays_" + std::to_string(i) +
                                         "_" + std::to_string(j)));
      oid_arrays_[i][j] = array.GetArray();

      local_oid_total += array.nbytes();
      o2g_size += o2g_[i][j].size();
      o2g_total_bytes += o2g_[i][j].nbytes();
      o2g_bucket_count += o2g_[i][j].bucket_count();
    }
  }

  nbytes = local_oid_total + o2g_total_bytes;
  double o2g_load_factor =
      o2g_bucket_count == 0 ? 0
                            : static_cast<double>(o2g_size) / o2g_bucket_count;
  VLOG(2) << "ArrowVertexMap<int64_t, uint64_t> "
          << "\n\tmemory: " << prettyprint_memory_size(nbytes)
          << "\n\to2g size: " << o2g_size
          << ", load factor: " << o2g_load_factor
          << "\n\to2g memory: " << prettyprint_memory_size(o2g_total_bytes);
}

template <typename OID_T, typename VID_T>
bool ArrowVertexMap<OID_T, VID_T>::GetOid(vid_t gid, oid_t& oid) const {
  fid_t fid = id_parser_.GetFid(gid);
  label_id_t label = id_parser_.GetLabelId(gid);
  int64_t offset = id_parser_.GetOffset(gid);
  if (fid < fnum_ && label < label_num_ && label >= 0) {
    auto array = oid_arrays_[fid][label];
    if (offset < array->length()) {
      oid = array->GetView(offset);
      return true;
    }
  }
  return false;
}

template <typename OID_T, typename VID_T>
bool ArrowVertexMap<OID_T, VID_T>::GetGid(fid_t fid, label_id_t label_id,
                                          oid_t oid, vid_t& gid) const {
  auto iter = o2g_[fid][label_id].find(oid);
  if (iter != o2g_[fid][label_id].end()) {
    gid = iter->second;
    return true;
  }
  return false;
}

template <typename OID_T, typename VID_T>
bool ArrowVertexMap<OID_T, VID_T>::GetGid(label_id_t label_id, oid_t oid,
                                          vid_t& gid) const {
  for (fid_t i = 0; i < fnum_; ++i) {
    if (GetGid(i, label_id, oid, gid)) {
      return true;
    }
  }
  return false;
}

template <typename OID_T, typename VID_T>
std::vector<OID_T> ArrowVertexMap<OID_T, VID_T>::GetOids(
    fid_t fid, label_id_t label_id) const {
  auto array = oid_arrays_[fid][label_id];
  std::vector<oid_t> oids;

  oids.resize(array->length());
  for (auto i = 0; i < array->length(); i++) {
    oids[i] = array->GetView(i);
  }

  return oids;
}

template <typename OID_T, typename VID_T>
std::shared_ptr<ArrowArrayType<OID_T>>
ArrowVertexMap<OID_T, VID_T>::GetOidArray(fid_t fid, label_id_t label_id) {
  return oid_arrays_[fid][label_id];
}

template <typename OID_T, typename VID_T>
size_t ArrowVertexMap<OID_T, VID_T>::GetTotalNodesNum() const {
  size_t num = 0;
  for (auto& vec : oid_arrays_) {
    for (auto& v : vec) {
      num += v->length();
    }
  }
  return num;
}

template <typename OID_T, typename VID_T>
size_t ArrowVertexMap<OID_T, VID_T>::GetTotalNodesNum(label_id_t label) const {
  size_t num = 0;
  for (auto& vec : oid_arrays_) {
    num += vec[label]->length();
  }
  return num;
}

template <typename OID_T, typename VID_T>
VID_T ArrowVertexMap<OID_T, VID_T>::GetInnerVertexSize(fid_t fid) const {
  size_t num = 0;
  for (auto& v : oid_arrays_[fid]) {
    num += v->length();
  }
  return static_cast<vid_t>(num);
}

template <typename OID_T, typename VID_T>
VID_T ArrowVertexMap<OID_T, VID_T>::GetInnerVertexSize(
    fid_t fid, label_id_t label_id) const {
  return static_cast<vid_t>(oid_arrays_[fid][label_id]->length());
}

template <typename OID_T, typename VID_T>
ObjectID ArrowVertexMap<OID_T, VID_T>::AddVertices(
    Client& client,
    std::map<label_id_t, std::vector<std::shared_ptr<oid_array_t>>>
        oid_arrays_map) {
  int extra_label_num = oid_arrays_map.size();
  std::vector<std::vector<std::shared_ptr<oid_array_t>>> oid_arrays;
  oid_arrays.resize(extra_label_num);
  for (auto& pair : oid_arrays_map) {
    oid_arrays[pair.first - label_num_] = pair.second;
  }
  return AddNewVertexLabels(client, std::move(oid_arrays));
}

template <typename OID_T, typename VID_T>
ObjectID ArrowVertexMap<OID_T, VID_T>::AddVertices(
    Client& client,
    std::map<label_id_t, std::vector<std::shared_ptr<arrow::ChunkedArray>>>
        oid_arrays_map) {
  int extra_label_num = oid_arrays_map.size();
  std::vector<std::vector<std::shared_ptr<arrow::ChunkedArray>>> oid_arrays;
  oid_arrays.resize(extra_label_num);
  for (auto& pair : oid_arrays_map) {
    oid_arrays[pair.first - label_num_] = pair.second;
  }
  return AddNewVertexLabels(client, std::move(oid_arrays));
}

template <typename OID_T, typename VID_T>
ObjectID ArrowVertexMap<OID_T, VID_T>::AddNewVertexLabels(
    Client& client,
    std::vector<std::vector<std::shared_ptr<oid_array_t>>> oid_arrays) {
  std::vector<std::vector<std::vector<std::shared_ptr<oid_array_t>>>> arrays(
      oid_arrays.size());
  for (size_t i = 0; i < oid_arrays.size(); ++i) {
    arrays[i].resize(fnum_);
    for (size_t j = 0; j < fnum_; ++j) {
      arrays[i][j] = {oid_arrays[i][j]};
    }
  }
  return addNewVertexLabels(client, std::move(arrays));
}

template <typename OID_T, typename VID_T>
ObjectID ArrowVertexMap<OID_T, VID_T>::AddNewVertexLabels(
    Client& client,
    std::vector<std::vector<std::shared_ptr<arrow::ChunkedArray>>> oid_arrays) {
  std::vector<std::vector<std::vector<std::shared_ptr<oid_array_t>>>> arrays(
      oid_arrays.size());
  for (size_t i = 0; i < oid_arrays.size(); ++i) {
    arrays[i].resize(fnum_);
    for (size_t j = 0; j < fnum_; ++j) {
      for (auto const& chunk : oid_arrays[i][j]->chunks()) {
        arrays[i][j].emplace_back(
            std::dynamic_pointer_cast<oid_array_t>(chunk));
      }
    }
  }
  return addNewVertexLabels(client, std::move(arrays));
}

template <typename OID_T, typename VID_T>
ObjectID ArrowVertexMap<OID_T, VID_T>::addNewVertexLabels(
    Client& client,
    std::vector<std::vector<std::vector<std::shared_ptr<oid_array_t>>>>
        oid_arrays) {
  using vineyard_oid_array_t =
      typename InternalType<oid_t>::vineyard_array_type;

  label_id_t extra_label_num = oid_arrays.size();

  std::vector<std::vector<vineyard_oid_array_t>> vy_oid_arrays;
  std::vector<std::vector<vineyard::Hashmap<oid_t, vid_t>>> vy_o2g;
  int total_label_num = label_num_ + extra_label_num;
  vy_oid_arrays.resize(fnum_);
  vy_o2g.resize(fnum_);
  for (fid_t i = 0; i < fnum_; ++i) {
    vy_oid_arrays[i].resize(extra_label_num);
    vy_o2g[i].resize(extra_label_num);
  }

  auto fn = [this, &client, &oid_arrays, &vy_oid_arrays, &vy_o2g](
                const label_id_t label, const fid_t fid) -> Status {
    std::shared_ptr<Object> object;
    std::shared_ptr<vineyard_oid_array_t> varray;
    {
      typename InternalType<oid_t>::vineyard_builder_type array_builder(
          client, std::move(oid_arrays[label - label_num_][fid]));
      RETURN_ON_ERROR(array_builder.Seal(client, object));
      varray = std::dynamic_pointer_cast<vineyard_oid_array_t>(object);
      vy_oid_arrays[fid][label - label_num_] = *varray;

      // release the reference
      oid_arrays[label - label_num_][fid].clear();
    }

    {
      vineyard::HashmapBuilder<oid_t, vid_t> builder(client);
      builder.AssociateDataBuffer(varray->GetBuffer());

      auto array = varray->GetArray();
      vid_t cur_gid = id_parser_.GenerateId(fid, label, 0);
      int64_t vnum = array->length();
      builder.reserve(static_cast<size_t>(vnum));
      for (int64_t k = 0; k < vnum; ++k) {
        builder.emplace(array->GetView(k), cur_gid);
        ++cur_gid;
      }
      RETURN_ON_ERROR(builder.Seal(client, object));
      vy_o2g[fid][label - label_num_] =
          *std::dynamic_pointer_cast<vineyard::Hashmap<oid_t, vid_t>>(object);
    }
    return Status::OK();
  };

  ThreadGroup tg(
      (static_cast<fid_t>(std::thread::hardware_concurrency()) + (fnum_ - 1)) /
      fnum_);
  for (label_id_t label = label_num_; label < label_num_ + extra_label_num;
       ++label) {
    for (fid_t fid = 0; fid < fnum_; ++fid) {
      tg.AddTask(fn, label, fid);
    }
  }

  Status status;
  for (auto const& s : tg.TakeResults()) {
    status += s;
  }
  VINEYARD_CHECK_OK(status);

  vineyard::ObjectMeta old_meta, new_meta;
  VINEYARD_CHECK_OK(client.GetMetaData(this->id(), old_meta));

  new_meta.SetTypeName(type_name<ArrowVertexMap<oid_t, vid_t>>());

  new_meta.AddKeyValue("fnum", fnum_);
  new_meta.AddKeyValue("label_num", total_label_num);

  size_t nbytes = 0;
  for (fid_t fid = 0; fid < fnum_; ++fid) {
    for (label_id_t label = 0; label < total_label_num; ++label) {
      std::string array_name =
          "oid_arrays_" + std::to_string(fid) + "_" + std::to_string(label);
      std::string map_name =
          "o2g_" + std::to_string(fid) + "_" + std::to_string(label);
      if (label < label_num_) {
        auto array_meta = old_meta.GetMemberMeta(array_name);
        new_meta.AddMember(array_name, array_meta);
        nbytes += array_meta.GetNBytes();

        auto map_meta = old_meta.GetMemberMeta(map_name);
        new_meta.AddMember(map_name, map_meta);
        nbytes += map_meta.GetNBytes();
      } else {
        new_meta.AddMember(array_name,
                           vy_oid_arrays[fid][label - label_num_].meta());
        nbytes += vy_oid_arrays[fid][label - label_num_].nbytes();

        new_meta.AddMember(map_name, vy_o2g[fid][label - label_num_].meta());
        nbytes += vy_o2g[fid][label - label_num_].nbytes();
      }
    }
  }

  new_meta.SetNBytes(nbytes);
  ObjectID ret;
  VINEYARD_CHECK_OK(client.CreateMetaData(new_meta, ret));
  VLOG(100) << "vertex map memory usage: "
            << prettyprint_memory_size(new_meta.MemoryUsage());
  return ret;
}

template <typename OID_T, typename VID_T>
void ArrowVertexMapBuilder<OID_T, VID_T>::set_fnum_label_num(
    fid_t fnum, label_id_t label_num) {
  fnum_ = fnum;
  label_num_ = label_num;
  oid_arrays_.resize(fnum_);
  o2g_.resize(fnum_);
  for (fid_t i = 0; i < fnum_; ++i) {
    oid_arrays_[i].resize(label_num_);
    o2g_[i].resize(label_num_);
  }
}

template <typename OID_T, typename VID_T>
void ArrowVertexMapBuilder<OID_T, VID_T>::set_oid_array(
    fid_t fid, label_id_t label,
    const typename InternalType<oid_t>::vineyard_array_type& array) {
  oid_arrays_[fid][label] = array;
}

template <typename OID_T, typename VID_T>
void ArrowVertexMapBuilder<OID_T, VID_T>::set_oid_array(
    fid_t fid, label_id_t label,
    const std::shared_ptr<typename InternalType<oid_t>::vineyard_array_type>&
        array) {
  oid_arrays_[fid][label] = *array;
}

template <typename OID_T, typename VID_T>
void ArrowVertexMapBuilder<OID_T, VID_T>::set_o2g(
    fid_t fid, label_id_t label, const vineyard::Hashmap<oid_t, vid_t>& rm) {
  o2g_[fid][label] = rm;
}

template <typename OID_T, typename VID_T>
void ArrowVertexMapBuilder<OID_T, VID_T>::set_o2g(
    fid_t fid, label_id_t label,
    const std::shared_ptr<vineyard::Hashmap<oid_t, vid_t>>& rm) {
  o2g_[fid][label] = *rm;
}

template <typename OID_T, typename VID_T>
Status ArrowVertexMapBuilder<OID_T, VID_T>::_Seal(
    vineyard::Client& client, std::shared_ptr<vineyard::Object>& object) {
  // ensure the builder hasn't been sealed yet.
  ENSURE_NOT_SEALED(this);

  RETURN_ON_ERROR(this->Build(client));

  auto vertex_map = std::make_shared<ArrowVertexMap<oid_t, vid_t>>();
  object = vertex_map;

  vertex_map->fnum_ = fnum_;
  vertex_map->label_num_ = label_num_;
  vertex_map->id_parser_.Init(fnum_, label_num_);

  vertex_map->oid_arrays_.resize(fnum_);
  for (fid_t i = 0; i < fnum_; ++i) {
    auto& array = vertex_map->oid_arrays_[i];
    array.resize(label_num_);
    for (label_id_t j = 0; j < label_num_; ++j) {
      array[j] = oid_arrays_[i][j].GetArray();
    }
  }

  vertex_map->o2g_ = o2g_;

  vertex_map->meta_.SetTypeName(type_name<ArrowVertexMap<oid_t, vid_t>>());

  vertex_map->meta_.AddKeyValue("fnum", fnum_);
  vertex_map->meta_.AddKeyValue("label_num", label_num_);

  size_t nbytes = 0;
  for (fid_t i = 0; i < fnum_; ++i) {
    for (label_id_t j = 0; j < label_num_; ++j) {
      vertex_map->meta_.AddMember(
          "oid_arrays_" + std::to_string(i) + "_" + std::to_string(j),
          oid_arrays_[i][j].meta());
      nbytes += oid_arrays_[i][j].nbytes();

      vertex_map->meta_.AddMember(
          "o2g_" + std::to_string(i) + "_" + std::to_string(j),
          o2g_[i][j].meta());
      nbytes += o2g_[i][j].nbytes();
    }
  }

  vertex_map->meta_.SetNBytes(nbytes);

  RETURN_ON_ERROR(client.CreateMetaData(vertex_map->meta_, vertex_map->id_));
  VLOG(100) << "vertex map memory usage: "
            << prettyprint_memory_size(vertex_map->meta_.MemoryUsage());

  // mark the builder as sealed
  this->set_sealed(true);
  return Status::OK();
}

template <typename OID_T, typename VID_T>
BasicArrowVertexMapBuilder<OID_T, VID_T>::BasicArrowVertexMapBuilder(
    vineyard::Client& client, fid_t fnum, label_id_t label_num,
    std::vector<std::vector<std::shared_ptr<oid_array_t>>> oid_arrays)
    : ArrowVertexMapBuilder<oid_t, vid_t>(client),
      fnum_(fnum),
      label_num_(label_num) {
  CHECK_EQ(oid_arrays.size(), label_num);
  oid_arrays_.resize(oid_arrays.size());
  for (label_id_t label = 0; label < label_num; ++label) {
    oid_arrays_[label].resize(fnum);
    for (fid_t fid = 0; fid < fnum; ++fid) {
      oid_arrays_[label][fid].push_back(std::move(oid_arrays[label][fid]));
    }
  }
  id_parser_.Init(fnum_, label_num_);
}

template <typename OID_T, typename VID_T>
BasicArrowVertexMapBuilder<OID_T, VID_T>::BasicArrowVertexMapBuilder(
    vineyard::Client& client, fid_t fnum, label_id_t label_num,
    std::vector<std::vector<std::shared_ptr<arrow::ChunkedArray>>> oid_arrays)
    : ArrowVertexMapBuilder<oid_t, vid_t>(client),
      fnum_(fnum),
      label_num_(label_num) {
  CHECK_EQ(oid_arrays.size(), label_num);
  oid_arrays_.resize(oid_arrays.size());
  for (label_id_t label = 0; label < label_num; ++label) {
    oid_arrays_[label].resize(fnum);
    for (fid_t fid = 0; fid < fnum; ++fid) {
      oid_arrays_[label][fid].reserve(oid_arrays[label][fid]->num_chunks());
      for (auto const& chunk : oid_arrays[label][fid]->chunks()) {
        oid_arrays_[label][fid].push_back(
            std::dynamic_pointer_cast<oid_array_t>(chunk));
      }
    }
  }
  id_parser_.Init(fnum_, label_num_);
}

template <typename OID_T, typename VID_T>
vineyard::Status BasicArrowVertexMapBuilder<OID_T, VID_T>::Build(
    vineyard::Client& client) {
  using vineyard_oid_array_t =
      typename InternalType<oid_t>::vineyard_array_type;

  this->set_fnum_label_num(fnum_, label_num_);

  auto fn = [&](const label_id_t label, const fid_t fid) -> Status {
    std::shared_ptr<Object> object;
    std::shared_ptr<vineyard_oid_array_t> varray;
    {
      typename InternalType<oid_t>::vineyard_builder_type array_builder(
          client, std::move(oid_arrays_[label][fid]));
      RETURN_ON_ERROR(array_builder.Seal(client, object));
      varray = std::dynamic_pointer_cast<vineyard_oid_array_t>(object);
      this->set_oid_array(fid, label, varray);

      // release the reference
      oid_arrays_[label][fid].clear();
    }
    {
      vineyard::HashmapBuilder<oid_t, vid_t> builder(client);
      builder.AssociateDataBuffer(varray->GetBuffer());

      auto array = varray->GetArray();
      vid_t cur_gid = id_parser_.GenerateId(fid, label, 0);
      int64_t vnum = array->length();
      builder.reserve(static_cast<size_t>(vnum));
      for (int64_t k = 0; k < vnum; ++k) {
        builder.emplace(array->GetView(k), cur_gid);
        ++cur_gid;
      }
      RETURN_ON_ERROR(builder.Seal(client, object));
      this->set_o2g(
          fid, label,
          std::dynamic_pointer_cast<vineyard::Hashmap<oid_t, vid_t>>(object));
    }
    return Status::OK();
  };

  ThreadGroup tg(
      (static_cast<fid_t>(std::thread::hardware_concurrency()) + (fnum_ - 1)) /
      fnum_);
  for (fid_t fid = 0; fid < fnum_; ++fid) {
    for (label_id_t label = 0; label < label_num_; ++label) {
      tg.AddTask(fn, label, fid);
    }
  }

  Status status;
  for (auto const& s : tg.TakeResults()) {
    status += s;
  }
  RETURN_ON_ERROR(status);
  return Status::OK();
}

}  // namespace vineyard

#endif  // MODULES_GRAPH_VERTEX_MAP_ARROW_VERTEX_MAP_IMPL_H_
