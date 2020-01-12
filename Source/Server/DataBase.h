#pragma once

#include "Common.h"

class DataBase
{
public:
    static const size_t IntValue = 0;
    static const size_t Int64Value = 1;
    static const size_t DoubleValue = 2;
    static const size_t BoolValue = 3;
    static const size_t StringValue = 4;
    static const size_t ArrayValue = 5;
    static const size_t DictValue = 6;

    using Array = vector<std::variant<int, int64, double, bool, string>>;
    using Dict = map<string, std::variant<int, int64, double, bool, string, Array>>;
    using Value = std::variant<int, int64, double, bool, string, Array, Dict>;
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
