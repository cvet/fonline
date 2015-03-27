#ifndef __PROPERTIES__
#define __PROPERTIES__

#include "Common.h"

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

class Property
{
    friend class PropertyRegistrator;
    friend class Properties;

public:
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

    bool       IsPOD();
    uint       GetIndex();
    AccessType GetAccess();
    uint       GetBaseSize();
    uchar*     GetData( void* obj, uint& data_size );
    void       SetData( void* obj, uchar* data, uint data_size, bool call_callbacks );
    void       SetSendIgnore( void* obj );

private:
    enum DataType
    {
        POD,
        String,
        Array,
        // Todo: Dict
    };

    Property();
    void*  CreateComplexValue( uchar* data, uint data_size );
    uchar* ExpandComplexValueData( void* base_ptr, uint& data_size, bool& need_delete );
    void   GenericGet( void* obj, void* ret_value );
    void   GenericSet( void* obj, void* new_value );

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

    // Static data
    string         propName;
    string         typeName;
    DataType       dataType;
    AccessType     accessType;
    asIObjectType* complexDataSubType;
    bool           generateRandomValue;
    bool           setDefaultValue;
    bool           checkMinValue;
    bool           checkMaxValue;
    int64          defaultValue;
    int64          minValue;
    int64          maxValue;

    // Dynamic data
    uint           regIndex;
    uint           podDataOffset;
    uint           complexDataIndex;
    uint           baseSize;
    int            getCallback;
    IntVec         setCallbacks;
    NativeCallback nativeSetCallback;
    NativeCallback nativeSendCallback;
    bool           getCallbackLocked;
    bool           setCallbackLocked;
    void*          sendIgnoreObj;
};
typedef vector< Property* > PropertyVec;

class PropertyRegistrator;
class Properties
{
    friend class PropertyRegistrator;
    friend class Property;

public:
    Properties( PropertyRegistrator* reg );
    ~Properties();
    Properties& operator=( const Properties& other );
    void*       FindData( const char* property_name );
    uint        StoreData( bool with_protected, PUCharVec** all_data, UIntVec** all_data_sizes );
    void        RestoreData( bool with_protected, const UCharVecVec& all_data );
    void        Save( void ( * save_func )( void*, size_t ) );
    void        Load( void* file, uint version );

private:
    PropertyRegistrator*  registrator;
    uchar*                podData;
    PUCharVec             complexData;
    UIntVec               complexDataSizes;
    PUCharVec             storeData;
    UIntVec               storeDataSizes;
    UnresolvedPropertyVec unresolvedProperties;
};

class PropertyRegistrator
{
    friend class Properties;
    friend class Property;

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
    uint        serializedPropertiesCount;

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
