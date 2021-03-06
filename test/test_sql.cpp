/*
 *  Copyright (C) 2018, Throughwave (Thailand) Co., Ltd.
 *  <kasidej dot bu at throughwave dot co dot th>
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

#include <string.h>

#include "../include/nogdb.h"
#include "runtest.h"
#include "test_exec.h"

using namespace std;
using namespace nogdb;

static string to_string(const RecordDescriptor& recD) {
    return "#" + to_string(recD.rid.first) + ":" + to_string(recD.rid.second);
}

static inline bool equalBytes(const Bytes &a, const Bytes &b) {
    return a.size() == b.size() && memcmp(a.getRaw(), b.getRaw(), a.size()) == 0;
}

static inline bool equalRecord(const Record &a, const Record &b) {
    return (a.getAll().size() == b.getAll().size()
            && equal(a.getAll().begin(),
                     a.getAll().end(),
                     b.getAll().begin(),
                     [](const pair<string, Bytes> &a, const pair<string, Bytes> &b) {
                         return a.first == b.first && equalBytes(a.second, b.second);
                     }));
}

static inline bool equalResult(const Result &a, const Result &b) {
    if (a.descriptor.rid.first != (ClassId)(-2)) {
        return a.descriptor == b.descriptor;
    } else {
        return equalRecord(a.record, b.record);
    }
}

static inline bool equalResultSet(const ResultSet &a, const ResultSet &b) {
    return (a.size() == b.size()
            && equal(a.begin(), a.end(), b.begin(), equalResult));
}


void test_sql_unrecognized_token_error() {
    Txn txn = Txn{*ctx, Txn::Mode::READ_WRITE};
    try {
        SQL::execute(txn, "128asyuiqwerhb;");
        assert(false);
    } catch (const Error &e) {
        REQUIRE(e, SQL_UNRECOGNIZED_TOKEN, "SQL_UNRECOGNIZED_TOKEN");
    }
    txn.commit();
}

void test_sql_syntax_error() {
    Txn txn = Txn{*ctx, Txn::Mode::READ_WRITE};
    try {
        SQL::execute(txn, "SELECT DELETE VERTEX;");
        assert(false);
    } catch (const Error &e) {
        REQUIRE(e, SQL_SYNTAX_ERROR, "SQL_SYNTAX_ERROR");
    }
    txn.commit();
}

void test_sql_create_class() {
    Txn txn = Txn{*ctx, Txn::Mode::READ_WRITE};
	try {
        // create
        SQL::Result result = SQL::execute(txn, "CREATE CLASS sql_class EXTENDS VERTEX");
        
        // check result.
        assert(result.type() == SQL::Result::CLASS_DESCRIPTOR);
        assert(result.get<ClassDescriptor>().name == "sql_class");
        ClassDescriptor schema = Db::getSchema(txn, "sql_class");
		assert(schema.name == "sql_class");
	} catch(const Error& e) {
		std::cout << "\nError: " << e.what() << std::endl;
		assert(false);
	}

    try {
        Class::drop(txn, "sql_class");
    } catch (...) { }
    txn.commit();
}

void test_sql_create_class_if_not_exists() {
    Txn txn = Txn{*ctx, Txn::Mode::READ_WRITE};
    // test not exists case.
    try {
        SQL::Result result = SQL::execute(txn, "CREATE CLASS sql_class IF NOT EXISTS EXTENDS VERTEX");
        assert(result.type() == SQL::Result::CLASS_DESCRIPTOR);
        assert(result.get<ClassDescriptor>().name == "sql_class");
    } catch (const Error &e) {
        std::cout << "\nError: " << e.what() << std::endl;
        assert(false);
    }

    // test exists case.
    try {
        SQL::execute(txn, "CREATE CLASS sql_class IF NOT EXISTS EXTENDS VERTEX");
        auto schema = Db::getSchema(txn, "sql_class");
        assert(schema.name == "sql_class");
    } catch (const Error &e) {
        std::cout << "\nError: " << e.what() << std::endl;
        assert(false);
    }

    try {
        Class::drop(txn, "sql_class");
    } catch (...) { }
    txn.commit();
}

void test_sql_create_class_extend() {
    Txn txn = Txn{*ctx, Txn::Mode::READ_WRITE};
    // create super class
    try {
        Class::create(txn, "sql_class", ClassType::VERTEX);
        Property::add(txn, "sql_class", "prop1", PropertyType::TEXT);
        Property::add(txn, "sql_class", "prop2", PropertyType::UNSIGNED_INTEGER);
    }  catch (const Error& ex) {
        std::cout << "\nError: " << ex.what() << std::endl;
        assert(false);
    }

    // create extend
    try {
        SQL::execute(txn, "CREATE CLASS sql_class_sub EXTENDS sql_class");
    }  catch (const Error& ex) {
        std::cout << "\nError: " << ex.what() << std::endl;
        assert(false);
    }

    // check result
    try {
        auto res = Db::getSchema(txn, "sql_class_sub");
        assert(res.type == ClassType::VERTEX);
        assert(res.properties.find("prop1") != res.properties.end());
        assert(res.properties.find("prop2") != res.properties.end());
        assert(res.properties["prop1"].type == PropertyType::TEXT);
        assert(res.properties["prop2"].type == PropertyType::UNSIGNED_INTEGER);
    } catch (const Error& ex) {
        std::cout << "\nError: " << ex.what() << std::endl;
        assert(false);
    }

    try {
        Class::drop(txn, "sql_class");
        Class::drop(txn, "sql_class_sub");
    } catch (...) { }
    txn.commit();
}

void test_sql_create_invalid_class() {
    Txn txn = Txn{*ctx, Txn::Mode::READ_WRITE};
    try {
        Class::create(txn, "sql_class", ClassType::VERTEX);
    } catch(const Error& ex) {
        std::cout << "\nError: " << ex.what() << std::endl;
        assert(false);
    }

    try {
        SQL::execute(txn, "CREATE CLASS '' EXTENDS VERTEX");
        assert(false);
    } catch(const Error& ex) {
        REQUIRE(ex, CTX_INVALID_CLASSNAME, "CTX_INVALID_CLASSNAME");
    }
    try {
        SQL::execute(txn, "CREATE CLASS sql_class EXTENDS VERTEX");
        assert(false);
    }  catch(const Error& ex) {
        REQUIRE(ex, CTX_DUPLICATE_CLASS, "CTX_DUPLICATE_CLASS");
    }
    try {
        SQL::execute(txn, "DROP CLASS sql_class");
    } catch(const Error& ex) {
        std::cout << "\nError: " << ex.what() << std::endl;
        assert(false);
    }
    txn.commit();
}

void test_sql_alter_class_name() {
    Txn txn = Txn{*ctx, Txn::Mode::READ_WRITE};
    // create class
    try {
        Class::create(txn, "sql_class", ClassType::VERTEX);
        Property::add(txn, "sql_class", "prop1", PropertyType::INTEGER);
        Property::add(txn, "sql_class", "prop2", PropertyType::TEXT);
    } catch(const Error& ex) {
        std::cout << "\nError: " << ex.what() << std::endl;
        assert(false);
    }

    // test alter NAME
    try {
        SQL::execute(txn, "ALTER CLASS sql_class NAME 'sql_class2'");
        auto res = Db::getSchema(txn, "sql_class2");
        assert(res.name == "sql_class2");
        assert(res.properties.at("prop1").type == PropertyType::INTEGER);
        assert(res.properties.at("prop2").type == PropertyType::TEXT);
    } catch(const Error& ex) {
        std::cout << "\nError: " << ex.what() << std::endl;
        assert(false);
    }
    
    try {
        Class::drop(txn, "sql_class2");
    } catch (...) { }
    txn.commit();
}

void test_sql_drop_class() {
    Txn txn = Txn{*ctx, Txn::Mode::READ_WRITE};
	try {
        Class::create(txn, "sql_class", ClassType::VERTEX);

        SQL::Result result = SQL::execute(txn, "DROP CLASS sql_class");
        assert(result.type() == SQL::Result::NO_RESULT);
	} catch(const Error& e) {
		std::cout << "\nError: " << e.what() << std::endl;
		assert(false);
	}

    // check result.
    try {
        auto schema = Db::getSchema(txn, "sql_class");
        assert(false);
    } catch (const Error &e) {
        assert(e.code() == CTX_NOEXST_CLASS);
    }
    txn.commit();
}

void test_sql_drop_class_if_exists() {
    Txn txn = Txn{*ctx, Txn::Mode::READ_WRITE};
    // test exists case.
    try {
        Class::create(txn, "sql_class", ClassType::VERTEX);

        SQL::Result result = SQL::execute(txn, "DROP CLASS sql_class IF EXISTS");
        assert(result.type() == SQL::Result::NO_RESULT);
    } catch(const Error& e) {
        std::cout << "\nError: " << e.what() << std::endl;
        assert(false);
    }

    // test not exists case.
    try {
        SQL::execute(txn, "DROP CLASS test_sql IF EXISTS");
    } catch(const Error& e) {
        std::cout << "\nError: " << e.what() << std::endl;
        assert(false);
    }
    txn.commit();
}

void test_sql_drop_invalid_class() {
    Txn txn = Txn{*ctx, Txn::Mode::READ_WRITE};
    try {
        SQL::execute(txn, "DROP CLASS ''");
        assert(false);
    } catch(const Error& ex) {
        REQUIRE(ex, CTX_NOEXST_CLASS, "CTX_NOEXST_CLASS");
    }
    try {
        SQL::execute(txn, "DROP CLASS sql_class");
        assert(false);
    } catch(const Error& ex) {
        REQUIRE(ex, CTX_NOEXST_CLASS, "CTX_NOEXST_CLASS");
        assert(ex.code() == CTX_NOEXST_CLASS);
    }
    txn.commit();
}

void test_sql_add_property() {
    Txn txn = Txn{*ctx, Txn::Mode::READ_WRITE};
	try {
		Class::create(txn, "sql_class", ClassType::VERTEX);
        SQL::Result result1 = SQL::execute(txn, "CREATE PROPERTY sql_class.prop1 TEXT");
        SQL::Result result2 = SQL::execute(txn, "CREATE PROPERTY sql_class.prop2 UNSIGNED_INTEGER");
        SQL::Result result3 = SQL::execute(txn, "CREATE PROPERTY sql_class.xml TEXT");
        assert(result1.type() == SQL::Result::PROPERTY_DESCRIPTOR);
        assert(result1.get<PropertyDescriptor>().type == PropertyType::TEXT);
        assert(result2.type() == SQL::Result::PROPERTY_DESCRIPTOR);
        assert(result2.get<PropertyDescriptor>().type == PropertyType::UNSIGNED_INTEGER);
        assert(result3.type() == SQL::Result::PROPERTY_DESCRIPTOR);
        assert(result3.get<PropertyDescriptor>().type == PropertyType::TEXT);
	}  catch(const Error& ex) {
		std::cout << "\nError: " << ex.what() << std::endl;
		assert(false);
	}
	try {
		auto schema = Db::getSchema(txn, "sql_class");
		assert(schema.name == "sql_class");
		assert(schema.properties.find("prop1") != schema.properties.end());
		assert(schema.properties.find("prop2") != schema.properties.end());
		assert(schema.properties["prop1"].type == PropertyType::TEXT);
		assert(schema.properties["prop2"].type == PropertyType::UNSIGNED_INTEGER);
	} catch(const Error& ex) {
		std::cout << "\nError: " << ex.what() << std::endl;
		assert(false);
    }
    txn.commit();
}

void test_sql_alter_property() {
    Txn txn = Txn{*ctx, Txn::Mode::READ_WRITE};
    try {
        Class::create(txn, "links", ClassType::EDGE);
        Property::add(txn, "links", "type", PropertyType::TEXT);
        Property::add(txn, "links", "expire", PropertyType::INTEGER);
    } catch(const Error& ex) {
        std::cout << "\nError: " << ex.what() << std::endl;
        assert(false);
    }
    try {
        SQL::execute(txn, "ALTER PROPERTY links.type NAME 'comments'");
        SQL::execute(txn, "ALTER PROPERTY links.expire NAME 'expired'");
        Property::add(txn, "links", "type", PropertyType::BLOB);
    } catch(const Error& ex) {
        std::cout << "\nError: " << ex.what() << std::endl;
        assert(false);
    }

    try {
        auto schema = Db::getSchema(txn, "links");
        assert(schema.name == "links");
        assert(schema.properties.find("type") != schema.properties.end());
        assert(schema.properties.find("comments") != schema.properties.end());
        assert(schema.properties.find("expire") == schema.properties.end());
        assert(schema.properties.find("expired") != schema.properties.end());
    } catch(const Error& ex) {
        std::cout << "\nError: " << ex.what() << std::endl;
        assert(false);
    }
    try {
        Class::drop(txn, "links");
    } catch(const Error& ex) {
        std::cout << "\nError: " << ex.what() << std::endl;
        assert(false);
    }
    txn.commit();
}

void test_sql_delete_property() {
    Txn txn = Txn{*ctx, Txn::Mode::READ_WRITE};
	try {
        SQL::execute(txn, "DROP PROPERTY sql_class.prop2");

        auto schema = Db::getSchema(txn, "sql_class");
        assert(schema.name == "sql_class");
        assert(schema.properties.find("prop2") == schema.properties.end());
	} catch(const Error& ex) {
		std::cout << "\nError: " << ex.what() << std::endl;
		assert(false);
	}

	try {
		Class::drop(txn, "sql_class");
	} catch(const Error& ex) {
		std::cout << "\nError: " << ex.what() << std::endl;
		assert(false);
    }
    txn.commit();
}

void test_sql_create_vertex() {
    init_vertex_book();
    auto txn = Txn{*ctx, Txn::Mode::READ_WRITE};
    try {
        SQL::Result result = SQL::execute(txn, "CREATE VERTEX books SET title='Harry Potter', words=4242424242, pages=865, price=49.99");
        assert(result.type() == SQL::Result::RECORD_DESCRIPTORS);
    } catch(const Error& ex) {
        std::cout << "\nError: " << ex.what() << std::endl;
        assert(false);
    }
    txn.commit();
    destroy_vertex_book();
}

void test_sql_create_edges() {
    init_vertex_book();
    init_vertex_person();
    init_edge_author();

    auto txn = Txn{*ctx, Txn::Mode::READ_WRITE};
    RecordDescriptor v1_1{}, v1_2{}, v2{};
    try {
        v1_1 = Vertex::create(txn, "books", Record().set("title", "Harry Potter").set("pages", 456).set("price", 24.5));
        v1_2 = Vertex::create(txn, "books", Record().set("title", "Fantastic Beasts").set("pages", 342).set("price", 21.0));
        v2 = Vertex::create(txn, "persons", Record().set("name", "J.K. Rowlings").set("age", 32));
    } catch(const Error& ex) {
        std::cout << "\nError: " << ex.what() << std::endl;
        assert(false);
    }

    try {
        SQL::execute(txn, "CREATE EDGE authors FROM " + to_string(v1_1) + " TO " + to_string(v2) + " SET time_used=365");
        SQL::execute(txn, "CREATE EDGE authors FROM (" + to_string(v1_1) + ", " + to_string(v1_2) + ") TO " + to_string(v2) + " SET time_used=180");
    } catch(const Error& ex) {
        std::cout << "\nError: " << ex.what() << std::endl;
        assert(false);
    }

    txn.commit();

    destroy_edge_author();
    destroy_vertex_person();
    destroy_vertex_book();
}

void test_sql_select_vertex() {
    init_vertex_person();
    init_vertex_book();

    auto txn = Txn{*ctx, Txn::Mode::READ_WRITE};
    try {
        auto records = std::vector<Record>{};
        records.push_back(Record{}
                          .set("title", "Percy Jackson")
                          .set("pages", 456)
                          .set("price", 24.5)
                          );
        records.push_back(Record{}
                          .set("title", "Batman VS Superman")
                          .set("words", 9999999ULL)
                          .set("price", 36.0)
                          );
        for(const auto& record: records) {
            Vertex::create(txn, "books", record);
        }
        Vertex::create(txn, "persons", Record{}
                              .set("name", "Jim Beans")
                              .set("age", 40U)
                              );
    } catch(const Error& ex) {
        std::cout << "\nError: " << ex.what() << std::endl;
        assert(false);
    }
    try {
        SQL::Result result = SQL::execute(txn, "SELECT * FROM books");
        assert(result.type() == SQL::Result::Type::RESULT_SET);
        ResultSet res = result.get<ResultSet>();
        assert(res[0].record.get("title").toText() == "Percy Jackson");
        assert(res[0].record.get("pages").toInt() == 456);
        assert(res[0].record.get("price").toReal() == 24.5);
        assert(res[0].record.get("words").empty());
        assert(res[1].record.get("title").toText() == "Batman VS Superman");
        assert(res[1].record.get("words").toBigIntU() == 9999999);
        assert(res[1].record.get("price").toReal() == 36.0);
        assert(res[1].record.get("pages").empty());

        result = SQL::execute(txn, "SELECT * FROM books");
        assert(result.type() == SQL::Result::Type::RESULT_SET);
        res = result.get<ResultSet>();
        assert(res.size() == 2);
        assert(res[0].record.get("title").toText() == "Percy Jackson");
        assert(res[0].record.get("pages").toInt() == 456);
        assert(res[0].record.get("price").toReal() == 24.5);
        assert(res[0].record.get("words").empty());
        assert(res[1].record.get("title").toText() == "Batman VS Superman");
        assert(res[1].record.get("words").toBigIntU() == 9999999);
        assert(res[1].record.get("price").toReal() == 36.0);
        assert(res[1].record.get("pages").empty());
    } catch(const Error& ex) {
        std::cout << "\nError: " << ex.what() << std::endl;
        assert(false);
    }
    txn.commit();

    destroy_vertex_book();
    destroy_vertex_person();
}

void test_sql_select_vertex_with_rid() {
    init_vertex_person();
    init_vertex_book();

    auto txn = Txn{*ctx, Txn::Mode::READ_WRITE};

    RecordDescriptor rid1, rid2;
    try {
        rid1 = Vertex::create(txn, "persons", Record()
                              .set("name", "Jim Beans")
                              .set("age", 40U)
                              );
        rid2 = Vertex::create(txn, "books", Record()
                                     .set("title", "Percy Jackson")
                                     .set("pages", 456)
                                     .set("price", 24.5)
                                     );
    } catch(const Error& ex) {
        std::cout << "\nError: " << ex.what() << std::endl;
        assert(false);
    }

    try {
        auto result = SQL::execute(txn, "SELECT FROM " + to_string(rid1));
        assert(result.type() == result.RESULT_SET);
        auto res = result.get<ResultSet>();
        assert(res.size() == 1);
        assert(res[0].descriptor == rid1);
    } catch(const Error& ex) {
        std::cout << "\nError: " << ex.what() << std::endl;
        assert(false);
    }

    try {
        auto result = SQL::execute(txn, "SELECT FROM (" + to_string(rid1) + ", " + to_string(rid2) + ")");
        assert(result.type() == result.RESULT_SET);
        auto res = result.get<ResultSet>();
        assert(res.size() == 2);
        assert((res[0].descriptor == rid1 && res[1].descriptor == rid2)
               || (res[0].descriptor == rid2 && res[1].descriptor == rid1));
    } catch(const Error& ex) {
        std::cout << "\nError: " << ex.what() << std::endl;
        assert(false);
    }

    txn.commit();

    destroy_vertex_book();
    destroy_vertex_person();
}

void test_sql_select_property() {
    init_vertex_person();

    auto txn = Txn{*ctx, Txn::Mode::READ_WRITE};

    RecordDescriptor rid1, ridRes;
    try {
        rid1 = Vertex::create(txn, "persons", Record()
                                     .set("name", "Jim Beans")
                                     .set("age", 40U)
                                     );
    } catch(const Error& ex) {
        std::cout << "\nError: " << ex.what() << std::endl;
        assert(false);
    }

    // select properties.
    try {
        SQL::Result result = SQL::execute(txn, "SELECT @recordId, name, age FROM " + to_string(rid1));
        assert(result.type() == result.RESULT_SET);
        auto res = result.get<ResultSet>();
        assert(res.size() == 1);
        assert(res[0].descriptor == RecordDescriptor(-2, 0));
        assert(res[0].record.get("name").toText() == "Jim Beans");
        assert(res[0].record.get("age").toIntU() == 40U);
        res[0].record.get("@recordId").convertTo(ridRes);
        assert(ridRes == rid1);
    } catch(const Error& ex) {
        std::cout << "\nError: " << ex.what() << std::endl;
        assert(false);
    }

    // select non-exist property.
    try {
        SQL::Result result = SQL::execute(txn, "SELECT nonExist FROM " + to_string(rid1));
        assert(result.type() == result.RESULT_SET);
        assert(result.get<ResultSet>().size() == 0);
    } catch(const Error& ex) {
        std::cout << "\nError: " << ex.what() << std::endl;
        assert(false);
    }

    txn.commit();

    destroy_vertex_person();
}

void test_sql_select_count() {
    init_vertex_person();

    auto txn = Txn{*ctx, Txn::Mode::READ_WRITE};

    RecordDescriptor rid1, rid2;
    try {
        Vertex::create(txn, "persons", Record().set("name", "Jim Beans").set("age", 40U));
        Vertex::create(txn, "persons", Record().set("name", "Jame Beans"));
        Vertex::create(txn, "persons");
    } catch(const Error& ex) {
        std::cout << "\nError: " << ex.what() << std::endl;
        assert(false);
    }

    try {
        auto result = SQL::execute(txn, "SELECT count(*) FROM persons");
        assert(result.type() == result.RESULT_SET);
        auto res = result.get<ResultSet>();
        assert(res.size() == 1);
        assert(res[0].descriptor == RecordDescriptor(-2, 0));
        assert(res[0].record.get("count()").toIntU() == 3);

        result = SQL::execute(txn, "SELECT count('name'), count(age) FROM persons");
        assert(result.type() == result.RESULT_SET);
        res = result.get<ResultSet>();
        assert(res.size() == 1);
        assert(res[0].descriptor == RecordDescriptor(-2, 0));
        assert(res[0].record.get("count(name)").toIntU() == 2);
        assert(res[0].record.get("count(age)").toIntU() == 1);

        // count empty result.
        result = SQL::execute(txn, "SELECT count(*) FROM persons WHERE name='Sam'");
        assert(result.type() == result.RESULT_SET);
        res = result.get<ResultSet>();
        assert(res.size() == 1);
        assert(res[0].descriptor == RecordDescriptor(-2, 0));
        assert(res[0].record.get("count()").toIntU() == 0);
    } catch(const Error& ex) {
        std::cout << "\nError: " << ex.what() << std::endl;
        assert(false);
    }

    txn.commit();

    destroy_vertex_person();
}

void test_sql_select_walk() {
    auto txn = Txn(*ctx, Txn::Mode::READ_WRITE);

    Class::create(txn, "v", ClassType::VERTEX);
    Class::create(txn, "eA", ClassType::EDGE);
    Class::create(txn, "eB", ClassType::EDGE);

    try {
        auto v1 = Vertex::create(txn, "v");
        auto v2 = Vertex::create(txn, "v");
        auto v3 = Vertex::create(txn, "v");
        auto v4 = Vertex::create(txn, "v");
        auto v5 = Vertex::create(txn, "v");
        auto eA13 = Edge::create(txn, "eA", v1, v3);
        auto eB14 = Edge::create(txn, "eB", v1, v4);
        auto eA23 = Edge::create(txn, "eA", v2, v3);
        auto eB24 = Edge::create(txn, "eB", v2, v4);
        auto eA35 = Edge::create(txn, "eA", v3, v5);

        auto result = SQL::execute(txn, "SELECT expand(outE()) FROM " + to_string(v1));
        assert(result.type() == result.RESULT_SET);
        auto res = result.get<ResultSet>();
        assert(res.size() == 2);
        assert(res[0].descriptor == eA13);
        assert(res[1].descriptor == eB14);

        result = SQL::execute(txn, "SELECT expand(inE()) FROM " + to_string(v3));
        assert(result.type() == result.RESULT_SET);
        res = result.get<ResultSet>();
        assert(res.size() == 2);
        assert(res[0].descriptor == eA23);
        assert(res[1].descriptor == eA13);

        result = SQL::execute(txn, "SELECT expand(bothE()) FROM " + to_string(v3));
        assert(result.type() == result.RESULT_SET);
        res = result.get<ResultSet>();
        assert(res.size() == 3);
        assert(res[0].descriptor == eA13);
        assert(res[1].descriptor == eA23);
        assert(res[2].descriptor == eA35);

        result = SQL::execute(txn, "SELECT expand(outV()) FROM " + to_string(eA13));
        assert(result.type() == result.RESULT_SET);
        res = result.get<ResultSet>();
        assert(res.size() == 1);
        assert(res[0].descriptor == v1);

        result = SQL::execute(txn, "SELECT expand(inV()) FROM " + to_string(eA13));
        assert(result.type() == result.RESULT_SET);
        res = result.get<ResultSet>();
        assert(res.size() == 1);
        assert(res[0].descriptor == v3);

        result = SQL::execute(txn, "SELECT expand(out()) FROM " + to_string(v1));
        assert(result.type() == result.RESULT_SET);
        res = result.get<ResultSet>();
        assert(res.size() == 2);
        assert(res[0].descriptor == v3);
        assert(res[1].descriptor == v4);

        result = SQL::execute(txn, "SELECT expand(in()) FROM " + to_string(v3));
        assert(result.type() == result.RESULT_SET);
        res = result.get<ResultSet>();
        assert(res.size() == 2);
        assert(res[0].descriptor == v2);
        assert(res[1].descriptor == v1);

        result = SQL::execute(txn, "SELECT expand(both()) FROM " + to_string(v3));
        assert(result.type() == result.RESULT_SET);
        res = result.get<ResultSet>();
        assert(res.size() == 3);
        assert(res[0].descriptor == v2);
        assert(res[1].descriptor == v1);
        assert(res[2].descriptor == v5);

        result = SQL::execute(txn, "SELECT expand(out('eA')) FROM " + to_string(v1));
        assert(result.type() == result.RESULT_SET);
        res = result.get<ResultSet>();
        assert(res.size() == 1);
        assert(res[0].descriptor == v3);

        result = SQL::execute(txn, "SELECT expand(in('eA', 'eB')) FROM " + to_string(v3));
        assert(result.type() == result.RESULT_SET);
        res = result.get<ResultSet>();
        assert(res.size() == 2);
        assert(res[0].descriptor == v2);
        assert(res[1].descriptor == v1);
    } catch(const Error& ex) {
        std::cout << "\nError: " << ex.what() << std::endl;
        assert(false);
    }

    Class::drop(txn, "v");
    Class::drop(txn, "eA");
    Class::drop(txn, "eB");
    
    txn.commit();
}

void test_sql_select_method_property() {
    auto txn = Txn(*ctx, Txn::Mode::READ_WRITE);

    Class::create(txn, "v", ClassType::VERTEX);
    Property::add(txn, "v", "propV", PropertyType::TEXT);
    Class::create(txn, "e", ClassType::EDGE);
    Property::add(txn, "e", "propE", PropertyType::TEXT);

    try {
        auto v1 = Vertex::create(txn, "v", Record().set("propV", "v1"));
        auto v2 = Vertex::create(txn, "v", Record().set("propV", "v2"));
        auto v3 = Vertex::create(txn, "v", Record().set("propV", "v3"));
        auto v4 = Vertex::create(txn, "v", Record().set("propV", "v4"));
        auto eA13 = Edge::create(txn, "e", v1, v3, Record().set("propE", "e1->3"));
        auto eB14 = Edge::create(txn, "e", v1, v4, Record().set("propE", "e1->4"));
        auto eB24 = Edge::create(txn, "e", v2, v4, Record().set("propE", "e2->4"));

        auto result = SQL::execute(txn, "SELECT inV().propV FROM " + to_string(eA13));
        assert(result.type() == result.RESULT_SET);
        auto res = result.get<ResultSet>();
        assert(res.size() == 1);
        assert(res[0].descriptor == RecordDescriptor(-2, 0));
        assert(res[0].record.getText("inV().propV") == "v3");

        result = SQL::execute(txn, "SELECT out()[0].propV FROM " + to_string(v1));
        assert(result.type() == result.RESULT_SET);
        res = result.get<ResultSet>();
        assert(res[0].descriptor == RecordDescriptor(-2, 0));
        assert(res[0].record.getText("out()[0].propV") == "v4");

        result = SQL::execute(txn, "SELECT propV, out()[0].propV FROM " + to_string(v1));
        assert(result.type() == result.RESULT_SET);
        res = result.get<ResultSet>();
        assert(res[0].descriptor == RecordDescriptor(-2, 0));
        assert(res[0].record.getText("propV") == "v1");
        assert(res[0].record.getText("out()[0].propV") == "v4");

        result = SQL::execute(txn, "SELECT out()[2].propV FROM " + to_string(v1));
        assert(result.type() == result.RESULT_SET);
        res = result.get<ResultSet>();
        assert(res.size() == 0);

        result = SQL::execute(txn, "SELECT propV, out()[2].propV FROM " + to_string(v1));
        assert(result.type() == result.RESULT_SET);
        res = result.get<ResultSet>();
        assert(res[0].descriptor == RecordDescriptor(-2, 0));
        assert(res[0].record.getText("propV") == "v1");
        assert(res[0].record.get("out()[0].propV").empty());
    } catch(const Error& ex) {
        std::cout << "\nError: " << ex.what() << std::endl;
        assert(false);
    }

    Class::drop(txn, "v");
    Class::drop(txn, "e");
    txn.commit();
}

void test_sql_select_alias_property() {
    auto txn = Txn(*ctx, Txn::Mode::READ_WRITE);

    Class::create(txn, "v", ClassType::VERTEX);
    Property::add(txn, "v", "propV", PropertyType::TEXT);
    Class::create(txn, "e", ClassType::EDGE);
    Property::add(txn, "e", "propE", PropertyType::TEXT);

    try {
        auto v1 = Vertex::create(txn, "v", Record().set("propV", "v1"));
        auto v2 = Vertex::create(txn, "v", Record().set("propV", "v2"));
        auto v3 = Vertex::create(txn, "v", Record().set("propV", "v3"));
        auto v4 = Vertex::create(txn, "v", Record().set("propV", "v4"));
        auto eA13 = Edge::create(txn, "e", v1, v3, Record().set("propE", "e1->3"));
        auto eB14 = Edge::create(txn, "e", v1, v4, Record().set("propE", "e1->4"));
        auto eB24 = Edge::create(txn, "e", v2, v4, Record().set("propE", "e2->4"));

        auto result = SQL::execute(txn, "SELECT inV().propV AS my_prop FROM " + to_string(eA13));
        assert(result.type() == result.RESULT_SET);
        auto res = result.get<ResultSet>();
        assert(res.size() == 1);
        assert(res[0].descriptor == RecordDescriptor(-2, 0));
        assert(res[0].record.getText("my_prop") == "v3");
    } catch(const Error& ex) {
        std::cout << "\nError: " << ex.what() << std::endl;
        assert(false);
    }

    Class::drop(txn, "v");
    Class::drop(txn, "e");
    txn.commit();
}

struct Coordinates {
    Coordinates(){};
    Coordinates(double x_, double y_): x{x_}, y{y_}{}
    double x{0.0};
    double y{0.0};
    string toHex() {
        string result{};
        for (size_t i=0; i<sizeof(struct Coordinates); i++) {
            char tmp[3];
            sprintf(tmp, "%02X", ((unsigned char *)this)[i]);
            result += tmp;
        }
        return result;
    }
};

void test_sql_select_vertex_condition() {
    auto txn = Txn(*ctx, Txn::Mode::READ_WRITE);
    Class::create(txn, "v", ClassType::VERTEX);
    Property::add(txn, "v", "text", nogdb::PropertyType::TEXT);
    Property::add(txn, "v", "int", nogdb::PropertyType::INTEGER);
    Property::add(txn, "v", "uint", nogdb::PropertyType::UNSIGNED_INTEGER);
    Property::add(txn, "v", "bigint", nogdb::PropertyType::BIGINT);
    Property::add(txn, "v", "ubigint", nogdb::PropertyType::UNSIGNED_BIGINT);
    Property::add(txn, "v", "real", nogdb::PropertyType::REAL);
    auto v1 = nogdb::Vertex::create(txn, "v", nogdb::Record{}.set("text", "A").set("int", 11).set("uint", 10200U).set("bigint", 200000LL).set("ubigint", 2000ULL).set("real", 4.5));
    auto v2 = nogdb::Vertex::create(txn, "v", nogdb::Record{}.set("text", "B1Y").set("int", 37).set("bigint", 280000LL).set("ubigint", 1800ULL).set("real", 5.0));
    auto v3 = nogdb::Vertex::create(txn, "v", nogdb::Record{}.set("text", "B2Y").set("uint", 10250U).set("bigint", 220000LL).set("ubigint", 2400ULL).set("real", 4.5));
    auto v4 = nogdb::Vertex::create(txn, "v", nogdb::Record{}.set("text", "CX").set("int", 28).set("uint", 11600U).set("ubigint", 900ULL).set("real", 3.5));
    auto v5 = nogdb::Vertex::create(txn, "v", nogdb::Record{}.set("text", "DX").set("int", 18).set("uint", 10475U).set("bigint", 300000LL).set("ubigint", 900ULL));

    try {
        auto result = SQL::execute(txn, "SELECT FROM v WHERE text='A'");
        assert(result.type() == result.RESULT_SET);
        auto res = result.get<ResultSet>();
        assert(res.size() == 1);
        assert(res[0].descriptor == v1);

        result = SQL::execute(txn, "SELECT FROM v WHERE text='Z'");
        assert(result.type() == result.RESULT_SET);
        res = result.get<ResultSet>();
        assert(res.size() == 0);

        result = SQL::execute(txn, "SELECT FROM v WHERE int=18");
        assert(result.type() == result.RESULT_SET);
        res = result.get<ResultSet>();
        assert(res.size() == 1);
        assert(res[0].descriptor == v5);

        result = SQL::execute(txn, "SELECT FROM v WHERE uint=11600");
        assert(result.type() == result.RESULT_SET);
        res = result.get<ResultSet>();
        assert(res.size() == 1);
        assert(res[0].descriptor == v4);

        result = SQL::execute(txn, "SELECT FROM v WHERE bigint=280000");
        assert(result.type() == result.RESULT_SET);
        res = result.get<ResultSet>();
        assert(res.size() == 1);
        assert(res[0].descriptor == v2);

        result = SQL::execute(txn, "SELECT FROM v WHERE ubigint=900");
        assert(result.type() == result.RESULT_SET);
        res = result.get<ResultSet>();
        assert(res.size() == 2);
        assert(res[0].descriptor == v4);
        assert(res[1].descriptor == v5);

        result = SQL::execute(txn, "SELECT FROM v WHERE real=4.5");
        assert(result.type() == result.RESULT_SET);
        res = result.get<ResultSet>();
        assert(res.size() == 2);
        assert(res[0].descriptor == v1);
        assert(res[1].descriptor == v3);
    } catch (const Error& ex) {
        std::cout << "\nError: " << ex.what() << std::endl;
        assert(false);
    }

    try {
        auto result = SQL::execute(txn, "SELECT FROM v WHERE text != 'A'");
        assert(result.type() == result.RESULT_SET);
        auto res = result.get<ResultSet>();
        assert(res.size() == 4);

        result = SQL::execute(txn, "SELECT FROM v WHERE int > 35");
        assert(result.type() == result.RESULT_SET);
        res = result.get<ResultSet>();
        assert(res.size() == 1);

        result = SQL::execute(txn, "SELECT FROM v WHERE real >= 4.5");
        assert(result.type() == result.RESULT_SET);
        res = result.get<ResultSet>();
        assert(res.size() == 3);

        result = SQL::execute(txn, "SELECT FROM v WHERE uint < 10300");
        assert(result.type() == result.RESULT_SET);
        res = result.get<ResultSet>();
        assert(res.size() == 2);

        result = SQL::execute(txn, "SELECT FROM v WHERE ubigint <= 900");
        assert(result.type() == result.RESULT_SET);
        res = result.get<ResultSet>();
        assert(res.size() == 2);

        result = SQL::execute(txn, "SELECT FROM v WHERE bigint IS NULL");
        assert(result.type() == result.RESULT_SET);
        res = result.get<ResultSet>();
        assert(res.size() == 1);

        result = SQL::execute(txn, "SELECT FROM v WHERE int IS NOT NULL");
        assert(result.type() == result.RESULT_SET);
        res = result.get<ResultSet>();
        assert(res.size() == 4);

        result = SQL::execute(txn, "SELECT FROM v WHERE text = 100");
        assert(result.type() == result.RESULT_SET);
        res = result.get<ResultSet>();
        assert(res.size() == 0);

        result = SQL::execute(txn, "SELECT FROM v WHERE ubigint = 2000");
        assert(result.type() == result.RESULT_SET);
        res = result.get<ResultSet>();
        assert(res.size() == 1);
    } catch (const Error& ex) {
        std::cout << "\nError: " << ex.what() << std::endl;
        assert(false);
    }

    try {
        auto result = SQL::execute(txn, "SELECT FROM v WHERE text CONTAIN 'a'");
        assert(result.type() == result.RESULT_SET);
        auto res = result.get<ResultSet>();
        assert(res.size() == 1);
        assert(res[0].descriptor == v1);

        result = SQL::execute(txn, "SELECT FROM v WHERE NOT (text CONTAIN 'b')");
        assert(result.type() == result.RESULT_SET);
        res = result.get<ResultSet>();
        assert(res.size() == 3);

        result = SQL::execute(txn, "SELECT FROM v WHERE text BEGIN WITH 'a'");
        assert(result.type() == result.RESULT_SET);
        res = result.get<ResultSet>();
        assert(res.size() == 1);
        assert(res[0].descriptor == v1);

        result = SQL::execute(txn, "SELECT FROM v WHERE NOT text BEGIN WITH CASE 'A'");
        assert(result.type() == result.RESULT_SET);
        res = result.get<ResultSet>();
        assert(res.size() == 4);

        result = SQL::execute(txn, "SELECT FROM v WHERE text END WITH 'x'");
        assert(result.type() == result.RESULT_SET);
        res = result.get<ResultSet>();
        assert(res.size() == 2);

        result = SQL::execute(txn, "SELECT FROM v WHERE NOT text END WITH CASE 'Y'");
        assert(result.type() == result.RESULT_SET);
        res = result.get<ResultSet>();
        assert(res.size() == 3);

        result = SQL::execute(txn, "SELECT FROM v WHERE text > 'B2Y'");
        assert(result.type() == result.RESULT_SET);
        res = result.get<ResultSet>();
        assert(res.size() == 2);

        result = SQL::execute(txn, "SELECT FROM v WHERE text >= 'B2Y'");
        assert(result.type() == result.RESULT_SET);
        res = result.get<ResultSet>();
        assert(res.size() == 3);

        result = SQL::execute(txn, "SELECT FROM v WHERE text < 'B2Y'");
        assert(result.type() == result.RESULT_SET);
        res = result.get<ResultSet>();
        assert(res.size() == 2);

        result = SQL::execute(txn, "SELECT FROM v WHERE text <= 'B2Y'");
        assert(result.type() == result.RESULT_SET);
        res = result.get<ResultSet>();
        assert(res.size() == 3);

        result = SQL::execute(txn, "SELECT FROM v WHERE text IN ['B1Y', 'A']");
        assert(result.type() == result.RESULT_SET);
        res = result.get<ResultSet>();
        assert(res.size() == 2);

        result = SQL::execute(txn, "SELECT FROM v WHERE text LIKE '%1%'");
        assert(result.type() == result.RESULT_SET);
        res = result.get<ResultSet>();
        assert(res.size() == 1);
        assert(res[0].descriptor == v2);
    } catch (const Error& ex) {
        std::cout << "\nError: " << ex.what() << std::endl;
        assert(false);
    }
}

void test_sql_select_vertex_with_expression() {
    auto txn = Txn{*ctx, Txn::Mode::READ_WRITE};
    Class::create(txn, "v", ClassType::VERTEX);
    Property::add(txn, "v", "prop1", PropertyType::TEXT);
    Property::add(txn, "v", "prop2", PropertyType::INTEGER);
    auto v1 = Vertex::create(txn, "v", Record().set("prop1", "AX").set("prop2", 1));
    auto v2 = Vertex::create(txn, "v", Record().set("prop1", "BX").set("prop2", 2));
    auto v3 = Vertex::create(txn, "v", Record().set("prop1", "C").set("prop2", 3));
    try {
        auto result = SQL::execute(txn, "SELECT FROM v WHERE prop1 END WITH 'X' OR prop2 >= 2");
        assert(result.type() == result.RESULT_SET);
        auto res = result.get<ResultSet>();
        assert(res.size() == 3);
    } catch (const Error& ex) {
        std::cout << "\nError: " << ex.what() << std::endl;
        assert(false);
    }

    try {
        auto result = SQL::execute(txn, "SELECT FROM v WHERE (prop1 = 'C' AND prop2 = 3) OR prop1 = 'AX'");
        assert(result.type() == result.RESULT_SET);
        auto res = result.get<ResultSet>();
        assert(res.size() == 2);
    } catch (const Error& ex) {
        std::cout << "\nError: " << ex.what() << std::endl;
        assert(false);
    }

    try {
        auto result = SQL::execute(txn, "SELECT FROM v WHERE (@className='v' AND prop2<2) OR (@className='x' AND prop2>0)");
        assert(result.type() == result.RESULT_SET);
        auto res = result.get<ResultSet>();
        assert(res.size() == 1);
        assert(res[0].descriptor == v1);
    } catch (const Error& ex) {
        std::cout << "\nError: " << ex.what() << std::endl;
        assert(false);
    }
}

void test_sql_select_nested_condition() {
    Txn txn(*ctx, Txn::Mode::READ_WRITE);
    Class::create(txn, "v", ClassType::VERTEX);
    Property::add(txn, "v", "prop1", PropertyType::TEXT);
    Property::add(txn, "v", "prop2", PropertyType::INTEGER);
    auto v1 = Vertex::create(txn, "v", Record().set("prop1", "AX").set("prop2", 1));
    auto v2 = Vertex::create(txn, "v", Record().set("prop1", "BX").set("prop2", 2));
    auto v3 = Vertex::create(txn, "v", Record().set("prop1", "C").set("prop2", 3));
    try {
        auto result = SQL::execute(txn, "SELECT * FROM (SELECT FROM v) WHERE prop2=1");
        assert(result.type() == result.RESULT_SET);
        auto res = result.get<ResultSet>();
        assert(res.size() == 1);
        assert(res[0].descriptor == v1);

        result = SQL::execute(txn, "SELECT * FROM (SELECT prop1, prop2 FROM v) WHERE prop2>2");
        assert(result.type() == result.RESULT_SET);
        res = result.get<ResultSet>();
        assert(res.size() == 1);
        assert(res[0].record.get("prop1").toText() == "C");

        result = SQL::execute(txn, "SELECT * FROM (SELECT @className, prop1, prop2 FROM v) WHERE @className='v' AND prop2<2");
        assert(result.type() == result.RESULT_SET);
        res = result.get<ResultSet>();
        assert(res.size() == 1);
        assert(res[0].record.get("prop1").toText() == "AX");
    } catch (const Error& ex) {
        std::cout << "\nError: " << ex.what() << std::endl;
        assert(false);
    }
}

void test_sql_select_skip_limit() {
    Txn txn(*ctx, Txn::Mode::READ_WRITE);
    Class::create(txn, "v", ClassType::VERTEX);
    Property::add(txn, "v", "prop1", PropertyType::TEXT);
    Property::add(txn, "v", "prop2", PropertyType::INTEGER);
    auto v1 = Vertex::create(txn, "v", Record().set("prop1", "A").set("prop2", 1));
    auto v2 = Vertex::create(txn, "v", Record().set("prop1", "B").set("prop2", 2));
    auto v3 = Vertex::create(txn, "v", Record().set("prop1", "C").set("prop2", 3));
    auto v4 = Vertex::create(txn, "v", Record().set("prop1", "D").set("prop2", 4));
    try {
        SQL::Result result = SQL::execute(txn, "SELECT * FROM v SKIP 1 LIMIT 2");
        assert(result.type() == result.RESULT_SET);
        ResultSet baseResult = Vertex::get(txn, "v");
        baseResult.erase(baseResult.begin(), baseResult.begin() + 1);
        baseResult.resize(2);
        assert(equalResultSet(result.get<ResultSet>(), baseResult));

        result = SQL::execute(txn, "SELECT * FROM (SELECT FROM v) WHERE prop2<3 SKIP 0 LIMIT 1");
        assert(result.type() == result.RESULT_SET);
        baseResult = Vertex::get(txn, "v", Condition("prop2").le(3));
        baseResult.resize(1);
        assert(equalResultSet(result.get<ResultSet>(), baseResult));
    } catch (const Error& ex) {
        std::cout << "\nError: " << ex.what() << std::endl;
        assert(false);
    }
}

void test_sql_select_group_by() {
    init_vertex_book();
    auto txn = Txn(*ctx, Txn::Mode::READ_WRITE);
    try {
        Record r{};
        r.set("title", "Lion King").set("price", 100.0);
        auto rdesc1 = Vertex::create(txn, "books", r);
        r.set("title", "Tarzan").set("price", 100.0);
        auto rdesc2 = Vertex::create(txn, "books", r);

        SQL::Result result = SQL::execute(txn, "SELECT * FROM books GROUP BY price");
        assert(result.type() == result.RESULT_SET);
        assert(result.get<ResultSet>().size() == 1);
        assert(result.get<ResultSet>()[0].descriptor == rdesc2);
    } catch (const Error &e) {
        cout << "\nError: " << e.what() << endl;
        assert(false);
    }
    txn.commit();
    destroy_vertex_book();
}

void test_sql_update_vertex_with_rid() {
    init_vertex_book();
    auto txn = Txn{*ctx, Txn::Mode::READ_WRITE};
    try {
        Record r{};
        r.set("title", "Lion King").set("price", 100.0).set("pages", 320);
        auto rdesc1 = Vertex::create(txn, "books", r);
        r.set("title", "Tarzan").set("price", 60.0).set("pages", 360);
        auto rdesc2 = Vertex::create(txn, "books", r);

        auto record = Db::getRecord(txn, rdesc1);
        assert(record.get("title").toText() == "Lion King");
        assert(record.get("price").toReal() == 100);
        assert(record.get("pages").toInt() == 320);

        SQL::execute(txn, "UPDATE " + to_string(rdesc1) + " SET price=50.0, pages=400, words=90000");
        auto res = Vertex::get(txn, "books");
        assert(res[0].record.get("title").toText() == "Lion King");
        auto t = res[0].record.get("price").toReal();
        assert(res[0].record.get("price").toReal() == 50);
        assert(res[0].record.get("pages").toInt() == 400);
        assert(res[0].record.get("words").toBigIntU() == 90000ULL);
        assert(res[1].record.get("title").toText() == "Tarzan");
        assert(res[1].record.get("price").toReal() == 60);
        assert(res[1].record.get("pages").toInt() == 360);
    } catch(const Error& ex) {
        std::cout << "\nError: " << ex.what() << std::endl;
        assert(false);
    }
    txn.commit();
    destroy_vertex_book();
}

void test_sql_update_vertex_with_condition() {
    init_vertex_book();
    auto txn = Txn{*ctx, Txn::Mode::READ_WRITE};
    try {
        Record r{};
        r.set("title", "Lion King").set("price", 100.0).set("pages", 320);
        auto rdesc1 = Vertex::create(txn, "books", r);
        r.set("title", "Tarzan").set("price", 60.0).set("pages", 360);
        auto rdesc2 = Vertex::create(txn, "books", r);

        auto record = Db::getRecord(txn, rdesc1);
        assert(record.get("title").toText() == "Lion King");
        assert(record.get("price").toReal() == 100);
        assert(record.get("pages").toInt() == 320);

        SQL::execute(txn, "UPDATE books SET price=50.0, pages=400, words=90000 where title='Lion King'");
        auto res = Vertex::get(txn, "books");
        assert(res[0].record.get("title").toText() == "Lion King");
        assert(res[0].record.get("price").toReal() == 50);
        assert(res[0].record.get("pages").toInt() == 400);
        assert(res[0].record.get("words").toBigIntU() == 90000ULL);
        assert(res[1].record.get("title").toText() == "Tarzan");
        assert(res[1].record.get("price").toReal() == 60);
        assert(res[1].record.get("pages").toInt() == 360);
    } catch(const Error& ex) {
        std::cout << "\nError: " << ex.what() << std::endl;
        assert(false);
    }
    txn.commit();
    destroy_vertex_book();
}

void test_sql_delete_vertex_with_rid() {
    init_vertex_book();
    init_vertex_person();
    init_edge_author();
    auto txn = Txn{*ctx, Txn::Mode::READ_WRITE};
    try {
        Record r1{}, r2{}, r3{};
        r1.set("title", "Harry Potter").set("pages", 456).set("price", 24.5);
        auto v1_1 = Vertex::create(txn, "books", r1);
        r1.set("title", "Fantastic Beasts").set("pages", 342).set("price", 21.0);
        auto v1_2 = Vertex::create(txn, "books", r1);
        r1.set("title", "Percy Jackson").set("pages", 800).set("price", 32.4);
        auto v1_3 = Vertex::create(txn, "books", r1);

        r2.set("name", "J.K. Rowlings").set("age", 32);
        auto v2_1 = Vertex::create(txn, "persons", r2);
        r2.set("name", "David Lahm").set("age", 29);
        auto v2_2 = Vertex::create(txn, "persons", r2);

        r3.set("time_used", 365U);
        auto e1 = Edge::create(txn, "authors", v1_1, v2_1, r3);
        r3.set("time_used", 180U);
        auto e2 = Edge::create(txn, "authors", v1_2, v2_1, r3);
        r3.set("time_used", 430U);
        auto e3 = Edge::create(txn, "authors", v1_3, v2_2, r3);

        SQL::execute(txn, "DELETE VERTEX " + to_string(v2_1));

        auto record = Db::getRecord(txn, v2_1);
        assert(record.empty());
        record = Db::getRecord(txn, v1_1);
        assert(!record.empty());
        record = Db::getRecord(txn, v1_2);
        assert(!record.empty());
        record = Db::getRecord(txn, e1);
        assert(record.empty());
        record = Db::getRecord(txn, e2);
        assert(record.empty());

    } catch(const Error& ex) {
        std::cout << "\nError: " << ex.what() << std::endl;
        assert(false);
    }
    txn.commit();

    destroy_edge_author();
    destroy_vertex_person();
    destroy_vertex_book();
}

void test_sql_delete_vertex_with_condition() {
    init_vertex_book();
    init_vertex_person();
    init_edge_author();
    auto txn = Txn{*ctx, Txn::Mode::READ_WRITE};
    try {
        Record r1{}, r2{}, r3{};
        r1.set("title", "Harry Potter").set("pages", 456).set("price", 24.5);
        auto v1_1 = Vertex::create(txn, "books", r1);
        r1.set("title", "Fantastic Beasts").set("pages", 342).set("price", 21.0);
        auto v1_2 = Vertex::create(txn, "books", r1);
        r1.set("title", "Percy Jackson").set("pages", 800).set("price", 32.4);
        auto v1_3 = Vertex::create(txn, "books", r1);

        r2.set("name", "J.K. Rowlings").set("age", 32);
        auto v2_1 = Vertex::create(txn, "persons", r2);
        r2.set("name", "David Lahm").set("age", 29);
        auto v2_2 = Vertex::create(txn, "persons", r2);

        r3.set("time_used", 365U);
        auto e1 = Edge::create(txn, "authors", v1_1, v2_1, r3);
        r3.set("time_used", 180U);
        auto e2 = Edge::create(txn, "authors", v1_2, v2_1, r3);
        r3.set("time_used", 430U);
        auto e3 = Edge::create(txn, "authors", v1_3, v2_2, r3);

        SQL::execute(txn, "DELETE VERTEX persons WHERE name='J.K. Rowlings'");

        auto record = Db::getRecord(txn, v2_1);
        assert(record.empty());
        record = Db::getRecord(txn, v1_1);
        assert(!record.empty());
        record = Db::getRecord(txn, v1_2);
        assert(!record.empty());
        record = Db::getRecord(txn, e1);
        assert(record.empty());
        record = Db::getRecord(txn, e2);
        assert(record.empty());

    } catch(const Error& ex) {
        std::cout << "\nError: " << ex.what() << std::endl;
        assert(false);
    }
    txn.commit();

    destroy_edge_author();
    destroy_vertex_person();
    destroy_vertex_book();
}

void test_sql_delete_edge_with_rid() {
    init_vertex_book();
    init_vertex_person();
    init_edge_author();

    auto txn = Txn{*ctx, Txn::Mode::READ_WRITE};
    try {
        Record r1{}, r2{}, r3{};
        r1.set("title", "Harry Potter").set("pages", 456).set("price", 24.5);
        auto v1 = Vertex::create(txn, "books", r1);
        r2.set("name", "J.K. Rowlings").set("age", 32);
        auto v2 = Vertex::create(txn, "persons", r2);
        r3.set("time_used", 365U);
        auto e1 = Edge::create(txn, "authors", v1, v2, r3);

        auto record = Db::getRecord(txn, e1);
        assert(record.get("time_used").toIntU() == 365U);

        SQL::execute(txn, "DELETE EDGE " + to_string(e1));

        auto res = Edge::get(txn, "authors");
        assert(res.size() == 0);
        Edge::destroy(txn, e1);

    } catch(const Error& ex) {
        std::cout << "\nError: " << ex.what() << std::endl;
        assert(false);
    }
    txn.commit();

    destroy_edge_author();
    destroy_vertex_person();
    destroy_vertex_book();
}

void test_sql_delete_edge_with_condition() {
    init_vertex_book();
    init_vertex_person();
    init_edge_author();

    auto txn = Txn{*ctx, Txn::Mode::READ_WRITE};
    try {
        Record r1{}, r2{}, r3{};
        r1.set("title", "Harry Potter").set("pages", 456).set("price", 24.5);
        auto v1 = Vertex::create(txn, "books", r1);
        r2.set("name", "J.K. Rowlings").set("age", 32);
        auto v2 = Vertex::create(txn, "persons", r2);
        r3.set("time_used", 365U);
        auto e1 = Edge::create(txn, "authors", v1, v2, r3);

        auto record = Db::getRecord(txn, e1);
        assert(record.get("time_used").toIntU() == 365U);

        SQL::execute(txn, "DELETE EDGE authors FROM (SELECT FROM books WHERE title='Harry Potter') TO (SELECT FROM persons WHERE name='J.K. Rowlings') WHERE time_used=365");

        auto res = Edge::get(txn, "authors");
        assert(res.size() == 0);
        Edge::destroy(txn, e1);

    } catch(const Error& ex) {
        std::cout << "\nError: " << ex.what() << std::endl;
        assert(false);
    }

    txn.commit();

    destroy_edge_author();
    destroy_vertex_person();
    destroy_vertex_book();
}

void test_sql_validate_property_type() {
    Txn txn = Txn(*ctx, Txn::Mode::READ_WRITE);

    SQL::execute(txn, "CREATE CLASS sql_valid_type IF NOT EXISTS EXTENDS VERTEX");
    SQL::execute(txn, "CREATE PROPERTY sql_valid_type.tiny IF NOT EXISTS TINYINT");
    SQL::execute(txn, "CREATE PROPERTY sql_valid_type.utiny IF NOT EXISTS UNSIGNED_TINYINT");
    SQL::execute(txn, "CREATE PROPERTY sql_valid_type.small IF NOT EXISTS SMALLINT");
    SQL::execute(txn, "CREATE PROPERTY sql_valid_type.usmall IF NOT EXISTS UNSIGNED_SMALLINT");
    SQL::execute(txn, "CREATE PROPERTY sql_valid_type.integer IF NOT EXISTS INTEGER");
    SQL::execute(txn, "CREATE PROPERTY sql_valid_type.uinteger IF NOT EXISTS UNSIGNED_INTEGER");
    SQL::execute(txn, "CREATE PROPERTY sql_valid_type.bigint IF NOT EXISTS BIGINT");
    SQL::execute(txn, "CREATE PROPERTY sql_valid_type.ubigint IF NOT EXISTS UNSIGNED_BIGINT");
    SQL::execute(txn, "CREATE PROPERTY sql_valid_type.text IF NOT EXISTS TEXT");
    SQL::execute(txn, "CREATE PROPERTY sql_valid_type.real IF NOT EXISTS REAL");
    SQL::execute(txn, "CREATE PROPERTY sql_valid_type.blob IF NOT EXISTS BLOB");

    try {
        Record props{};
        int8_t tiny = INT8_MIN;
        uint8_t utiny = UINT8_MAX;
        int16_t small = SHRT_MIN;
        uint16_t usmall = USHRT_MAX;
        int32_t integer = INT32_MIN;
        uint32_t uinteger = UINT32_MAX;
        int64_t bigint = INT64_MIN;
        uint64_t ubigint = UINT64_MAX;
        string text = "hello world!";
        double real = 0.42;
        Coordinates blob(0.421, 0.842);

        props.set("tiny", tiny);
        props.set("utiny", utiny);
        props.set("small", small);
        props.set("usmall", usmall);
        props.set("integer", integer);
        props.set("uinteger", uinteger);
        props.set("bigint", bigint);
        props.set("ubigint", ubigint);
        props.set("text", text);
        props.set("real", real);
        props.set("blob", blob);
        Vertex::create(txn, "sql_valid_type", props);

        string sqlCreate = (string("CREATE VERTEX sql_valid_type ")
                      + "SET tiny=" + to_string(tiny)
                      + ", utiny=" + to_string(utiny)
                      + ", small=" + to_string(small)
                      + ", usmall=" + to_string(usmall)
                      + ", integer=" + to_string(integer)
                      + ", uinteger=" + to_string(uinteger)
                      + ", bigint=" + to_string(bigint)
                      + ", ubigint=" + to_string(ubigint)
                      + ", text='" + text + "'"
                      + ", real=" + to_string(real)
                      + ", blob=X'" + blob.toHex() + "'");
        SQL::execute(txn, sqlCreate);

        auto res = Vertex::get(txn, "sql_valid_type");
        assert(res.size() == 2);

        res = Vertex::get(txn, "sql_valid_type",
                                Condition("tiny").eq(tiny)
                                && Condition("utiny").eq(utiny)
                                && Condition("small").eq(small)
                                && Condition("usmall").eq(usmall)
                                && Condition("integer").eq(integer)
                                && Condition("uinteger").eq(uinteger)
                                && Condition("bigint").eq(bigint)
                                && Condition("ubigint").eq(ubigint)
                                && Condition("text").eq(text)
                                && Condition("real").eq(real)
                                && Condition("blob").eq(blob));
        assert(res.size() == 2);

        string sqlSelect = (string("SELECT * FROM sql_valid_type ")
                            + "WHERE tiny=" + to_string(tiny)
                            + " AND utiny=" + to_string(utiny)
                            + " AND small=" + to_string(small)
                            + " AND usmall=" + to_string(usmall)
                            + " AND integer=" + to_string(integer)
                            + " AND uinteger=" + to_string(uinteger)
                            + " AND bigint=" + to_string(bigint)
                            + " AND ubigint=" + to_string(ubigint)
                            + " AND text='" + text + "'"
                            + " AND real=" + to_string(real)
                            + " AND blob=X'" + blob.toHex() + "'");
        auto result = SQL::execute(txn, sqlSelect);
        assert(result.type() == result.RESULT_SET);
        assert(result.get<ResultSet>().size() == 2);
    } catch (const Error &e) {
        cout << "\nError: " << e.what() << endl;
        assert(false);
    }

    SQL::execute(txn, "DROP CLASS sql_valid_type IF EXISTS");
    txn.commit();
}

void test_sql_traverse() {
    Txn txn(*ctx, Txn::Mode::READ_WRITE);
    Class::create(txn, "V", ClassType::VERTEX);
    Property::add(txn, "V", "p", PropertyType::TEXT);
    Class::create(txn, "EL", ClassType::EDGE);
    Class::create(txn, "ER", ClassType::EDGE);

    try {
        auto v1 = Vertex::create(txn, "V", Record().set("p", "v1"));
        auto v21 = Vertex::create(txn, "V", Record().set("p", "v21"));
        auto v22 = Vertex::create(txn, "V", Record().set("p", "v22"));
        auto v31 = Vertex::create(txn, "V", Record().set("p", "v31"));
        auto v32 = Vertex::create(txn, "V", Record().set("p", "v32"));
        auto v33 = Vertex::create(txn, "V", Record().set("p", "v33"));
        auto e1_21 = Edge::create(txn, "EL", v1, v21);
        auto e1_22 = Edge::create(txn, "ER", v1, v22);
        auto e21_31 = Edge::create(txn, "EL", v21, v31);
        auto e21_32 = Edge::create(txn, "ER", v21, v32);
        auto e22_33 = Edge::create(txn, "EL", v22, v33);

        SQL::Result result = SQL::execute(txn, "TRAVERSE all() FROM " + to_string(v21));
        assert(result.type() == result.RESULT_SET);
        assert(equalResultSet(result.get<ResultSet>(), Traverse::allEdgeDfs(txn, v21, 0, UINT_MAX)));

        result = SQL::execute(txn, "TRAVERSE out() FROM " + to_string(v1));
        assert(result.type() == result.RESULT_SET);
        assert(equalResultSet(result.get<ResultSet>(), Traverse::outEdgeDfs(txn, v1, 0, UINT_MAX)));

        result = SQL::execute(txn, "TRAVERSE in() FROM " + to_string(v32));
        assert(result.type() == result.RESULT_SET);
        assert(equalResultSet(result.get<ResultSet>(), Traverse::inEdgeDfs(txn, v32, 0, UINT_MAX)));

        result = SQL::execute(txn, "TRAVERSE out('EL') FROM " + to_string(v1));
        assert(result.type() == result.RESULT_SET);
        assert(equalResultSet(result.get<ResultSet>(), Traverse::outEdgeDfs(txn, v1, 0, UINT_MAX, {"EL"})));

        result = SQL::execute(txn, "TRAVERSE in('ER') FROM " + to_string(v33) + " MINDEPTH 2");
        assert(result.type() == result.RESULT_SET);
        assert(equalResultSet(result.get<ResultSet>(), Traverse::inEdgeDfs(txn, v33, 2, UINT_MAX, {"ER"})));

        result = SQL::execute(txn, "TRAVERSE all('EL') FROM " + to_string(v21) + " MINDEPTH 1 MAXDEPTH 1 STRATEGY BREADTH_FIRST");
        assert(result.type() == result.RESULT_SET);
        assert(equalResultSet(result.get<ResultSet>(), Traverse::allEdgeBfs(txn, v21, 1, 1, {"EL"})));

        result = SQL::execute(txn, "SELECT p FROM (TRAVERSE out() FROM " + to_string(v1) + ") WHERE p = 'v22'");
        assert(result.type() == result.RESULT_SET);
        assert(result.get<ResultSet>().size() == 1);
        assert(result.get<ResultSet>()[0].record.getText("p") == "v22");
    } catch (const Error &e) {
        cout << "\nError: " << e.what() << endl;
        assert(false);
    }

    Class::drop(txn, "V");
    Class::drop(txn, "EL");
    Class::drop(txn, "ER");
    txn.commit();
}
