import os
import sys
import time

import numpy
import numpy as np
import pandas
import pandas as pa
import pyarrow as pa
import pyarrow.parquet as pq
import h5py

import vineyard

def test_pandas_parquet(client, fuse_path):
    df = pandas.DataFrame({'a': [1, 2, 3], 'b': [4, 5, 6]})
    table = pa.Table.from_pandas(df)
    rid = client.put(table)
    client.persist(rid)
    client.put_name(rid, 'test_parquet')
    table2 = pq.read_table(os.path.join(fuse_path, 'test_parquet.parquet'))
    print(table2)

    pq.write_table(table, os.path.join(fuse_path, 'test_parquet_new.parquet'))

    time.sleep(5)
    table3 = client.get(client.get_name('test_parquet_new'))
    print(table3)

def test_numpy_hdf5(client, fuse_path):
    arr = np.random.rand(100, 100)
    rid = client.put(arr)
    client.persist(rid)
    client.put_name(rid, 'test_hdf5')
    with h5py.File(os.path.join(fuse_path, 'test_hdf5.h5'), 'r') as f:
        arr2 = f['data'][:]
    print(arr2)

    with h5py.File(os.path.join(fuse_path, '/tmp/test_hdf5_new.h5'), 'w') as f:
        f.create_dataset(name='data', data=arr)

    with h5py.File(os.path.join(fuse_path, 'test_hdf5_new.h5'), 'w') as f:
        f.create_dataset(name='data', data=arr)

    time.sleep(5)
    arr3 = client.get(client.get_name('test_hdf5_new'))
    print(arr3)

def main():
    socket = sys.argv[1]
    fuse_path = sys.argv[2]
    client = vineyard.connect(socket)
    # test_pandas_parquet(client, fuse_path)
    test_numpy_hdf5(client, fuse_path)

if __name__ == '__main__':
    main()
