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

#ifndef __SQL_HPP_INCLUDED_
#define __SQL_HPP_INCLUDED_

#include <iostream>
#include <sstream>

#include "nogdb.h"

namespace nogdb {

    namespace sql_parser {

        using namespace std;

        /* Forward declaration */
        class Token;

        class PropertyType;

        class Bytes;

        class RecordDescriptor;

        class Record;

        class Result;

        class ResultSet;

        typedef set<RecordDescriptor> RecordDescriptorSet;

        class CreateEdgeArgs;

        class SelectArgs;

        /*
         * Each token coming out of the lexer is an instance of
         * this structure. Tokens are also used as part of an expression.
         *
         * Note if Token.z==NULL then Token.n and Token.t are undefined and
         * may contain random values. Do not make any assumptions about Token.n
         * and Token.t when Token.z==NULL.
         */
        class Token {
        public:
            const char *z;  /* Text of the token.  Not NULL-terminated! */
            int n;          /* Number of characters in this token */
            int t;          /* Token type ID */

            inline string toString() const {
                string s = string(this->z, this->n);
                return dequote(s);
            }

            inline string toRawString() const {
                return string(this->z, this->n);
            }

            Bytes toBytes() const;

        private:
            bool operator<(const Token &other) const;

            string &dequote(string &z) const;
        };

        enum class PropertyTypeExt {
            RESULT_SET
        };

        class PropertyType {
        public:
            PropertyType(const nogdb::PropertyType &t_) : t(BASE), base(t_) {}

            PropertyType(const PropertyTypeExt &t_) : t(EXTEND), ext(t_) {}

            bool operator==(const PropertyType &other) const;

            bool operator!=(const PropertyType &other) const;

            inline nogdb::PropertyType toBase() const {
                return this->t == BASE ? this->base : nogdb::PropertyType::UNDEFINED;
            }

        private:
            enum {
                BASE, EXTEND
            } t;
            union {
                nogdb::PropertyType base;
                PropertyTypeExt ext;
            };
        };

        class Bytes : public nogdb::Bytes {
        public:
            Bytes();

            template<typename T>
            Bytes(T data, PropertyType type_) : nogdb::Bytes(data), t(type_) {}

            Bytes(const unsigned char *data, size_t len, PropertyType type_);

            Bytes(nogdb::Bytes &&bytes_, PropertyType type_ = nogdb::PropertyType::UNDEFINED);

            Bytes(PropertyType type_);

            Bytes(ResultSet &&res);

            bool operator<(const Bytes &other) const;

            inline PropertyType type() const { return this->t; }

            inline ResultSet &results() const { return *this->r; }

            inline const nogdb::Bytes &getBase() const { return *this; }

        private:
            PropertyType t;
            shared_ptr<ResultSet> r{nullptr};
        };


        /* An inherited class of Bytes for construct with Token and operand '<'*/
        class RecordDescriptor : public nogdb::RecordDescriptor {
        public:
            using nogdb::RecordDescriptor::RecordDescriptor;

            RecordDescriptor() : nogdb::RecordDescriptor() {}

            RecordDescriptor(const Token &classID, const Token &positionID)
                    : nogdb::RecordDescriptor(stoi(string(classID.z, classID.n)),
                                              stoi(string(positionID.z, positionID.n))) {}

            RecordDescriptor(const nogdb::RecordDescriptor &recD) : nogdb::RecordDescriptor(recD) {}

            RecordDescriptor(nogdb::RecordDescriptor &&recD) : nogdb::RecordDescriptor(move(recD)) {}

            bool operator<(const RecordDescriptor &other) const {
                return this->rid < other.rid;
            }

            string toString() const {
                // format: "#<classID>:<posID>"
                return "#" + to_string(this->rid.first) + ":" + to_string(this->rid.second);
            }
        };


        class Record {
        public:
            Record() = default;

            Record(nogdb::Record &&rec);

            Record &set(const string &propName, const Bytes &value);

            Record &set(const string &propName, Bytes &&value);

            Record &set(pair<string, Bytes> &&prop);

            const map<string, Bytes> &getAll() const;

            Bytes get(const string &propName) const;

            bool empty() const;

            nogdb::Record toBaseRecord() const;

        private:
            map<string, Bytes> properties{};
        };


        class Result {
        public:
            Result() = default;

            Result(nogdb::Result &&result) : Result(move(result.descriptor), move(result.record)) {}

            Result(RecordDescriptor &&rid, Record &&record_) : descriptor(move(rid)), record(move(record_)) {}

            RecordDescriptor descriptor{};
            Record record{};

            nogdb::Result toBaseResult() const {
                return nogdb::Result(descriptor, record.toBaseRecord());
            }
        };

        class ResultSet : public vector<Result> {
        public:
            using vector<Result>::vector;
            using vector<Result>::insert;

            ResultSet();

            ResultSet(nogdb::ResultSet &&res);

            ResultSet(nogdb::ResultSetCursor &res, int skip, int limit);

            string descriptorsToString() const;
        };

        class Condition : public nogdb::Condition {
        public:
            Condition(const string &propName = "") : nogdb::Condition(propName) {}

            Condition(const nogdb::Condition &rhs) : nogdb::Condition(rhs) {}

            using nogdb::Condition::eq;

            Condition eq(const Bytes &value) const {
                if (!value.empty()) {
                    return this->eq(value.getBase());
                } else {
                    return this->null();
                }
            }
        };


        /* A class for arguments holder */
        template<class E>
        class Holder {
        public:
            Holder() : Holder(static_cast<E>(0)) {}

            Holder(E type_, shared_ptr<void> value_ = nullptr) : type(type_), value(value_) {}

            E type;
            shared_ptr<void> value;

            template<typename T>
            inline T &get() const {
                return *static_pointer_cast<T>(this->value);
            }

        protected:
        };

        enum class TargetType {
            NO_TARGET,
            CLASS,              // std::string
            RIDS,               // sql_parser::RecordDescriptorSet
            NESTED,             // sql_parser::SelectArgs
            NESTED_TRAVERSE,    // sql_paresr::TraverseArgs
        };
        enum class WhereType {
            NO_COND,
            CONDITION,  // nogdb::Condition
            MULTI_COND  // nogdb::MultiCondition
        };
        enum class ProjectionType {
            PROPERTY,   // std::string
            FUNCTION,   // sql_parser::Function
            METHOD,     // std::pair<sql_parser::Projection, sql_parser::Projection>
            ARRAY_SELECTOR, // std::pair<sql_parser::Projection, unsigned long>
            ALIAS,      // std::pair<sql_parser::Projection, string>
        };
        typedef Holder<TargetType> Target;  /* A classname, set of rid or nested select statement */
        typedef Holder<WhereType> Where;    /* A condition class, expression class or empty condition */
        typedef Holder<ProjectionType> Projection;

        class Function {
        public:
            enum class Id {
                UNDEFINE,
                COUNT,
                MIN, MAX,
                IN, IN_E, IN_V,
                OUT, OUT_E, OUT_V,
                BOTH, BOTH_E,
                EXPAND,
            };

            Function() = default;

            Function(const string &name, vector<Projection> &&args = {});

            string name;
            Id id;
            vector<Projection> args;

            Bytes execute(Txn &txn, const Result &input) const;

            Bytes executeGroupResult(const ResultSet &input) const;

            Bytes executeExpand(Txn &txn, ResultSet &input) const;

            bool isGroupResult() const;

            bool isWalkResult() const;

            bool isExpand() const;

            string toString() const;

        private:
            static Bytes count(const ResultSet &input, const vector<Projection> &args);

//            static Bytes min(const ResultSet &input, const vector<Projection> &args);
//            static Bytes max(const ResultSet &input, const vector<Projection> &args);
            static Bytes walkIn(Txn &txn, const Result &input, const vector<Projection> &args);

            static Bytes walkInEdge(Txn &txn, const Result &input, const vector<Projection> &args);

            static Bytes walkInVertex(Txn &txn, const Result &input, const vector<Projection> &args);

            static Bytes walkOut(Txn &txn, const Result &input, const vector<Projection> &args);

            static Bytes walkOutEdge(Txn &txn, const Result &input, const vector<Projection> &args);

            static Bytes walkOutVertex(Txn &txn, const Result &input, const vector<Projection> &args);

            static Bytes walkBoth(Txn &txn, const Result &input, const vector<Projection> &args);

            static Bytes walkBothEdge(Txn &txn, const Result &input, const vector<Projection> &args);

            static Bytes expand(Txn &txn, ResultSet &input, const vector<Projection> &args);

            static ClassFilter argsToClassFilter(const vector<Projection> &args);
        };

        /* An arguments for create edge statement */
        class CreateEdgeArgs {
        public:
            CreateEdgeArgs() = default;

            CreateEdgeArgs(const Token &name_, Target &&src_, Target &&dest_, nogdb::Record &&prop_)
                    : name(name_.toString()), src(move(src_)), dest(move(dest_)), prop(move(prop_)) {};

            string name;
            Target src;
            Target dest;
            nogdb::Record prop;
        };

        /* An arguments for select statement */
        class SelectArgs {
        public:
            SelectArgs() = default;

            SelectArgs(vector<Projection> &&proj, Target &&from_, Where &&where_, const string &group_, void *order_,
                       int skip_, int limit_)
                    : projections(move(proj)), from(move(from_)), where(move(where_)), group(group_), order(order_),
                      skip(skip_), limit(limit_) {}

            ~SelectArgs() = default;

            vector<Projection> projections;
            Target from;
            Where where;
            string group;
            void *order;
            int skip;       /* Number of records you want to skip from the start of the result-set. */
            int limit;      /* Maximum number of records in the result-set. */
        };

        /* An arguments for update statement */
        class UpdateArgs {
        public:
            UpdateArgs() = default;

            UpdateArgs(Target &&target_, nogdb::Record &&prop_, Where &&where_)
                    : target(move(target_)), prop(move(prop_)), where(move(where_)) {}

            Target target;
            nogdb::Record prop;
            Where where;
        };

        /* An arguments for delete vertex statement */
        class DeleteVertexArgs {
        public:
            DeleteVertexArgs() = default;

            DeleteVertexArgs(Target &&target_, Where &&where_) : target(move(target_)), where(move(where_)) {}

            Target target;
            Where where;
        };

        /* An arguments for delete edge statement */
        class DeleteEdgeArgs {
        public:
            DeleteEdgeArgs() = default;

            DeleteEdgeArgs(Target &&target_, Target &&from_, Target &&to_, Where &&where_)
                    : target(move(target_)), from(move(from_)), to(move(to_)), where(move(where_)) {}

            Target target;
            Target from;
            Target to;
            Where where;
        };

        /* An arguments for tarverse statement */
        class TraverseArgs {
        public:
            TraverseArgs() = default;

            string direction;
            set<string> filter;
            RecordDescriptor root;
            long long minDepth;
            long long maxDepth;
            string strategy;
        };
    };
}

#endif
