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

// use polars_core::prelude as polars;
// use polars_core::prelude::NamedFrom;

use crate::client::*;

/// Convert a Polars error to a Vineyard error, as orphan impls are not allowed
/// in Rust
///
/// Usage:
///
/// ```no_run
/// let x = polars::DataFrame::new(...).map_err(error)?;
/// ```
// fn error(error: polars::PolarsError) -> VineyardError {
//     VineyardError::invalid(format!("{}", error))
// }

#[derive(Debug, Default)]
pub struct DataFrame {
    meta: ObjectMeta,
    // dataframe: polars::DataFrame,
}

impl_typename!(DataFrame, "vineyard::DataFrame");

// impl Object for DataFrame {
//     fn construct(&mut self, meta: ObjectMeta) -> Result<()> {
//         vineyard_assert_typename(typename::<Self>(), meta.get_typename()?)?;
//         let size = meta.get_usize("__values_-size")?;
//         // let mut columns = Vec::with_capacity(size);
//         for i in 0..size {
//             // let column_meta = meta.get_member::<ObjectMeta>(&format!("__values_{}_", i))?;
//             // let column = downcast_object::<Series>(column_meta.clone())?;
//             // columns.push(column);
//         }
//         return Ok(());
//     }
// }

// register_vineyard_object!(DataFrame);

// impl DataFrame {
//     pub fn new_boxed(meta: ObjectMeta) -> Result<Box<dyn Object>> {
//         let mut object = Box::<Self>::default();
//         object.construct(meta)?;
//         Ok(object)
//     }
// }

// impl AsRef<polars::DataFrame> for DataFrame {
//     fn as_ref(&self) -> &polars::DataFrame {
//         &self.dataframe
//     }
// }
