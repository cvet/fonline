#ifndef __METHODS__
#define __METHODS__

class MethodRegistrator;
class Method
{
    friend class MethodRegistrator;

public:
    enum CallType
    {
        LocalServer  = 0x01,
        LocalClient  = 0x02,
        LocalCommon  = 0x04,
        RemoteServer = 0x10,
        RemoteClient = 0x20,

        ClientMask   = 0x26,
        ServerMask   = 0x15,
    };

    const char* GetDecl();
    uint        GetRegIndex();
    CallType    GetCallType();
    string      SetMainCallback( const char* script_func );
    string      AddWatcherCallback( const char* script_func );

private:
    Method();
    static void Wrap( asIScriptGeneric* gen );

    // Static data
    string   methodDecl;
    string   bindFunc;
    string   bindDecl;
    CallType callType;

    // Dynamic data
    MethodRegistrator* registrator;
    uint               regIndex;
    UIntVec            callbackBindIds;
};
typedef vector< Method* > MethodVec;

class MethodRegistrator
{
    friend class Method;

public:
    MethodRegistrator( bool is_server );
    ~MethodRegistrator();
    bool    Init();
    Method* Register( const char* class_name, const char* decl, const char* bind_func, Method::CallType call );
    bool    FinishRegistration();
    Method* Get( uint reg_index );

private:
    bool      registrationFinished;
    bool      isServer;
    MethodVec registeredMethods;
};

#endif // __METHODS__
