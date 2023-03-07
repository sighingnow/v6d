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

package client

import (
	"strconv"

	"github.com/v6d-io/v6d/go/vineyard/pkg/common/types"
)

type ObjectMeta struct {
	meta       map[string]any
	bufferSet  BufferSet
	inComplete bool
}

func (m *ObjectMeta) Init() {
	m.meta = make(map[string]any)
}

func (m *ObjectMeta) SetInstanceId(id types.InstanceID) {
	m.meta["instance_id"] = id
}

func (m *ObjectMeta) SetTransient(transient bool) {
	m.meta["transient"] = true
}

func (m *ObjectMeta) AddKeyValue(key string, value any) {
	m.meta[key] = value
}

func (m *ObjectMeta) HasKey(key string) bool {
	if _, ok := m.meta[key]; !ok {
		return false
	}
	return true
}

func (m *ObjectMeta) SetNBytes(nbytes int) {
	m.meta["nbytes"] = nbytes
}

func (m *ObjectMeta) InComplete() bool {
	return m.inComplete
}

func (m *ObjectMeta) MetaData() any {
	return m.meta
}

func (m *ObjectMeta) SetId(id types.ObjectID) {
	m.meta["id"] = strconv.FormatUint(id, 10)
}

func (m *ObjectMeta) SetSignature(signature types.Signature) {
	m.meta["signature"] = signature
}

func (m *ObjectMeta) Reset() {
	m.meta = make(map[string]any)
	m.bufferSet.Reset()
	m.inComplete = false
}

func (m *ObjectMeta) SetMetaData(val map[string]any) {
	m.meta = val
}
