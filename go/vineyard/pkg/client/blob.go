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
	"fmt"

	arrow "github.com/apache/arrow/go/v11/arrow/memory"

	"github.com/v6d-io/v6d/go/vineyard/pkg/common/types"
)

type Blob struct {
	ID   types.ObjectID
	Size int

	*arrow.Buffer
}

func (b *Blob) Data() ([]byte, error) {
	if b.Size > 0 && b.Buffer.Len() == 0 {
		return nil, fmt.Errorf("The object might be a (partially) remote object "+
			"and the payload data is not locally available: %d", b.ID)
	}
	return b.Buffer.Bytes(), nil
}

type BufferSet struct {
	// buffer arrow.
}

func (b *BufferSet) EmplaceBuffer(id types.ObjectID) {
}

func (b *BufferSet) Reset() {
}

type BlobWriter struct {
	ID   types.ObjectID
	Size int

	*arrow.Buffer
}

func (b *BlobWriter) Reset(id types.ObjectID, size int, buffer *arrow.Buffer) {
	b.ID = id
	b.Size = size
	b.Buffer = buffer
}
