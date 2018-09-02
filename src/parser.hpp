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

#ifndef __PARSER_HPP_INCLUDED_
#define __PARSER_HPP_INCLUDED_

#include <map>
#include <vector>
#include <utility>

#include "datatype.hpp"
#include "lmdb_engine.hpp"
#include "schema.hpp"

#include "nogdb_types.h"
#include "schema_adapter.hpp"

namespace nogdb {

    namespace parser {

        constexpr size_t UINT8_BITS_COUNT = 8 * sizeof(uint8_t);
        constexpr size_t UINT16_BITS_COUNT = 8 * sizeof(uint16_t);
        constexpr size_t UINT32_BITS_COUNT = 8 * sizeof(uint32_t);

        const std::string EMPTY_STRING = std::string{"\n"};
        constexpr size_t SIZE_OF_EMPTY_STRING = strlen(EMPTY_STRING.c_str());

        constexpr size_t VERTEX_SRC_DST_RAW_DATA_LENGTH = 2 * (sizeof(ClassId) + sizeof(PositionId));

        class Parser {
        public:
            Parser() = delete;

            ~Parser() noexcept = delete;

            //-------------------------
            // Common parsers
            //-------------------------
            static Blob parseRecord(const Record &record,
                                    const adapter::schema::PropertyNameMapInfo &properties) {
                auto dataSize = size_t{0};
                // calculate a raw data size of properties in a record
                for (const auto &property: record.getAll()) {
                    auto foundProperty = properties.find(property.first);
                    if (foundProperty == properties.cend()) {
                        throw NOGDB_CONTEXT_ERROR(NOGDB_CTX_NOEXST_PROPERTY);
                    }
                    dataSize += getRawDataSize(property.second.size());
                    //TODO: check if having any index?
                }
                // calculate a raw data from basic property info (only version and txn id)
                dataSize += getRawDataSize(2 * sizeof(TxnId));
                return parseRecord(record, dataSize, properties);
            }

            static Record parseRawData(const storage_engine::lmdb::Result &rawData,
                                       const adapter::schema::PropertyIdMapInfo &propertyInfos,
                                       bool isEdge = false) {
                if (rawData.empty) {
                    return Record{};
                }
                Record::PropertyToBytesMap properties{};
                auto rawDataBlob = rawData.data.blob();
                auto offset = size_t{0};
                if (isEdge) {
                    offset = VERTEX_SRC_DST_RAW_DATA_LENGTH;
                }
                if (rawDataBlob.capacity() == 0) {
                    throw NOGDB_CONTEXT_ERROR(NOGDB_CTX_UNKNOWN_ERR);
                } else if (rawDataBlob.capacity() >= 2 * sizeof(uint16_t)) {
                    //TODO: should be concerned about ENDIAN?
                    // NOTE: each property block consists of property id, flag, size, and value
                    // when option flag = 0
                    // +----------------------+--------------------+-----------------------+-----------+
                    // | propertyId (16bits)  | option flag (1bit) | propertySize (7bits)  |   value   | (next block) ...
                    // +----------------------+--------------------+-----------------------+-----------+
                    // when option flag = 1 (for extra large size of value)
                    // +----------------------+--------------------+------------------------+-----------+
                    // | propertyId (16bits)  | option flag (1bit) | propertySize (31bits)  |   value   | (next block) ...
                    // +----------------------+--------------------+------------------------+-----------+
                    while (offset < rawDataBlob.size()) {
                        auto propertyId = PropertyId{};
                        auto optionFlag = uint8_t{};
                        offset = rawDataBlob.retrieve(&propertyId, offset, sizeof(PropertyId));
                        rawDataBlob.retrieve(&optionFlag, offset, sizeof(optionFlag));
                        auto propertySize = size_t{};
                        if ((optionFlag & 0x1) == 1) {
                            //extra large size of value (exceed 127 bytes)
                            auto tmpSize =  uint32_t{};
                            offset = rawDataBlob.retrieve(&tmpSize, offset, sizeof(uint32_t));
                            propertySize = static_cast<size_t>(tmpSize >> 1);
                        } else {
                            //normal size of value (not exceed 127 bytes)
                            auto tmpSize = uint8_t{};
                            offset = rawDataBlob.retrieve(&tmpSize, offset, sizeof(uint8_t));
                            propertySize = static_cast<size_t>(tmpSize >> 1);
                        }
                        auto foundInfo = propertyInfos.find(propertyId);
                        if (foundInfo != propertyInfos.cend()) {
                            if (propertySize > 0) {
                                Blob::Byte byteData[propertySize];
                                offset = rawDataBlob.retrieve(byteData, offset, propertySize);
                                properties[foundInfo->second.name] = Bytes{byteData, propertySize};
                            } else {
                                properties[foundInfo->second.name] = Bytes{};
                            }
                        } else {
                            offset += propertySize;
                        }
                    }
                }
                return Record(properties);
            }

            static Record parseRawDataWithBasicInfo(const std::string &className,
                                                    const RecordId &rid,
                                                    const storage_engine::lmdb::Result &rawData,
                                                    const adapter::schema::PropertyIdMapInfo& propertyInfos,
                                                    const ClassType& classType) {
                return parseRawData(rawData, propertyInfos, classType == ClassType::EDGE)
                        .setBasicInfoIfNotExists(CLASS_NAME_PROPERTY, className)
                        .setBasicInfoIfNotExists(RECORD_ID_PROPERTY, rid2str(rid))
                        .setBasicInfoIfNotExists(DEPTH_PROPERTY, 0U);
            }

            //-------------------------
            // Edge only parsers
            //-------------------------
            static Blob parseEdgeVertexSrcDst(const RecordId &srcRid, const RecordId &dstRid) {
                auto value = Blob(VERTEX_SRC_DST_RAW_DATA_LENGTH);
                value.append(&srcRid.first, sizeof(ClassId));
                value.append(&srcRid.second, sizeof(PositionId));
                value.append(&dstRid.first, sizeof(ClassId));
                value.append(&dstRid.second, sizeof(PositionId));
                return value;
            }

            static std::pair<RecordId, RecordId> parseEdgeRawDataVertexSrcDst(const Blob &blob) {
                require(blob.size() >= VERTEX_SRC_DST_RAW_DATA_LENGTH);
                auto srcVertexRid = RecordId{};
                auto dstVertexRid = RecordId{};
                auto offset = size_t{0};
                offset = blob.retrieve(&srcVertexRid.first, offset, sizeof(ClassId));
                offset = blob.retrieve(&srcVertexRid.second, offset, sizeof(PositionId));
                offset = blob.retrieve(&dstVertexRid.first, offset, sizeof(ClassId));
                blob.retrieve(&dstVertexRid.first, offset, sizeof(PositionId));
                return std::make_pair(srcVertexRid, dstVertexRid);
            }

            static Blob parseEdgeRawDataVertexSrcDstAsBlob(const Blob &blob) {
                require(blob.size() >= VERTEX_SRC_DST_RAW_DATA_LENGTH);
                Blob::Byte byteData[VERTEX_SRC_DST_RAW_DATA_LENGTH];
                blob.retrieve(byteData, 0, VERTEX_SRC_DST_RAW_DATA_LENGTH);
                return Blob(byteData, VERTEX_SRC_DST_RAW_DATA_LENGTH);
            }

            static Blob parseEdgeRawDataAsBlob(const Blob& blob) {
                if (blob.size() > VERTEX_SRC_DST_RAW_DATA_LENGTH) {
                    auto offset = VERTEX_SRC_DST_RAW_DATA_LENGTH;
                    auto rawDataSize = blob.size() - offset;
                    Blob::Byte byteData[rawDataSize];
                    blob.retrieve(byteData, offset, sizeof(byteData));
                    return Blob(byteData, rawDataSize);
                } else {
                    return Blob();
                }
            }

        private:

            static void buildRawData(Blob& blob, const PropertyId& propertyId, const Bytes& rawData) {
                if (rawData.size() < std::pow(2, UINT8_BITS_COUNT - 1)) {
                    auto size = static_cast<uint8_t>(rawData.size()) << 1;
                    blob.append(&propertyId, sizeof(PropertyId));
                    blob.append(&size, sizeof(uint8_t));
                    blob.append(static_cast<void *>(rawData.getRaw()), rawData.size());
                } else {
                    auto size = (static_cast<uint32_t>(rawData.size()) << 1) + 0x1;
                    blob.append(&propertyId, sizeof(PropertyId));
                    blob.append(&size, sizeof(uint32_t));
                    blob.append(static_cast<void *>(rawData.getRaw()), rawData.size());
                }
            }

            static Blob parseRecord(const Record &record,
                                    const size_t dataSize,
                                    const adapter::schema::PropertyNameMapInfo &properties) {
                if (dataSize <= 0) {
                    // create an empty property as a raw data for a class
                    auto value = Blob(SIZE_OF_EMPTY_STRING);
                    value.append(static_cast<void *>(&EMPTY_STRING), SIZE_OF_EMPTY_STRING);
                    return value;
                } else {
                    // create properties as a raw data for a class
                    auto value = Blob(dataSize);
                    for (const auto &property: properties) {
                        auto propertyId = static_cast<PropertyId>(property.second.id);
                        auto rawData = record.get(property.first);
                        require(property.second.id < std::pow(2, UINT16_BITS_COUNT));
                        require(rawData.size() < std::pow(2, UINT32_BITS_COUNT - 1));
                        buildRawData(value, propertyId, rawData);
                    }
                    return value;
                }
            }

            static size_t getRawDataSize(size_t size) {
                return sizeof(PropertyId) + size +
                       ((size >= std::pow(2, UINT8_BITS_COUNT - 1)) ? sizeof(uint32_t) : sizeof(uint8_t));
            };
        };
    }

}

#endif
