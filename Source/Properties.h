#ifndef __PROPERTIES__
#define __PROPERTIES__

#include "Common.h"

struct Property;
typedef void ( *NativeCallback )( void* obj, Property* prop, void* cur_value, void* old_value );

struct UnresolvedProperty
{
    string Name;
    uint   Size;
    uint64 Value;
};
typedef vector< UnresolvedProperty* > UnresolvedPropertyVec;

class PropertyAccessor
{
    friend class PropertyRegistrator;

public:
    void* GetData( void* obj, uint& data_size );
    void  SetData( void* obj, void* data, uint data_size, bool call_callbacks );
    void  SetSendIgnore( void* obj );

private:
    PropertyAccessor( uint index );

    void  GenericGet( void* obj, void* ret_value );
    void  GenericSet( void* obj, void* new_value );
    void* GetComplexData( Property* prop, void* base_data, uint& data_size );

    template< typename T >
    T Get( void* obj )
    {
        T ret_value = 0;
        GenericGet( obj, &ret_value );
        return ret_value;
    }

    template< typename T >
    void Set( void* obj, T new_value )
    {
        GenericSet( obj, &new_value );
    }

    uint  propertyIndex;
    bool  getCallbackLocked;
    bool  setCallbackLocked;
    void* sendIgnoreObj;
};

struct Property
{
    enum AccessType
    {
        Virtual             = 0x0001,
        VirtualClient       = 0x0002,
        VirtualServer       = 0x0004,
        Private             = 0x0010,
        PrivateClient       = 0x0020,
        PrivateServer       = 0x0040,
        Public              = 0x0100,
        PublicModifiable    = 0x0200,
        Protected           = 0x1000,
        ProtectedModifiable = 0x2000,

        VirtualMask         = 0x000F,
        PrivateMask         = 0x00F0,
        PublicMask          = 0x0F00,
        ProtectedMask       = 0xF000,
        ClientMask          = 0x0022,
        ServerMask          = 0x0044,
        ModifiableMask      = 0x2200,
    };

    enum DataType
    {
        POD,
        String,
        Array,
        // Todo: Dict
    };

    // Static data
    string            Name;
    string            TypeName;
    DataType          Type;
    AccessType        Access;
    asIObjectType*    ComplexDataSubType;
    bool              GenerateRandomValue;
    bool              SetDefaultValue;
    bool              CheckMinValue;
    bool              CheckMaxValue;
    int64             DefaultValue;
    int64             MinValue;
    int64             MaxValue;

    // Dynamic data
    uint              Index;
    uint              ComplexTypeIndex;
    uint              Offset;
    uint              Size;
    PropertyAccessor* Accessor;
    int               GetCallback;
    IntVec            SetCallbacks;
    NativeCallback    NativeSetCallback;
    NativeCallback    NativeSendCallback;
};
typedef vector< Property* > PropertyVec;

class PropertyRegistrator;
class Properties
{
    friend class PropertyRegistrator;
    friend class PropertyAccessor;

public:
    Properties( PropertyRegistrator* reg );
    ~Properties();
    Properties& operator=( const Properties& other );
    void*       FindData( const char* property_name );
    uint        StoreData( bool with_protected, PUCharVec** data, UIntVec** data_sizes );
    void        RestoreData( bool with_protected, const UCharVecVec& data );
    void        Save( void ( * save_func )( void*, size_t ) );
    void        Load( void* file, uint version );

private:
    PropertyRegistrator*  registrator;
    uchar*                podData;
    PtrVec                complexData;
    PUCharVec             storeData;
    UIntVec               storeDataSizes;
    UnresolvedPropertyVec unresolvedProperties;
};

class PropertyRegistrator
{
    friend class Properties;
    friend class PropertyAccessor;

public:
    PropertyRegistrator( bool is_server, const char* class_name );
    ~PropertyRegistrator();
    Property* Register( const char* type_name, const char* name, Property::AccessType access, bool generate_random_value = false, int64* default_value = NULL, int64* min_value = NULL, int64* max_value = NULL );
    void      FinishRegistration();
    Property* Find( const char* property_name );
    Property* Get( uint property_index );
    string    SetGetCallback( const char* property_name, const char* script_func );
    string    AddSetCallback( const char* property_name, const char* script_func );
    void      SetNativeSetCallback( const char* property_name, NativeCallback callback );
    void      SetNativeSendCallback( NativeCallback callback );
    uint      GetWholeDataSize();

private:
    bool        registrationFinished;
    bool        isServer;
    string      scriptClassName;
    PropertyVec registeredProperties;

    // POD info
    uint      wholePodDataSize;
    BoolVec   publicPodDataSpace;
    BoolVec   protectedPodDataSpace;
    BoolVec   privatePodDataSpace;
    PUCharVec podDataPool;

    // Complex types info
    uint complexPropertiesCount;
};

#endif // __PROPERTIES__
