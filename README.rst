.. raw:: html

    <h1 align="center">
        <img src="https://v6d.io/_static/vineyard_logo.png" width="397" alt="vineyard">
    </h1>
    <p align="center">
        an in-memory immutable data manager
    </p>

|Build and Test| |Coverage| |Docs| |Artifact HUB|

Vineyard is a distributed in-memory data manager for big data tasks.

Conceptual Overview
-------------------

Vineyard builds on Kubernetes as a system for deploying and scaling, and
managing distributed shared data for dedicated engines in big data pipelines,
as shown in the following diagram:

.. image:: https://v6d.io/_static/vineyard_k8s_arch.png
   :alt: Vineyard architecture

Introducing the typical big data pipeline
-----------------------------------------

.. image:: https://v6d.io/_static/vineyard_k8s_pipeline.png
   :alt: Typical big data pipeline

Existing solutions that utilize a distributed file system to store the
intermediate data significantly increase the implementation complexity
whereas the extra costs (e.g., I/O, serialization/deserialization) pull
down the end-to-end performance of the big data workflow. More specifically,

1. Saving/loading the data to/from the external storage requires lots
   of serializations/deserializations, memory-copies, and IO costs;

2. Newly emerged engines are hard to be integrated with existing engines
   seamlessly, as they often lack support of various data formats and I/O
   interfaces in the fragmented ecosystem; and

3. Using files to exchange the intermediate data makes the data sharing
   a barrier in the parallel computing workflow; hence it is hard for
   cross-engine pipelining. Moreover, each engine has no control on how
   other engines are partitioning and storing the data, thus incurs lots
   of data transformation when consuming the data that could be eliminated
   with a better "data-workload" alignment.

What vineyard brings to big data pipelines
------------------------------------------

Vineyard addresses the challenges in both implementation complexity and
end-to-end efficiency by providing high-level data abstractions and solutions
to cross-engine data-sharing issues. To achieve this, vineyard provides

.. image:: https://v6d.io/_static/vineyard_k8s_pipeline_revisit.png
   :alt: Big data pipeline on vineyard

1. Cross-system in-memory distributed data sharing in a zero-copy fashion.

2. Out-of-the-box high-level data access interface to support easier I/O
   and data integration for engines.

3. Co-scheduling "data" and "workload" to improve the overall workflow
   efficiency for data-intensive applications together with Kubernetes.

Core components of vineyard
---------------------------

Distributed data sharding
^^^^^^^^^^^^^^^^^^^^^^^^^

1. The metadata service layer at the bottom utilizes ETCD to store metadata
   of the distributed data objects. In general, the metadata describes the
   data object's structure and references the data payloads which are the
   chunks of memory space to store data values.

2. The shared memory store layer employs the shared memory on each cluster
   node to store the "local" data payloads referenced by the metadata. The
   data payloads can be shared to on-top applications in a zero-copy fashion
   with zero extra cost.

3. The medium-level and high level data abstraction provides the application
   the capability of out-of-the-box integration and cross-engine sharing with
   other workloads that runs on vineyard.

Co-scheduling data and workloads
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Vineyard has deeply integrated with kuberneters, and co-schedules "data" and
"workloads" on Kubernetes for data intensive applications, and open a change
to build a new paradigm for big-data analytical workloads on cloud-native
infrastructures.

Vineyard first abstract objects as custom resource definitions (CRDs) on
Kubernetes to make the shared data observable. Vineyard includes a scheduler
plugin to try its best to place job pods to where the required data lies to
improve the data locality between workloads on Vineyard.

Examples
^^^^^^^^

A simplified case is demonstrated as the following figure,

.. image:: https://v6d.io/_static/vineyard_k8s_scheduler.png
   :alt: How vineyard scheduler works

A typical big data analytical pipeline often involves many different specific
workloads, and one will require other's output as input, as task B requires
task A's result as input. The task A is scheduled to node1 and node2, generating
chunks A, B, C and D as results, and they forms a global object in vineyard,
both the global object and those local chunks are observable by Kubernetes
as custom resource definitions.

When task B being launched, it may be scheduled to any other pods, and due
to the lack of co-located data, migration will be triggered to prepare input
for task B. With vineyard's scheduler plugin enabled, Kubernetes will place
pods of task B as "near" to its input as possible. In the above figure a
worker for task B has been scheduled to node1 and due to resource constraints
the node2 cannot satisfy the requirements for another worker anymore, thus
the worker will be scheduled to node3, but the overhead of intermediate data
movement has been optimized.

In real world scenarios the cluster will be large to thousands of machines,
the pipeline could be complex, and the data will be at large volume, then the
overhead of data movement may dominant the overall performance of data-intensive
applications. Vineyard fills such a missing piece in CNCF's landscape to make
big data applications works efficient on cloud-native infrastructures.

Deployment
----------

For better leveraging the scale-in/out capability of Kubernetes for worker pods of
a data analytical job, vineyard could be deployed on Kubernetes to as a DaemonSet
in Kubernetes cluster. Vineyard pods shares memory with worker pods using a UNIX
domain socket with fine-grained access control.

The UNIX domain socket can be either mounted on ``hostPath`` or via a ``PersistentVolumeClaim``.
When users bundle vineyard and the workload to the same pod, the UNIX domain socket
could also be shared using an ``emptyDir``.

Deployment with Helm
^^^^^^^^^^^^^^^^^^^^

Vineyard also has tight integration with Kubernetes and Helm. Vineyard can be deployed
with ``helm``:

.. code:: shell

   helm repo add vineyard https://vineyard.oss-ap-southeast-1.aliyuncs.com/charts/
   helm install vineyard vineyard/vineyard

In the further vineyard will improve the integration with Kubernetes by abstract
vineyard objects as as Kubernetes resources (i.e., CRDs), and leverage a vineyard
operator to operate vineyard cluster.

Install vineyard
----------------

Vineyard is distributed as a `python package`_ and can be easily installed with ``pip``:

.. code:: shell

   pip3 install vineyard

The latest version of online documentation can be found at https://v6d.io.

If you want to build vineyard from source, please refer to `Installation`_.

Getting involved
----------------

- Join in the `Slack channel`_ for discussion.
- Read `contribution guide`_.
- Please report bugs by submitting a GitHub issue.
- Submit contributions using pull requests.

Thank you in advance for your contributions to vineyard!

Code of Conduct
---------------

Vineyard follows the [CNCF Code of Conduct](https://github.com/cncf/foundation/blob/master/code-of-conduct.md).

License
-------

**Vineyard** is distributed under `Apache License 2.0`_. Please note that
third-party libraries may not have the same license as vineyard.

Acknowledgements
----------------

- `apache-arrow <https://github.com/apache-arrow/granula>`_, a cross-language development platform for in-memory analytics;
- `boost-leaf <https://github.com/boostorg/leaf>`_, a C++ lightweight error augmentation framework;
- `dlmalloc <http://gee.cs.oswego.edu/dl/html/malloc.htmlp>`_, Doug Lea's memory allocator;
- `etcd-cpp-apiv3 <https://github.com/etcd-cpp-apiv3/etcd-cpp-apiv3>`_, a C++ API for etcd's v3 client API;
- `flat_hash_map <https://github.com/skarupke/flat_hash_map>`_, an efficient hashmap implementation;
- `tbb <https://github.com/oneapi-src/oneTBB>`_ a C++ library for threading building blocks.
- `pybind11 <https://github.com/pybind/pybind11>`_, a library for seamless operability between C++11 and Python;
- `s3fs <https://github.com/dask/s3fs>`_, a library provide a convenient Python filesystem interface for S3.
- `uri <https://github.com/cpp-netlib/uri>`_, a library for URI parsing.
- `nlohmann/json <https://github.com/nlohmann/json>`_, a json library for modern c++.

.. _Mars: https://github.com/mars-project/mars
.. _GraphScope: https://github.com/alibaba/GraphScope
.. _Installation: https://github.com/alibaba/v6d/blob/main/docs/notes/install.rst
.. _Apache License 2.0: https://github.com/alibaba/v6d/blob/main/LICENSE
.. _contribution guide: https://github.com/alibaba/v6d/blob/main/CONTRIBUTING.rst
.. _time series prediction with LSTM: https://github.com/L1aoXingyu/code-of-learn-deep-learning-with-pytorch/blob/master/chapter5_RNN/time-series/lstm-time-series.ipynb
.. _python package: https://pypi.org/project/vineyard/
.. _Slack channel: https://join.slack.com/t/v6d/shared_invite/zt-peiowbbr-ckIMcPg1NhPXlckJO55ymw

.. |Build and Test| image:: https://github.com/alibaba/v6d/workflows/Build%20and%20Test/badge.svg
   :target: https://github.com/alibaba/v6d/actions?workflow=Build%20and%20Test
.. |Coverage| image:: https://codecov.io/gh/alibaba/v6d/branch/main/graph/badge.svg
   :target: https://codecov.io/gh/alibaba/v6d
.. |Docs| image:: https://img.shields.io/badge/docs-latest-brightgreen.svg
   :target: https://v6d.io
.. |Artifact HUB| image:: https://img.shields.io/endpoint?url=https://artifacthub.io/badge/repository/vineyard
   :target: https://artifacthub.io/packages/helm/vineyard/vineyard
