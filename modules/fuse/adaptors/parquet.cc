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

#include "fuse/adaptors/parquet.h"

#if defined(WITH_PARQUET)

#include <limits>
#include <memory>

#include "arrow/api.h"
#include "arrow/io/api.h"
#include "parquet/api/reader.h"
#include "parquet/api/schema.h"
#include "parquet/api/writer.h"
#include "parquet/arrow/reader.h"
#include "parquet/arrow/schema.h"
#include "parquet/arrow/writer.h"

namespace vineyard {
namespace fuse {

static std::shared_ptr<arrow::Buffer> view(
    const size_t estimate_size,
    std::vector<std::shared_ptr<arrow::RecordBatch>> const& batches) {
  ::parquet::WriterProperties::Builder builder;
  builder.encoding(::parquet::Encoding::PLAIN);
  builder.disable_dictionary();
  // builder.compression(::parquet::Compression::UNCOMPRESSED);
  builder.disable_statistics();
  builder.write_batch_size(std::numeric_limits<int32_t>::max());
  builder.max_row_group_length(std::numeric_limits<int32_t>::max());
  std::shared_ptr<::parquet::WriterProperties> props = builder.build();

  std::shared_ptr<arrow::Table> table;
  VINEYARD_CHECK_OK(RecordBatchesToTable(batches, &table));
  std::shared_ptr<arrow::io::BufferOutputStream> sink;
  CHECK_ARROW_ERROR_AND_ASSIGN(
      sink, arrow::io::BufferOutputStream::Create(estimate_size + (4 << 20)));
  CHECK_ARROW_ERROR(
      ::parquet::arrow::WriteTable(*table, arrow::default_memory_pool(), sink,
                                   std::numeric_limits<int32_t>::max(), props));
  std::shared_ptr<arrow::Buffer> buffer;
  CHECK_ARROW_ERROR_AND_ASSIGN(buffer, sink->Finish());

  return buffer;
}

std::shared_ptr<arrow::Buffer> parquet_view(
    std::shared_ptr<vineyard::DataFrame>& df) {
  auto estimate_size = df->meta().MemoryUsage();
  auto batch = df->AsBatch();
  return view(estimate_size, {batch});
}

std::shared_ptr<arrow::Buffer> parquet_view(
    std::shared_ptr<vineyard::RecordBatch>& rb) {
  auto estimate_size = rb->meta().MemoryUsage();
  auto batch = rb->GetRecordBatch();
  return view(estimate_size, {batch});
}

std::shared_ptr<arrow::Buffer> parquet_view(
    std::shared_ptr<vineyard::Table>& tb) {
  auto estimate_size = tb->meta().MemoryUsage();
  std::vector<std::shared_ptr<arrow::RecordBatch>> batches;
  for (auto const& batch : tb->batches()) {
    batches.emplace_back(batch->GetRecordBatch());
  }
  return view(estimate_size, batches);
}

static void from_parquet_view(Client* client, std::string const& name,
                              std::shared_ptr<arrow::io::RandomAccessFile> fp) {
  auto pool = arrow::default_memory_pool();
  // Open Parquet file reader
  std::unique_ptr<parquet::arrow::FileReader> arrow_reader;
  CHECK_ARROW_ERROR(parquet::arrow::OpenFile(fp, pool, &arrow_reader));

  // Read entire file as a single Arrow table
  std::shared_ptr<arrow::Table> table;
  CHECK_ARROW_ERROR(arrow_reader->ReadTable(&table));

  // build it into vineyard
  TableBuilder builder(*client, table);
  auto tb = builder.Seal(*client);
  VINEYARD_CHECK_OK(client->Persist(tb->id()));
  VINEYARD_CHECK_OK(client->PutName(tb->id(), name));
}

static void from_parquet_view(Client* client, std::string const& name,
                              std::shared_ptr<arrow::io::BufferReader> reader) {
  from_parquet_view(
      client, name,
      std::dynamic_pointer_cast<arrow::io::RandomAccessFile>(reader));
}

void from_parquet_view(Client* client, std::string const& name,
                       std::shared_ptr<arrow::BufferBuilder> buffer) {
  // recover table from buffer
  auto reader = std::make_shared<arrow::io::BufferReader>(buffer->data(),
                                                          buffer->length());
  from_parquet_view(client, name, reader);
}

void from_parquet_view(Client* client, std::string const& name,
                       std::shared_ptr<arrow::Buffer> buffer) {
  // recover table from buffer
  auto reader =
      std::make_shared<arrow::io::BufferReader>(buffer->data(), buffer->size());
  from_parquet_view(client, name, reader);
}

}  // namespace fuse
}  // namespace vineyard

#endif
