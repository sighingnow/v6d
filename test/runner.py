#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import contextlib
import importlib
import importlib.util
import os
import platform
import socket
import subprocess
import sys
import time
from argparse import ArgumentParser
from typing import Union

if 'NON_RANDOM_IPC_SOCKET' in os.environ:
    VINEYARD_CI_IPC_SOCKET = '/tmp/vineyard.ci.sock'
else:
    VINEYARD_CI_IPC_SOCKET = '/tmp/vineyard.ci.%s.sock' % time.time()


VINEYARD_FUSE_MOUNT_DIR = '/tmp/vineyard_fuse.%s' % time.time()
find_executable_generic = None
start_program_generic = None
find_port = None
port_is_inuse = None
build_artifact_directory = None


def prepare_runner_environment():
    utils = os.path.join(
        os.path.dirname(os.path.abspath(__file__)),
        '..',
        'python',
        'vineyard',
        'deploy',
        'utils.py',
    )
    spec = importlib.util.spec_from_file_location("vineyard._contrib", utils)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)

    global find_executable_generic
    global start_program_generic
    global find_port
    global port_is_inuse
    find_executable_generic = getattr(mod, 'find_executable')
    start_program_generic = getattr(mod, 'start_program')
    find_port = getattr(mod, 'find_port')
    port_is_inuse = getattr(mod, 'port_is_inuse')


prepare_runner_environment()


@contextlib.contextmanager
def envvars(key, value=None, append=False):
    items = key
    if isinstance(key, str):
        items = {key: value}
    original_items = dict()
    for k, v in items.items():
        original_items[k] = os.environ.get(k, None)
        if append and original_items[k] is not None:
            os.environ[k] = original_items[k] + ':' + v
        else:
            os.environ[k] = v

    yield os.environ

    for k, v in original_items.items():
        if v is not None:
            os.environ[k] = v
        else:
            del os.environ[k]


def find_executable(name):
    binary_dir = os.environ.get(
        'VINEYARD_EXECUTABLE_DIR', os.path.join(build_artifact_directory, 'bin')
    )
    return find_executable_generic(name, search_paths=[binary_dir])


def start_program(*args, **kwargs):
    binary_dir = os.environ.get(
        'VINEYARD_EXECUTABLE_DIR', os.path.join(build_artifact_directory, 'bin')
    )
    print('binary_dir = ', binary_dir)
    return start_program_generic(*args, search_paths=[binary_dir], **kwargs)


@contextlib.contextmanager
def start_fuse():
    if platform.system() != 'Linux':
        print('can not mount fuse on the non-linux system yet')
        return
    with contextlib.ExitStack() as stack:
        vfm = find_executable("vineyard-fusermount")
        os.mkdir(VINEYARD_FUSE_MOUNT_DIR)

        proc = start_program(
            vfm,
            '-f',
            '-s',
            '--vineyard-socket=%s' % VINEYARD_CI_IPC_SOCKET,
            VINEYARD_FUSE_MOUNT_DIR,
        )
        yield stack.enter_context(proc)

        # cleanup
        try:
            subprocess.run(['umount', '-f', '-l', VINEYARD_FUSE_MOUNT_DIR], check=False)
        except Exception:  # pylint: disable=broad-except
            pass


@contextlib.contextmanager
def start_etcd():
    with contextlib.ExitStack() as stack:
        client_port = find_port()
        peer_port = find_port()
        if platform.system() == 'Linux':
            data_dir_base = '/dev/shm'
        else:
            data_dir_base = '/tmp'
        proc = start_program(
            'etcd',
            '--data-dir',
            '%s/etcd-%s' % (data_dir_base, time.time()),
            '--listen-peer-urls',
            'http://0.0.0.0:%d' % peer_port,
            '--listen-client-urls',
            'http://0.0.0.0:%d' % client_port,
            '--advertise-client-urls',
            'http://127.0.0.1:%d' % client_port,
            '--initial-cluster',
            'default=http://127.0.0.1:%d' % peer_port,
            '--initial-advertise-peer-urls',
            'http://127.0.0.1:%d' % peer_port,
            verbose=True,
        )
        ctx_entered = stack.enter_context(proc)
        # waiting for etcd to be ready
        print('waiting for etcd to be ready .')
        while not port_is_inuse(client_port):
            time.sleep(1)
            print('.', end='')
        print('', end='\n')
        yield ctx_entered, 'http://127.0.0.1:%d' % client_port


@contextlib.contextmanager
def start_redis():
    with contextlib.ExitStack() as stack:
        redis_port = find_port()
        proc = start_program(
            'redis-server',
            '--port',
            str(redis_port),
            verbose=True,
        )
        ctx_entered = stack.enter_context(proc)
        # waiting for redis to be ready
        print('waiting for redis to be ready .')
        while not port_is_inuse(redis_port):
            time.sleep(1)
            print('.', end='')
        print('', end='\n')
        yield ctx_entered, 'redis://127.0.0.1:%d' % redis_port


@contextlib.contextmanager
def start_metadata_engine(meta):
    with contextlib.ExitStack() as stack:
        meta_engine = None
        if meta == 'etcd':
            meta_engine = start_etcd()
        if meta == 'redis':
            meta_engine = start_redis()
        if meta_engine is not None:
            yield stack.enter_context(meta_engine)
        else:
            yield None, None


def make_metadata_settings(meta, endpoint, prefix):
    if meta == 'local':
        return ['--meta', 'local']
    if meta == 'etcd':
        return ['--meta', 'etcd', '--etcd_endpoint', endpoint, '--etcd_prefix', prefix]
    if meta == 'redis':
        return [
            '--meta',
            'redis',
            '--redis_endpoint',
            endpoint,
            '--redis_prefix',
            prefix,
        ]
    raise ValueError("invalid argument: unknown metadata backend: '%s'" % meta)


@contextlib.contextmanager
def start_vineyardd(
    metadata_settings,
    size=3 * 1024 * 1024 * 1024,
    default_ipc_socket=VINEYARD_CI_IPC_SOCKET,
    idx=None,
    spill_path="",
    spill_upper_rate=0.8,
    spill_lower_rate=0.3,
    **kw,
):
    rpc_socket_port = find_port()
    if idx is not None:
        socket = '%s.%d' % (default_ipc_socket, idx)
    else:
        socket = default_ipc_socket
    with contextlib.ExitStack() as stack:
        proc = start_program(
            'vineyardd',
            '--size',
            str(size),
            '--socket',
            socket,
            '--rpc_socket_port',
            str(rpc_socket_port),
            *metadata_settings,
            '--spill_path',
            spill_path,
            '--spill_lower_rate',
            str(spill_lower_rate),
            '--spill_upper_rate',
            str(spill_upper_rate),
            verbose=True,
            **kw,
        )
        yield stack.enter_context(proc), rpc_socket_port


@contextlib.contextmanager
def start_multiple_vineyardd(
    metadata_settings,
    size=1 * 1024 * 1024 * 1024,
    default_ipc_socket=VINEYARD_CI_IPC_SOCKET,
    instance_size=1,
    **kw,
):
    with contextlib.ExitStack() as stack:
        jobs = []
        for idx in range(instance_size):
            job = start_vineyardd(
                metadata_settings,
                size=size,
                default_ipc_socket=default_ipc_socket,
                idx=idx,
                **kw,
            )
            jobs.append(job)
        yield [stack.enter_context(job) for job in jobs]


@contextlib.contextmanager
def start_zookeeper():
    kafka_dir = os.environ.get('KAFKA_HOME', ".")
    with contextlib.ExitStack() as stack:
        proc = start_program(
            kafka_dir + '/bin/zookeeper-server-start.sh',
            kafka_dir + 'config/zookeeper.properties',
        )
        yield stack.enter_context(proc)


@contextlib.contextmanager
def start_kafka_server():
    kafka_dir = os.environ.get('KAFKA_HOME', ".")
    with contextlib.ExitStack() as stack:
        proc = start_program(
            kafka_dir + '/bin/kafka-server-start.sh',
            kafka_dir + 'config/zookeeper.properties',
            verbose=True,
        )
        yield stack.enter_context(proc)


def resolve_mpiexec_cmdargs():
    if 'open' in subprocess.getoutput('mpiexec -V').lower():
        return [
            'mpiexec',
            '--allow-run-as-root',
            '-mca',
            'orte_allowed_exit_without_sync',
            '1',
            '-mca',
            'btl_vader_single_copy_mechanism',
            'none',
        ]
    else:
        return ['mpiexec']


mpiexec_cmdargs = resolve_mpiexec_cmdargs()


def include_test(tests, test_name):
    if not tests:
        return True
    for test in tests:
        if test in test_name:
            return True
    return False


def run_test(
    tests,
    test_name,
    *args,
    nproc=1,
    capture=False,
    vineyard_ipc_socket=VINEYARD_CI_IPC_SOCKET,
) -> Union[int, str]:
    if not include_test(tests, test_name):
        return None
    print(
        f'running test case -*-*-*-*-*-  {test_name}  -*-*-*-*-*-*-*-',
        flush=True,
    )
    arg_reps = []
    for arg in args:
        if isinstance(arg, str):
            arg_reps.append(arg)
        else:
            arg_reps.append(repr(arg))
    cmdargs = (
        mpiexec_cmdargs
        + [
            '-n',
            str(nproc),
            '--host',
            'localhost:%d' % nproc,
            find_executable(test_name),
            vineyard_ipc_socket,
        ]
        + arg_reps
    )
    output = None
    if capture:
        output = subprocess.check_output(cmdargs)
    else:
        output = subprocess.check_call(
            cmdargs, cwd=os.path.join(os.path.dirname(os.path.abspath(__file__)), '..')
        )
    time.sleep(1)
    return output


def get_data_path(name):
    default_data_dir = os.path.join(
        os.path.dirname(os.path.abspath(__file__)), '..', '..', 'gstest'
    )
    binary_dir = os.environ.get('VINEYARD_DATA_DIR', default_data_dir)
    if name is None:
        return binary_dir
    else:
        return os.path.join(binary_dir, name)


def run_invalid_client_test(tests, host, port):
    def send_garbage_bytes(bytes):
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.connect((host, port))
        sock.sendall(bytes)
        sock.close()

    if not include_test(tests, 'invalid_client_test'):
        return

    send_garbage_bytes(b'\x01')
    send_garbage_bytes(b'\x0001')
    send_garbage_bytes(b'\x0101')
    send_garbage_bytes(b'\x000001')
    send_garbage_bytes(b'\x010101')
    send_garbage_bytes(b'\x00000001')
    send_garbage_bytes(b'\x01010101')
    send_garbage_bytes(b'\x0000000001')
    send_garbage_bytes(b'\x0101010101')
    send_garbage_bytes(b'\x000000000001')
    send_garbage_bytes(b'\x010101010101')
    send_garbage_bytes(b'\x00000000000001')
    send_garbage_bytes(b'\x01010101010101')
    send_garbage_bytes(b'\x01010101010101')
    send_garbage_bytes(b'1' * 1)
    send_garbage_bytes(b'1' * 10)
    send_garbage_bytes(b'1' * 100)
    send_garbage_bytes(b'1' * 1000)
    send_garbage_bytes(b'1' * 10000)
    send_garbage_bytes(b'1' * 100000)
    send_garbage_bytes(b'\xFF' * 1)
    send_garbage_bytes(b'\xFF' * 10)
    send_garbage_bytes(b'\xFF' * 100)
    send_garbage_bytes(b'\xFF' * 1000)
    send_garbage_bytes(b'\xFF' * 10000)
    send_garbage_bytes(b'\xFF' * 100000)


def run_single_vineyardd_tests(meta, endpoints, tests):
    meta_prefix = 'vineyard_test_%s' % time.time()
    metadata_settings = make_metadata_settings(meta, endpoints, meta_prefix)
    with start_vineyardd(
        metadata_settings,
        default_ipc_socket=VINEYARD_CI_IPC_SOCKET,
    ) as (_, rpc_socket_port):
        run_test(tests, 'array_test')
        run_test(tests, 'array_two_clients_test')
        # FIXME: cannot be safely dtor after #350 and #354.
        # run_test('allocator_test')
        run_test(tests, 'arrow_data_structure_test')
        run_test(tests, 'clear_test')
        run_test(tests, 'custom_vector_test')
        run_test(tests, 'dataframe_test')
        run_test(tests, 'delete_test')
        run_test(tests, 'get_wait_test')
        run_test(tests, 'get_object_test')
        run_test(tests, 'global_object_test')
        run_test(tests, 'hashmap_test')
        run_test(tests, 'id_test')
        run_test(tests, 'invalid_connect_test', '127.0.0.1:%d' % rpc_socket_port)
        run_test(tests, 'large_meta_test')
        run_test(tests, 'list_object_test')
        run_test(tests, 'lru_test')
        run_test(tests, 'mutable_blob_test')
        run_test(tests, 'name_test')
        run_test(tests, 'persist_test')
        run_test(tests, 'plasma_test')
        run_test(tests, 'release_test')
        run_test(tests, 'remote_buffer_test', '127.0.0.1:%d' % rpc_socket_port)
        run_test(tests, 'rpc_delete_test', '127.0.0.1:%d' % rpc_socket_port)
        run_test(tests, 'rpc_get_object_test', '127.0.0.1:%d' % rpc_socket_port)
        run_test(tests, 'rpc_test', '127.0.0.1:%d' % rpc_socket_port)
        run_test(tests, 'scalar_test')
        run_test(tests, 'sequence_test')
        run_test(tests, 'server_status_test')
        run_test(tests, 'session_test')
        run_test(tests, 'signature_test')
        run_test(tests, 'shallow_copy_test')
        run_test(tests, 'shared_memory_test')
        run_test(tests, 'stream_test')
        run_test(tests, 'tensor_test')
        run_test(tests, 'typename_test')
        run_test(tests, 'version_test')
        run_test(tests, 'hosseinmoein_dataframe_test')

        # test invalid inputs from client
        run_invalid_client_test(tests, '127.0.0.1', rpc_socket_port)

    with start_vineyardd(
        metadata_settings,
        size=2048,
        default_ipc_socket=VINEYARD_CI_IPC_SOCKET,
        spill_path='/tmp/spill_path',
    ):
        run_test(tests, 'spill_test')


def run_scale_in_out_tests(meta, endpoints, instance_size=4):
    meta_prefix = 'vineyard_test_%s' % time.time()
    metadata_settings = make_metadata_settings(meta, endpoints, meta_prefix)
    with start_multiple_vineyardd(
        metadata_settings,
        default_ipc_socket=VINEYARD_CI_IPC_SOCKET,
        instance_size=instance_size,
    ) as instances:
        time.sleep(5)
        with start_vineyardd(
            metadata_settings,
            default_ipc_socket=VINEYARD_CI_IPC_SOCKET,
            idx=instance_size,
        ):
            time.sleep(5)
            instances[0][0].terminate()
            time.sleep(5)

    # run with serious contention on etcd.
    with start_multiple_vineyardd(
        metadata_settings,
        default_ipc_socket=VINEYARD_CI_IPC_SOCKET,
        instance_size=instance_size,
        nowait=True,
    ) as instances:  # pylint: disable=unused-variable
        time.sleep(5)


def run_fuse_test(meta, endpoints):
    meta_prefix = 'vineyard_test_%s' % time.time()
    metadata_settings = make_metadata_settings(meta, endpoints, meta_prefix)

    with start_vineyardd(
        metadata_settings, default_ipc_socket=VINEYARD_CI_IPC_SOCKET
    ) as (_, rpc_socket_port), start_fuse() as _:
        start_time = time.time()
        subprocess.check_call(
            [
                'pytest',
                '-s',
                '-vvv',
                '--durations=0',
                '--log-cli-level',
                'DEBUG',
                'modules/fuse/tests',
                '--vineyard-ipc-socket=%s' % VINEYARD_CI_IPC_SOCKET,
                '--vineyard-endpoint=localhost:%s' % rpc_socket_port,
                '--vineyard-fuse-mount-dir=%s' % VINEYARD_FUSE_MOUNT_DIR,
            ],
            cwd=os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'),
        )
        print(
            'running fuse tests use %s seconds' % (time.time() - start_time),
        )


def run_python_tests(meta, endpoints, tests):
    meta_prefix = 'vineyard_test_%s' % time.time()
    metadata_settings = make_metadata_settings(meta, endpoints, meta_prefix)

    with start_vineyardd(
        metadata_settings, default_ipc_socket=VINEYARD_CI_IPC_SOCKET
    ) as (_, rpc_socket_port):
        start_time = time.time()
        test_args = []
        if tests:
            for test in tests:
                test_args.append('-k')
                test_args.append(test)
        subprocess.check_call(
            [
                'pytest',
                '-s',
                '-vvv',
                '--durations=0',
                '--log-cli-level',
                'DEBUG',
                'python/vineyard/core',
                'python/vineyard/data',
                'python/vineyard/shared_memory',
                *test_args,
                '--vineyard-ipc-socket=%s' % VINEYARD_CI_IPC_SOCKET,
                '--vineyard-endpoint=localhost:%s' % rpc_socket_port,
            ],
            cwd=os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'),
        )
        print(
            'running python tests use %s seconds' % (time.time() - start_time),
            flush=True,
        )


def run_python_contrib_ml_tests(meta, endpoints):
    meta_prefix = 'vineyard_test_%s' % time.time()
    metadata_settings = make_metadata_settings(meta, endpoints, meta_prefix)

    with start_vineyardd(
        metadata_settings, default_ipc_socket=VINEYARD_CI_IPC_SOCKET
    ) as (_, rpc_socket_port):
        start_time = time.time()
        subprocess.check_call(
            [
                'pytest',
                '-s',
                '-vvv',
                '--durations=0',
                '--log-cli-level',
                'DEBUG',
                'python/vineyard/contrib/ml',
                '--vineyard-ipc-socket=%s' % VINEYARD_CI_IPC_SOCKET,
                '--vineyard-endpoint=localhost:%s' % rpc_socket_port,
            ],
            cwd=os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'),
        )
        print(
            'running python contrib ml tests use %s seconds'
            % (time.time() - start_time),
            flush=True,
        )


def run_python_contrib_dask_tests(meta, endpoints):
    meta_prefix = 'vineyard_test_%s' % time.time()
    metadata_settings = make_metadata_settings(meta, endpoints, meta_prefix)

    instance_size = 4
    with start_multiple_vineyardd(
        metadata_settings,
        default_ipc_socket=VINEYARD_CI_IPC_SOCKET,
        instance_size=instance_size,
        nowait=True,
    ) as instances:  # noqa: F841, pylint: disable=unused-variable
        vineyard_ipc_sockets = ','.join(
            ['%s.%d' % (VINEYARD_CI_IPC_SOCKET, i) for i in range(instance_size)]
        )
        start_time = time.time()
        subprocess.check_call(
            [
                'pytest',
                '-s',
                '-vvv',
                '--durations=0',
                '--log-cli-level',
                'DEBUG',
                'python/vineyard/contrib/dask',
                '--vineyard-ipc-sockets=%s' % vineyard_ipc_sockets,
            ],
            cwd=os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'),
        )
        print(
            'running python contrib dask tests use %s seconds'
            % (time.time() - start_time),
            flush=True,
        )


def run_python_deploy_tests(meta, endpoints, with_migration):
    meta_prefix = 'vineyard_test_%s' % time.time()
    metadata_settings = make_metadata_settings(meta, endpoints, meta_prefix)

    instance_size = 4
    extra_args = []
    if with_migration:
        extra_args.append('--with-migration')
    with start_multiple_vineyardd(
        metadata_settings,
        default_ipc_socket=VINEYARD_CI_IPC_SOCKET,
        instance_size=instance_size,
        nowait=True,
    ) as instances:  # noqa: F841, pylint: disable=unused-variable
        vineyard_ipc_sockets = ','.join(
            ['%s.%d' % (VINEYARD_CI_IPC_SOCKET, i) for i in range(instance_size)]
        )
        start_time = time.time()
        subprocess.check_call(
            [
                'pytest',
                '-s',
                '-vvv',
                '--durations=0',
                '--log-cli-level',
                'DEBUG',
                'python/vineyard/deploy/tests',
                '--vineyard-ipc-sockets=%s' % vineyard_ipc_sockets,
            ]
            + extra_args,
            cwd=os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'),
        )
        print(
            'running python distributed tests use %s seconds'
            % (time.time() - start_time),
            flush=True,
        )


def run_io_adaptor_tests(meta, endpoints):
    meta_prefix = 'vineyard_test_%s' % time.time()
    metadata_settings = make_metadata_settings(meta, endpoints, meta_prefix)

    with start_vineyardd(
        metadata_settings, default_ipc_socket=VINEYARD_CI_IPC_SOCKET
    ) as (_, rpc_socket_port):
        start_time = time.time()
        subprocess.check_call(
            [
                'pytest',
                '-s',
                '-vvv',
                '--durations=0',
                '--log-cli-level',
                'DEBUG',
                'modules/io/python/drivers/io/tests',
                '--vineyard-ipc-socket=%s' % VINEYARD_CI_IPC_SOCKET,
                '--vineyard-endpoint=localhost:%s' % rpc_socket_port,
                '--test-dataset=%s' % get_data_path(None),
            ],
            cwd=os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'),
        )
        print(
            'running io adaptors tests use %s seconds' % (time.time() - start_time),
            flush=True,
        )


def run_io_adaptor_distributed_tests(meta, endpoints, with_migration):
    meta_prefix = 'vineyard_test_%s' % time.time()
    metadata_settings = make_metadata_settings(meta, endpoints, meta_prefix)

    instance_size = 2
    extra_args = []
    if with_migration:
        extra_args.append('--with-migration')
    with start_multiple_vineyardd(
        metadata_settings,
        default_ipc_socket=VINEYARD_CI_IPC_SOCKET,
        instance_size=instance_size,
        nowait=True,
    ) as instances:
        vineyard_ipc_sockets = ','.join(
            ['%s.%d' % (VINEYARD_CI_IPC_SOCKET, i) for i in range(instance_size)]
        )
        rpc_socket_port = instances[0][1]
        start_time = time.time()
        subprocess.check_call(
            [
                'pytest',
                '-s',
                '-vvv',
                '--durations=0',
                '--log-cli-level',
                'DEBUG',
                'modules/io/python/drivers/io/tests/test_migrate_stream.py',
                '--vineyard-endpoint=localhost:%s' % rpc_socket_port,
                '--vineyard-ipc-sockets=%s' % vineyard_ipc_sockets,
            ]
            + extra_args,
            cwd=os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'),
        )
        print(
            'running distributed io adaptors tests use %s seconds'
            % (time.time() - start_time),
            flush=True,
        )


def parse_sys_args():
    default_builder_dir = os.path.join(
        os.path.dirname(os.path.abspath(__file__)),
        '..',
        'build',
    )
    if os.path.exists('shared-lib'):
        default_builder_dir = '.'
    if os.path.exists('build'):
        default_builder_dir = 'build'

    arg_parser = ArgumentParser()

    arg_parser.add_argument(
        '-b',
        '--build-dir',
        type=str,
        default=default_builder_dir,
        help='Directory where the build artifacts are generated',
    )
    arg_parser.add_argument(
        '-m',
        '--meta',
        type=str,
        default='etcd',
        help='Metadata backend to testing, could be "etcd", "redis", or "local"',
    )
    arg_parser.add_argument(
        '--with-cpp',
        action='store_true',
        default=False,
        help='Whether to run C++ tests',
    )
    arg_parser.add_argument(
        '--with-python',
        action='store_true',
        default=False,
        help='Whether to run python tests',
    )
    arg_parser.add_argument(
        '--with-io',
        action='store_true',
        default=False,
        help='Whether to run IO adaptors tests',
    )
    arg_parser.add_argument(
        '--with-deployment',
        action='store_true',
        default=False,
        help='Whether to run deployment and scaling in/out tests',
    )
    arg_parser.add_argument(
        '--with-migration',
        action='store_true',
        default=False,
        help='Whether to run object migration tests',
    )
    arg_parser.add_argument(
        '--with-contrib',
        action='store_true',
        default=False,
        help="Whether to run python contrib tests",
    )

    arg_parser.add_argument(
        '--with-fuse',
        action='store_true',
        default=False,
        help="whether to run fuse test",
    )
    arg_parser.add_argument(
        '--tests',
        action='extend',
        nargs="*",
        type=str,
        help="Specify tests cases ro run",
    )

    return arg_parser, arg_parser.parse_args()


def execute_tests(args):
    if args.with_cpp:
        with start_metadata_engine(args.meta) as (_, endpoints):
            run_single_vineyardd_tests(args.meta, endpoints, args.tests)

        if args.with_deployment:
            with start_metadata_engine(args.meta) as (_, endpoints):
                run_scale_in_out_tests(args.meta, endpoints, instance_size=4)

    if args.with_python:
        with start_metadata_engine(args.meta) as (_, endpoints):
            run_python_tests(args.meta, endpoints, args.tests)

        if args.with_contrib:
            with start_metadata_engine(args.meta) as (_, endpoints):
                run_python_contrib_ml_tests(args.meta, endpoints)
            with start_metadata_engine(args.meta) as (_, endpoints):
                run_python_contrib_dask_tests(args.meta, endpoints)

        if args.with_deployment:
            with start_metadata_engine(args.meta) as (_, endpoints):
                run_python_deploy_tests(args.meta, endpoints, args.with_migration)

    if args.with_io:
        with start_metadata_engine(args.meta) as (_, endpoints):
            run_io_adaptor_tests(args.meta, endpoints)
        with start_metadata_engine(args.meta) as (_, endpoints):
            run_io_adaptor_distributed_tests(args.meta, endpoints, args.with_migration)

    if args.with_fuse:
        with start_metadata_engine(args.meta) as (_, endpoints):
            run_fuse_test(args.meta, endpoints)


def main():
    parser, args = parse_sys_args()

    if not (args.with_cpp or args.with_python or args.with_io or args.with_fuse):
        print(
            'Error: \n\tat least one of of --with-{cpp,python,io,fuse} needs '
            'to be specified\n'
        )
        parser.print_help()
        sys.exit(1)

    global build_artifact_directory
    build_artifact_directory = args.build_dir

    built_shared_libs = os.path.join(os.path.abspath(args.build_dir), 'shared-lib')
    with envvars('LD_LIBRARY_PATH', built_shared_libs, append=True):
        execute_tests(args)


if __name__ == '__main__':
    main()
