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
package io.v6d.spark

import io.v6d.core.client.IPCClient
import io.v6d.core.common.util.{Env, ObjectID}
import io.v6d.spark.rdd.{TableRDD, VineyardRDD}
import org.apache.arrow.vector.VectorSchemaRoot
import org.apache.spark.graphx.{CSVGraphLoader, GraphLoader}
import org.apache.spark.{SparkConf, SparkContext}
import org.apache.spark.sql.SparkSession
import org.apache.spark.sql.execution.arrow.ArrowWriter
import org.apache.spark.sql.util.ArrowUtils
import org.apache.spark.sql.vineyard.{ArrowContext, DataContext}

object SparkApp {
  def main(args: Array[String]): Unit = {
    val conf = new SparkConf()
    conf
      .setAppName("Spark on Vineyard")
      .setMaster("local[2]")
      // ensure all executor ready
      .set("spark.scheduler.minRegisteredResourcesRatio", "1.0")

    val kind = args(0)
    val edges = args(1)

    val spark = SparkSession
      .builder()
      .config(conf)
      // enable hive
      .enableHiveSupport()
      .getOrCreate()
    val sc = spark.sparkContext

    if (kind == "csv") {
      testLoadAndWriteWithCSV(spark, sc, edges)
    } else {
      testLoadAndWriteWithVineyard(spark, sc, edges)
    }
    // testHiveTable(spark, sc)
    // testInsertHiveTable(spark, sc)

    sc.stop()
  }

  def time[R](tag: String, block: => R): R = {
    val t0 = System.nanoTime()
    val result = block    // call-by-name
    val t1 = System.nanoTime()
    println(tag + ": Elapsed time: " + (t1 - t0) + " ns")
    result
  }

  case class Edge(src: Long, dst: Long, data: Double)
    extends Serializable {
    override def toString: String = {
      src + "," + dst + "," + data
    }
  }

  def testLoadAndWriteWithCSV(spark: SparkSession,
                              sc: SparkContext,
                              edges: String): Unit = {
    val g = time("loading graph from csv: ", {
      val g = CSVGraphLoader.edgeListFile(sc, edges, false, 1)
      print("loaded graph: ", g.numEdges, g.numVertices)
      g
    })

    val df = spark.createDataFrame(g.edges.map(e => Edge(e.srcId, e.dstId, e.attr)))
    val files = time("save edges to csv: ", {
      df.write.csv(edges + "_output")
    })
  }

  def testLoadAndWriteWithVineyard(spark: SparkSession,
                                   sc: SparkContext,
                                   edges: String): Unit = {
    val g = time("loading graph from csv: ", {
      val g = GraphLoader.edgeListFile(sc, edges, false, 1)
      print("loaded graph: ", g.numEdges, g.numVertices)
      g
    })

    val df = spark.createDataFrame(g.edges.map(e => Edge(e.srcId, e.dstId, e.attr)))
    val files = time("save edges to vineyard: ", {
      // val allocator = ArrowContext.rootAllocator
      // val rdd = g.edges.map(e => e.srcId)
      // val root = VectorSchemaRoot.create(DataContext.toArrowSchema(df.schema, ""), allocator)
      // val writer = ArrowWriter.create(root)
      df.write.parquet(edges + "_parquet")  // should in same performance with to vineyard
    })
  }

  // def testHiveTable(spark: SparkSession, sc: SparkContext): Unit = {
  //   import spark.implicits._
  //
  //   spark.sql("show databases;").show()
  //   spark.sql("show tables;").show()
  //   spark.sql("select sum(foo) from pokes;").show()
  // }
  //
  // def testInsertHiveTable(spark: SparkSession, sc: SparkContext): Unit = {
  //   import spark.implicits._
  //
  //   // spark.sql("CREATE TABLE IF NOT EXISTS src (key INT, value STRING) USING hive")
  //   // spark.sql("LOAD DATA LOCAL INPATH '/opt/spark/examples/src/main/resources/kv1.txt' INTO TABLE src")
  //
  //   spark.sql("show tables;").show()
  //   spark.sql("select * from src").show()
  //   spark.sql("select * from src_parquet").show()
  // }
  //
  // def testRDDFromHive(spark: SparkSession, sc: SparkContext): Unit = {
  //   // HiveContext
  // }
}
