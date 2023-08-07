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

use std::env::VarError as EnvVarError;
use std::io::Error as IOError;
use std::num::{ParseFloatError, ParseIntError, TryFromIntError};
use std::sync::PoisonError;

use num_derive::{FromPrimitive, ToPrimitive};
use serde_json::Error as JSONError;
use thiserror::Error;

use super::uuid::ObjectID;

#[derive(Debug, Clone, Default, PartialEq, Eq, FromPrimitive, ToPrimitive)]
pub enum StatusCode {
    #[default]
    OK = 0,
    Invalid = 1,
    KeyError = 2,
    TypeError = 3,
    IOError = 4,
    EndOfFile = 5,
    NotImplemented = 6,
    AssertionFailed = 7,
    UserInputError = 8,

    ObjectExists = 11,
    ObjectNotExists = 12,
    ObjectSealed = 13,
    ObjectNotSealed = 14,
    ObjectIsBlob = 15,
    ObjectTypeError = 16,
    ObjectSpilled = 17,
    ObjectNotSpilled = 18,

    MetaTreeInvalid = 21,
    MetaTreeTypeInvalid = 22,
    MetaTreeTypeNotExists = 23,
    MetaTreeNameInvalid = 24,
    MetaTreeNameNotExists = 25,
    MetaTreeLinkInvalid = 26,
    MetaTreeSubtreeNotExists = 27,

    VineyardServerNotReady = 31,
    ArrowError = 32,
    ConnectionFailed = 33,
    ConnectionError = 34,
    EtcdError = 35,
    AlreadyStopped = 36,
    RedisError = 37,

    NotEnoughMemory = 41,
    StreamDrained = 42,
    StreamFailed = 43,
    InvalidStreamState = 44,
    StreamOpened = 45,

    GlobalObjectInvalid = 51,

    UnknownError = 255,
}

#[derive(Error, Debug, Clone)]
pub struct VineyardError {
    pub code: StatusCode,
    pub message: String,
}

impl From<IOError> for VineyardError {
    fn from(error: IOError) -> Self {
        VineyardError {
            code: StatusCode::IOError,
            message: format!("internal io error: {}", error),
        }
    }
}

impl From<EnvVarError> for VineyardError {
    fn from(error: EnvVarError) -> Self {
        VineyardError {
            code: StatusCode::IOError,
            message: format!("env var error: {}", error),
        }
    }
}

impl From<ParseIntError> for VineyardError {
    fn from(error: ParseIntError) -> Self {
        VineyardError {
            code: StatusCode::IOError,
            message: format!("parse int error: {}", error),
        }
    }
}

impl From<ParseFloatError> for VineyardError {
    fn from(error: ParseFloatError) -> Self {
        VineyardError {
            code: StatusCode::IOError,
            message: format!("parse float error: {}", error),
        }
    }
}

impl From<TryFromIntError> for VineyardError {
    fn from(error: TryFromIntError) -> Self {
        VineyardError {
            code: StatusCode::IOError,
            message: format!("try from int error: {}", error),
        }
    }
}

impl<T> From<PoisonError<T>> for VineyardError {
    fn from(error: PoisonError<T>) -> Self {
        VineyardError {
            code: StatusCode::Invalid,
            message: format!("lock poison error: {}", error),
        }
    }
}

impl From<JSONError> for VineyardError {
    fn from(error: JSONError) -> Self {
        VineyardError {
            code: StatusCode::MetaTreeInvalid,
            message: error.to_string(),
        }
    }
}

impl std::fmt::Display for VineyardError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "Vineyard error {:?}: {}", self.code, self.message)
    }
}

pub type Result<T> = std::result::Result<T, VineyardError>;

impl VineyardError {
    pub fn new<T: Into<String>>(code: StatusCode, message: T) -> Self {
        VineyardError {
            code,
            message: message.into(),
        }
    }

    pub fn wrap<T: Into<String>>(&self, message: T) -> Self {
        if self.ok() {
            return self.clone();
        }
        VineyardError {
            code: self.code.clone(),
            message: format!("{}: {}", self.message, message.into()),
        }
    }

    pub fn invalid<T: Into<String>>(message: T) -> Self {
        VineyardError {
            code: StatusCode::Invalid,
            message: message.into(),
        }
    }

    pub fn key_error<T: Into<String>>(message: T) -> Self {
        VineyardError {
            code: StatusCode::KeyError,
            message: message.into(),
        }
    }

    pub fn type_error<T: Into<String>>(message: T) -> Self {
        VineyardError {
            code: StatusCode::TypeError,
            message: message.into(),
        }
    }

    pub fn io_error<T: Into<String>>(message: T) -> Self {
        VineyardError {
            code: StatusCode::IOError,
            message: message.into(),
        }
    }

    pub fn end_of_file<T: Into<String>>(message: T) -> Self {
        VineyardError {
            code: StatusCode::EndOfFile,
            message: message.into(),
        }
    }

    pub fn not_implemented<T: Into<String>>(message: T) -> Self {
        VineyardError {
            code: StatusCode::NotImplemented,
            message: message.into(),
        }
    }

    pub fn assertion_failed<T: Into<String>>(message: T) -> Self {
        VineyardError {
            code: StatusCode::AssertionFailed,
            message: message.into(),
        }
    }

    pub fn user_input_error<T: Into<String>>(message: T) -> Self {
        VineyardError {
            code: StatusCode::UserInputError,
            message: message.into(),
        }
    }

    pub fn object_exists<T: Into<String>>(message: T) -> Self {
        VineyardError {
            code: StatusCode::ObjectExists,
            message: message.into(),
        }
    }

    pub fn object_not_exists<T: Into<String>>(message: T) -> Self {
        VineyardError {
            code: StatusCode::ObjectNotExists,
            message: message.into(),
        }
    }

    pub fn object_sealed<T: Into<String>>(message: T) -> Self {
        VineyardError {
            code: StatusCode::ObjectSealed,
            message: message.into(),
        }
    }

    pub fn object_not_sealed<T: Into<String>>(message: T) -> Self {
        VineyardError {
            code: StatusCode::ObjectNotSealed,
            message: message.into(),
        }
    }

    pub fn object_is_blob<T: Into<String>>(message: T) -> Self {
        VineyardError {
            code: StatusCode::ObjectIsBlob,
            message: message.into(),
        }
    }

    pub fn object_type_error<U: Into<String>, V: Into<String>>(expected: U, actual: V) -> Self {
        VineyardError {
            code: StatusCode::ObjectTypeError,
            message: format!(
                "expect typename '{}', but got '{}'",
                expected.into(),
                actual.into()
            ),
        }
    }

    pub fn object_spilled(object_id: ObjectID) -> Self {
        VineyardError {
            code: StatusCode::ObjectSpilled,
            message: format!("object '{}' has already been spilled", object_id),
        }
    }

    pub fn object_not_spilled(object_id: ObjectID) -> Self {
        VineyardError {
            code: StatusCode::ObjectNotSpilled,
            message: format!("object '{}' hasn't been spilled yet", object_id),
        }
    }

    pub fn meta_tree_invalid<T: Into<String>>(message: T) -> Self {
        VineyardError {
            code: StatusCode::MetaTreeInvalid,
            message: message.into(),
        }
    }

    pub fn meta_tree_type_invalid<T: Into<String>>(message: T) -> Self {
        VineyardError {
            code: StatusCode::MetaTreeTypeInvalid,
            message: message.into(),
        }
    }

    pub fn meta_tree_type_not_exists<T: Into<String>>(message: T) -> Self {
        VineyardError {
            code: StatusCode::MetaTreeTypeNotExists,
            message: message.into(),
        }
    }

    pub fn meta_tree_name_invalid<T: Into<String>>(message: T) -> Self {
        VineyardError {
            code: StatusCode::MetaTreeNameInvalid,
            message: message.into(),
        }
    }

    pub fn meta_tree_name_not_exists<T: Into<String>>(message: T) -> Self {
        VineyardError {
            code: StatusCode::MetaTreeNameNotExists,
            message: message.into(),
        }
    }

    pub fn meta_tree_link_invalid<T: Into<String>>(message: T) -> Self {
        VineyardError {
            code: StatusCode::MetaTreeLinkInvalid,
            message: message.into(),
        }
    }

    pub fn meta_tree_subtree_not_exists<T: Into<String>>(message: T) -> Self {
        VineyardError {
            code: StatusCode::MetaTreeSubtreeNotExists,
            message: message.into(),
        }
    }

    pub fn vineyard_server_not_ready<T: Into<String>>(message: T) -> Self {
        VineyardError {
            code: StatusCode::VineyardServerNotReady,
            message: message.into(),
        }
    }

    pub fn arrow_error<T: Into<String>>(message: T) -> Self {
        VineyardError {
            code: StatusCode::ArrowError,
            message: message.into(),
        }
    }

    pub fn connection_failed<T: Into<String>>(message: T) -> Self {
        VineyardError {
            code: StatusCode::ConnectionFailed,
            message: message.into(),
        }
    }

    pub fn etcd_error<T: Into<String>>(message: T) -> Self {
        VineyardError {
            code: StatusCode::EtcdError,
            message: message.into(),
        }
    }

    pub fn redis_error<T: Into<String>>(message: T) -> Self {
        VineyardError {
            code: StatusCode::RedisError,
            message: message.into(),
        }
    }

    pub fn already_stopped<T: Into<String>>(message: T) -> Self {
        VineyardError {
            code: StatusCode::AlreadyStopped,
            message: message.into(),
        }
    }

    pub fn not_enough_memory<T: Into<String>>(message: T) -> Self {
        VineyardError {
            code: StatusCode::NotEnoughMemory,
            message: message.into(),
        }
    }

    pub fn stream_drained<T: Into<String>>(message: T) -> Self {
        VineyardError {
            code: StatusCode::StreamDrained,
            message: message.into(),
        }
    }

    pub fn stream_failed<T: Into<String>>(message: T) -> Self {
        VineyardError {
            code: StatusCode::StreamFailed,
            message: message.into(),
        }
    }

    pub fn invalid_stream_state<T: Into<String>>(message: T) -> Self {
        VineyardError {
            code: StatusCode::InvalidStreamState,
            message: message.into(),
        }
    }

    pub fn stream_opened<T: Into<String>>(message: T) -> Self {
        VineyardError {
            code: StatusCode::StreamOpened,
            message: message.into(),
        }
    }

    pub fn global_object_invalid<T: Into<String>>(message: T) -> Self {
        VineyardError {
            code: StatusCode::GlobalObjectInvalid,
            message: message.into(),
        }
    }

    pub fn unknown_error<T: Into<String>>(message: T) -> Self {
        VineyardError {
            code: StatusCode::UnknownError,
            message: message.into(),
        }
    }

    pub fn ok(&self) -> bool {
        return self.code == StatusCode::OK;
    }

    pub fn is_invalid(&self) -> bool {
        return self.code == StatusCode::Invalid;
    }

    pub fn is_key_error(&self) -> bool {
        return self.code == StatusCode::KeyError;
    }

    pub fn is_type_error(&self) -> bool {
        return self.code == StatusCode::TypeError;
    }

    pub fn is_io_error(&self) -> bool {
        return self.code == StatusCode::IOError;
    }

    pub fn is_end_of_file(&self) -> bool {
        return self.code == StatusCode::EndOfFile;
    }

    pub fn is_not_implemented(&self) -> bool {
        return self.code == StatusCode::NotImplemented;
    }

    pub fn is_assertion_failed(&self) -> bool {
        return self.code == StatusCode::AssertionFailed;
    }

    pub fn is_user_input_error(&self) -> bool {
        return self.code == StatusCode::UserInputError;
    }

    pub fn is_object_exists(&self) -> bool {
        return self.code == StatusCode::ObjectExists;
    }

    pub fn is_object_not_exists(&self) -> bool {
        return self.code == StatusCode::ObjectNotExists;
    }

    pub fn is_object_sealed(&self) -> bool {
        return self.code == StatusCode::ObjectSealed;
    }

    pub fn is_object_not_sealed(&self) -> bool {
        return self.code == StatusCode::ObjectNotSealed;
    }

    pub fn is_object_is_blob(&self) -> bool {
        return self.code == StatusCode::ObjectIsBlob;
    }

    pub fn is_object_type_error(&self) -> bool {
        return self.code == StatusCode::ObjectTypeError;
    }

    pub fn is_object_spilled(&self) -> bool {
        return self.code == StatusCode::ObjectSpilled;
    }

    pub fn is_object_not_spilled(&self) -> bool {
        return self.code == StatusCode::ObjectNotSpilled;
    }

    pub fn is_meta_tree_invalid(&self) -> bool {
        return self.code == StatusCode::MetaTreeInvalid
            || self.code == StatusCode::MetaTreeNameInvalid
            || self.code == StatusCode::MetaTreeTypeInvalid
            || self.code == StatusCode::MetaTreeLinkInvalid;
    }

    pub fn is_meta_tree_element_not_exists(&self) -> bool {
        return self.code == StatusCode::MetaTreeNameNotExists
            || self.code == StatusCode::MetaTreeTypeNotExists
            || self.code == StatusCode::MetaTreeSubtreeNotExists;
    }

    pub fn is_vineyard_server_not_ready(&self) -> bool {
        return self.code == StatusCode::VineyardServerNotReady;
    }

    pub fn is_arrow_error(&self) -> bool {
        return self.code == StatusCode::ArrowError;
    }

    pub fn is_connection_failed(&self) -> bool {
        return self.code == StatusCode::ConnectionFailed;
    }

    pub fn is_connection_error(&self) -> bool {
        return self.code == StatusCode::ConnectionError;
    }

    pub fn is_etcd_error(&self) -> bool {
        return self.code == StatusCode::EtcdError;
    }

    pub fn is_already_stopped(&self) -> bool {
        return self.code == StatusCode::AlreadyStopped;
    }

    pub fn is_not_enough_memory(&self) -> bool {
        return self.code == StatusCode::NotEnoughMemory;
    }

    pub fn is_stream_drained(&self) -> bool {
        return self.code == StatusCode::StreamDrained;
    }

    pub fn is_stream_failed(&self) -> bool {
        return self.code == StatusCode::StreamFailed;
    }

    pub fn is_invalid_stream_state(&self) -> bool {
        return self.code == StatusCode::InvalidStreamState;
    }

    pub fn is_stream_opened(&self) -> bool {
        return self.code == StatusCode::StreamOpened;
    }

    pub fn is_global_object_invalid(&self) -> bool {
        return self.code == StatusCode::GlobalObjectInvalid;
    }

    pub fn is_unknown_error(&self) -> bool {
        return self.code == StatusCode::UnknownError;
    }

    pub fn code(&self) -> &StatusCode {
        return &self.code;
    }

    pub fn message(&self) -> &String {
        return &self.message;
    }
}

pub fn vineyard_check_ok<T: std::fmt::Debug>(status: Result<T>) {
    if status.is_err() {
        panic!("Error occurs: {:?}.", status)
    }
}

pub fn vineyard_assert<T: Into<String>>(condition: bool, message: T) -> Result<()> {
    if !condition {
        return Err(VineyardError::assertion_failed(format!(
            "assertion failed: {}",
            message.into()
        )));
    }
    return Ok(());
}

pub fn vineyard_assert_typename<U: Into<String> + PartialEq<V>, V: Into<String>>(
    expect: U,
    actual: V,
) -> Result<()> {
    if expect != actual {
        return Err(VineyardError::object_type_error(
            expect.into(),
            actual.into(),
        ));
    }
    return Ok(());
}
