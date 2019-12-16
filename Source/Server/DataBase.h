#pragma once

#include "Common.h"
#include "mapbox/variant.hpp"

class DataBase
{
public:
    static const int IntValue = 0;
    static const int Int64Value = 1;
    static const int DoubleValue = 2;
    static const int BoolValue = 3;
    static const int StringValue = 4;
    static const int ArrayValue = 5;
    static const int DictValue = 6;

    using Array = vector<mapbox::util::variant<int, int64, double, bool, string>>;
    using Dict = map<string, mapbox::util::variant<int, int64, double, bool, string, Array>>;
    using Value = mapbox::util::variant<int, int64, double, bool, string, Array, Dict>;
    using Document = map<string, Value>;
    using Collection = map<uint, Document>;
    using Collections = map<string, Collection>;
    using RecordsState = map<string, set<uint>>;

private:
    bool changesStarted;
    Collections recordChanges;
    RecordsState newRecords;
    RecordsState deletedRecords;

protected:
    virtual Document GetRecord(const string& collection_name, uint id) = 0;
    virtual void InsertRecord(const string& collection_name, uint id, const Document& doc) = 0;
    virtual void UpdateRecord(const string& collection_name, uint id, const Document& doc) = 0;
    virtual void DeleteRecord(const string& collection_name, uint id) = 0;
    virtual void CommitRecords() = 0;

public:
    virtual ~DataBase() = default;
    virtual UIntVec GetAllIds(const string& collection_name) = 0;
    Document Get(const string& collection_name, uint id);

    void StartChanges();
    void Insert(const string& collection_name, uint id, const Document& doc);
    void Update(const string& collection_name, uint id, const string& key, const Value& value);
    void Delete(const string& collection_name, uint id);
    void CommitChanges();
};

extern DataBase* DbStorage;
extern DataBase* DbHistory;

DataBase* GetDataBase(const string& connection_info);
