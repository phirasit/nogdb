/*
 *  Copyright (C) 2018, Throughwave (Thailand) Co., Ltd.
 *  <peerawich at throughwave dot co dot th>
 *
 *  This file is part of libnogdb, the NogDB core library in C++.
 *
 *  libnogdb is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU Affero General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Affero General Public License for more details.
 *
 *  You should have received a copy of the GNU Affero General Public License
 *  along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 */

#pragma once

#include <iostream>
#include <map>
#include <vector>
#include <set>
#include <utility>

#include "nogdb_errors.h"
#include "nogdb_compare.h"
#include "nogdb_types.h"
#include "nogdb_context.h"
#include "nogdb_txn.h"
#include "nogdb_sql.h"

namespace nogdb {

  //*************************************************************
  //*  NogDB class operations.                                  *
  //*************************************************************

  struct Class {
    Class() = delete;

    ~Class() noexcept = delete;

    static const ClassDescriptor create(Txn &txn, const std::string &className, ClassType type);

    static const ClassDescriptor
    createExtend(Txn &txn, const std::string &className, const std::string &superClass);

    static void drop(Txn &txn, const std::string &className);

    static void alter(Txn &txn, const std::string &oldClassName, const std::string &newClassName);
  };

  //*************************************************************
  //*  NogDB property operations.                               *
  //*************************************************************

  struct Property {
    Property() = delete;

    ~Property() noexcept = delete;

    static const PropertyDescriptor
    add(Txn &txn, const std::string &className, const std::string &propertyName, PropertyType type);

    static void alter(Txn &txn, const std::string &className, const std::string &oldPropertyName,
                      const std::string &newPropertyName);

    static void remove(Txn &txn, const std::string &className, const std::string &propertyName);

    static const IndexDescriptor
    createIndex(Txn &txn, const std::string &className, const std::string &propertyName, bool isUnique = false);

    static void dropIndex(Txn &txn, const std::string &className, const std::string &propertyName);
  };

  //*************************************************************
  //*  NogDB database operations.                               *
  //*************************************************************

  struct DB {
    DB() = delete;

    ~DB() noexcept = delete;

    static DBInfo getDBInfo(const Txn &txn);

    static Record getRecord(const Txn &txn, const RecordDescriptor &recordDescriptor);

    static const std::vector<ClassDescriptor> getClasses(const Txn &txn);

    static const std::vector<PropertyDescriptor> getProperties(const Txn &txn, const std::string &className);

    static const std::vector<PropertyDescriptor> getProperties(const Txn &txn, const ClassDescriptor &classDescriptor);

    static const std::vector<IndexDescriptor> getIndexes(const Txn &txn, const ClassDescriptor &classDescriptor);

    static const ClassDescriptor getClass(const Txn &txn, const std::string &className);

    static const ClassDescriptor getClass(const Txn &txn, const ClassId &classId);

    static const PropertyDescriptor
    getProperty(const Txn &txn, const std::string &className, const std::string &propertyName);

    static const IndexDescriptor
    getIndex(const Txn &txn, const std::string &className, const std::string &propertyName);

  };

  //*************************************************************
  //*  NogDB vertex operations.                                 *
  //*************************************************************

  struct Vertex {
    Vertex() = delete;

    ~Vertex() noexcept = delete;

    static const RecordDescriptor create(Txn &txn, const std::string &className, const Record &record = Record{});

    static void update(Txn &txn, const RecordDescriptor &recordDescriptor, const Record &record);

    static void destroy(Txn &txn, const RecordDescriptor &recordDescriptor);

    static void destroy(Txn &txn, const std::string &className);

    static ResultSet get(const Txn &txn, const std::string &className);

    static ResultSet getExtend(const Txn &txn, const std::string &className);

    static ResultSetCursor getCursor(const Txn &txn, const std::string &className);

    static ResultSetCursor getExtendCursor(const Txn &txn, const std::string &className);

    static ResultSet
    getInEdge(const Txn &txn, const RecordDescriptor &recordDescriptor, const GraphFilter &edgeFilter = GraphFilter{});

    static ResultSet getOutEdge(const Txn &txn, const RecordDescriptor &recordDescriptor,
                                const GraphFilter &edgeFilter = GraphFilter{});

    static ResultSet getAllEdge(const Txn &txn, const RecordDescriptor &recordDescriptor,
                                const GraphFilter &edgeFilter = GraphFilter{});

    static ResultSetCursor getInEdgeCursor(const Txn &txn, const RecordDescriptor &recordDescriptor,
                                           const GraphFilter &edgeFilter = GraphFilter{});

    static ResultSetCursor getOutEdgeCursor(const Txn &txn, const RecordDescriptor &recordDescriptor,
                                            const GraphFilter &edgeFilter = GraphFilter{});

    static ResultSetCursor getAllEdgeCursor(const Txn &txn, const RecordDescriptor &recordDescriptor,
                                            const GraphFilter &edgeFilter = GraphFilter{});

    static ResultSet get(const Txn &txn, const std::string &className, const Condition &condition);

    static ResultSet get(const Txn &txn, const std::string &className, bool (*condition)(const Record &));

    static ResultSet get(const Txn &txn, const std::string &className, const MultiCondition &exp);

    static ResultSet getExtend(const Txn &txn, const std::string &className, const Condition &condition);

    static ResultSet getExtend(const Txn &txn, const std::string &className, bool (*condition)(const Record &));

    static ResultSet getExtend(const Txn &txn, const std::string &className, const MultiCondition &exp);

    static ResultSet getIndex(const Txn &txn, const std::string &className, const Condition &condition);

    static ResultSet getIndex(const Txn &txn, const std::string &className, const MultiCondition &exp);

    static ResultSet getExtendIndex(const Txn &txn, const std::string &className, const Condition &condition);

    static ResultSet getExtendIndex(const Txn &txn, const std::string &className, const MultiCondition &exp);

    static ResultSetCursor getCursor(const Txn &txn, const std::string &className, const Condition &condition);

    static ResultSetCursor getCursor(const Txn &txn, const std::string &className, bool (*condition)(const Record &));

    static ResultSetCursor getCursor(const Txn &txn, const std::string &className, const MultiCondition &exp);

    static ResultSetCursor getExtendCursor(const Txn &txn, const std::string &className, const Condition &condition);

    static ResultSetCursor
    getExtendCursor(const Txn &txn, const std::string &className, bool (*condition)(const Record &));

    static ResultSetCursor getExtendCursor(const Txn &txn, const std::string &className, const MultiCondition &exp);

    static ResultSetCursor getIndexCursor(const Txn &txn, const std::string &className, const Condition &condition);

    static ResultSetCursor getIndexCursor(const Txn &txn, const std::string &className, const MultiCondition &exp);

    static ResultSetCursor
    getExtendIndexCursor(const Txn &txn, const std::string &className, const Condition &condition);

    static ResultSetCursor
    getExtendIndexCursor(const Txn &txn, const std::string &className, const MultiCondition &exp);

  };

  //*************************************************************
  //*  NogDB edge operations.                                   *
  //*************************************************************

  struct Edge {
    Edge() = delete;

    ~Edge() noexcept = delete;

    static const RecordDescriptor
    create(Txn &txn, const std::string &className, const RecordDescriptor &srcVertexRecordDescriptor,
           const RecordDescriptor &dstVertexRecordDescriptor, const Record &record = Record{});

    static void update(Txn &txn, const RecordDescriptor &recordDescriptor, const Record &record);

    static void updateSrc(Txn &txn, const RecordDescriptor &recordDescriptor,
                          const RecordDescriptor &newSrcVertexRecordDescriptor);

    static void
    updateDst(Txn &txn, const RecordDescriptor &recordDescriptor, const RecordDescriptor &newDstRecordDescriptor);

    static void destroy(Txn &txn, const RecordDescriptor &recordDescriptor);

    static void destroy(Txn &txn, const std::string &className);

    static ResultSet get(const Txn &txn, const std::string &className);

    static ResultSet getExtend(const Txn &txn, const std::string &className);

    static ResultSetCursor getCursor(const Txn &txn, const std::string &className);

    static ResultSetCursor getExtendCursor(const Txn &txn, const std::string &className);

    static Result getSrc(const Txn &txn, const RecordDescriptor &recordDescriptor);

    static Result getDst(const Txn &txn, const RecordDescriptor &recordDescriptor);

    static ResultSet getSrcDst(const Txn &txn, const RecordDescriptor &recordDescriptor);

    static ResultSet get(const Txn &txn, const std::string &className, const Condition &condition);

    static ResultSet get(const Txn &txn, const std::string &className, bool (*condition)(const Record &));

    static ResultSet get(const Txn &txn, const std::string &className, const MultiCondition &exp);

    static ResultSet getExtend(const Txn &txn, const std::string &className, const Condition &condition);

    static ResultSet getExtend(const Txn &txn, const std::string &className, bool (*condition)(const Record &));

    static ResultSet getExtend(const Txn &txn, const std::string &className, const MultiCondition &exp);

    static ResultSet getIndex(const Txn &txn, const std::string &className, const Condition &condition);

    static ResultSet getIndex(const Txn &txn, const std::string &className, const MultiCondition &exp);

    static ResultSet getExtendIndex(const Txn &txn, const std::string &className, const Condition &condition);

    static ResultSet getExtendIndex(const Txn &txn, const std::string &className, const MultiCondition &exp);

    static ResultSetCursor getCursor(const Txn &txn, const std::string &className, const Condition &condition);

    static ResultSetCursor getCursor(const Txn &txn, const std::string &className, bool (*condition)(const Record &));

    static ResultSetCursor getCursor(const Txn &txn, const std::string &className, const MultiCondition &exp);

    static ResultSetCursor getExtendCursor(const Txn &txn, const std::string &className, const Condition &condition);

    static ResultSetCursor
    getExtendCursor(const Txn &txn, const std::string &className, bool (*condition)(const Record &));

    static ResultSetCursor getExtendCursor(const Txn &txn, const std::string &className, const MultiCondition &exp);

    static ResultSetCursor getIndexCursor(const Txn &txn, const std::string &className, const Condition &condition);

    static ResultSetCursor getIndexCursor(const Txn &txn, const std::string &className, const MultiCondition &exp);

    static ResultSetCursor
    getExtendIndexCursor(const Txn &txn, const std::string &className, const Condition &condition);

    static ResultSetCursor
    getExtendIndexCursor(const Txn &txn, const std::string &className, const MultiCondition &exp);
  };

  //*************************************************************
  //*  NogDB graph traversal operations.                        *
  //*************************************************************

  struct Traverse {
    Traverse() = delete;

    ~Traverse() noexcept = delete;

    static ResultSet
    inEdgeBfs(const Txn &txn, const RecordDescriptor &recordDescriptor, unsigned int minDepth, unsigned int maxDepth,
              const GraphFilter &edgeFilter = GraphFilter{},
              const GraphFilter &vertexFilter = GraphFilter{});

    static ResultSet
    outEdgeBfs(const Txn &txn, const RecordDescriptor &recordDescriptor, unsigned int minDepth, unsigned int maxDepth,
               const GraphFilter &edgeFilter = GraphFilter{},
               const GraphFilter &vertexFilter = GraphFilter{});

    static ResultSet
    allEdgeBfs(const Txn &txn, const RecordDescriptor &recordDescriptor, unsigned int minDepth, unsigned int maxDepth,
               const GraphFilter &edgeFilter = GraphFilter{},
               const GraphFilter &vertexFilter = GraphFilter{});

    static ResultSet
    inEdgeDfs(const Txn &txn, const RecordDescriptor &recordDescriptor, unsigned int minDepth, unsigned int maxDepth,
              const GraphFilter &edgeFilter = GraphFilter{},
              const GraphFilter &vertexFilter = GraphFilter{});

    static ResultSet
    outEdgeDfs(const Txn &txn, const RecordDescriptor &recordDescriptor, unsigned int minDepth, unsigned int maxDepth,
               const GraphFilter &edgeFilter = GraphFilter{},
               const GraphFilter &vertexFilter = GraphFilter{});

    static ResultSet
    allEdgeDfs(const Txn &txn, const RecordDescriptor &recordDescriptor, unsigned int minDepth, unsigned int maxDepth,
               const GraphFilter &edgeFilter = GraphFilter{},
               const GraphFilter &vertexFilter = GraphFilter{});

    static ResultSet shortestPath(const Txn &txn,
                                  const RecordDescriptor &srcVertexRecordDescriptor,
                                  const RecordDescriptor &dstVertexRecordDescriptor,
                                  const GraphFilter &edgeFilter = GraphFilter{},
                                  const GraphFilter &vertexFilter = GraphFilter{});

    static ResultSetCursor
    inEdgeBfsCursor(const Txn &txn, const RecordDescriptor &recordDescriptor, unsigned int minDepth, unsigned int maxDepth,
                    const GraphFilter &edgeFilter = GraphFilter{},
                    const GraphFilter &vertexFilter = GraphFilter{});

    static ResultSetCursor
    outEdgeBfsCursor(const Txn &txn, const RecordDescriptor &recordDescriptor, unsigned int minDepth, unsigned int maxDepth,
                     const GraphFilter &edgeFilter = GraphFilter{},
                     const GraphFilter &vertexFilter = GraphFilter{});

    static ResultSetCursor
    allEdgeBfsCursor(const Txn &txn, const RecordDescriptor &recordDescriptor, unsigned int minDepth, unsigned int maxDepth,
                     const GraphFilter &edgeFilter = GraphFilter{},
                     const GraphFilter &vertexFilter = GraphFilter{});

    static ResultSetCursor
    inEdgeDfsCursor(const Txn &txn, const RecordDescriptor &recordDescriptor, unsigned int minDepth, unsigned int maxDepth,
                    const GraphFilter &edgeFilter = GraphFilter{},
                    const GraphFilter &vertexFilter = GraphFilter{});

    static ResultSetCursor
    outEdgeDfsCursor(const Txn &txn, const RecordDescriptor &recordDescriptor, unsigned int minDepth, unsigned int maxDepth,
                     const GraphFilter &edgeFilter = GraphFilter{},
                     const GraphFilter &vertexFilter = GraphFilter{});

    static ResultSetCursor
    allEdgeDfsCursor(const Txn &txn, const RecordDescriptor &recordDescriptor, unsigned int minDepth, unsigned int maxDepth,
                     const GraphFilter &edgeFilter = GraphFilter{},
                     const GraphFilter &vertexFilter = GraphFilter{});

    static ResultSetCursor shortestPathCursor(const Txn &txn,
                                              const RecordDescriptor &srcVertexRecordDescriptor,
                                              const RecordDescriptor &dstVertexRecordDescriptor,
                                              const GraphFilter &edgeFilter = GraphFilter{},
                                              const GraphFilter &vertexFilter = GraphFilter{});
  };

}
