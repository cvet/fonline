#ifndef __PROPERTIES__
#define __PROPERTIES__

#define PROPERTIES_HEADER()                                          \
    static PropertyRegistrator * PropertiesRegistrator;              \
    static vector< pair< const char*, Property** > > PropertiesList; \
    static void SetPropertyRegistrator( PropertyRegistrator * registrator )
#define PROPERTIES_IMPL( class_name )                                             \
    PropertyRegistrator * class_name::PropertiesRegistrator;                      \
    vector< pair< const char*, Property** > > class_name::PropertiesList;         \
    void class_name::SetPropertyRegistrator( PropertyRegistrator * registrator )  \
    {                                                                             \
        SAFEDEL( PropertiesRegistrator );                                         \
        PropertiesRegistrator = registrator;                                      \
        PropertiesRegistrator->FinishRegistration();                              \
        for( auto it = PropertiesList.begin(); it != PropertiesList.end(); it++ ) \
        {                                                                         \
            *it->second = PropertiesRegistrator->Find( it->first );               \
            RUNTIME_ASSERT_STR( *it->second, it->first );                         \
        }                                                                         \
    }

#define CLASS_PROPERTY( prop_type, prop )                                                                       \
    static Property * Property ## prop;                                                                         \
    inline prop_type Get ## prop() { return Property ## prop->GetValue< prop_type >( this ); }                  \
    inline void      Set ## prop( prop_type value ) { Property ## prop->SetValue< prop_type >( this, value ); } \
    inline bool      IsNonEmpty ## prop() { uint data_size = 0; Property ## prop->GetRawData( this, data_size ); return data_size > 0; }
#define CLASS_PROPERTY_IMPL( class_name, prop )                                                    \
    Property * class_name::Property ## prop;                                                       \
    struct _ ## class_name ## Property ## prop ## Initializer                                      \
    {                                                                                              \
        _ ## class_name ## Property ## prop ## Initializer()                                       \
        {                                                                                          \
            class_name::PropertiesList.push_back( PAIR( # prop, &class_name::Property ## prop ) ); \
        }                                                                                          \
    } _ ## class_name ## Property ## prop ## Initializer
#define CLASS_PROPERTY_ALIAS( prop_type, prop ) \
    prop_type Get ## prop();                    \
    void Set ## prop( prop_type value );
#define CLASS_PROPERTY_ALIAS_IMPL( base_class_name, class_name, prop_type, prop )                                        \
    prop_type base_class_name::Get ## prop() { return Props.GetPropValue< prop_type >( class_name::Property ## prop ); } \
    void base_class_name::Set ## prop( prop_type value ) { Props.SetPropValue< prop_type >( class_name::Property ## prop, value ); }

extern string WriteValue( void* ptr, int type_id, asIObjectType* as_obj_type, bool* is_hashes, int deep );
extern void*  ReadValue( const char* value, int type_id, asIObjectType* as_obj_type, bool* is_hashes, int deep, void* pod_buf, bool& is_error );

class Entity;
class Property;
typedef void ( *NativeSendCallback )( Entity* entity, Property* prop );
typedef void ( *NativeCallback )( Entity* entity, Property* prop, void* cur_value, void* old_value );
typedef vector< NativeCallback > NativeCallbackVec;

class PropertyRegistrator;
class Properties;
class Property
{
    friend class PropertyRegistrator;
    friend class Properties;

public:
    enum AccessType
    {
        PrivateCommon        = 0x0010,
        PrivateClient        = 0x0020,
        PrivateServer        = 0x0040,
        Public               = 0x0100,
        PublicModifiable     = 0x0200,
        PublicFullModifiable = 0x0400,
        Protected            = 0x1000,
        ProtectedModifiable  = 0x2000,
        VirtualPrivateCommon = 0x0011,
        VirtualPrivateClient = 0x0021,
        VirtualPrivateServer = 0x0041,
        VirtualPublic        = 0x0101,
        VirtualProtected     = 0x1001,

        VirtualMask          = 0x000F,
        PrivateMask          = 0x00F0,
        PublicMask           = 0x0F00,
        ProtectedMask        = 0xF000,
        ClientOnlyMask       = 0x0020,
        ServerOnlyMask       = 0x0040,
        ModifiableMask       = 0x2600,
    };

    const char*    GetName();
    const char*    GetTypeName();
    uint           GetRegIndex();
    int            GetEnumValue();
    AccessType     GetAccess();
    uint           GetBaseSize();
    asIObjectType* GetASObjectType();
    bool           IsPOD();
    bool           IsDict();
    bool           IsHash();
    bool           IsEnum();
    bool           IsReadable();
    bool           IsWritable();
    bool           IsConst();

    template< typename T >
    T GetValue( Entity* entity )
    {
        RUNTIME_ASSERT( sizeof( T ) == baseSize );
        T ret_value = 0;
        GenericGet( entity, &ret_value );
        return ret_value;
    }

    template< typename T >
    void SetValue( Entity* entity, T new_value )
    {
        RUNTIME_ASSERT( sizeof( T ) == baseSize );
        GenericSet( entity, &new_value );
    }

    uchar* GetRawData( Entity* entity, uint& data_size );
    void   SetData( Entity* entity, uchar* data, uint data_size );
    int    GetPODValueAsInt( Entity* entity );
    void   SetPODValueAsInt( Entity* entity, int value );
    string SetGetCallback( const char* script_func );
    string AddSetCallback( const char* script_func, bool deferred );

private:
    enum DataType
    {
        POD,
        String,
        Array,
        Dict,
    };

    Property();
    void*  CreateComplexValue( uchar* data, uint data_size );
    uchar* ExpandComplexValueData( void* pvalue, uint& data_size, bool& need_delete );
    void   AddRefComplexValue( void* value );
    void   ReleaseComplexValue( void* value );
    void   GenericGet( Entity* entity, void* ret_value );
    void   GenericSet( Entity* entity, void* new_value );
    uchar* GetPropRawData( Properties* properties, uint& data_size );
    void   SetPropRawData( Properties* properties, uchar* data, uint data_size );

    // Static data
    string         propName;
    string         typeName;
    DataType       dataType;
    int            asObjTypeId;
    asIObjectType* asObjType;
    bool           isHash;
    bool           isHashSubType0;
    bool           isHashSubType1;
    bool           isHashSubType2;
    bool           isIntDataType;
    bool           isSignedIntDataType;
    bool           isFloatDataType;
    bool           isBoolDataType;
    bool           isEnumDataType;
    bool           isArrayOfString;
    bool           isDictOfString;
    bool           isDictOfArray;
    bool           isDictOfArrayOfString;
    AccessType     accessType;
    bool           isConst;
    bool           isReadable;
    bool           isWritable;
    bool           checkMinValue;
    bool           checkMaxValue;
    int64          minValue;
    int64          maxValue;

    // Dynamic data
    PropertyRegistrator* registrator;
    uint                 regIndex;
    uint                 getIndex;
    int                  enumValue;
    uint                 podDataOffset;
    uint                 complexDataIndex;
    uint                 baseSize;
    uint                 getCallback;
    uint                 getCallbackArgs;
    UIntVec              setCallbacks;
    IntVec               setCallbacksArgs;
    BoolVec              setCallbacksDeferred;
    bool                 setCallbacksAnyNewValue;
    NativeCallback       nativeSetCallback;
    NativeSendCallback   nativeSendCallback;
};
typedef vector< Property* > PropertyVec;

class Properties
{
    friend class PropertyRegistrator;
    friend class Property;

public:
    Properties( PropertyRegistrator* reg );
    Properties( const Properties& other );
    ~Properties();
    Properties& operator=( const Properties& other );
    Property*   FindByEnum( int enum_value );
    void*       FindData( const char* property_name );
    uint        StoreData( bool with_protected, PUCharVec** all_data, UIntVec** all_data_sizes );
    void        RestoreData( PUCharVec& all_data, UIntVec& all_data_sizes );
    void        RestoreData( UCharVecVec& all_data );
    bool        LoadFromText( const StrMap& key_values );
    void        SaveToText( StrMap& key_values, Properties* base );
    bool        LoadPropertyFromText( Property* prop, const char* value );
    string      SavePropertyToText( Property* prop );
    static int  GetValueAsInt( Entity* entity, int enum_value );
    static void SetValueAsInt( Entity* entity, int enum_value, int value );
    static bool SetValueAsIntByName( Entity* entity, const char* enum_name, int value );
    static bool SetValueAsIntProps( Properties* props, int enum_value, int value );
    string      GetClassName();
    void        SetSendIgnore( Property* prop, Entity* entity );

    template< typename T >
    T GetPropValue( Property* prop )
    {
        RUNTIME_ASSERT( sizeof( T ) == prop->baseSize );
        uint   data_size;
        uchar* data = prop->GetPropRawData( this, data_size );
        T      result;
        if( prop->dataType != Property::DataType::POD )
        {
            void* p = prop->CreateComplexValue( data, data_size );
            memcpy( &result, &p, sizeof( T ) );
        }
        else
        {
            memcpy( &result, data, sizeof( T ) );
        }
        return result;
    }

    template< typename T >
    void SetPropValue( Property* prop, T value )
    {
        RUNTIME_ASSERT( sizeof( T ) == prop->baseSize );
        if( prop->dataType != Property::DataType::POD )
        {
            bool   need_delete = false;
            uint   data_size;
            uchar* data = prop->ExpandComplexValueData( &value, data_size, need_delete );
            prop->SetPropRawData( this, data, data_size );
            if( need_delete )
                delete[] data;
        }
        else
        {
            prop->SetPropRawData( this, (uchar*) &value, sizeof( T ) );
        }
    }

private:
    Properties();

    PropertyRegistrator* registrator;
    uchar*               podData;
    PUCharVec            complexData;
    UIntVec              complexDataSizes;
    PUCharVec            storeData;
    UIntVec              storeDataSizes;
    UShortVec            storeDataComplexIndicies;
    StrMap               unresolvedProperties;
    bool*                getCallbackLocked;
    Entity*              sendIgnoreEntity;
    Property*            sendIgnoreProperty;
};

class PropertyRegistrator
{
    friend class Properties;
    friend class Property;

public:
    PropertyRegistrator( bool is_server, const char* class_name );
    ~PropertyRegistrator();
    bool      Init();
    Property* Register( const char* type_name, const char* name, Property::AccessType access, bool is_const, const char* group = nullptr, int64* min_value = nullptr, int64* max_value = nullptr );
    void      SetDefaults( const char* group = nullptr, int64* min_value = nullptr, int64* max_value = nullptr );
    void      FinishRegistration();
    uint      GetCount();
    Property* Find( const char* property_name );
    Property* FindByEnum( int enum_value );
    Property* Get( uint property_index );
    void      SetNativeSetCallback( const char* property_name, NativeCallback callback );
    void      SetNativeSendCallback( NativeSendCallback callback );
    uint      GetWholeDataSize();
    string    GetClassName();

    static NativeCallbackVec GlobalSetCallbacks;

private:
    bool                        registrationFinished;
    bool                        isServer;
    string                      scriptClassName;
    PropertyVec                 registeredProperties;
    string                      enumTypeName;
    map< string, ScriptArray* > enumGroups;
    uint                        getPropertiesCount;

    // POD info
    uint      wholePodDataSize;
    BoolVec   publicPodDataSpace;
    BoolVec   protectedPodDataSpace;
    BoolVec   privatePodDataSpace;
    PUCharVec podDataPool;

    // Complex types info
    uint      complexPropertiesCount;
    UShortVec publicComplexDataProps;
    UShortVec protectedComplexDataProps;
    UShortVec publicProtectedComplexDataProps;
    UShortVec privateComplexDataProps;

    // Option defaults
    char*  defaultGroup;
    int64* defaultMinValue;
    int64* defaultMaxValue;
};

#endif // __PROPERTIES__
