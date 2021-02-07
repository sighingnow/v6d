/** Copyright 2020-2021 Alibaba Group Holding Limited.

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
// Code generated by lister-gen. DO NOT EDIT.

package v1alpha1

import (
	v1alpha1 "github.com/v6d-io/v6d/k8s/api/k8s/v1alpha1"
	"k8s.io/apimachinery/pkg/api/errors"
	"k8s.io/apimachinery/pkg/labels"
	"k8s.io/client-go/tools/cache"
)

// LocalObjectLister helps list LocalObjects.
// All objects returned here must be treated as read-only.
type LocalObjectLister interface {
	// List lists all LocalObjects in the indexer.
	// Objects returned here must be treated as read-only.
	List(selector labels.Selector) (ret []*v1alpha1.LocalObject, err error)
	// LocalObjects returns an object that can list and get LocalObjects.
	LocalObjects(namespace string) LocalObjectNamespaceLister
	LocalObjectListerExpansion
}

// localObjectLister implements the LocalObjectLister interface.
type localObjectLister struct {
	indexer cache.Indexer
}

// NewLocalObjectLister returns a new LocalObjectLister.
func NewLocalObjectLister(indexer cache.Indexer) LocalObjectLister {
	return &localObjectLister{indexer: indexer}
}

// List lists all LocalObjects in the indexer.
func (s *localObjectLister) List(selector labels.Selector) (ret []*v1alpha1.LocalObject, err error) {
	err = cache.ListAll(s.indexer, selector, func(m interface{}) {
		ret = append(ret, m.(*v1alpha1.LocalObject))
	})
	return ret, err
}

// LocalObjects returns an object that can list and get LocalObjects.
func (s *localObjectLister) LocalObjects(namespace string) LocalObjectNamespaceLister {
	return localObjectNamespaceLister{indexer: s.indexer, namespace: namespace}
}

// LocalObjectNamespaceLister helps list and get LocalObjects.
// All objects returned here must be treated as read-only.
type LocalObjectNamespaceLister interface {
	// List lists all LocalObjects in the indexer for a given namespace.
	// Objects returned here must be treated as read-only.
	List(selector labels.Selector) (ret []*v1alpha1.LocalObject, err error)
	// Get retrieves the LocalObject from the indexer for a given namespace and name.
	// Objects returned here must be treated as read-only.
	Get(name string) (*v1alpha1.LocalObject, error)
	LocalObjectNamespaceListerExpansion
}

// localObjectNamespaceLister implements the LocalObjectNamespaceLister
// interface.
type localObjectNamespaceLister struct {
	indexer   cache.Indexer
	namespace string
}

// List lists all LocalObjects in the indexer for a given namespace.
func (s localObjectNamespaceLister) List(selector labels.Selector) (ret []*v1alpha1.LocalObject, err error) {
	err = cache.ListAllByNamespace(s.indexer, s.namespace, selector, func(m interface{}) {
		ret = append(ret, m.(*v1alpha1.LocalObject))
	})
	return ret, err
}

// Get retrieves the LocalObject from the indexer for a given namespace and name.
func (s localObjectNamespaceLister) Get(name string) (*v1alpha1.LocalObject, error) {
	obj, exists, err := s.indexer.GetByKey(s.namespace + "/" + name)
	if err != nil {
		return nil, err
	}
	if !exists {
		return nil, errors.NewNotFound(v1alpha1.Resource("localobject"), name)
	}
	return obj.(*v1alpha1.LocalObject), nil
}
