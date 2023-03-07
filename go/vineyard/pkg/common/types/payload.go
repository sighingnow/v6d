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

package types

type Payload struct {
	ID         ObjectID `json:"object_id"`
	StoreFd    int      `json:"store_fd"`
	ArenaFd    int      `json:"-"`
	DataOffset int      `json:"data_offset"`
	DataSize   int      `json:"data_size"`
	MapSize    int      `json:"map_size"`
	Pointer    *int     `json:"-"`
	Owing      bool     `json:"is_owner"`
	Sealed     bool     `json:"is_sealed"`
	GPU        bool     `json:"is_gpu"`
}
