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
	"testing"

	"github.com/apache/arrow/go/v11/arrow"
	"github.com/apache/arrow/go/v11/arrow/array"
	"github.com/apache/arrow/go/v11/arrow/memory"
	"github.com/v6d-io/v6d/go/vineyard/pkg/common"
	"github.com/v6d-io/v6d/go/vineyard/pkg/common/types"
)

func TestIPCClientConnect(t *testing.T) {
	client, err := NewIPCClient(GetDefaultIPCSocket())
	if err != nil {
		t.Fatalf("connect to ipc server failed: %+v", err)
	}
	defer client.Disconnect()
}

func TestIPCClientBuffer(t *testing.T) {
	client, err := NewIPCClient(GetDefaultIPCSocket())
	if err != nil {
		t.Fatalf("connect to ipc server failed: %+v", err)
	}
	defer client.Disconnect()

	// create buffer
	buffer, err := client.CreateBuffer(1024)
	if err != nil {
		t.Fatalf("create buffer failed: %+v", err)
	}
	bufferBytes := buffer.Bytes()
	for i := 0; i < 1024; i++ {
		bufferBytes[i] = byte(i)
	}

	// get buffer
	buffer2, err := client.GetBuffer(buffer.ID, true)
	if err != nil {
		t.Fatalf("get buffer failed: %+v", err)
	}
	for i := 0; i < 1024; i++ {
		if bufferBytes[i] != buffer2.Bytes()[i] {
			t.Fatalf("buffer content not match")
		}
	}
}

func TestIPCClientName(t *testing.T) {
	client, err := NewIPCClient(GetDefaultIPCSocket())
	if err != nil {
		t.Fatalf("connect to ipc server failed: %+v", err)
	}
	defer client.Disconnect()

	name := "test_name"
	nameNoExist := "undefined_name"

	var id1 types.ObjectID = types.GenerateObjectID()
	if err := client.PutName(id1, name); err != nil {
		if status, ok := err.(*common.Status); ok {
			t.Log("get name return code", status.Code)
		} else {
			t.Error("get name failed", err)
		}
	}
	id2, err := client.GetName(name, false)
	if err != nil {
		if status, ok := err.(*common.Status); ok {
			if status.Code == common.KObjectNotExists {
				t.Log("get object not exist")
			}
		} else {
			t.Error("get name failed", err)
		}
	}
	if id1 != id2 {
		t.Error("put name id and get name id is not match")
	}
	t.Log("put name and get name success!")

	if _, err := client.GetName(nameNoExist, false); err != nil {
		if status, ok := err.(*common.Status); ok {
			if status.Code == common.KObjectNotExists {
				t.Log("get object not exist")
			}
		} else {
			t.Error("get name failed", err)
		}
	}
	t.Log("get no exist name test success")

	if err := client.DropName(name); err != nil {
		if status, ok := err.(*common.Status); ok {
			if status.Code == common.KObjectNotExists {
				t.Log("drop object not exist")
			}
		} else {
			t.Error("drop name failed", err)
		}
	}
	t.Log("drop name success")

	if _, err := client.GetName(name, false); err != nil {
		if status, ok := err.(*common.Status); ok {
			if status.Code == common.KObjectNotExists {
				t.Log("get object not exist")
			}
		} else {
			t.Error("get name failed", err)
		}
	}
}

func TestIPCClientArrowDataStructure(t *testing.T) {
	pool := memory.NewGoAllocator()

	lb := array.NewFixedSizeListBuilder(pool, 3, arrow.PrimitiveTypes.Int64)
	defer lb.Release()

	vb := lb.ValueBuilder().(*array.Int64Builder)
	defer vb.Release()

	vb.Reserve(10)

	lb.Append(true)
	vb.Append(0)
	vb.Append(1)
	vb.Append(2)

	lb.AppendNull()
	vb.AppendValues([]int64{-1, -1, -1}, nil)

	lb.Append(true)
	vb.Append(3)
	vb.Append(4)
	vb.Append(5)

	lb.Append(true)
	vb.Append(6)
	vb.Append(7)
	vb.Append(8)

	lb.AppendNull()

	arr := lb.NewArray().(*array.FixedSizeList)
	defer arr.Release()

	fmt.Printf("NullN()   = %d\n", arr.NullN())
	fmt.Printf("Len()     = %d\n", arr.Len())
	fmt.Printf("Type()    = %v\n", arr.DataType())
	fmt.Printf("List      = %v\n", arr)

	client, err := NewIPCClient(GetDefaultIPCSocket())
	if err != nil {
		t.Fatalf("connect to ipc server failed: %+v", err)
	}
	defer client.Disconnect()

	// // var array vineyard.ArrayBuilder
	// // array.Init(&ipcClient, arr)
	// // array.Seal()
	// // // TODO: how to get array's id

	// ipcClient.Persist(array.Id())
}
