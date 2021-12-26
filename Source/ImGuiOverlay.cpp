#include "ImGuiOverlay.h"

FOnline::Overlay *Overlayer;
FOnline::OverlayFlag Flag = { 0 };

const string TextMessage[] =
{
	"Error, overlay already init.",
	"Error, overlay finishing.",

	"Init overlay.",
	"Finish overlay."
};

namespace IndexMessage
{
	enum Enum
	{
		Error_AlreadyInit,
		Error_AlreadyFinishing,

		Info_Init,
		Info_Finish
	};
};

inline void Message( const IndexMessage::Enum index )
{
	WriteLog( "%s\n", TextMessage[ ( size_t )index ].c_str( ) );
}

inline void Assert( const IndexMessage::Enum index )
{
	Message( index );
	assert( TextMessage[ ( size_t )index ].c_str( ) );
}

using namespace FOnline;

Overlay * FOnline::GetOverlay( )
{
	if( Flag.IsFinish )
	{
		Assert( IndexMessage::Error_AlreadyFinishing );
		return nullptr;
	}

	if( Overlayer )
		return Overlayer;
	
	if( Flag.IsInit )
	{
		Assert( IndexMessage::Error_AlreadyInit );
		return nullptr;
	}
	Overlayer = CreateOverlay( );
	return Overlayer;
}

void FOnline::FinishOverlay( )
{
	if( Flag.IsFinish )
	{
		Assert( IndexMessage::Error_AlreadyFinishing );
		return;
	}

	Flag.IsFinish = true;
	Overlayer->Finish( );
	DestroyOverlay( Overlayer );
	Overlayer = nullptr;
}

FOnline::Overlay::Overlay( )
{
}

void FOnline::Overlay::Finish( )
{
}

OverlayFlag * FOnline::Overlay::GetFlag( )
{
	return &Flag;
}

void FOnline::Overlay::Init( )
{
}
