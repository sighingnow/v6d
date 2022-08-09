/** Copyright 2020-2022 Alibaba Group Holding Limited.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
package io.v6d.core.common.util;

import lombok.EqualsAndHashCode;

/** Vineyard Signature definition. */
@EqualsAndHashCode(callSuper = false)
public class Signature {
    private long id = -1L;

    public static Signature InvalidSignature = new Signature(-1L);

    public Signature(long id) {
        this.id = id;
    }

    public static Signature fromString(String id) {
        return new Signature(Long.parseUnsignedLong(id.substring(1), 16));
    }

    public long Value() {
        return this.id;
    }

    @Override
    public String toString() {
        return String.format("o%016x", id);
    }
}
