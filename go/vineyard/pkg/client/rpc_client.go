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
	"net"
	"strconv"
	"strings"

	"github.com/v6d-io/v6d/go/vineyard/pkg/client/io"
	"github.com/v6d-io/v6d/go/vineyard/pkg/common"
	"github.com/v6d-io/v6d/go/vineyard/pkg/common/types"
)

type RPCClient struct {
	ClientBase
	remoteInstanceID types.InstanceID
}

func NewRPCClient(rpcEndpoint string) (*RPCClient, error) {
	c := &RPCClient{}
	c.RPCEndpoint = rpcEndpoint

	addresses := strings.Split(rpcEndpoint, ":")
	var port uint16
	if addresses[1] == "" {
		_, port = GetDefaultRPCHostAndPort()
	} else {
		if p, err := strconv.Atoi(addresses[1]); err != nil {
			_, port = GetDefaultRPCHostAndPort()
		} else {
			port = uint16(p)
		}
	}

	var conn net.Conn
	if err := io.ConnectRPCSocketRetry(addresses[0], port, &conn); err != nil {
		return nil, err
	}
	c.conn = conn

	messageOut := common.WriteRegisterRequest(common.VINEYARD_VERSION_STRING)
	if err := c.doWrite(messageOut); err != nil {
		return nil, err
	}
	var reply common.RegisterReply
	if err := c.doReadReply(&reply); err != nil {
		return nil, err
	}

	c.connected = true
	c.IPCSocket = reply.IPCSocket
	c.instanceID = types.UnspecifiedInstanceID()
	c.serverVersion = reply.Version

	c.remoteInstanceID = reply.InstanceID
	// TODO: compatible server check
	return c, nil
}
