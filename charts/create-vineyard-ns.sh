#!/bin/bash

set -x
set -e
set -o pipefail

namespace=vineyard-system
kubectl create ns ${namespace} || true

kubectl get secret pull-acr --namespace=default -o yaml | sed "s/namespace: default/namespace: ${namespace}/" | kubectl create -f -
kubectl get configmap hdfs-hadoop --namespace=hdfs -o yaml | sed "s/namespace: hdfs/namespace: ${namespace}/" | kubectl apply -f -

set +x
set +e
set +o pipefail
