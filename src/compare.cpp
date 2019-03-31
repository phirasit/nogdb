/*
 *  Copyright (C) 2019, NogDB <https://nogdb.org>
 *  <nogdb at throughwave dot co dot th>
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

#include <regex>
#include <utility>

#include "compare.hpp"
#include "datarecord.hpp"
#include "index.hpp"
#include "relation.hpp"
#include "schema.hpp"

namespace nogdb {

using adapter::schema::ClassAccessInfo;
using adapter::schema::PropertyNameMapInfo;
using adapter::relation::Direction;
using compare::RecordCompare;

namespace compare {

    bool RecordCompare::compareBytesValue(const Bytes& value, PropertyType type, const Condition& condition)
    {
        if (condition.comp == Condition::Comparator::IN) {
            for (const auto& valueBytes : condition.valueSet) {
                if (genericCompareFunc(
                        value, type, valueBytes, Bytes {}, Condition::Comparator::EQUAL, condition.isIgnoreCase)
                    ^ condition.isNegative) {
                    return true;
                }
            }
        } else if (condition.comp >= Condition::Comparator::BETWEEN && condition.comp <= Condition::Comparator::BETWEEN_NO_BOUND) {
            return genericCompareFunc(
                       value, type, condition.valueSet[0], condition.valueSet[1], condition.comp, condition.isIgnoreCase)
                ^ condition.isNegative;
        } else {
            return genericCompareFunc(value, type, condition.valueBytes, Bytes {}, condition.comp, condition.isIgnoreCase) ^ condition.isNegative;
        }
        return false;
    }

    bool RecordCompare::compareRecordByCondition(const Record& record, const PropertyType& propertyType,
        const Condition& condition)
    {
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
            if (!record.get(condition.propName).empty()) {
                return compareBytesValue(record.get(condition.propName), propertyType, condition);
            }
            break;
        }
        return false;
    }

    bool RecordCompare::compareRecordByCondition(const Record& record,
        const PropertyNameMapInfo& propertyNameMapInfo,
        const Condition& condition)
    {
        auto foundProperty = propertyNameMapInfo.find(condition.propName);
        if (foundProperty == propertyNameMapInfo.cend()) {
            /**
             * Do not throw NOGDB_CTX_NOEXST_PROPERTY as it is used in graph filter which has
             * multiple edge comparison with a different set of properties
             */
            return false;
        }
        return compareRecordByCondition(record, foundProperty->second.type, condition);
    }

    bool RecordCompare::compareRecordByMultiCondition(const Record& record,
        const PropertyNameMapInfo& propertyNameMapInfo,
        const MultiCondition& multiCondition)
    {
        auto propertyTypes = PropertyMapType {};
        for (const auto& conditionNode : multiCondition.conditions) {
            auto conditionNodePtr = conditionNode.lock();
            require(conditionNodePtr != nullptr);
            auto& condition = conditionNodePtr->getCondition();
            auto foundConditionProperty = propertyTypes.find(condition.propName);
            if (foundConditionProperty == propertyTypes.cend()) {
                auto foundProperty = propertyNameMapInfo.find(condition.propName);
                /**
                 * Do not throw NOGDB_CTX_NOEXST_PROPERTY as it is used in graph filter which has
                 * multiple edge comparison with a different set of properties
                 */
                if (foundProperty != propertyNameMapInfo.cend()) {
                    propertyTypes.emplace(condition.propName, foundProperty->second.type);
                }
            }
        }
        return multiCondition.execute(record, propertyTypes);
    }

    ClassFilter RecordCompare::getFilterClasses(const Transaction& txn, const GraphFilter& filter)
    {
        auto classFilter = ClassFilter {};
        classFilter.onlyClasses.insert(filter._onlyClasses.cbegin(), filter._onlyClasses.cend());
        for (const auto& onlySubOfClass : filter._onlySubOfClasses) {
            auto superClassInfo = txn._adapter->dbClass()->getInfo(onlySubOfClass);
            if (superClassInfo.type != ClassType::UNDEFINED) {
                classFilter.onlyClasses.insert(superClassInfo.name);
                auto subClassesInfo = txn._adapter->dbClass()->getSubClassInfos(superClassInfo.id);
                for (const auto& subClassInfo : subClassesInfo) {
                    classFilter.onlyClasses.insert(subClassInfo.name);
                }
            }
        }
        classFilter.ignoreClasses.insert(filter._ignoreClasses.cbegin(), filter._ignoreClasses.cend());
        for (const auto& ignoreSubOfClass : filter._ignoreSubOfClasses) {
            auto superClassInfo = txn._adapter->dbClass()->getInfo(ignoreSubOfClass);
            if (superClassInfo.type != ClassType::UNDEFINED) {
                classFilter.ignoreClasses.insert(superClassInfo.name);
                auto subClassesInfo = txn._adapter->dbClass()->getSubClassInfos(superClassInfo.id);
                for (const auto& subClassInfo : subClassesInfo) {
                    classFilter.ignoreClasses.insert(subClassInfo.name);
                }
            }
        }
        return classFilter;
    }

    RecordDescriptor RecordCompare::filterRecord(const Transaction& txn,
        const RecordDescriptor& recordDescriptor,
        const GraphFilter& filter,
        const ClassFilter& classFilter)
    {
        return filterResult(txn, recordDescriptor, filter, classFilter).descriptor;
    }

    Result RecordCompare::filterResult(const Transaction& txn,
        const RecordDescriptor& recordDescriptor,
        const GraphFilter& filter,
        const ClassFilter& classFilter)
    {
        auto classInfo = txn._adapter->dbClass()->getInfo(recordDescriptor.rid.first);
        // filter classes
        if (!classFilter.onlyClasses.empty()) {
            if (classFilter.onlyClasses.find(classInfo.name) == classFilter.onlyClasses.cend()) {
                return Result {};
            }
        }
        // filter excluded classes
        if (!classFilter.ignoreClasses.empty()) {
            if (classFilter.ignoreClasses.find(classInfo.name) != classFilter.ignoreClasses.cend()) {
                return Result {};
            }
        }

        auto record = txn._interface->record()->getRecordWithBasicInfo(classInfo, recordDescriptor);
        if (filter._mode == GraphFilter::FilterMode::CONDITION) {
            auto condition = filter._condition.get();
            auto propertyNameMapInfo = txn._interface->schema()->getPropertyNameMapInfo(classInfo.id, classInfo.superClassId);
            auto cmpResult = RecordCompare::compareRecordByCondition(record, propertyNameMapInfo, *condition);
            return cmpResult ? Result { recordDescriptor, record } : Result {};
        } else if (filter._mode == GraphFilter::FilterMode::MULTI_CONDITION) {
            auto multiCondition = filter._multiCondition.get();
            auto propertyNameMapInfo = txn._interface->schema()->getPropertyNameMapInfo(classInfo.id, classInfo.superClassId);
            auto cmpResult = RecordCompare::compareRecordByMultiCondition(
                record, propertyNameMapInfo, *multiCondition);
            return cmpResult ? Result { recordDescriptor, record } : Result {};
        } else {
            if (filter._function != nullptr) {
                return (*filter._function)(record) ? Result { recordDescriptor, record } : Result {};
            }
        }
        return Result { recordDescriptor, record };
    }

    std::vector<std::pair<RecordDescriptor, RecordDescriptor>>
    RecordCompare::filterIncidentEdges(const Transaction& txn,
        const RecordId& vertex,
        const Direction& direction,
        const GraphFilter& filter,
        const ClassFilter& classFilter)
    {
        auto edgeRecordDescriptors = std::vector<std::pair<RecordDescriptor, RecordDescriptor>> {};
        auto edgeNeighbours = std::vector<std::pair<RecordId, RecordId>> {};
        switch (direction) {
        case Direction::IN:
            edgeNeighbours = txn._interface->graph()->getInEdgeAndNeighbours(vertex);
            break;
        case Direction::OUT:
            edgeNeighbours = txn._interface->graph()->getOutEdgeAndNeighbours(vertex);
            break;
        case Direction::ALL:
            edgeNeighbours = txn._interface->graph()->getInEdgeAndNeighbours(vertex);
            auto moreEdges = txn._interface->graph()->getOutEdgeAndNeighbours(vertex);
            edgeNeighbours.insert(edgeNeighbours.cend(), moreEdges.cbegin(), moreEdges.cend());
            break;
        }

        for (const auto& edgeNeighbour : edgeNeighbours) {
            auto edgeRdesc = RecordDescriptor { edgeNeighbour.first };
            auto neighbourRdesc = RecordDescriptor { edgeNeighbour.second };
            if (filterRecord(txn, edgeRdesc, filter, classFilter) != RecordDescriptor {}) {
                edgeRecordDescriptors.emplace_back(std::make_pair(edgeRdesc, neighbourRdesc));
            }
        }

        return edgeRecordDescriptors;
    }

    std::vector<RecordId>
    RecordCompare::resolveEdgeRecordIds(const Transaction& txn, const RecordId& recordId, const Direction& direction)
    {
        auto edgeRecordIds = std::vector<RecordId> {};
        switch (direction) {
        case Direction::IN:
            edgeRecordIds = txn._interface->graph()->getInEdges(recordId);
            break;
        case Direction::OUT:
            edgeRecordIds = txn._interface->graph()->getOutEdges(recordId);
            break;
        default:
            edgeRecordIds = txn._interface->graph()->getInEdges(recordId);
            auto outEdges = txn._interface->graph()->getOutEdges(recordId);
            edgeRecordIds.insert(edgeRecordIds.cend(), outEdges.cbegin(), outEdges.cend());
            break;
        }
        return edgeRecordIds;
    }

    ResultSet RecordCompare::compareCondition(const Transaction& txn,
        const ClassAccessInfo& classInfo,
        const PropertyNameMapInfo& propertyNameMapInfo,
        const Condition& condition,
        bool searchIndexOnly)
    {
        auto foundProperty = propertyNameMapInfo.find(condition.propName);
        if (foundProperty == propertyNameMapInfo.cend()) {
            return ResultSet {};
        }
        auto propertyInfo = foundProperty->second;
        auto foundIndex = txn._interface->index()->hasIndex(classInfo, propertyInfo, condition);
        if (foundIndex.first) {
            auto indexedRecords = txn._interface->index()->getRecord(propertyInfo, foundIndex.second, condition);
            return txn._interface->record()->getResultSet(classInfo, indexedRecords);
        } else {
            if (!searchIndexOnly) {
                return txn._interface->record()->getResultSetByCondition(classInfo, propertyInfo.type, condition);
            }
        }
        return ResultSet {};
    }

    ResultSet RecordCompare::compareMultiCondition(const Transaction& txn,
        const ClassAccessInfo& classInfo,
        const PropertyNameMapInfo& propertyNameMapInfo,
        const MultiCondition& multiCondition,
        bool searchIndexOnly)
    {
        auto conditionProperties = PropertyNameMapInfo {};
        for (const auto& conditionNode : multiCondition.conditions) {
            auto conditionNodePtr = conditionNode.lock();
            require(conditionNodePtr != nullptr);
            auto& condition = conditionNodePtr->getCondition();
            auto foundConditionProperty = conditionProperties.find(condition.propName);
            if (foundConditionProperty == conditionProperties.cend()) {
                auto foundProperty = propertyNameMapInfo.find(condition.propName);
                if (foundProperty != propertyNameMapInfo.cend()) {
                    conditionProperties.emplace(condition.propName, foundProperty->second);
                }
            }
        }

        auto foundIndex = txn._interface->index()->hasIndex(classInfo, conditionProperties, multiCondition);
        if (foundIndex.first) {
            auto indexedRecords = txn._interface->index()->getRecord(conditionProperties, foundIndex.second, multiCondition);
            return txn._interface->record()->getResultSet(classInfo, indexedRecords);
        } else {
            if (!searchIndexOnly) {
                return txn._interface->record()->getResultSetByMultiCondition(classInfo, conditionProperties, multiCondition);
            }
        }
        return ResultSet {};
    }

    std::vector<RecordDescriptor>
    RecordCompare::compareConditionRdesc(const Transaction& txn,
        const ClassAccessInfo& classInfo,
        const PropertyNameMapInfo& propertyNameMapInfo,
        const Condition& condition,
        bool searchIndexOnly)
    {
        auto foundProperty = propertyNameMapInfo.find(condition.propName);
        if (foundProperty == propertyNameMapInfo.cend()) {
            return std::vector<RecordDescriptor> {};
        }
        auto propertyInfo = foundProperty->second;
        auto foundIndex = txn._interface->index()->hasIndex(classInfo, propertyInfo, condition);
        if (foundIndex.first) {
            return txn._interface->index()->getRecord(propertyInfo, foundIndex.second, condition);
        } else {
            if (!searchIndexOnly) {
                return txn._interface->record()->getRecordDescriptorByCondition(classInfo, propertyInfo.type, condition);
            }
        }
        return std::vector<RecordDescriptor> {};
    }

    std::vector<RecordDescriptor>
    RecordCompare::compareMultiConditionRdesc(const Transaction& txn,
        const ClassAccessInfo& classInfo,
        const PropertyNameMapInfo& propertyNameMapInfo,
        const MultiCondition& conditions,
        bool searchIndexOnly)
    {
        auto conditionProperties = PropertyNameMapInfo {};
        for (const auto& conditionNode : conditions.conditions) {
            auto conditionNodePtr = conditionNode.lock();
            require(conditionNodePtr != nullptr);
            auto& condition = conditionNodePtr->getCondition();
            auto foundConditionProperty = conditionProperties.find(condition.propName);
            if (foundConditionProperty == conditionProperties.cend()) {
                auto foundProperty = propertyNameMapInfo.find(condition.propName);
                if (foundProperty != propertyNameMapInfo.cend()) {
                    conditionProperties.emplace(condition.propName, foundProperty->second);
                }
            }
        }

        auto foundIndex = txn._interface->index()->hasIndex(classInfo, conditionProperties, conditions);
        if (foundIndex.first) {
            return txn._interface->index()->getRecord(conditionProperties, foundIndex.second, conditions);
        } else {
            if (!searchIndexOnly) {
                return txn._interface->record()->getRecordDescriptorByMultiCondition(
                    classInfo, conditionProperties, conditions);
            }
        }
        return std::vector<RecordDescriptor> {};
    }

    unsigned int RecordCompare::compareConditionCount(const Transaction& txn,
        const ClassAccessInfo& classInfo,
        const PropertyNameMapInfo& propertyNameMapInfo,
        const Condition& condition,
        bool searchIndexOnly)
    {
        auto foundProperty = propertyNameMapInfo.find(condition.propName);
        if (foundProperty == propertyNameMapInfo.cend()) {
            return 0;
        }
        auto propertyInfo = foundProperty->second;
        auto foundIndex = txn._interface->index()->hasIndex(classInfo, propertyInfo, condition);
        if (foundIndex.first) {
            return txn._interface->index()->getCountRecord(propertyInfo, foundIndex.second, condition);
        } else {
            if (!searchIndexOnly) {
                return txn._interface->record()->getCountRecordByCondition(classInfo, propertyInfo.type, condition);
            }
        }
        return 0;
    }

    unsigned int RecordCompare::compareMultiConditionCount(const Transaction& txn,
        const ClassAccessInfo& classInfo,
        const PropertyNameMapInfo& propertyNameMapInfo,
        const MultiCondition& conditions,
        bool searchIndexOnly)
    {
        auto conditionProperties = PropertyNameMapInfo {};
        for (const auto& conditionNode : conditions.conditions) {
            auto conditionNodePtr = conditionNode.lock();
            require(conditionNodePtr != nullptr);
            auto& condition = conditionNodePtr->getCondition();
            auto foundConditionProperty = conditionProperties.find(condition.propName);
            if (foundConditionProperty == conditionProperties.cend()) {
                auto foundProperty = propertyNameMapInfo.find(condition.propName);
                if (foundProperty != propertyNameMapInfo.cend()) {
                    conditionProperties.emplace(condition.propName, foundProperty->second);
                }
            }
        }

        auto foundIndex = txn._interface->index()->hasIndex(classInfo, conditionProperties, conditions);
        if (foundIndex.first) {
            return txn._interface->index()->getCountRecord(conditionProperties, foundIndex.second, conditions);
        } else {
            if (!searchIndexOnly) {
                return txn._interface->record()->getCountRecordByMultiCondition(
                    classInfo, conditionProperties, conditions);
            }
        }
        return 0;
    }

    ResultSet RecordCompare::compareEdgeCondition(const Transaction& txn,
        const RecordDescriptor& recordDescriptor,
        const Direction& direction,
        const Condition& condition)
    {
        auto edgeRecordIds = resolveEdgeRecordIds(txn, recordDescriptor.rid, direction);
        auto resultSet = ResultSet {};
        auto edgeInfos = std::map<ClassId, std::pair<ClassAccessInfo, PropertyType>> {};
        for (const auto& edgeRecordId : edgeRecordIds) {
            auto edgeClassInfo = ClassAccessInfo {};
            auto propertyType = PropertyType::UNDEFINED;

            auto foundEdgeInfo = edgeInfos.find(edgeRecordId.first);
            if (foundEdgeInfo == edgeInfos.cend()) {
                edgeClassInfo = txn._adapter->dbClass()->getInfo(edgeRecordId.first);
                auto propertyNameMapInfo = txn._interface->schema()->getPropertyNameMapInfo(
                    edgeClassInfo.id, edgeClassInfo.superClassId);
                auto foundProperty = propertyNameMapInfo.find(condition.propName);
                if (foundProperty == propertyNameMapInfo.cend()) {
                    continue;
                }
                propertyType = foundProperty->second.type;
                edgeInfos.emplace(std::make_pair(edgeRecordId.first, std::make_pair(edgeClassInfo, propertyType)));
            } else {
                edgeClassInfo = foundEdgeInfo->second.first;
                propertyType = foundEdgeInfo->second.second;
            }

            auto edgeRecord = txn._interface->record()->getRecordWithBasicInfo(edgeClassInfo, RecordDescriptor { edgeRecordId });
            if (compareRecordByCondition(edgeRecord, propertyType, condition)) {
                resultSet.emplace_back(Result { RecordDescriptor { edgeRecordId }, edgeRecord });
            }
        }
        return resultSet;
    }

    ResultSet RecordCompare::compareEdgeCondition(const Transaction& txn,
        const RecordDescriptor& recordDescriptor,
        const Direction& direction,
        bool (*condition)(const Record&))
    {
        auto edgeRecordIds = resolveEdgeRecordIds(txn, recordDescriptor.rid, direction);
        auto resultSet = ResultSet {};
        auto edgeInfos = std::map<ClassId, ClassAccessInfo> {};
        for (const auto& edgeRecordId : edgeRecordIds) {
            auto edgeClassInfo = ClassAccessInfo {};

            auto foundEdgeInfo = edgeInfos.find(edgeRecordId.first);
            if (foundEdgeInfo == edgeInfos.cend()) {
                edgeClassInfo = txn._adapter->dbClass()->getInfo(edgeRecordId.first);
                edgeInfos.emplace(std::make_pair(edgeRecordId.first, edgeClassInfo));
            } else {
                edgeClassInfo = foundEdgeInfo->second;
            }

            auto edgeRecord = txn._interface->record()->getRecordWithBasicInfo(edgeClassInfo, RecordDescriptor { edgeRecordId });
            if (condition(edgeRecord)) {
                resultSet.emplace_back(Result { RecordDescriptor { edgeRecordId }, edgeRecord });
            }
        }
        return resultSet;
    }

    ResultSet RecordCompare::compareEdgeMultiCondition(const Transaction& txn,
        const RecordDescriptor& recordDescriptor,
        const Direction& direction,
        const MultiCondition& multiCondition)
    {
        auto edgeRecordIds = resolveEdgeRecordIds(txn, recordDescriptor.rid, direction);
        auto resultSet = ResultSet {};
        auto edgeInfos = std::map<ClassId, std::pair<ClassAccessInfo, PropertyMapType>> {};
        for (const auto& edgeRecordId : edgeRecordIds) {
            auto edgeClassInfo = ClassAccessInfo {};
            auto propertyTypes = PropertyMapType {};

            auto foundEdgeInfo = edgeInfos.find(edgeRecordId.first);
            if (foundEdgeInfo == edgeInfos.cend()) {
                edgeClassInfo = txn._adapter->dbClass()->getInfo(edgeRecordId.first);
                auto propertyNameMapInfo = txn._interface->schema()->getPropertyNameMapInfo(edgeClassInfo.id, edgeClassInfo.superClassId);
                for (const auto& property : propertyNameMapInfo) {
                    propertyTypes.emplace(property.first, property.second.type);
                }
                edgeInfos.emplace(std::make_pair(edgeRecordId.first, std::make_pair(edgeClassInfo, propertyTypes)));
            } else {
                edgeClassInfo = foundEdgeInfo->second.first;
                propertyTypes = foundEdgeInfo->second.second;
            }

            auto edgeRecord = txn._interface->record()->getRecordWithBasicInfo(edgeClassInfo, RecordDescriptor { edgeRecordId });
            if (multiCondition.execute(edgeRecord, propertyTypes)) {
                resultSet.emplace_back(Result { RecordDescriptor { edgeRecordId }, edgeRecord });
            }
        }
        return resultSet;
    }

    std::vector<RecordDescriptor>
    RecordCompare::compareEdgeConditionRdesc(const Transaction& txn,
        const RecordDescriptor& recordDescriptor,
        const Direction& direction,
        const Condition& condition)
    {
        auto edgeRecordIds = resolveEdgeRecordIds(txn, recordDescriptor.rid, direction);
        auto recordDescriptors = std::vector<RecordDescriptor> {};
        auto edgeInfos = std::map<ClassId, std::pair<ClassAccessInfo, PropertyType>> {};
        for (const auto& edgeRecordId : edgeRecordIds) {
            auto edgeClassInfo = ClassAccessInfo {};
            auto propertyType = PropertyType::UNDEFINED;

            auto foundEdgeInfo = edgeInfos.find(edgeRecordId.first);
            if (foundEdgeInfo == edgeInfos.cend()) {
                edgeClassInfo = txn._adapter->dbClass()->getInfo(edgeRecordId.first);
                auto propertyNameMapInfo = txn._interface->schema()->getPropertyNameMapInfo(edgeClassInfo.id, edgeClassInfo.superClassId);
                auto foundProperty = propertyNameMapInfo.find(condition.propName);
                if (foundProperty == propertyNameMapInfo.cend()) {
                    continue;
                }
                propertyType = foundProperty->second.type;
                edgeInfos.emplace(std::make_pair(edgeRecordId.first, std::make_pair(edgeClassInfo, propertyType)));
            } else {
                edgeClassInfo = foundEdgeInfo->second.first;
                propertyType = foundEdgeInfo->second.second;
            }

            auto edgeRecord = txn._interface->record()->getRecordWithBasicInfo(edgeClassInfo, RecordDescriptor { edgeRecordId });
            if (compareRecordByCondition(edgeRecord, propertyType, condition)) {
                recordDescriptors.emplace_back(RecordDescriptor { edgeRecordId });
            }
        }
        return recordDescriptors;
    }

    std::vector<RecordDescriptor>
    RecordCompare::compareEdgeConditionRdesc(const Transaction& txn,
        const RecordDescriptor& recordDescriptor,
        const Direction& direction,
        bool (*condition)(const Record&))
    {
        auto edgeRecordIds = resolveEdgeRecordIds(txn, recordDescriptor.rid, direction);
        auto recordDescriptors = std::vector<RecordDescriptor> {};
        auto edgeInfos = std::map<ClassId, ClassAccessInfo> {};
        for (const auto& edgeRecordId : edgeRecordIds) {
            auto edgeClassInfo = ClassAccessInfo {};

            auto foundEdgeInfo = edgeInfos.find(edgeRecordId.first);
            if (foundEdgeInfo == edgeInfos.cend()) {
                edgeClassInfo = txn._adapter->dbClass()->getInfo(edgeRecordId.first);
                edgeInfos.emplace(std::make_pair(edgeRecordId.first, edgeClassInfo));
            } else {
                edgeClassInfo = foundEdgeInfo->second;
            }

            auto edgeRecord = txn._interface->record()->getRecordWithBasicInfo(edgeClassInfo, RecordDescriptor { edgeRecordId });
            if (condition(edgeRecord)) {
                recordDescriptors.emplace_back(RecordDescriptor { edgeRecordId });
            }
        }
        return recordDescriptors;
    }

    std::vector<RecordDescriptor>
    RecordCompare::compareEdgeMultiConditionRdesc(const Transaction& txn,
        const RecordDescriptor& recordDescriptor,
        const Direction& direction,
        const MultiCondition& multiCondition)
    {
        auto edgeRecordIds = resolveEdgeRecordIds(txn, recordDescriptor.rid, direction);
        auto recordDescriptors = std::vector<RecordDescriptor> {};
        auto edgeInfos = std::map<ClassId, std::pair<ClassAccessInfo, PropertyMapType>> {};
        for (const auto& edgeRecordId : edgeRecordIds) {
            auto edgeClassInfo = ClassAccessInfo {};
            auto propertyTypes = PropertyMapType {};

            auto foundEdgeInfo = edgeInfos.find(edgeRecordId.first);
            if (foundEdgeInfo == edgeInfos.cend()) {
                edgeClassInfo = txn._adapter->dbClass()->getInfo(edgeRecordId.first);
                auto propertyNameMapInfo = txn._interface->schema()->getPropertyNameMapInfo(edgeClassInfo.id, edgeClassInfo.superClassId);
                for (const auto& property : propertyNameMapInfo) {
                    propertyTypes.emplace(property.first, property.second.type);
                }
                edgeInfos.emplace(std::make_pair(edgeRecordId.first, std::make_pair(edgeClassInfo, propertyTypes)));
            } else {
                edgeClassInfo = foundEdgeInfo->second.first;
                propertyTypes = foundEdgeInfo->second.second;
            }

            auto edgeRecord = txn._interface->record()->getRecordWithBasicInfo(edgeClassInfo, RecordDescriptor { edgeRecordId });
            if (multiCondition.execute(edgeRecord, propertyTypes)) {
                recordDescriptors.emplace_back(RecordDescriptor { edgeRecordId });
            }
        }
        return recordDescriptors;
    }

}
}
