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

#ifndef __INDEX_HPP_INCLUDED_
#define __INDEX_HPP_INCLUDED_

#include <vector>
#include <tuple>

#include "schema.hpp"
#include "datastore.hpp"
#include "base_txn.hpp"

#include "nogdb_types.h"
#include "nogdb_txn.h"
#include "nogdb_compare.h"

namespace nogdb {
    struct Index {
        Index() = delete;

        ~Index() noexcept = delete;

        typedef std::tuple<IndexId, bool, PropertyType> IndexPropertyType;

        static void addIndex(BaseTxn &txn, IndexId indexId, PositionId positionId, const Bytes &bytesValue,
                             PropertyType type, bool isUnique);

        static void deleteIndex(BaseTxn &txn, IndexId indexId, PositionId positionId, const Bytes &bytesValue,
                                PropertyType type, bool isUnique);

        template<typename T>
        static void deleteIndexCursor(Datastore::CursorHandler *cursorHandler, PositionId positionId, T value) {
            for (auto keyValue = Datastore::getSetKeyCursor(cursorHandler, value);
                 !keyValue.empty();
                 keyValue = Datastore::getNextCursor(cursorHandler)) {
                auto key = Datastore::getKeyAsNumeric<T>(keyValue);
                if (value == *key) {
                    auto valueAsPositionId = Datastore::getValueAsNumeric<PositionId>(keyValue);
                    if (positionId == *valueAsPositionId) {
                        Datastore::deleteCursor(cursorHandler);
                        break;
                    }
                } else {
                    break;
                }
            }
        }

        inline static void
        deleteIndexCursor(Datastore::CursorHandler *cursorHandler, PositionId positionId, const std::string &value) {
            for (auto keyValue = Datastore::getSetKeyCursor(cursorHandler, value);
                 !keyValue.empty();
                 keyValue = Datastore::getNextCursor(cursorHandler)) {
                auto key = Datastore::getKeyAsString(keyValue);
                if (value == key) {
                    auto valueAsPositionId = Datastore::getValueAsNumeric<PositionId>(keyValue);
                    if (positionId == *valueAsPositionId) {
                        Datastore::deleteCursor(cursorHandler);
                        break;
                    }
                } else {
                    break;
                }
            }
        }

        static std::pair<IndexPropertyType, bool>
        hasIndex(ClassId classId, const ClassInfo &classInfo, const Condition &condition);

        static std::pair<std::map<std::string, IndexPropertyType>, bool>
        hasIndex(ClassId classId, const ClassInfo &classInfo, const MultiCondition &conditions);

        static std::vector<RecordDescriptor> getIndexRecord(const Txn &txn,
                                                            ClassId classId,
                                                            IndexPropertyType indexPropertyType,
                                                            const Condition &condition,
                                                            bool isNegative = false);

        static std::vector<RecordDescriptor> getIndexRecord(const Txn &txn, ClassId classId,
                                                            const std::map<std::string, IndexPropertyType> &indexPropertyTypes,
                                                            const MultiCondition &conditions);

        static std::vector<RecordDescriptor>
        getLessEqual(const Txn &txn, ClassId classId, const IndexPropertyType &indexPropertyType, const Bytes &value);

        static std::vector<RecordDescriptor>
        getLess(const Txn &txn, ClassId classId, const IndexPropertyType &indexPropertyType, const Bytes &value);

        static std::vector<RecordDescriptor>
        getEqual(const Txn &txn, ClassId classId, const IndexPropertyType &indexPropertyType, const Bytes &value);

        static std::vector<RecordDescriptor>
        getGreaterEqual(const Txn &txn, ClassId classId, const IndexPropertyType &indexPropertyType,
                        const Bytes &value);

        static std::vector<RecordDescriptor>
        getGreater(const Txn &txn, ClassId classId, const IndexPropertyType &indexPropertyType, const Bytes &value);

        static std::vector<RecordDescriptor> getBetween(const Txn &txn,
                                                        ClassId classId,
                                                        const IndexPropertyType &indexPropertyType,
                                                        const Bytes &lowerBound,
                                                        const Bytes &upperBound,
                                                        const std::pair<bool, bool> &isIncludeBound);

        template<typename T>
        static std::vector<RecordDescriptor>
        getLess(const Txn &txn, ClassId classId, IndexId indexId, bool isUnique, T value, bool includeEqual = false) {
            auto dsTxnHandler = txn.txnBase->getDsTxnHandler();
            if (value < 0) {
                auto dataIndexDBHandlerNegative =
                        Datastore::openDbi(dsTxnHandler,
                                           TB_INDEXING_PREFIX + std::to_string(indexId) + "_negative",
                                           true, isUnique);
                auto cursorHandlerNegative = Datastore::openCursor(dsTxnHandler, dataIndexDBHandlerNegative);
                return backwardSearchIndex(cursorHandlerNegative, classId, value, includeEqual);
            } else {
                auto dataIndexDBHandlerPositive =
                        Datastore::openDbi(dsTxnHandler,
                                           TB_INDEXING_PREFIX + std::to_string(indexId) + "_positive",
                                           true, isUnique);
                auto dataIndexDBHandlerNegative =
                        Datastore::openDbi(dsTxnHandler,
                                           TB_INDEXING_PREFIX + std::to_string(indexId) + "_negative",
                                           true, isUnique);
                auto cursorHandlerPositive = Datastore::openCursor(dsTxnHandler, dataIndexDBHandlerPositive);
                auto cursorHandlerNegative = Datastore::openCursor(dsTxnHandler, dataIndexDBHandlerNegative);
                auto positiveResult = backwardSearchIndex(cursorHandlerPositive, classId, value, includeEqual);
                auto negativeResult = backwardSearchIndex(cursorHandlerNegative, classId, value, includeEqual);
                positiveResult.insert(positiveResult.end(), negativeResult.cbegin(), negativeResult.cend());
                return positiveResult;
            }
        };

        template<typename T>
        static std::vector<RecordDescriptor>
        getEqual(const Txn &txn, ClassId classId, IndexId indexId, bool isUnique, T value) {
            auto dsTxnHandler = txn.txnBase->getDsTxnHandler();
            if (value < 0) {
                auto dataIndexDBHandlerNegative =
                        Datastore::openDbi(dsTxnHandler,
                                           TB_INDEXING_PREFIX + std::to_string(indexId) + "_negative",
                                           true, isUnique);
                auto cursorHandlerNegative = Datastore::openCursor(dsTxnHandler, dataIndexDBHandlerNegative);
                return exactMatchIndex(cursorHandlerNegative, classId, value);
            } else {
                auto dataIndexDBHandlerPositive =
                        Datastore::openDbi(dsTxnHandler,
                                           TB_INDEXING_PREFIX + std::to_string(indexId) + "_positive",
                                           true, isUnique);
                auto cursorHandlerPositive = Datastore::openCursor(dsTxnHandler, dataIndexDBHandlerPositive);
                return exactMatchIndex(cursorHandlerPositive, classId, value);
            }
        };

        template<typename T>
        static std::vector<RecordDescriptor>
        getGreater(const Txn &txn, ClassId classId, IndexId indexId, bool isUnique, T value,
                   bool includeEqual = false) {
            auto dsTxnHandler = txn.txnBase->getDsTxnHandler();
            if (value < 0) {
                auto dataIndexDBHandlerPositive =
                        Datastore::openDbi(dsTxnHandler,
                                           TB_INDEXING_PREFIX + std::to_string(indexId) + "_positive",
                                           true, isUnique);
                auto dataIndexDBHandlerNegative =
                        Datastore::openDbi(dsTxnHandler,
                                           TB_INDEXING_PREFIX + std::to_string(indexId) + "_negative",
                                           true, isUnique);
                auto cursorHandlerPositive = Datastore::openCursor(dsTxnHandler, dataIndexDBHandlerPositive);
                auto cursorHandlerNegative = Datastore::openCursor(dsTxnHandler, dataIndexDBHandlerNegative);
                auto positiveResult = forwardSearchIndex(cursorHandlerPositive, classId, value, includeEqual);
                auto negativeResult = forwardSearchIndex(cursorHandlerNegative, classId, value, includeEqual);
                positiveResult.insert(positiveResult.end(), negativeResult.cbegin(), negativeResult.cend());
                return positiveResult;
            } else {
                auto dataIndexDBHandlerPositive =
                        Datastore::openDbi(dsTxnHandler,
                                           TB_INDEXING_PREFIX + std::to_string(indexId) + "_positive",
                                           true, isUnique);
                auto cursorHandlerPositive = Datastore::openCursor(dsTxnHandler, dataIndexDBHandlerPositive);
                return forwardSearchIndex(cursorHandlerPositive, classId, value, includeEqual);
            }
        };

        template<typename T>
        static std::vector<RecordDescriptor> getBetween(const Txn &txn,
                                                        ClassId classId,
                                                        IndexId indexId,
                                                        bool isUnique,
                                                        T lowerBound,
                                                        T upperBound,
                                                        const std::pair<bool, bool> &isIncludeBound) {
            auto dsTxnHandler = txn.txnBase->getDsTxnHandler();
            if (lowerBound < 0 && upperBound < 0) {
                auto dataIndexDBHandlerNegative =
                        Datastore::openDbi(dsTxnHandler,
                                           TB_INDEXING_PREFIX + std::to_string(indexId) + "_negative",
                                           true, isUnique);
                auto cursorHandlerNegative = Datastore::openCursor(dsTxnHandler, dataIndexDBHandlerNegative);
                return betweenSearchIndex(cursorHandlerNegative, classId, lowerBound, upperBound, isIncludeBound);
            } else if (lowerBound < 0 && upperBound >= 0) {
                auto dataIndexDBHandlerPositive =
                        Datastore::openDbi(dsTxnHandler,
                                           TB_INDEXING_PREFIX + std::to_string(indexId) + "_positive",
                                           true, isUnique);
                auto dataIndexDBHandlerNegative =
                        Datastore::openDbi(dsTxnHandler,
                                           TB_INDEXING_PREFIX + std::to_string(indexId) + "_negative",
                                           true, isUnique);
                auto cursorHandlerPositive = Datastore::openCursor(dsTxnHandler, dataIndexDBHandlerPositive);
                auto cursorHandlerNegative = Datastore::openCursor(dsTxnHandler, dataIndexDBHandlerNegative);
                auto positiveResult = betweenSearchIndex(cursorHandlerPositive, classId, lowerBound, upperBound,
                                                         isIncludeBound);
                auto negativeResult = betweenSearchIndex(cursorHandlerNegative, classId, lowerBound, upperBound,
                                                         isIncludeBound);
                positiveResult.insert(positiveResult.end(), negativeResult.cbegin(), negativeResult.cend());
                return positiveResult;
            } else {
                auto dataIndexDBHandlerPositive =
                        Datastore::openDbi(dsTxnHandler,
                                           TB_INDEXING_PREFIX + std::to_string(indexId) + "_positive",
                                           true, isUnique);
                auto cursorHandlerPositive = Datastore::openCursor(dsTxnHandler, dataIndexDBHandlerPositive);
                return betweenSearchIndex(cursorHandlerPositive, classId, lowerBound, upperBound, isIncludeBound);
            }
        };

        template<typename T>
        static std::vector<RecordDescriptor>
        exactMatchIndex(Datastore::CursorHandler *cursorHandler, ClassId classId, const T &value) {
            auto result = std::vector<RecordDescriptor>{};
            for (auto keyValue = Datastore::getSetKeyCursor(cursorHandler, value);
                 !keyValue.empty();
                 keyValue = Datastore::getNextCursor(cursorHandler)) {
                auto key = Datastore::getKeyAsNumeric<T>(keyValue);
                if (*key == value) {
                    auto positionId = Datastore::getValueAsNumeric<PositionId>(keyValue);
                    result.emplace_back(RecordDescriptor{classId, *positionId});
                } else {
                    break;
                }
            }
            return result;
        };

        inline static std::vector<RecordDescriptor>
        exactMatchIndex(Datastore::CursorHandler *cursorHandler, ClassId classId, const std::string &value) {
            auto result = std::vector<RecordDescriptor>{};
            for (auto keyValue = Datastore::getSetKeyCursor(cursorHandler, value);
                 !keyValue.empty();
                 keyValue = Datastore::getNextCursor(cursorHandler)) {
                auto key = Datastore::getKeyAsString(keyValue);
                if (key == value) {
                    auto positionId = Datastore::getValueAsNumeric<PositionId>(keyValue);
                    result.emplace_back(RecordDescriptor{classId, *positionId});
                } else {
                    break;
                }
            }
            return result;
        };

        template<typename T>
        static std::vector<RecordDescriptor> backwardSearchIndex(Datastore::CursorHandler *cursorHandler,
                                                                 ClassId classId, const T &value,
                                                                 bool isInclude = true) {
            auto result = std::vector<RecordDescriptor>{};
            if (isInclude) {
                auto partialResult = exactMatchIndex(cursorHandler, classId, value);
                result.insert(result.end(), partialResult.cbegin(), partialResult.cend());
            }
            auto keyValue = Datastore::getSetRangeCursor(cursorHandler, value);
            if (!keyValue.empty()) {
                for (auto keyValue = Datastore::getPrevCursor(cursorHandler);
                     !keyValue.empty();
                     keyValue = Datastore::getPrevCursor(cursorHandler)) {
                    auto positionId = Datastore::getValueAsNumeric<PositionId>(keyValue);
                    result.emplace_back(RecordDescriptor{classId, *positionId});
                }
            }
            return result;
        };

        template<typename T>
        static std::vector<RecordDescriptor> forwardSearchIndex(Datastore::CursorHandler *cursorHandler,
                                                                ClassId classId, const T &value,
                                                                bool isInclude = true) {
            auto result = std::vector<RecordDescriptor>{};
            for (auto keyValue = Datastore::getSetRangeCursor(cursorHandler, value);
                 !keyValue.empty();
                 keyValue = Datastore::getNextCursor(cursorHandler)) {
                if (!isInclude) {
                    auto key = Datastore::getKeyAsNumeric<T>(keyValue);
                    if (*key == value) continue;
                    else isInclude = true;
                }
                auto positionId = Datastore::getValueAsNumeric<PositionId>(keyValue);
                result.emplace_back(RecordDescriptor{classId, *positionId});
            }
            return result;
        };

        inline static std::vector<RecordDescriptor> forwardSearchIndex(Datastore::CursorHandler *cursorHandler,
                                                                       ClassId classId, const std::string &value,
                                                                       bool isInclude = true) {
            auto result = std::vector<RecordDescriptor>{};
            for (auto keyValue = Datastore::getSetRangeCursor(cursorHandler, value);
                 !keyValue.empty();
                 keyValue = Datastore::getNextCursor(cursorHandler)) {
                if (!isInclude) {
                    auto key = Datastore::getKeyAsString(keyValue);
                    if (key == value) continue;
                    else isInclude = true;
                }
                auto positionId = Datastore::getValueAsNumeric<PositionId>(keyValue);
                result.emplace_back(RecordDescriptor{classId, *positionId});
            }
            return result;
        };

        template<typename T>
        static std::vector<RecordDescriptor> betweenSearchIndex(Datastore::CursorHandler *cursorHandler,
                                                                ClassId classId,
                                                                const T &lower,
                                                                const T &upper,
                                                                const std::pair<bool, bool> &isIncludeBound) {
            auto result = std::vector<RecordDescriptor>{};
            for (auto keyValue = Datastore::getSetRangeCursor(cursorHandler, lower);
                 !keyValue.empty();
                 keyValue = Datastore::getNextCursor(cursorHandler)) {
                auto key = Datastore::getKeyAsNumeric<T>(keyValue);
                if (!isIncludeBound.first && (*key == lower)) continue;
                if ((!isIncludeBound.second && (*key == upper)) || (*key > upper)) break;
                auto positionId = Datastore::getValueAsNumeric<PositionId>(keyValue);
                result.emplace_back(RecordDescriptor{classId, *positionId});
            }
            return result;
        };

        inline static std::vector<RecordDescriptor> betweenSearchIndex(Datastore::CursorHandler *cursorHandler,
                                                                       ClassId classId,
                                                                       const std::string &lower,
                                                                       const std::string &upper,
                                                                       const std::pair<bool, bool> &isIncludeBound) {
            auto result = std::vector<RecordDescriptor>{};
            for (auto keyValue = Datastore::getSetRangeCursor(cursorHandler, lower);
                 !keyValue.empty();
                 keyValue = Datastore::getNextCursor(cursorHandler)) {
                auto key = Datastore::getKeyAsString(keyValue);
                if (!isIncludeBound.first && (key == lower)) continue;
                if ((!isIncludeBound.second && (key == upper)) || (key > upper)) break;
                auto positionId = Datastore::getValueAsNumeric<PositionId>(keyValue);
                result.emplace_back(RecordDescriptor{classId, *positionId});
            }
            return result;
        };

        static const std::vector<Condition::Comparator> validComparators;

    };

}

#endif
