#pragma once

#include "Common.h"

#include "DataBase.h"
#include "Properties.h"

class PropertiesSerializator : public StaticClass
{
public:
    static DataBase::Document SaveToDbDocument(Properties* props, Properties* base);
    static bool LoadFromDbDocument(Properties* props, const DataBase::Document& doc);
    static DataBase::Value SavePropertyToDbValue(Properties* props, Property* prop);
};
