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
	static void FakeWrap( asIScriptGeneric* gen );

    // Static data
    string   methodDecl;
    string   bindFunc;
    string   bindDecl;
	string   fileName;
    CallType callType;

    // Dynamic data
    MethodRegistrator* registrator;
    uint               regIndex;
    uint               callbackBindId;
    UIntVec            watcherBindIds;
};
typedef vector< Method* > MethodVec;

class Methods
{
    friend class Method;

public:
    // void SetWatcher(asIScriptFunction* func, bool enable);

private:
    // MethodRegistrator* registrator;
    UIntVec watcherBindIds;
};

class MethodRegistrator
{
    friend class Method;

public:
    MethodRegistrator( bool is_server, const char* class_name );
    ~MethodRegistrator();
    bool    Init();
    Method* Register( const char* decl, const char* bind_func, Method::CallType call, const string& cur_file);
    bool    FinishRegistration();
    Method* Get( uint reg_index );

private:
    bool      registrationFinished;
    bool      isServer;
    string    scriptClassName;
    MethodVec registeredMethods;
};

#endif // __METHODS__
