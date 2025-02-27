#! /usr/bin/env python
# -*- coding: utf-8 -*-
#
# Copyright 2020-2023 Alibaba Group Holding Limited.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

import contextlib
import copy
import inspect
import threading
import warnings

from vineyard._C import IPCClient
from vineyard._C import Object
from vineyard._C import ObjectID
from vineyard._C import ObjectMeta
from vineyard._C import RPCClient


class BuilderContext:
    def __init__(self):
        self.__factory = dict()

    def __str__(self) -> str:
        return str(self.__factory)

    def register(self, type_id, builder):
        """Register a Python type to the builder context.

        Parameters
        ----------
        type_id: Python type
            Like `int`, or `numpy.ndarray`

        builder: callable, e.g., a method, callable object
            A builder translates a python object to vineyard, it accepts a Python
            value as parameter, and returns an vineyard object as result.
        """
        self.__factory[type_id] = builder

    def run(self, client, value, **kw):
        """Follows the MRO to find the proper builder for given python value.

        Here "Follows the MRO" implies:

        - If the type of python value has been found in the context, the registered
          builder will be used.

        - If not, it follows the MRO chain from down to top to find a registered
          Python type and used the associated builder.

        - When the traversal reaches the :code:`object` type, since there's a default
          builder that serialization the python value, the parameter will be serialized
          and be put into a blob.
        """

        # if the python value comes from a vineyard object, we choose to just reuse it.

        # N.B.: don't do that.
        #
        # we still construct meta, and optimize the duplicate copy in blob builder
        #
        # base = getattr(value, '__vineyard_ref', None)
        # if base:
        #     return base.meta

        for ty in type(value).__mro__:
            if ty in self.__factory:
                builder_func_sig = inspect.getfullargspec(self.__factory[ty])
                if (
                    'builder' in builder_func_sig.args
                    or builder_func_sig.varkw is not None
                ):
                    kw['builder'] = self
                return self.__factory[ty](client, value, **kw)
        raise RuntimeError('Unknown type to build as vineyard object')

    def __call__(self, client, value, **kw):
        return self.run(client, value, **kw)

    def extend(self, builders=None):
        builder = BuilderContext()
        builder.__factory = copy.copy(  # pylint: disable=unused-private-member
            self.__factory
        )
        if builders:
            builder.__factory.update(builders)
        return builder


default_builder_context = BuilderContext()

_builder_context_local = threading.local()
_builder_context_local.default_builder = default_builder_context


def get_current_builders():
    '''Obtain the current builder context.'''
    default_builder = getattr(_builder_context_local, 'default_builder', None)
    if not default_builder:
        default_builder = default_builder_context.extend()
    return default_builder


@contextlib.contextmanager
def builder_context(builders=None, base=None):
    """Open a new context for register builders, without populting outside global
    environment.

    See Also:
        resolver_context
        driver_context
    """
    current_builder = get_current_builders()
    try:
        builders = builders or dict()
        base = base or current_builder
        local_builder = base.extend(builders)
        _builder_context_local.default_builder = local_builder
        yield local_builder
    finally:
        _builder_context_local.default_builder = current_builder


def put(client, value, builder=None, persist=False, name=None, **kwargs):
    """Put python value to vineyard.

    .. code:: python

        >>> arr = np.arange(8)
        >>> arr_id = client.put(arr)
        >>> arr_id
        00002ec13bc81226

    Parameters:
        client: IPCClient
            The vineyard client to use.
        value:
            The python value that will be put to vineyard. Supported python value
            types are decided by modules that registered to vineyard. By default,
            python value can be put to vineyard after serialized as a bytes buffer
            using pickle.
        builder: optional
            When putting python value to vineyard, an optional *builder* can be
            specified to tell vineyard how to construct the corresponding vineyard
            :class:`Object`. If not specified, the default builder context will be
            used to select a proper builder.
        persist: bool, optional
            If true, persist the object after creation.
        name: str, optional
            If given, the name will be automatically associated with the resulted
            object. Note that only take effect when the object is persisted.
        kw:
            User-specific argument that will be passed to the builder.

    Returns:
        ObjectID: The result object id will be returned.
    """
    if builder is not None:
        return builder(client, value, **kwargs)

    meta = get_current_builders().run(client, value, **kwargs)

    # the builders is expected to return an :class:`ObjectMeta`, or an
    # :class:`Object` (in the `bytes_builder` and `memoryview` builder).
    if isinstance(meta, (ObjectMeta, Object)):
        r = meta.id
    else:
        r = meta  # None or ObjectID

    if isinstance(r, ObjectID):
        if persist:
            client.persist(r)
        if name is not None:
            if persist:
                client.put_name(r, name)
            else:
                warnings.warn(
                    "The object is not persisted, the name won't be associated.",
                    UserWarning,
                )
    return r


setattr(IPCClient, 'put', put)
setattr(RPCClient, 'put', put)

__all__ = [
    'default_builder_context',
    'builder_context',
    'get_current_builders',
]
