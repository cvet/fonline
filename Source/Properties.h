#ifndef __PROPERTIES__
#define __PROPERTIES__

#define PROPERTIES_HEADER()                                                  \
    static PropertyRegistrator * PropertiesRegistrator;                      \
    static vector< pair< const char*, Property** > > PropertiesList;         \
    static void SetPropertyRegistrator( PropertyRegistrator * registrator ); \
    Properties                                       Props
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
    inline bool      Is ## prop() { uint data_size = 0; Property ## prop->GetRawData( this, data_size ); return data_size > 0; }
#define CLASS_PROPERTY_IMPL( class_name, prop )                                                    \
    Property * class_name::Property ## prop;                                                       \
    struct _ ## class_name ## Property ## prop ## Initializer                                      \
    {                                                                                              \
        _ ## class_name ## Property ## prop ## Initializer()                                       \
        {                                                                                          \
            class_name::PropertiesList.push_back( PAIR( # prop, &class_name::Property ## prop ) ); \
        }                                                                                          \
    } _ ## class_name ## Property ## prop ## Initializer

class Property;
typedef void ( *NativeCallback )( void* obj, Property* prop, void* cur_value, void* old_value );

struct UnresolvedProperty
{
    char*  Name;
    char*  TypeName;
    uchar* Data;
    uint   DataSize;
};
typedef vector< UnresolvedProperty* > UnresolvedPropertyVec;

class PropertyRegistrator;
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
        ModifiableMask       = 0x2200,
    };

    const char*    GetName();
    uint           GetRegIndex();
    int            GetEnumValue();
    AccessType     GetAccess();
    uint           GetBaseSize();
    asIObjectType* GetASObjectType();
    bool           IsPOD();
    bool           IsDict();
    bool           IsReadable();
    bool           IsWritable();
    void           SetSendIgnore( void* obj );
    bool           IsQuestValue();

    template< typename T >
    T GetValue( void* obj )
    {
        RUNTIME_ASSERT( sizeof( T ) == baseSize );
        T ret_value = 0;
        GenericGet( obj, &ret_value );
        return ret_value;
    }

    template< typename T >
    void SetValue( void* obj, T new_value )
    {
        RUNTIME_ASSERT( sizeof( T ) == baseSize );
        GenericSet( obj, &new_value );
    }

    uchar* GetRawData( void* obj, uint& data_size );
    void   SetRawData( void* obj, uchar* data, uint data_size );
    void   SetData( void* obj, uchar* data, uint data_size );
    int    GetPODValueAsInt( void* obj );
    void   SetPODValueAsInt( void* obj, int value );
    string SetGetCallback( const char* script_func );
    string AddSetCallback( const char* script_func );

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
    uchar* ExpandComplexValueData( void* base_ptr, uint& data_size, bool& need_delete );
    void   AddRefComplexValue( void* value );
    void   ReleaseComplexValue( void* value );
    void   GenericGet( void* obj, void* ret_value );
    void   GenericSet( void* obj, void* new_value );

    // Static data
    string         propName;
    string         typeName;
    DataType       dataType;
    asIObjectType* asObjType;
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
    bool           isReadable;
    bool           isWritable;
    bool           generateRandomValue;
    bool           setDefaultValue;
    bool           checkMinValue;
    bool           checkMaxValue;
    int64          defaultValue;
    int64          minValue;
    int64          maxValue;
    uint           questValue;

    // Dynamic data
    PropertyRegistrator* registrator;
    uint                 regIndex;
    int                  enumValue;
    uint                 podDataOffset;
    uint                 complexDataIndex;
    uint                 baseSize;
    uint                 getCallback;
    uint                 getCallbackArgs;
    UIntVec              setCallbacks;
    IntVec               setCallbacksArgs;
    bool                 setCallbacksAnyOldValue;
    NativeCallback       nativeSetCallback;
    NativeCallback       nativeSendCallback;
    bool                 getCallbackLocked;
    bool                 setCallbackLocked;
    void*                sendIgnoreObj;
};
typedef vector< Property* > PropertyVec;

class Properties
{
    friend class PropertyRegistrator;
    friend class Property;

public:
    Properties( PropertyRegistrator* reg, bool* obj_is_destroyed );
    ~Properties();
    Properties& operator=( const Properties& other );
    void*       FindData( const char* property_name );
    uint        StoreData( bool with_protected, PUCharVec** all_data, UIntVec** all_data_sizes );
    void        RestoreData( PUCharVec& all_data, UIntVec& all_data_sizes );
    void        RestoreData( UCharVecVec& all_data );
    void        Save( void ( * save_func )( void*, size_t ) );
    bool        Load( void* file, uint version );
    bool        LoadFromText( const char* text, hash* pid = NULL );
    int         GetValueAsInt( int enum_value );
    void        SetValueAsInt( int enum_value, int value );
    bool        SetValueAsIntByName( const char* enum_name, int value );
    uint        GetQuestStr( uint property_index );

private:
    PropertyRegistrator*  registrator;
    uchar*                podData;
    PUCharVec             complexData;
    UIntVec               complexDataSizes;
    PUCharVec             storeData;
    UIntVec               storeDataSizes;
    UShortVec             storeDataComplexIndicies;
    UnresolvedPropertyVec unresolvedProperties;
    bool*                 objIsDestroyed;
};

class PropertyRegistrator
{
    friend class Properties;
    friend class Property;

public:
    PropertyRegistrator( bool is_server, const char* class_name );
    ~PropertyRegistrator();
    bool      Init();
    Property* Register( const char* type_name, const char* name, Property::AccessType access, uint quest_value = 0, const char* group = NULL, bool* generate_random_value = NULL, int64* default_value = NULL, int64* min_value = NULL, int64* max_value = NULL );
    void      SetDefaults( const char* group = NULL, bool* generate_random_value = NULL, int64* default_value = NULL, int64* min_value = NULL, int64* max_value = NULL );
    void      FinishRegistration();
    uint      GetCount();
    Property* Find( const char* property_name );
    Property* FindByEnum( int enum_value );
    Property* Get( uint property_index );
    void      SetNativeSetCallback( const char* property_name, NativeCallback callback );
    void      SetNativeSendCallback( NativeCallback callback );
    uint      GetWholeDataSize();

private:
    bool                        registrationFinished;
    bool                        isServer;
    string                      scriptClassName;
    PropertyVec                 registeredProperties;
    string                      enumTypeName;
    map< string, ScriptArray* > enumGroups;

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
    bool*  defaultGenerateRandomValue;
    int64* defaultDefaultValue;
    int64* defaultMinValue;
    int64* defaultMaxValue;
};

#endif // __PROPERTIES__
