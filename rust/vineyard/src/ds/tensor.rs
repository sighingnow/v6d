// Copyright 2020-2023 Alibaba Group Holding Limited.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

use std::rc::Rc;

use arrow;
use downcast_rs::impl_downcast;

use crate::client::*;

use super::arrow::*;

pub trait Tensor: Object {
}

impl_downcast!(Tensor);

pub fn downcast_tensor<T: Tensor>(object: Box<dyn Tensor>) -> Result<Box<T>> {
    return object
        .downcast::<T>()
        .map_err(|_| VineyardError::invalid(format!("downcast object to tensor failed",)));
}

pub fn downcast_tensor_ref<T: Tensor>(object: &dyn Tensor) -> Result<&T> {
    return object
        .downcast_ref::<T>()
        .ok_or(VineyardError::invalid(format!(
            "downcast object '{:?}' to tensor failed",
            object.meta().get_typename()?,
        )));
}

pub fn downcast_tensor_rc<T: Tensor>(object: Rc<dyn Tensor>) -> Result<Rc<T>> {
    return object
        .downcast_rc::<T>()
        .map_err(|_| VineyardError::invalid(format!("downcast object to tensor failed",)));
}

#[derive(Debug)]
pub struct NumericTensor<T: NumericType> {
    meta: ObjectMeta,
    tensor: arrow::tensor::Tensor<<T as ToArrowType>::Type>,

}