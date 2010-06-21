#ifndef _PERKS_H_
#define _PERKS_H_

class CCritter;

namespace Perks
{
	extern bool Disabled[0x100];
	extern int ScriptBindId[0x100];
	extern DWORD Range[0x100];
	extern DWORD Level[0x100];

	void Clear();
	bool Check(CCritter* cr, BYTE perk, DWORD flags);

#ifdef FONLINE_SERVER
	void Up(CCritter* cr, BYTE perk, DWORD perk_flags, bool force);
	void Down(CCritter* cr, BYTE perk);
	void CheckValid(CCritter* cr);
#endif
}

#endif //_PERKS_H_