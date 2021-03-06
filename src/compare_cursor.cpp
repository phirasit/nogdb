/*
 *  Copyright (C) 2018, Throughwave (Thailand) Co., Ltd.
 *  <peerawich at throughwave dot co dot th>
 *
 *  This file is part of libnogdb, the NogDB core library in C++.
 *
 *  libnogdb is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include <cassert>
#include <vector>

#include "shared_lock.hpp"
#include "constant.hpp"
#include "env_handler.hpp"
#include "datastore.hpp"
#include "graph.hpp"
#include "parser.hpp"
#include "generic.hpp"
#include "compare.hpp"
#include "index.hpp"

#include "nogdb_errors.h"
#include "nogdb_compare.h"

namespace nogdb {

//*****************************************************************
//*  compare by condition and multi-condition object              *
//*****************************************************************

    std::vector<RecordDescriptor>
    Compare::getRdescCondition(const Txn &txn, const std::vector<ClassInfo> &classInfos, const Condition &condition,
                               PropertyType type) {
        auto result = std::vector<RecordDescriptor>{};
        try {
            for (const auto &classInfo: classInfos) {
                auto classDBHandler = Datastore::openDbi(txn.txnBase->getDsTxnHandler(), std::to_string(classInfo.id), true);
                auto cursorHandler = Datastore::CursorHandlerWrapper(txn.txnBase->getDsTxnHandler(), classDBHandler);
                auto keyValue = Datastore::getNextCursor(cursorHandler.get());
                while (!keyValue.empty()) {
                    auto key = Datastore::getKeyAsNumeric<PositionId>(keyValue);
                    if (*key != EM_MAXRECNUM) {
                        auto record = Parser::parseRawData(keyValue, classInfo.propertyInfo);
                        auto tmpRecord = record.set(CLASS_NAME_PROPERTY, classInfo.name)
                                .set(RECORD_ID_PROPERTY, rid2str(RecordId{classInfo.id, *key}));
                        if (condition.comp != Condition::Comparator::IS_NULL &&
                            condition.comp != Condition::Comparator::NOT_NULL) {
                            if (tmpRecord.get(condition.propName).empty()) {
                                keyValue = Datastore::getNextCursor(cursorHandler.get());
                                continue;
                            }
                            if (compareBytesValue(tmpRecord.get(condition.propName), type, condition)) {
                                result.push_back(RecordDescriptor{classInfo.id, *key});
                            }
                        } else {
                            switch (condition.comp) {
                                case Condition::Comparator::IS_NULL:
                                    if (tmpRecord.get(condition.propName).empty()) {
                                        result.push_back(RecordDescriptor{classInfo.id, *key});
                                    }
                                    break;
                                case Condition::Comparator::NOT_NULL:
                                    if (!tmpRecord.get(condition.propName).empty()) {
                                        result.push_back(RecordDescriptor{classInfo.id, *key});
                                    }
                                    break;
                                default:
                                    throw Error(CTX_INVALID_COMPARATOR, Error::Type::CONTEXT);
                            }
                        }
                    }
                    keyValue = Datastore::getNextCursor(cursorHandler.get());
                }
            }
        } catch (Datastore::ErrorType &err) {
            throw Error(err, Error::Type::DATASTORE);
        }
        return result;
    }

    std::vector<RecordDescriptor>
    Compare::getRdescMultiCondition(const Txn &txn,
                                    const std::vector<ClassInfo> &classInfos,
                                    const MultiCondition &conditions,
                                    const PropertyMapType &types) {
        auto result = std::vector<RecordDescriptor>{};
        try {
            for (const auto &classInfo: classInfos) {
                auto classDBHandler = Datastore::openDbi(txn.txnBase->getDsTxnHandler(), std::to_string(classInfo.id), true);
                auto cursorHandler = Datastore::CursorHandlerWrapper(txn.txnBase->getDsTxnHandler(), classDBHandler);
                auto keyValue = Datastore::getNextCursor(cursorHandler.get());
                while (!keyValue.empty()) {
                    auto key = Datastore::getKeyAsNumeric<PositionId>(keyValue);
                    if (*key != EM_MAXRECNUM) {
                        auto record = Parser::parseRawData(keyValue, classInfo.propertyInfo);
                        auto tmpRecord = record.set(CLASS_NAME_PROPERTY, classInfo.name)
                                .set(RECORD_ID_PROPERTY, rid2str(RecordId{classInfo.id, *key}));
                        if (conditions.execute(tmpRecord, types)) {
                            result.push_back(RecordDescriptor{classInfo.id, *key});
                        }
                    }
                    keyValue = Datastore::getNextCursor(cursorHandler.get());
                }
            }
        } catch (Datastore::ErrorType &err) {
            throw Error(err, Error::Type::DATASTORE);
        }
        return result;
    }

    std::vector<RecordDescriptor>
    Compare::getRdescEdgeCondition(const Txn &txn, const RecordDescriptor &recordDescriptor,
                                   const std::vector<ClassId> &edgeClassIds,
                                   std::vector<RecordId> (Graph::*func)(const BaseTxn &txn, const RecordId &rid, const ClassId &classId),
                                   const Condition &condition, PropertyType type) {
        switch (Generic::checkIfRecordExist(txn, recordDescriptor)) {
            case RECORD_NOT_EXIST:
                throw Error(GRAPH_NOEXST_VERTEX, Error::Type::GRAPH);
            case RECORD_NOT_EXIST_IN_MEMORY:
                return std::vector<RecordDescriptor>{};
            default:
                auto result = std::vector<RecordDescriptor>{};
                try {
                    auto classDescriptor = Schema::ClassDescriptorPtr{};
                    auto classPropertyInfo = ClassPropertyInfo{};
                    auto classDBHandler = Datastore::DBHandler{};
                    auto filter = [&condition, &type](const Record &record) {
                        if (condition.comp != Condition::Comparator::IS_NULL &&
                            condition.comp != Condition::Comparator::NOT_NULL) {
                            auto recordValue = record.get(condition.propName);
                            if (recordValue.empty()) {
                                return false;
                            }
                            if (compareBytesValue(recordValue, type, condition)) {
                                return true;
                            }
                        } else {
                            switch (condition.comp) {
                                case Condition::Comparator::IS_NULL:
                                    if (record.get(condition.propName).empty()) {
                                        return true;
                                    }
                                    break;
                                case Condition::Comparator::NOT_NULL:
                                    if (!record.get(condition.propName).empty()) {
                                        return true;
                                    }
                                    break;
                                default:
                                    throw Error(CTX_INVALID_COMPARATOR, Error::Type::CONTEXT);
                            }
                        }
                        return false;
                    };
                    auto retrieve = [&](std::vector<RecordDescriptor> &result, const RecordId &edge) {
                        if (classDescriptor == nullptr || classDescriptor->id != edge.first) {
                            classDescriptor = Generic::getClassDescriptor(txn, edge.first, ClassType::UNDEFINED);
                            classPropertyInfo = Generic::getClassMapProperty(*txn.txnBase, classDescriptor);
                            classDBHandler = Datastore::openDbi(txn.txnBase->getDsTxnHandler(), std::to_string(edge.first), true);
                        }
                        auto keyValue = Datastore::getRecord(txn.txnBase->getDsTxnHandler(), classDBHandler, edge.second);
                        auto record = Parser::parseRawData(keyValue, classPropertyInfo);
                        auto name = BaseTxn::getCurrentVersion(*txn.txnBase, classDescriptor->name).first;
                        auto tmpRecord = record.set(CLASS_NAME_PROPERTY, name).set(RECORD_ID_PROPERTY, rid2str(edge));
                        if (filter(tmpRecord)) {
                            result.push_back(RecordDescriptor{edge});
                        }
                    };
                    if (edgeClassIds.empty()) {
                        for (const auto &edge: ((*txn.txnCtx.dbRelation).*func)(*txn.txnBase, recordDescriptor.rid, 0)) {
                            retrieve(result, edge);
                        }
                    } else {
                        for (const auto &edgeId: edgeClassIds) {
                            for (const auto &edge: ((*txn.txnCtx.dbRelation).*func)(*txn.txnBase, recordDescriptor.rid, edgeId)) {
                                retrieve(result, edge);
                            }
                        }
                    }
                } catch (Graph::ErrorType &err) {
                    if (err == GRAPH_NOEXST_VERTEX) {
                        throw Error(GRAPH_UNKNOWN_ERR, Error::Type::GRAPH);
                    } else {
                        throw Error(err, Error::Type::GRAPH);
                    }
                } catch (Datastore::ErrorType &err) {
                    throw Error(err, Error::Type::DATASTORE);
                }
                return result;
        }
    }

    std::vector<RecordDescriptor>
    Compare::getRdescEdgeMultiCondition(const Txn &txn, const RecordDescriptor &recordDescriptor,
                                        const std::vector<ClassId> &edgeClassIds,
                                        std::vector<RecordId>
                                        (Graph::*func)(const BaseTxn &baseTxn, const RecordId &rid, const ClassId &classId),
                                        const MultiCondition &conditions, const PropertyMapType &types) {
        switch (Generic::checkIfRecordExist(txn, recordDescriptor)) {
            case RECORD_NOT_EXIST:
                throw Error(GRAPH_NOEXST_VERTEX, Error::Type::GRAPH);
            case RECORD_NOT_EXIST_IN_MEMORY:
                return std::vector<RecordDescriptor>{};
            default:
                auto result = std::vector<RecordDescriptor>{};
                try {
                    auto classDescriptor = Schema::ClassDescriptorPtr{};
                    auto classPropertyInfo = ClassPropertyInfo{};
                    auto classDBHandler = Datastore::DBHandler{};
                    auto retrieve = [&](std::vector<RecordDescriptor> &result, const RecordId &edge) {
                        if (classDescriptor == nullptr || classDescriptor->id != edge.first) {
                            classDescriptor = Generic::getClassDescriptor(txn, edge.first, ClassType::UNDEFINED);
                            classPropertyInfo = Generic::getClassMapProperty(*txn.txnBase, classDescriptor);
                            classDBHandler = Datastore::openDbi(txn.txnBase->getDsTxnHandler(), std::to_string(edge.first), true);
                        }
                        auto keyValue = Datastore::getRecord(txn.txnBase->getDsTxnHandler(), classDBHandler, edge.second);
                        auto record = Parser::parseRawData(keyValue, classPropertyInfo);
                        auto name = BaseTxn::getCurrentVersion(*txn.txnBase, classDescriptor->name).first;
                        auto tmpRecord = record.set(CLASS_NAME_PROPERTY, name).set(RECORD_ID_PROPERTY, rid2str(edge));
                        if (conditions.execute(tmpRecord, types)) {
                            result.push_back(RecordDescriptor{edge});
                        }
                    };
                    if (edgeClassIds.empty()) {
                        for (const auto &edge: ((*txn.txnCtx.dbRelation).*func)(*txn.txnBase, recordDescriptor.rid, 0)) {
                            retrieve(result, edge);
                        }
                    } else {
                        for (const auto &edgeId: edgeClassIds) {
                            for (const auto &edge: ((*txn.txnCtx.dbRelation).*func)(*txn.txnBase, recordDescriptor.rid, edgeId)) {
                                retrieve(result, edge);
                            }
                        }
                    }
                } catch (Graph::ErrorType &err) {
                    if (err == GRAPH_NOEXST_VERTEX) {
                        throw Error(GRAPH_UNKNOWN_ERR, Error::Type::GRAPH);
                    } else {
                        throw Error(err, Error::Type::GRAPH);
                    }
                } catch (Datastore::ErrorType &err) {
                    throw Error(err, Error::Type::DATASTORE);
                }
                return result;
        }
    }

    std::vector<RecordDescriptor>
    Compare::compareConditionRdesc(const Txn &txn, const std::string &className, ClassType type,
                                   const Condition &condition, bool searchIndexOnly) {
        auto propertyType = PropertyType::UNDEFINED;
        auto classDescriptors = Generic::getMultipleClassDescriptor(txn, std::set<std::string>{className}, type);
        auto classInfos = Generic::getMultipleClassMapProperty(*txn.txnBase, classDescriptors);
        for (const auto &classInfo: classInfos) {
            auto propertyInfo = classInfo.propertyInfo.nameToDesc.find(condition.propName);
            if (propertyInfo != classInfo.propertyInfo.nameToDesc.cend()) {
                if (propertyType == PropertyType::UNDEFINED) {
                    propertyType = propertyInfo->second.type;
                } else {
                    if (propertyType != propertyInfo->second.type) {
                        throw Error(CTX_CONFLICT_PROPTYPE, Error::Type::CONTEXT);
                    }
                }
            }
        }
        if (propertyType == PropertyType::UNDEFINED) {
            throw Error(CTX_NOEXST_PROPERTY, Error::Type::CONTEXT);
        }
        auto &classId = (*classDescriptors.cbegin())->id;
        auto foundIndex = Index::hasIndex(classId, *classInfos.cbegin(), condition);
        if (foundIndex.second) {
            return Index::getIndexRecord(txn, classId, foundIndex.first, condition);
        }
        if (searchIndexOnly) {
            return std::vector<RecordDescriptor>{};
        } else {
            return getRdescCondition(txn, classInfos, condition, propertyType);
        }
    }

    std::vector<RecordDescriptor>
    Compare::compareMultiConditionRdesc(const Txn &txn, const std::string &className, ClassType type,
                                        const MultiCondition &conditions, bool searchIndexOnly) {
        // check if all conditions are valid
        auto conditionPropertyTypes = PropertyMapType{};
        for (const auto &conditionNode: conditions.conditions) {
            auto conditionNodePtr = conditionNode.lock();
            assert(conditionNodePtr != nullptr);
            auto &condition = conditionNodePtr->getCondition();
            conditionPropertyTypes.emplace(condition.propName, PropertyType::UNDEFINED);
        }
        assert(!conditionPropertyTypes.empty());

        auto classDescriptors = Generic::getMultipleClassDescriptor(txn, std::set<std::string>{className}, type);
        auto classInfos = Generic::getMultipleClassMapProperty(*txn.txnBase, classDescriptors);
        auto numOfUndefPropertyType = conditionPropertyTypes.size();
        for (const auto &classInfo: classInfos) {
            for (auto &property: conditionPropertyTypes) {
                auto propertyInfo = classInfo.propertyInfo.nameToDesc.find(property.first);
                if (propertyInfo != classInfo.propertyInfo.nameToDesc.cend()) {
                    if (property.second == PropertyType::UNDEFINED) {
                        property.second = propertyInfo->second.type;
                        --numOfUndefPropertyType;
                    } else {
                        if (property.second != propertyInfo->second.type) {
                            throw Error(CTX_CONFLICT_PROPTYPE, Error::Type::CONTEXT);
                        }
                    }
                }
            }
        }
        if (numOfUndefPropertyType != 0) {
            throw Error(CTX_NOEXST_PROPERTY, Error::Type::CONTEXT);
        }
        auto &classId = (*classDescriptors.cbegin())->id;
        auto foundIndex = Index::hasIndex(classId, *classInfos.cbegin(), conditions);
        if (foundIndex.second) {
            return Index::getIndexRecord(txn, classId, foundIndex.first, conditions);
        }
        if (searchIndexOnly) {
            return std::vector<RecordDescriptor>{};
        } else {
            return getRdescMultiCondition(txn, classInfos, conditions, conditionPropertyTypes);
        }
    }

    std::vector<RecordDescriptor>
    Compare::compareEdgeConditionRdesc(const Txn &txn, const RecordDescriptor &recordDescriptor,
                                       std::vector<RecordId>
                                       (Graph::*func1)(const BaseTxn &baseTxn, const RecordId &rid, const ClassId &classId),
                                       std::vector<ClassId>
                                       (Graph::*func2)(const BaseTxn &baseTxn, const RecordId &rid),
                                       const Condition &condition, const ClassFilter &classFilter) {
        auto classDescriptor = Generic::getClassDescriptor(txn, recordDescriptor.rid.first, ClassType::VERTEX);
        auto propertyType = PropertyType::UNDEFINED;
        auto edgeClassIds = std::vector<ClassId>{};
        auto validateProperty = [&](const std::vector<ClassInfo> &classInfos, const std::string &propName) {
            auto isFoundProperty = false;
            for (const auto &classInfo: classInfos) {
                edgeClassIds.push_back(classInfo.id);
                auto propertyInfo = classInfo.propertyInfo.nameToDesc.find(propName);
                if (propertyInfo != classInfo.propertyInfo.nameToDesc.cend()) {
                    if (propertyType == PropertyType::UNDEFINED) {
                        propertyType = propertyInfo->second.type;
                        isFoundProperty = true;
                    } else {
                        if (propertyType != propertyInfo->second.type) {
                            throw Error(CTX_CONFLICT_PROPTYPE, Error::Type::CONTEXT);
                        }
                    }
                }
            }
            return isFoundProperty;
        };

        auto edgeClassDescriptors = Generic::getMultipleClassDescriptor(txn, classFilter.getClassName(), ClassType::EDGE);
        if (!edgeClassDescriptors.empty()) {
            auto edgeClassInfos = Generic::getMultipleClassMapProperty(*txn.txnBase, edgeClassDescriptors);
            if (!validateProperty(edgeClassInfos, condition.propName)) {
                throw Error(CTX_NOEXST_PROPERTY, Error::Type::CONTEXT);
            }
        } else {
            auto edgeClassIds = ((*txn.txnCtx.dbRelation).*func2)(*txn.txnBase, recordDescriptor.rid);
            auto edgeClassDescriptors = Generic::getMultipleClassDescriptor(txn, edgeClassIds, ClassType::EDGE);
            auto edgeClassInfos = Generic::getMultipleClassMapProperty(*txn.txnBase, edgeClassDescriptors);
            if (!validateProperty(edgeClassInfos, condition.propName)) {
                throw Error(CTX_NOEXST_PROPERTY, Error::Type::CONTEXT);
            }
        }
        return getRdescEdgeCondition(txn, recordDescriptor, edgeClassIds, func1, condition, propertyType);
    }

    std::vector<RecordDescriptor>
    Compare::compareEdgeMultiConditionRdesc(const Txn &txn, const RecordDescriptor &recordDescriptor,
                                            std::vector<RecordId>
                                            (Graph::*func1)(const BaseTxn &baseTxn, const RecordId &rid, const ClassId &classId),
                                            std::vector<ClassId>
                                            (Graph::*func2)(const BaseTxn &baseTxn, const RecordId &rid),
                                            const MultiCondition &conditions, const ClassFilter &classFilter) {
        // check if all conditions are valid
        auto conditionPropertyTypes = PropertyMapType{};
        for (const auto &conditionNode: conditions.conditions) {
            auto conditionNodePtr = conditionNode.lock();
            assert(conditionNodePtr != nullptr);
            auto &condition = conditionNodePtr->getCondition();
            conditionPropertyTypes.emplace(condition.propName, PropertyType::UNDEFINED);
        }
        assert(!conditionPropertyTypes.empty());

        auto classDescriptor = Generic::getClassDescriptor(txn, recordDescriptor.rid.first, ClassType::VERTEX);
        auto edgeClassIds = std::vector<ClassId> {};
        auto validateAndResolveProperties = [&](const std::vector<ClassInfo> &classInfos,
                                                PropertyMapType &propertyTypes) {
            auto numOfUndefPropertyType = propertyTypes.size();
            for (const auto &classInfo: classInfos) {
                edgeClassIds.push_back(classInfo.id);
                for (auto &property: propertyTypes) {
                    auto propertyInfo = classInfo.propertyInfo.nameToDesc.find(property.first);
                    if (propertyInfo != classInfo.propertyInfo.nameToDesc.cend()) {
                        if (property.second == PropertyType::UNDEFINED) {
                            property.second = propertyInfo->second.type;
                            --numOfUndefPropertyType;
                        } else {
                            if (property.second != propertyInfo->second.type) {
                                throw Error(CTX_CONFLICT_PROPTYPE, Error::Type::CONTEXT);
                            }
                        }
                    }
                }
            }
            return numOfUndefPropertyType == 0;
        };

        auto edgeClassDescriptors = Generic::getMultipleClassDescriptor(txn, classFilter.getClassName(), ClassType::EDGE);
        if (!edgeClassDescriptors.empty()) {
            auto edgeClassInfos = Generic::getMultipleClassMapProperty(*txn.txnBase, edgeClassDescriptors);
            if (!validateAndResolveProperties(edgeClassInfos, conditionPropertyTypes)) {
                throw Error(CTX_NOEXST_PROPERTY, Error::Type::CONTEXT);
            }
        } else {
            auto edgeClassIds = ((*txn.txnCtx.dbRelation).*func2)(*txn.txnBase, recordDescriptor.rid);
            auto edgeClassDescriptors = Generic::getMultipleClassDescriptor(txn, edgeClassIds, ClassType::EDGE);
            auto edgeClassInfos = Generic::getMultipleClassMapProperty(*txn.txnBase, edgeClassDescriptors);
            if (!validateAndResolveProperties(edgeClassInfos, conditionPropertyTypes)) {
                throw Error(CTX_NOEXST_PROPERTY, Error::Type::CONTEXT);
            }
        }
        return getRdescEdgeMultiCondition(txn, recordDescriptor, edgeClassIds, func1, conditions, conditionPropertyTypes);
    }

//*****************************************************************
//*  compare by a conditional function                            *
//*****************************************************************

    std::vector<RecordDescriptor>
    Compare::getRdescCondition(const Txn &txn, const std::vector<ClassInfo> &classInfos,
                               bool (*condition)(const Record &record)) {
        auto result = std::vector<RecordDescriptor>{};
        try {
            for (const auto &classInfo: classInfos) {
                auto classDBHandler = Datastore::openDbi(txn.txnBase->getDsTxnHandler(), std::to_string(classInfo.id), true);
                auto cursorHandler = Datastore::CursorHandlerWrapper(txn.txnBase->getDsTxnHandler(), classDBHandler);
                auto keyValue = Datastore::getNextCursor(cursorHandler.get());
                while (!keyValue.empty()) {
                    auto key = Datastore::getKeyAsNumeric<PositionId>(keyValue);
                    if (*key != EM_MAXRECNUM) {
                        auto record = Parser::parseRawData(keyValue, classInfo.propertyInfo);
                        auto tmpRecord = record.set(CLASS_NAME_PROPERTY, classInfo.name)
                                .set(RECORD_ID_PROPERTY, rid2str(RecordId{classInfo.id, *key}));
                        if ((*condition)(tmpRecord)) {
                            result.push_back(RecordDescriptor{classInfo.id, *key});
                        }
                    }
                    keyValue = Datastore::getNextCursor(cursorHandler.get());
                }
            }
        } catch (Datastore::ErrorType &err) {
            throw Error(err, Error::Type::DATASTORE);
        }
        return result;
    }

    std::vector<RecordDescriptor>
    Compare::compareConditionRdesc(const Txn &txn, const std::string &className, ClassType type,
                                   bool (*condition)(const Record &)) {
        auto classDescriptors = Generic::getMultipleClassDescriptor(txn, std::set<std::string>{className}, type);
        auto classInfos = Generic::getMultipleClassMapProperty(*txn.txnBase, classDescriptors);
        return getRdescCondition(txn, classInfos, condition);
    }

    std::vector<RecordDescriptor>
    Compare::getRdescEdgeCondition(const Txn &txn, const RecordDescriptor &recordDescriptor,
                                   const std::vector<ClassId> &edgeClassIds,
                                   std::vector<RecordId>
                                   (Graph::*func)(const BaseTxn &baseTxn, const RecordId &rid, const ClassId &classId),
                                   bool (*condition)(const Record &record)) {
        switch (Generic::checkIfRecordExist(txn, recordDescriptor)) {
            case RECORD_NOT_EXIST:
                throw Error(GRAPH_NOEXST_VERTEX, Error::Type::GRAPH);
            case RECORD_NOT_EXIST_IN_MEMORY:
                return std::vector<RecordDescriptor>{};
            default:
                auto result = std::vector<RecordDescriptor>{};
                try {
                    auto classDescriptor = Schema::ClassDescriptorPtr{};
                    auto classPropertyInfo = ClassPropertyInfo{};
                    auto classDBHandler = Datastore::DBHandler{};
                    auto retrieve = [&](std::vector<RecordDescriptor> &result, const RecordId &edge) {
                        if (classDescriptor == nullptr || classDescriptor->id != edge.first) {
                            classDescriptor = Generic::getClassDescriptor(txn, edge.first, ClassType::UNDEFINED);
                            classPropertyInfo = Generic::getClassMapProperty(*txn.txnBase, classDescriptor);
                            classDBHandler = Datastore::openDbi(txn.txnBase->getDsTxnHandler(), std::to_string(edge.first), true);
                        }
                        auto keyValue = Datastore::getRecord(txn.txnBase->getDsTxnHandler(), classDBHandler, edge.second);
                        auto record = Parser::parseRawData(keyValue, classPropertyInfo);
                        auto name = BaseTxn::getCurrentVersion(*txn.txnBase, classDescriptor->name).first;
                        auto tmpRecord = record.set(CLASS_NAME_PROPERTY, name).set(RECORD_ID_PROPERTY, rid2str(edge));
                        if ((*condition)(tmpRecord)) {
                            result.push_back(RecordDescriptor{edge});
                        }
                    };
                    if (edgeClassIds.empty()) {
                        for (const auto &edge: ((*txn.txnCtx.dbRelation).*func)(*txn.txnBase, recordDescriptor.rid, 0)) {
                            retrieve(result, edge);
                        }
                    } else {
                        for (const auto &edgeId: edgeClassIds) {
                            for (const auto &edge: ((*txn.txnCtx.dbRelation).*func)(*txn.txnBase, recordDescriptor.rid, edgeId)) {
                                retrieve(result, edge);
                            }
                        }
                    }
                } catch (Graph::ErrorType &err) {
                    if (err == GRAPH_NOEXST_VERTEX) {
                        throw Error(GRAPH_UNKNOWN_ERR, Error::Type::GRAPH);
                    } else {
                        throw Error(err, Error::Type::GRAPH);
                    }
                } catch (Datastore::ErrorType &err) {
                    throw Error(err, Error::Type::DATASTORE);
                }
                return result;
        }
    }

    std::vector<RecordDescriptor>
    Compare::compareEdgeConditionRdesc(const Txn &txn, const RecordDescriptor &recordDescriptor,
                                       std::vector<RecordId>
                                       (Graph::*func1)(const BaseTxn &baseTxn, const RecordId &rid, const ClassId &classId),
                                       std::vector<ClassId>
                                       (Graph::*func2)(const BaseTxn &baseTxn, const RecordId &rid),
                                       bool (*condition)(const Record &), const ClassFilter &classFilter) {
        auto classDescriptor = Generic::getClassDescriptor(txn, recordDescriptor.rid.first, ClassType::VERTEX);
        auto edgeClassIds = std::vector<ClassId>{};
        auto edgeClassDescriptors = Generic::getMultipleClassDescriptor(txn, classFilter.getClassName(), ClassType::EDGE);
        if (!edgeClassDescriptors.empty()) {
            auto edgeClassInfos = Generic::getMultipleClassMapProperty(*txn.txnBase, edgeClassDescriptors);
            for (const auto &classInfo: edgeClassInfos) {
                edgeClassIds.push_back(classInfo.id);
            }
        } else {
            auto edgeClassIds = ((*txn.txnCtx.dbRelation).*func2)(*txn.txnBase, recordDescriptor.rid);
            auto edgeClassDescriptors = Generic::getMultipleClassDescriptor(txn, edgeClassIds, ClassType::EDGE);
            auto edgeClassInfos = Generic::getMultipleClassMapProperty(*txn.txnBase, edgeClassDescriptors);
            for (const auto &classInfo: edgeClassInfos) {
                edgeClassIds.push_back(classInfo.id);
            }
        }
        return getRdescEdgeCondition(txn, recordDescriptor, edgeClassIds, func1, condition);
    }

}
