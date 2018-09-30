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

#include <functional>
#include <queue>

#include "constant.hpp"
#include "lmdb_engine.hpp"
#include "parser.hpp"
#include "schema.hpp"

#include "nogdb_compare.h"
#include "nogdb_types.h"
#include "nogdb_txn.h"

namespace nogdb {
  struct Algorithm {
    Algorithm() = delete;

    ~Algorithm() noexcept = delete;

    static ResultSet breadthFirstSearch(const Txn &txn,
                                        const RecordDescriptor &recordDescriptor,
                                        unsigned int minDepth,
                                        unsigned int maxDepth,
                                        const std::vector<ClassId> &edgeClassIds,
                                        std::vector<RecordId>
                                        (Graph::*edgeFunc)(const BaseTxn &baseTxn, const RecordId &rid,
                                                           const ClassId &classId),
                                        RecordId (Graph::*vertexFunc)(const BaseTxn &baseTxn, const RecordId &rid),
                                        const PathFilter &pathFilter) {
      const auto searchResultDescriptor = breadthFirstSearchRdesc(txn, recordDescriptor, minDepth, maxDepth,
                                                                  edgeClassIds, edgeFunc, vertexFunc, pathFilter);

      ResultSet result(searchResultDescriptor.size());
      std::transform(searchResultDescriptor.begin(), searchResultDescriptor.end(), result.begin(),
                     [&txn](const RecordDescriptor &descriptor) {
                       const auto &record = retrieveRecord(txn, descriptor);
                       record.setBasicInfo(DEPTH_PROPERTY, descriptor.depth);
                       return Result(descriptor, record);
                     });

      return result;
    }

    static ResultSet depthFirstSearch(const Txn &txn,
                                      const RecordDescriptor &recordDescriptor,
                                      unsigned int minDepth,
                                      unsigned int maxDepth,
                                      const std::vector<ClassId> &edgeClassIds,
                                      std::vector<RecordId>
                                      (Graph::*edgeFunc)(const BaseTxn &baseTxn, const RecordId &rid,
                                                         const ClassId &classId),
                                      RecordId (Graph::*vertexFunc)(const BaseTxn &baseTxn, const RecordId &rid),
                                      const PathFilter &pathFilter) {
      const auto searchResultDescriptor = depthFirstSearchRdesc(txn, recordDescriptor, minDepth, maxDepth,
                                                                edgeClassIds, edgeFunc, vertexFunc, pathFilter);
      ResultSet result(searchResultDescriptor.size());
      std::transform(searchResultDescriptor.begin(), searchResultDescriptor.end(), result.begin(),
                     [&txn](const RecordDescriptor &descriptor) {
                       const auto &record = retrieveRecord(txn, descriptor);
                       record.setBasicInfo(DEPTH_PROPERTY, descriptor.depth);
                       return Result(descriptor, record);
                     });

      return result;
    }

    static ResultSet bfsShortestPath(const Txn &txn,
                                     const RecordDescriptor &srcVertexRecordDescriptor,
                                     const RecordDescriptor &dstVertexRecordDescriptor,
                                     const std::vector<ClassId> &edgeClassIds,
                                     const PathFilter &pathFilter) {
      const auto searchResultDescriptor = bfsShortestPathRdesc(txn, srcVertexRecordDescriptor,
                                                               dstVertexRecordDescriptor, edgeClassIds, pathFilter);

      ResultSet result(searchResultDescriptor.size());
      std::transform(searchResultDescriptor.begin(), searchResultDescriptor.end(), result.begin(),
                     [&txn](const RecordDescriptor &descriptor) {
                       const auto &record = retrieveRecord(txn, descriptor);
                       record.setBasicInfo(DEPTH_PROPERTY, descriptor.depth);
                       return Result(descriptor, record);
                     });

      return result;
    }

    template<typename CostFuncType,
        typename T = typename std::result_of<CostFuncType(const Txn &, const RecordDescriptor &)>::type,
        typename CompareT = std::greater<T>>
    static std::pair<T, ResultSet> dijkstraShortestPath(const Txn &txn,
                                                        const RecordDescriptor &srcVertexRecordDescriptor,
                                                        const RecordDescriptor &dstVertexRecordDescriptor,
                                                        const CostFuncType &costFunction,
                                                        const std::vector<ClassId> &edgeClassIds,
                                                        const PathFilter &pathFilter) {
      auto result = dijkstraShortestPathRdesc<CostFuncType, T, CompareT>(txn, srcVertexRecordDescriptor,
                                                                         dstVertexRecordDescriptor,
                                                                         costFunction,
                                                                         edgeClassIds, pathFilter);
      const auto &descriptors = result.second;
      ResultSet resultSet(descriptors.size());
      std::transform(descriptors.begin(), descriptors.end(), resultSet.begin(),
                     [&txn](const RecordDescriptor &descriptor) {
                       return Result(descriptor, retrieveRecord(txn, descriptor.rid));
                     });

      return {result.first, resultSet};
    }

    static std::vector<RecordDescriptor>
    breadthFirstSearchRdesc(const Txn &txn,
                            const RecordDescriptor &recordDescriptor,
                            unsigned int minDepth,
                            unsigned int maxDepth,
                            const std::vector<ClassId> &edgeClassIds,
                            std::vector<RecordId> (Graph::*edgeFunc)(const BaseTxn &baseTxn, const RecordId &rid,
                                                                     const ClassId &classId),
                            RecordId (Graph::*vertexFunc)(const BaseTxn &baseTxn, const RecordId &rid),
                            const PathFilter &pathFilter) {
      switch (Generic::checkIfRecordExist(txn, recordDescriptor)) {
        case RECORD_NOT_EXIST:
          throw NOGDB_GRAPH_ERROR(NOGDB_GRAPH_NOEXST_VERTEX);
        case RECORD_NOT_EXIST_IN_MEMORY:
          return ((minDepth == 0) && (minDepth <= maxDepth)) ?
                 std::vector<RecordDescriptor>{recordDescriptor} : std::vector<RecordDescriptor>{};
        default:
          auto result = std::vector<RecordDescriptor>{};
          auto classDescriptor = Schema::ClassDescriptorPtr{};
          auto classPropertyInfo = ClassPropertyInfo{};
          auto classDBHandler = storage_engine::lmdb::Dbi{};
          auto visited = std::unordered_set<RecordId, Graph::RecordIdHash>{recordDescriptor.rid};
          auto queue = std::queue<RecordId>{};

          unsigned int currentLevel = 0u;
          RecordId firstRecordId{recordDescriptor.rid};

          queue.push(recordDescriptor.rid);

          try {
            auto addUniqueVertex = [&](const RecordId &vertex, const PathFilter &pathFilter) {
              if (visited.find(vertex) != visited.cend()) return;
              auto tmpRdesc = pathFilter.isEnable() ?
                              retrieveRdesc(txn, classDescriptor, classPropertyInfo,
                                            classDBHandler, vertex, pathFilter, ClassType::VERTEX) :
                              RecordDescriptor{vertex};
              if ((currentLevel + 1 >= minDepth) && (currentLevel + 1 <= maxDepth) &&
                  (tmpRdesc != RecordDescriptor{})) {
                tmpRdesc.depth = currentLevel + 1;
                result.emplace_back(tmpRdesc);
              }

              visited.insert(vertex);

              if (firstRecordId == RecordId{}) {
                firstRecordId = vertex;
              }

              if ((currentLevel + 1 < maxDepth) && (tmpRdesc != RecordDescriptor{})) {
                queue.push(vertex);
              }
            };

            if (minDepth == 0) {
              result.emplace_back(recordDescriptor);
            }

            while (!queue.empty()) {
              auto vertexId = queue.front();
              queue.pop();

              if (vertexId == firstRecordId) {
                currentLevel += (vertexId != recordDescriptor.rid);
                firstRecordId = RecordId{};
              }

              const auto edgeRecordDescriptors = getIncidentEdges(txn, classDescriptor, classPropertyInfo,
                                                                  classDBHandler, edgeFunc, vertexId, pathFilter,
                                                                  edgeClassIds);

              for (const auto &edge: edgeRecordDescriptors) {
                if (vertexFunc != nullptr) {
                  addUniqueVertex(((*txn._txnCtx.dbRelation).*vertexFunc)(*(txn._txnBase), edge.rid), pathFilter);
                } else {
                  auto vertices = txn._txnCtx.dbRelation->getVertexSrcDst(*(txn._txnBase), edge.rid);
                  addUniqueVertex(vertices.first != vertexId ? vertices.first : vertices.second, pathFilter);
                }
              }
            }
          } catch (const Error &err) {
            if (err.code() == NOGDB_GRAPH_NOEXST_VERTEX) {
              throw NOGDB_GRAPH_ERROR(NOGDB_GRAPH_UNKNOWN_ERR);
            } else {
              throw err;
            }
          }
          return result;
      }
    }

    static std::vector<RecordDescriptor>
    depthFirstSearchRdesc(const Txn &txn,
                          const RecordDescriptor &recordDescriptor,
                          unsigned int minDepth,
                          unsigned int maxDepth,
                          const std::vector<ClassId> &edgeClassIds,
                          std::vector<RecordId>
                          (Graph::*edgeFunc)(const BaseTxn &baseTxn, const RecordId &rid, const ClassId &classId),
                          RecordId (Graph::*vertexFunc)(const BaseTxn &baseTxn, const RecordId &rid),
                          const PathFilter &pathFilter) {
      switch (Generic::checkIfRecordExist(txn, recordDescriptor)) {
        case RECORD_NOT_EXIST:
          throw NOGDB_GRAPH_ERROR(NOGDB_GRAPH_NOEXST_VERTEX);
        case RECORD_NOT_EXIST_IN_MEMORY:
          return ((minDepth == 0) && (minDepth <= maxDepth)) ?
                 std::vector<RecordDescriptor>{recordDescriptor} : std::vector<RecordDescriptor>{};
        default:
          auto result = std::vector<RecordDescriptor>{};
          auto classDescriptor = Schema::ClassDescriptorPtr{};
          auto classPropertyInfo = ClassPropertyInfo{};
          auto classDBHandler = storage_engine::lmdb::Dbi{};
          try {

            std::unordered_set<RecordId, Graph::RecordIdHash> visited{};
            std::vector<std::vector<RecordId>> stk{{recordDescriptor.rid}};
            unsigned int currentLevel = 0u;

            while (!stk[currentLevel].empty()) {

              const RecordId vertex = stk[currentLevel].back();
              stk[currentLevel].pop_back();

              if (visited.find(vertex) == visited.cend()) {
                visited.insert(vertex);

                if (currentLevel >= minDepth) {
                  auto tmpRdesc = pathFilter.isEnable() ?
                                  retrieveRdesc(txn, classDescriptor, classPropertyInfo,
                                                classDBHandler, vertex,
                                                currentLevel ? pathFilter : PathFilter{},
                                                ClassType::VERTEX) :
                                  RecordDescriptor{vertex};

                  if (tmpRdesc != RecordDescriptor{}) {
                    tmpRdesc.depth = currentLevel;
                    result.push_back(tmpRdesc);
                  }
                }

                if (currentLevel < maxDepth) {

                  ++currentLevel;
                  if (currentLevel == stk.size()) {
                    stk.emplace_back(decltype(stk)::value_type());
                  }

                  const auto edgeRecordDescriptors = getIncidentEdges(txn, classDescriptor,
                                                                      classPropertyInfo,
                                                                      classDBHandler, edgeFunc, vertex,
                                                                      pathFilter,
                                                                      edgeClassIds);

                  for (auto it = edgeRecordDescriptors.crbegin(); it != edgeRecordDescriptors.crend(); ++it) {
                    const RecordDescriptor &edge = *it;

                    RecordId nextVertex;
                    if (vertexFunc != nullptr) {
                      nextVertex = ((*txn._txnCtx.dbRelation).*vertexFunc)(*(txn._txnBase), edge.rid);
                    } else {
                      auto vertices = txn._txnCtx.dbRelation->getVertexSrcDst(*(txn._txnBase), edge.rid);
                      nextVertex = (vertices.first != vertex ? vertices.first : vertices.second);
                    }

                    if (visited.find(nextVertex) != visited.cend()) continue;

                    stk[currentLevel].emplace_back(std::move(nextVertex));
                  }
                }
              }

              while (currentLevel && stk[currentLevel].empty()) {
                --currentLevel;
              }
            }
          } catch (const Error &err) {
            if (err.code() == NOGDB_GRAPH_NOEXST_VERTEX) {
              throw NOGDB_GRAPH_ERROR(NOGDB_GRAPH_UNKNOWN_ERR);
            } else {
              throw err;
            }
          }
          return result;
      }
    }

    static std::vector<RecordDescriptor>
    bfsShortestPathRdesc(const Txn &txn,
                         const RecordDescriptor &srcVertexRecordDescriptor,
                         const RecordDescriptor &dstVertexRecordDescriptor,
                         const std::vector<ClassId> &edgeClassIds,
                         const PathFilter &pathFilter) {
      auto srcStatus = Generic::checkIfRecordExist(txn, srcVertexRecordDescriptor);
      auto dstStatus = Generic::checkIfRecordExist(txn, dstVertexRecordDescriptor);
      if (srcStatus == RECORD_NOT_EXIST) {
        throw NOGDB_GRAPH_ERROR(NOGDB_GRAPH_NOEXST_SRC);
      } else if (dstStatus == RECORD_NOT_EXIST) {
        throw NOGDB_GRAPH_ERROR(NOGDB_GRAPH_NOEXST_DST);
      } else if (srcStatus == RECORD_NOT_EXIST_IN_MEMORY || dstStatus == RECORD_NOT_EXIST_IN_MEMORY) {
        return std::vector<RecordDescriptor>{};
      } else {
        auto result = std::vector<RecordDescriptor>{};
        auto classDescriptor = Schema::ClassDescriptorPtr{};
        auto classPropertyInfo = ClassPropertyInfo{};
        auto classDBHandler = storage_engine::lmdb::Dbi{};
        try {
          if (srcVertexRecordDescriptor == dstVertexRecordDescriptor) {
            result.emplace_back(srcVertexRecordDescriptor);
          } else {
            bool found = false;
            auto visited = std::unordered_map<RecordId, std::pair<RecordDescriptor, RecordId>, Graph::RecordIdHash>{};
            visited.insert({srcVertexRecordDescriptor.rid, {RecordDescriptor{}, RecordId{}}});
            auto queue = std::queue<RecordId>{};
            queue.push(srcVertexRecordDescriptor.rid);
            while (!queue.empty() && !found) {
              auto vertex = queue.front();
              queue.pop();

              const auto edgeRecordDescriptors = getOutEdges(txn, classDescriptor, classPropertyInfo,
                                                             classDBHandler, vertex, pathFilter, edgeClassIds);

              for (const auto &edge: edgeRecordDescriptors) {
                auto nextVertex = txn._txnCtx.dbRelation->getVertexDst(*(txn._txnBase), edge.rid);
                if (visited.find(nextVertex) == visited.cend()) {
                  auto tmpRdesc = (pathFilter.isSetVertex() || pathFilter.isSetEdge()) ?
                                  retrieveRdesc(txn, classDescriptor, classPropertyInfo,
                                                classDBHandler, nextVertex, pathFilter, ClassType::VERTEX)
                                                                                       : RecordDescriptor{nextVertex};
                  if (tmpRdesc != RecordDescriptor{}) {
                    visited.insert({nextVertex, {tmpRdesc, vertex}});
                    queue.push(nextVertex);
                  }
                }
                if (nextVertex == dstVertexRecordDescriptor.rid) {
                  found = true;
                  break;
                }
              }
            }

            if (found) {
              auto vertex = dstVertexRecordDescriptor.rid;
              while (vertex != srcVertexRecordDescriptor.rid) {
                auto data = visited.at(vertex);
                result.emplace_back(data.first);
                vertex = data.second;
              }
              result.emplace_back(srcVertexRecordDescriptor);
              std::reverse(result.begin(), result.end());
              auto currentLevel = 0U;
              for (auto &res: result) {
                res.depth = currentLevel++;
              }
            }
          }
        } catch (const Error &err) {
          if (err.code() == NOGDB_GRAPH_NOEXST_VERTEX) {
            throw NOGDB_GRAPH_ERROR(NOGDB_GRAPH_UNKNOWN_ERR);
          } else {
            throw err;
          }
        }
        return result;
      }
    }

    template<typename CostFuncType,
        typename T = typename std::result_of<CostFuncType(const Txn &, const RecordDescriptor &)>::type,
        typename CompareT = std::greater<T>>
    static std::pair<T, std::vector<RecordDescriptor>>
    dijkstraShortestPathRdesc(const Txn &txn,
                              const RecordDescriptor &srcVertexRecordDescriptor,
                              const RecordDescriptor &dstVertexRecordDescriptor,
                              const CostFuncType &costFunction,
                              const std::vector<ClassId> &edgeClassIds,
                              const PathFilter &pathFilter) {

      auto srcStatus = Generic::checkIfRecordExist(txn, srcVertexRecordDescriptor);
      auto dstStatus = Generic::checkIfRecordExist(txn, dstVertexRecordDescriptor);

      if (srcStatus == RECORD_NOT_EXIST) {
        throw NOGDB_GRAPH_ERROR(NOGDB_GRAPH_NOEXST_SRC);
      } else if (dstStatus == RECORD_NOT_EXIST) {
        throw NOGDB_GRAPH_ERROR(NOGDB_GRAPH_NOEXST_DST);
      } else if (srcStatus == RECORD_NOT_EXIST_IN_MEMORY || dstStatus == RECORD_NOT_EXIST_IN_MEMORY) {
        return {T(), std::vector<RecordDescriptor>{}};
      } else {

        auto classDescriptor = Schema::ClassDescriptorPtr{};
        auto classPropertyInfo = ClassPropertyInfo{};
        auto classDBHandler = storage_engine::lmdb::Dbi{};

        auto parent = std::unordered_map<RecordId, RecordDescriptor, Graph::RecordIdHash>{};
        auto distance = std::unordered_map<RecordId, T, Graph::RecordIdHash>{};

        using nodeInfo = std::pair<T, RecordId>;
        class Comparator {
        public:
          inline bool operator()(const nodeInfo &n, const nodeInfo &m) {
            return CompareT()(n.first, m.first);
          }
        };
        auto heap = std::priority_queue<nodeInfo, std::vector<nodeInfo>, Comparator>{};

        const RecordId srcId = srcVertexRecordDescriptor.rid;
        const RecordId dstId = dstVertexRecordDescriptor.rid;

        heap.push(nodeInfo{T(), srcId});
        distance.insert({srcId, T()});

        while (!heap.empty()) {

          const T dist = heap.top().first;
          const RecordId vertex = heap.top().second;

          heap.pop();

          if (vertex == dstId) {
            break;
          }

          if (CompareT()(dist, distance.at(vertex))) {
            continue;
          }

          const auto edgeRecordDescriptors = getOutEdges(txn, classDescriptor, classPropertyInfo,
                                                         classDBHandler, vertex, pathFilter, edgeClassIds);

          for (const auto &edge : edgeRecordDescriptors) {
            auto nextVertex = txn._txnCtx.dbRelation->getVertexDst(*(txn._txnBase), edge.rid);
            auto tmpRdesc = pathFilter.isEnable() ?
                            retrieveRdesc(txn, classDescriptor, classPropertyInfo,
                                          classDBHandler, nextVertex, pathFilter, ClassType::VERTEX) :
                            RecordDescriptor{nextVertex};

            if (tmpRdesc == RecordDescriptor{}) continue;

            const T nextDist = dist + costFunction(txn, edge);

            if (distance.find(nextVertex) == distance.end() || CompareT()(distance.at(nextVertex), nextDist)) {

              parent[nextVertex] = edge;
              distance[nextVertex] = nextDist;

              heap.push(nodeInfo{nextDist, nextVertex});
            }
          }
        }

        if (distance.find(dstId) == distance.cend()) {
          return {T(), {}};
        }

        std::vector<RecordDescriptor> result{dstId};
        for (RecordId vertex = dstId; vertex != srcId;) {
          vertex = txn._txnCtx.dbRelation->getVertexSrc(*(txn._txnBase), parent.at(vertex).rid);
          result.emplace_back(vertex);
        }

        std::reverse(result.begin(), result.end());
        for (unsigned int i = 0u; i < result.size(); ++i) {
          result[i].depth = i;
        }

        return {distance.at(dstId), result};
      }
    }

    inline static Result retrieve(const Txn &txn,
                                  Schema::ClassDescriptorPtr &classDescriptor,
                                  ClassPropertyInfo &classPropertyInfo,
                                  storage_engine::lmdb::Dbi &classDBHandler,
                                  const RecordId &rid,
                                  const PathFilter &pathFilter,
                                  ClassType type) {
      auto dsTxnHandler = txn._txnBase->getDsTxnHandler();
      if (classDescriptor == nullptr || classDescriptor->id != rid.first) {
        classDescriptor = Generic::getClassInfo(txn, rid.first, ClassType::UNDEFINED);
        classPropertyInfo = Generic::getClassMapProperty(*txn._txnBase, classDescriptor);
        classDBHandler = dsTxnHandler->openDbi(std::to_string(rid.first), true);
      }
      auto className = BaseTxn::getCurrentVersion(*txn._txnBase, classDescriptor->name).first;
      auto rawData = classDBHandler.get(rid.second);
      auto record = Parser::parseRawDataWithBasicInfo(className, rid, rawData, classPropertyInfo);
      if (pathFilter.isSetVertex() && type == ClassType::VERTEX) {
        if ((*pathFilter.vertexFilter)(record)) {
          return Result{RecordDescriptor{rid}, record};
        } else {
          return Result{};
        }
      } else if (pathFilter.isSetEdge() && type == ClassType::EDGE) {
        if ((*pathFilter.edgeFilter)(record)) {
          return Result{RecordDescriptor{rid}, record};
        } else {
          return Result{};
        }
      } else {
        return Result{RecordDescriptor{rid}, record};
      }
    }

    inline static RecordDescriptor retrieveRdesc(const Txn &txn,
                                                 Schema::ClassDescriptorPtr &classDescriptor,
                                                 ClassPropertyInfo &classPropertyInfo,
                                                 storage_engine::lmdb::Dbi &classDBHandler,
                                                 const RecordId &rid,
                                                 const PathFilter &pathFilter,
                                                 ClassType type) {
      auto dsTxnHandler = txn._txnBase->getDsTxnHandler();
      if (classDescriptor == nullptr || classDescriptor->id != rid.first) {
        classDescriptor = Generic::getClassInfo(txn, rid.first, ClassType::UNDEFINED);
        classPropertyInfo = Generic::getClassMapProperty(*txn._txnBase, classDescriptor);
        classDBHandler = dsTxnHandler->openDbi(std::to_string(rid.first), true);
      }
      auto className = BaseTxn::getCurrentVersion(*txn._txnBase, classDescriptor->name).first;
      auto rawData = classDBHandler.get(rid.second);
      auto record = Parser::parseRawDataWithBasicInfo(className, rid, rawData, classPropertyInfo);
      if (pathFilter.isSetVertex() && type == ClassType::VERTEX) {
        if ((*pathFilter.vertexFilter)(record)) {
          return RecordDescriptor{rid};
        } else {
          return RecordDescriptor{};
        }
      } else if (pathFilter.isSetEdge() && type == ClassType::EDGE) {
        if ((*pathFilter.edgeFilter)(record)) {
          return RecordDescriptor{rid};
        } else {
          return RecordDescriptor{};
        }
      } else {
        return RecordDescriptor{rid};
      }
    }

    inline static Record retrieveRecord(const Txn &txn, const RecordDescriptor &descriptor) {
      auto dsTxnHandler = txn._txnBase->getDsTxnHandler();
      auto classDescriptor = Generic::getClassInfo(txn, descriptor.rid.first,
                                                   ClassType::UNDEFINED);
      auto classPropertyInfo = Generic::getClassMapProperty(*txn._txnBase, classDescriptor);
      auto classDBHandler = dsTxnHandler->openDbi(std::to_string(descriptor.rid.first), true);
      auto keyValue = classDBHandler.get(descriptor.rid.second);
      auto className = BaseTxn::getCurrentVersion(*txn._txnBase, classDescriptor->name).first;

      return Parser::parseRawDataWithBasicInfo(className, descriptor.rid, keyValue, classPropertyInfo);
    }

    inline static std::vector<RecordDescriptor>
    getIncidentEdges(const Txn &txn,
                     Schema::ClassDescriptorPtr &classDescriptor,
                     ClassPropertyInfo &classPropertyInfo,
                     storage_engine::lmdb::Dbi &classDBHandler,
                     std::vector<RecordId>
                     (Graph::*edgeFunc)(const BaseTxn &baseTxn, const RecordId &rid,
                                        const ClassId &classId),
                     const RecordId &vertex,
                     const PathFilter &pathFilter,
                     const std::vector<ClassId> &edgeClassIds) {

      auto edgeRecordDescriptors = std::vector<RecordDescriptor>{};
      if (edgeClassIds.empty()) {
        for (const auto &edge: ((*txn._txnCtx.dbRelation).*edgeFunc)(*(txn._txnBase), vertex, 0)) {
          auto tmpRdesc = (pathFilter.isSetVertex() || pathFilter.isSetEdge()) ?
                          retrieveRdesc(txn, classDescriptor, classPropertyInfo,
                                        classDBHandler, edge, pathFilter, ClassType::EDGE) :
                          RecordDescriptor{edge};
          if (tmpRdesc != RecordDescriptor{}) {
            edgeRecordDescriptors.emplace_back(RecordDescriptor{edge});
          }
        }
      } else {
        for (const auto &edgeId: edgeClassIds) {
          for (const auto &edge: ((*txn._txnCtx.dbRelation).*edgeFunc)(*(txn._txnBase), vertex, edgeId)) {
            auto tmpRdesc = (pathFilter.isSetVertex() || pathFilter.isSetEdge()) ?
                            retrieveRdesc(txn, classDescriptor, classPropertyInfo,
                                          classDBHandler, edge, pathFilter, ClassType::EDGE) :
                            RecordDescriptor{edge};
            if (tmpRdesc != RecordDescriptor{}) {
              edgeRecordDescriptors.emplace_back(RecordDescriptor{edge});
            }
          }
        }
      }

      return edgeRecordDescriptors;
    }

    inline static std::vector<RecordDescriptor>
    getOutEdges(const Txn &txn,
                Schema::ClassDescriptorPtr &classDescriptor,
                ClassPropertyInfo &classPropertyInfo,
                storage_engine::lmdb::Dbi &classDBHandler,
                const RecordId &vertex,
                const PathFilter &pathFilter,
                const std::vector<ClassId> &edgeClassIds) {

      std::vector<RecordDescriptor> edgeRecordDescriptors{};

      if (edgeClassIds.empty()) {
        for (const auto &edge: txn._txnCtx.dbRelation->getEdgeOut(*(txn._txnBase), vertex, 0)) {
          auto tmpRdesc = pathFilter.isEnable() ?
                          retrieveRdesc(txn, classDescriptor, classPropertyInfo,
                                        classDBHandler, edge, pathFilter, ClassType::EDGE) :
                          RecordDescriptor{edge};
          if (tmpRdesc != RecordDescriptor{}) {
            edgeRecordDescriptors.emplace_back(RecordDescriptor{edge});
          }
        }
      } else {
        for (const auto &edgeId: edgeClassIds) {
          for (const auto &edge: txn._txnCtx.dbRelation->getEdgeOut(*(txn._txnBase), vertex, edgeId)) {
            auto tmpRdesc = pathFilter.isEnable() ?
                            retrieveRdesc(txn, classDescriptor, classPropertyInfo,
                                          classDBHandler, edge, pathFilter, ClassType::EDGE) :
                            RecordDescriptor{edge};
            if (tmpRdesc != RecordDescriptor{}) {
              edgeRecordDescriptors.emplace_back(RecordDescriptor{edge});
            }
          }
        }
      }

      return edgeRecordDescriptors;
    }
  };

}
