/** Copyright 2021 Alibaba Group Holding Limited.

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

#include <sys/mman.h>

#include <time.h>

#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <chrono>

#include "arrow/status.h"
#include "arrow/util/io_util.h"
#include "glog/logging.h"

#include "basic/ds/array.h"
#include "client/allocator.h"
#include "client/client.h"
#include "client/ds/object_meta.h"
#include "common/util/env.h"
#include "common/util/functions.h"

#define JEMALLOC_NO_DEMANGLE
#include "jemalloc/include/jemalloc/jemalloc.h"
#undef JEMALLOC_NO_DEMANGLE

#include "malloc/allocator.h"

#include "alloc_bench.h"

using namespace vineyard;  // NOLINT(build/namespaces)

#define BENCH_VINEYARD
// #define BENCH_JEMALLOC
// #define BENCH_SYSTEM

// void alloc_with_malloc(size_t rounds, std::vector<size_t> const &sizes) {
//   for (size_t round = 0; round < rounds; ++round) {
//     for (size_t s: sizes) {
//       free(malloc(s));
//     }
//   }
// }

// void alloc_with_jemalloc(size_t rounds, std::vector<size_t> const &sizes) {
//   for (size_t round = 0; round < rounds; ++round) {
//     for (size_t s: sizes) {
//       vineyard_je_free(vineyard_je_malloc(s));
//     }
//   }
// }

// void alloc_with_vineyard_alloc(size_t rounds, std::vector<size_t> const &sizes,
//                                VineyardAllocator<void> &allocator) {
//   for (size_t round = 0; round < rounds; ++round) {
//     for (size_t s: sizes) {
//       allocator.Free(allocator.Allocate(s));
//     }
//   }
// }

size_t GetMillisecondCount()
{
  return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}

void bench() {
  size_t iterCount = 100000000;
  // size_t iterCount = 100;
  size_t maxItems = 1 << 18;  // 512 KB
  uint64_t minItemSizeExp = 10; // 1K
  size_t maxItemSizeExp = 14; // 1K

	size_t dummyCtr = 0;
	size_t rssMax = 0;
	size_t rss;
	size_t allocatedSz = 0;
	size_t allocatedSzMax = 0;

	uint32_t reincarnation = 0;

	Pareto_80_20_6_Data paretoData;
	assert( maxItems <= UINT32_MAX );
	Pareto_80_20_6_Init( paretoData, (uint32_t)maxItems );

	struct TestBin
	{
		uint8_t* ptr;
		uint32_t sz;
		uint32_t reincarnation;
	};

	TestBin* baseBuff = nullptr; 
#if defined(BENCH_SYSTEM)
	baseBuff = reinterpret_cast<TestBin*>( malloc( maxItems * sizeof(TestBin) ) );
#elif defined(BENCH_JEMALLOC)
	baseBuff = reinterpret_cast<TestBin*>( vineyard_je_malloc( maxItems * sizeof(TestBin) ) );
#else
	baseBuff = reinterpret_cast<TestBin*>( vineyard_malloc( maxItems * sizeof(TestBin) ) );
#endif
	assert( baseBuff );
	allocatedSz +=  maxItems * sizeof(TestBin);
	memset( baseBuff, 0, maxItems * sizeof( TestBin ) );

	PRNG rng(41);

  size_t start = GetMillisecondCount();

  for ( size_t k=0 ; k<32; ++k )
	{
		for ( size_t j=0;j<iterCount>>5; ++j )
		{
			uint32_t rnum1 = rng.rng32();
			uint32_t rnum2 = rng.rng32();
			size_t idx = Pareto_80_20_6_Rand( paretoData, rnum1, rnum2 );
      // printf("idx = %d, rnum1 = %d, rnum2 = %d \n", idx, rnum1, rnum2);
			if ( baseBuff[idx].ptr )
			{
#if defined(BENCH_SYSTEM)
				free( baseBuff[idx].ptr );
#elif defined(BENCH_JEMALLOC)
				vineyard_je_free( baseBuff[idx].ptr );
#else
      // LOG(INFO) << "to free: " << baseBuff[idx].ptr;
				vineyard_free( baseBuff[idx].ptr );
#endif

				baseBuff[idx].ptr = 0;
			}
			else
			{
				size_t sz = calcSizeWithStatsAdjustment( std::max(rng.rng64(), (1UL << minItemSizeExp)), maxItemSizeExp );
				baseBuff[idx].sz = (uint32_t)sz;

#if defined(BENCH_SYSTEM)
				baseBuff[idx].ptr = reinterpret_cast<uint8_t*>( malloc( sz ) );
#elif defined(BENCH_JEMALLOC)
				baseBuff[idx].ptr = reinterpret_cast<uint8_t*>( vineyard_je_malloc( sz ) );
#else
				baseBuff[idx].ptr = reinterpret_cast<uint8_t*>( vineyard_malloc( sz ) );
#endif
        // LOG(INFO) << "ptr = " << reinterpret_cast<void *>(baseBuff[idx].ptr);
				memset( baseBuff[idx].ptr, (uint8_t)sz, sz );
			}
		}
	}

  size_t elapsed = GetMillisecondCount() - start;
#if defined(BENCH_SYSTEM)
  LOG(INFO) << "system malloc usage: " << elapsed << " milliseconds";
#elif defined(BENCH_JEMALLOC)
  LOG(INFO) << "jemalloc usage: " << elapsed << " milliseconds";
#else
  LOG(INFO) << "vineyard malloc usage: " << elapsed << " milliseconds";
#endif
}

int main(int argc, char** argv) {
  if (argc < 1) {
    printf("usage ./bench_allocator");
    return 1;
  }

  bench();

  LOG(INFO) << "Finish allocator benchmarks...";
  return 0;
}
