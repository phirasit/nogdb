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

#include "runtest.h"

struct myobject {
    myobject() {};

    myobject(int x_, unsigned long long y_, double z_) : x{x_}, y{y_}, z{z_} {}

    int x{0};
    unsigned long long y{0};
    double z{0.0};
};

constexpr int int_value = -42;
constexpr unsigned int uint_value = 42;
constexpr char tinyint_value = -128;
constexpr unsigned char utinyint_value = 255;
constexpr short smallint_value = -32768;
constexpr unsigned short usmallint_value = 65535;
constexpr long long bigint_value = -424242424242;
constexpr unsigned long long ubigint_value = 424242424242;
constexpr double real_value = 42.4242;
const std::string text_value = "hello world";
const myobject blob_value = {42, 42424242424242ULL, 42.42};

void test_bytes_only() {
    nogdb::Bytes int_vb{int_value}, uint_vb{uint_value},
            tinyint_vb{tinyint_value}, utinyint_vb{utinyint_value},
            smallint_vb{smallint_value}, usmallint_vb{usmallint_value},
            bigint_vb{bigint_value}, ubigint_vb{ubigint_value},
            real_vb{real_value}, text_vb{text_value},
            blob_vb{blob_value};
    assert(int_vb.toInt() == int_value);
    assert(uint_vb.toIntU() == uint_value);
    assert(tinyint_vb.toTinyInt() == tinyint_value);
    assert(utinyint_vb.toTinyIntU() == utinyint_value);
    assert(smallint_vb.toSmallInt() == smallint_value);
    assert(usmallint_vb.toSmallIntU() == usmallint_value);
    assert(bigint_vb.toBigInt() == bigint_value);
    assert(ubigint_vb.toBigIntU() == ubigint_value);
    assert(real_vb.toReal() == real_value);
    assert(text_vb.toText() == text_value);
    auto tmp = myobject{};
    blob_vb.convertTo(tmp);
    assert(tmp.x == blob_value.x);
    assert(tmp.y == blob_value.y);
    assert(tmp.z == blob_value.z);
}

void test_record_with_bytes() {
    nogdb::Record r{};
    r.set("int", int_value)
            .set("uint", uint_value)
            .set("tinyint", tinyint_value)
            .set("utinyint", utinyint_value)
            .set("smallint", smallint_value)
            .set("usmallint", usmallint_value)
            .set("bigint", bigint_value)
            .set("ubigint", ubigint_value)
            .set("real", real_value)
            .set("text", text_value)
            .set("blob", nogdb::Bytes{blob_value})
            .set("null", "");

    assert(r.get("int").toInt() == int_value);
    assert(r.get("uint").toIntU() == uint_value);
    assert(r.get("bigint").toBigInt() == bigint_value);
    assert(r.get("ubigint").toBigIntU() == ubigint_value);
    assert(r.get("tinyint").toTinyInt() == tinyint_value);
    assert(r.get("utinyint").toTinyIntU() == utinyint_value);
    assert(r.get("smallint").toSmallInt() == smallint_value);
    assert(r.get("usmallint").toSmallIntU() == usmallint_value);
    assert(r.get("real").toReal() == real_value);
    assert(r.get("text").toText() == text_value);
    assert(r.get("null").toText() == "");
    assert(r.get("invalid").toText() == "");

    assert(r.getInt("int") == int_value);
    assert(r.getIntU("uint") == uint_value);
    assert(r.getTinyInt("tinyint") == tinyint_value);
    assert(r.getTinyIntU("utinyint") == utinyint_value);
    assert(r.getSmallInt("smallint") == smallint_value);
    assert(r.getSmallIntU("usmallint") == usmallint_value);
    assert(r.getBigInt("bigint") == bigint_value);
    assert(r.getBigIntU("ubigint") == ubigint_value);
    assert(r.getReal("real") == real_value);
    assert(r.getText("text") == text_value);
    assert(r.getText("invalid") == "");

    auto bytes_tmp = myobject{};
    r.get("blob").convertTo(bytes_tmp);
    assert(bytes_tmp.x == blob_value.x);
    assert(bytes_tmp.y == blob_value.y);
    assert(bytes_tmp.z == blob_value.z);

    assert(r.get("int").size() == sizeof(int_value));
    assert(r.get("uint").size() == sizeof(uint_value));
    assert(r.get("tinyint").size() == sizeof(tinyint_value));
    assert(r.get("utinyint").size() == sizeof(utinyint_value));
    assert(r.get("smallint").size() == sizeof(smallint_value));
    assert(r.get("usmallint").size() == sizeof(usmallint_value));
    assert(r.get("bigint").size() == sizeof(bigint_value));
    assert(r.get("ubigint").size() == sizeof(ubigint_value));
    assert(r.get("real").size() == sizeof(real_value));
    assert(r.get("text").size() == text_value.length());
    assert(r.get("null").size() == std::string{""}.length());
    assert(r.get("blob").size() == sizeof(blob_value));

    auto int_b_copy = r.get("int");
    assert(int_b_copy.toInt() == int_value);
    auto int_b_assign = nogdb::Bytes{};
    int_b_assign = int_b_copy;
    assert(int_b_assign.toInt() == int_value);

    auto uint_b_copy = r.get("uint");
    assert(uint_b_copy.toInt() == uint_value);
    auto uint_b_assign = nogdb::Bytes{};
    uint_b_assign = uint_b_copy;
    assert(uint_b_assign.toIntU() == uint_value);

    auto tinyint_b_copy = r.get("tinyint");
    assert(tinyint_b_copy.toTinyInt() == tinyint_value);
    auto tinyint_b_assign = nogdb::Bytes{};
    tinyint_b_assign = tinyint_b_copy;
    assert(tinyint_b_assign.toTinyInt() == tinyint_value);

    auto utinyint_b_copy = r.get("utinyint");
    assert(utinyint_b_copy.toTinyIntU() == utinyint_value);
    auto utinyint_b_assign = nogdb::Bytes{};
    utinyint_b_assign = utinyint_b_copy;
    assert(utinyint_b_assign.toTinyIntU() == utinyint_value);

    auto smallint_b_copy = r.get("smallint");
    assert(smallint_b_copy.toSmallInt() == smallint_value);
    auto smallint_b_assign = nogdb::Bytes{};
    smallint_b_assign = smallint_b_copy;
    assert(smallint_b_assign.toSmallInt() == smallint_value);

    auto usmallint_b_copy = r.get("usmallint");
    assert(usmallint_b_copy.toSmallIntU() == usmallint_value);
    auto usmallint_b_assign = nogdb::Bytes{};
    usmallint_b_assign = usmallint_b_copy;
    assert(usmallint_b_assign.toSmallIntU() == usmallint_value);

    auto bigint_b_copy = r.get("bigint");
    assert(bigint_b_copy.toBigInt() == bigint_value);
    auto bigint_b_assign = nogdb::Bytes{};
    bigint_b_assign = bigint_b_copy;
    assert(bigint_b_assign.toBigInt() == bigint_value);

    auto ubigint_b_copy = r.get("ubigint");
    assert(ubigint_b_copy.toBigIntU() == ubigint_value);
    auto ubigint_b_assign = nogdb::Bytes{};
    ubigint_b_assign = ubigint_b_copy;
    assert(ubigint_b_assign.toBigIntU() == ubigint_value);

    auto real_b_copy = r.get("real");
    assert(real_b_copy.toReal() == real_value);
    auto real_b_assign = nogdb::Bytes{};
    real_b_assign = real_b_copy;
    assert(real_b_assign.toReal() == real_value);

    auto text_b_copy = r.get("text");
    assert(text_b_copy.toText() == text_value);
    auto text_b_assign = nogdb::Bytes{};
    text_b_assign = text_b_copy;
    assert(text_b_assign.toText() == text_value);

    auto bytes_b_copy = r.get("blob");
    auto bytes_copy_tmp = myobject{};
    bytes_b_copy.convertTo(bytes_copy_tmp);
    assert(bytes_copy_tmp.x == blob_value.x);
    assert(bytes_copy_tmp.y == blob_value.y);
    assert(bytes_copy_tmp.z == blob_value.z);
    auto bytes_b_assign = nogdb::Bytes{};
    bytes_b_assign = bytes_b_copy;
    auto bytes_assign_tmp = myobject{};
    bytes_b_assign.convertTo(bytes_assign_tmp);
    assert(bytes_assign_tmp.x == blob_value.x);
    assert(bytes_assign_tmp.y == blob_value.y);
    assert(bytes_assign_tmp.z == blob_value.z);

    r.unset("int");
    assert(r.get("int").empty());
    r.clear();
    assert(r.empty());

}

void test_invalid_record_with_bytes() {
    nogdb::Record r{};
    try {
        r.getInt("int");
    } catch (const nogdb::Error &err) {
        REQUIRE(err, CTX_NOEXST_PROPERTY, "CTX_NOEXST_PROPERTY");
    }
    try {
        r.getIntU("uint");
    } catch (const nogdb::Error &err) {
        REQUIRE(err, CTX_NOEXST_PROPERTY, "CTX_NOEXST_PROPERTY");
    }
    try {
        r.getTinyInt("tinyint");
    } catch (const nogdb::Error &err) {
        REQUIRE(err, CTX_NOEXST_PROPERTY, "CTX_NOEXST_PROPERTY");
    }
    try {
        r.getTinyIntU("utinyint");
    } catch (const nogdb::Error &err) {
        REQUIRE(err, CTX_NOEXST_PROPERTY, "CTX_NOEXST_PROPERTY");
    }
    try {
        r.getSmallInt("smallint");
    } catch (const nogdb::Error &err) {
        REQUIRE(err, CTX_NOEXST_PROPERTY, "CTX_NOEXST_PROPERTY");
    }
    try {
        r.getSmallIntU("usmallint");
    } catch (const nogdb::Error &err) {
        REQUIRE(err, CTX_NOEXST_PROPERTY, "CTX_NOEXST_PROPERTY");
    }
    try {
        r.getBigInt("bigint");
    } catch (const nogdb::Error &err) {
        REQUIRE(err, CTX_NOEXST_PROPERTY, "CTX_NOEXST_PROPERTY");
    }
    try {
        r.getBigIntU("ubigint");
    } catch (const nogdb::Error &err) {
        REQUIRE(err, CTX_NOEXST_PROPERTY, "CTX_NOEXST_PROPERTY");
    }
    try {
        r.getReal("real");
    } catch (const nogdb::Error &err) {
        REQUIRE(err, CTX_NOEXST_PROPERTY, "CTX_NOEXST_PROPERTY");
    }
    try {
        r.getText("text");
    } catch (const nogdb::Error &err) {
        REQUIRE(err, CTX_NOEXST_PROPERTY, "CTX_NOEXST_PROPERTY");
    }

}
