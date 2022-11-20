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

#if defined(WITH_CSV)

#include <limits>
#include <memory>

#include <arrow/csv/api.h>
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
  std::shared_ptr<arrow::Table> table;
  VINEYARD_CHECK_OK(RecordBatchesToTable(batches, &table));
  std::shared_ptr<arrow::io::BufferOutputStream> sink;
  CHECK_ARROW_ERROR_AND_ASSIGN(
      sink, arrow::io::BufferOutputStream::Create(estimate_size + (4 << 20)));

  // Write incrementally
  auto write_options = arrow::csv::WriteOptions::Defaults();
  auto maybe_writer =
      arrow::csv::MakeCSVWriter(sink, table->schema(), write_options);
  std::shared_ptr<arrow::ipc::RecordBatchWriter> writer = *maybe_writer;
  CHECK_ARROW_ERROR(writer->WriteTable(*table));
  CHECK_ARROW_ERROR(writer->Close());

  std::shared_ptr<arrow::Buffer> buffer;
  CHECK_ARROW_ERROR_AND_ASSIGN(buffer, sink->Finish());
  return buffer;
}

std::shared_ptr<arrow::Buffer> csv_view(
    std::shared_ptr<vineyard::DataFrame>& df) {
  auto estimate_size = df->meta().MemoryUsage();
  auto batch = df->AsBatch();
  return view(estimate_size, {batch});
}

std::shared_ptr<arrow::Buffer> csv_view(
    std::shared_ptr<vineyard::RecordBatch>& rb) {
  auto estimate_size = rb->meta().MemoryUsage();
  auto batch = rb->GetRecordBatch();
  return view(estimate_size, {batch});
}

std::shared_ptr<arrow::Buffer> csv_view(std::shared_ptr<vineyard::Table>& tb) {
  auto estimate_size = tb->meta().MemoryUsage();
  std::vector<std::shared_ptr<arrow::RecordBatch>> batches;
  for (auto const& batch : tb->batches()) {
    batches.emplace_back(batch->GetRecordBatch());
  }
  return view(estimate_size, batches);
}

static void from_csv_view(Client* client, std::string const& name,
                          std::shared_ptr<arrow::io::RandomAccessFile> fp) {
  arrow::io::IOContext io_context = arrow::io::default_io_context();

  auto read_options = arrow::csv::ReadOptions::Defaults();
  read_options.use_threads = true;
  auto parse_options = arrow::csv::ParseOptions::Defaults();
  parse_options.delimiter = ',';
  auto convert_options = arrow::csv::ConvertOptions::Defaults();

  // Instantiate TableReader from input stream and options
  auto maybe_reader = arrow::csv::TableReader::Make(
      io_context, fp, read_options, parse_options, convert_options);
  std::shared_ptr<arrow::csv::TableReader> reader = *maybe_reader;

  // Read table from CSV file
  auto maybe_table = reader->Read();
  std::shared_ptr<arrow::Table> table = *maybe_table;

  // build it into vineyard
  TableBuilder builder(*client, table);
  auto tb = builder.Seal(*client);
  VINEYARD_CHECK_OK(client->Persist(tb->id()));
  VINEYARD_CHECK_OK(client->PutName(tb->id(), name));
}

static void from_csv_view(Client* client, std::string const& name,
                          std::shared_ptr<arrow::io::BufferReader> reader) {
  from_csv_view(client, name,
                std::dynamic_pointer_cast<arrow::io::RandomAccessFile>(reader));
}

void from_csv_view(Client* client, std::string const& name,
                   std::shared_ptr<arrow::BufferBuilder> buffer) {
  // recover table from buffer
  auto reader = std::make_shared<arrow::io::BufferReader>(buffer->data(),
                                                          buffer->length());
  from_csv_view(client, name, reader);
}

void from_csv_view(Client* client, std::string const& name,
                   std::shared_ptr<arrow::Buffer> buffer) {
  // recover table from buffer
  auto reader =
      std::make_shared<arrow::io::BufferReader>(buffer->data(), buffer->size());
  from_csv_view(client, name, reader);
}

}  // namespace fuse
}  // namespace vineyard

#endif
