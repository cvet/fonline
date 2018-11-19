#include "Common.h"
#include "Mapper.h"
#include "Exception.h"
#include "wx/wx.h"

class FOnlineEditor: public wxApp
{
public:
    virtual bool OnInit() override
    {
        InitialSetup( argc, argv );

        // Threading
        Thread::SetCurrentName( "GUI" );

        // Exceptions
        CatchExceptions( "FOnlineEditor", FONLINE_VERSION );

        // Timer
        Timer::Init();

        // Logging
        LogToFile( "FOnlineEditor.log" );

        // Options
        GetClientOptions();

        return true;
    }
};

wxIMPLEMENT_APP( FOnlineEditor );
