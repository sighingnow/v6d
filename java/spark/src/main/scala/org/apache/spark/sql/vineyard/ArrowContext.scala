package org.apache.spark.sql.vineyard

import org.apache.spark.sql.util.ArrowUtils

object ArrowContext {
  val rootAllocator = ArrowUtils.rootAllocator
}
