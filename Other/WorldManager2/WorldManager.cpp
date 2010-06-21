//---------------------------------------------------------------------------

#include <vcl.h>
#pragma hdrstop

#include "WorldManager.h"
//---------------------------------------------------------------------------
#pragma package(smart_init)
#pragma resource "*.dfm"
TMainForm *MainForm;
//---------------------------------------------------------------------------
#define CFG_FILE_APP_NAME 		"Options"
#define CFG_FILE 				".\\WorldManager.cfg"
AnsiString GetString(const char* app, const char* key, const char* def_val, const char* fname)
{
	char buf[2048];
	GetPrivateProfileString(app,key,def_val,buf,2047,fname);
	return AnsiString(buf);
}
//---------------------------------------------------------------------------
int GetInt(const char* app, const char* key, int def_val, const char* fname)
{
	return GetPrivateProfileInt(app,key,def_val,fname);
}
//---------------------------------------------------------------------------
void SetString(const char* app, const char* key, const char* val, const char* fname)
{
	WritePrivateProfileString(app,key,val,fname);
}
//---------------------------------------------------------------------------
void SetInt(const char* app, const char* key, int val, const char* fname)
{
	char buf[32];
	itoa(val,buf,10);
	WritePrivateProfileString(app,key,buf,fname);
}
//---------------------------------------------------------------------------
__fastcall TMainForm::TMainForm(TComponent* Owner)
	: TForm(Owner)
{
	MapF2=new TPicture();
    MapF2->LoadFromFile(GetString(CFG_FILE_APP_NAME,"MapImage","",CFG_FILE));

    MeetX=-1;MeetY=-1;
    IsPressed=false;
	PressX=0;PressY=0;
	CoordX=0;CoordY=0;
	MapX=0;MapY=0;

	AnsiString str=GetString(CFG_FILE_APP_NAME,"DefaultMask","",CFG_FILE);
    if(!str.IsEmpty()) LoadMasks(str);
    str=GetString(CFG_FILE_APP_NAME,"Cities","",CFG_FILE);
    if(!str.IsEmpty()) LoadCities(str);
//    str=GetString("Maps","");
//    if(!str.IsEmpty()) ;
//    str=GetString("Relief","");
//    if(!str.IsEmpty()) ;
	str=GetString(CFG_FILE_APP_NAME,"GenerateWorld","",CFG_FILE);
    if(!str.IsEmpty()) LoadWorld(str);

    DrawMap();
}
//---------------------------------------------------------------------------
void TMainForm::DrawMap()
{
	TCanvas* map=ImgMap->Canvas;

    map->Brush->Color=(TColor)0xFFFFFF;
	map->Rectangle(-1,-1,ImgMap->Width+1,ImgMap->Height+1);
	map->Draw(MapX,MapY,MapF2->Graphic);
    for(int i=0;i<GM_MAXZONEX;i++) map->TextOutA(MapX+i*GM_ZONE_LEN+GM_ZONE_LEN/2,MapY-20,AnsiString(i));
    for(int i=0;i<GM_MAXZONEY;i++) map->TextOutA(MapX-20,MapY+i*GM_ZONE_LEN+GM_ZONE_LEN/2,AnsiString(i));

    if(CbShowLocations->Checked)
    {
    	map->Brush->Color=(TColor)0x00FF00;
    	for(int i=0;i<(int)World.size();i++)
        {
        	WorldCity& w=World[i];
            City* city=GetCity(w.CityNum);
            if(!city) continue;
            int px=MapX+w.ZoneX;
            int py=MapY+w.ZoneY;
        	map->Ellipse(px+city->Size,py+city->Size,px-city->Size,py-city->Size);
            map->TextOutA(px,py,city->Name);
        }
    }

    if(Pages->TabIndex==TAB_MASKS)
    {
    	map->Brush->Color=(TColor)0xFFFFFF;
    	int mask_cnt=0;
        for(int i=0;i<(int)Masks.size();i++)
        {
            ZoneMask& mask=Masks[i];
            if(!mask.ToDraw) continue;
            int zx=GM_ZONE(-MapX);
            if(zx<0) zx=0;
            int zx_=zx+GM_ZONE(ImgMap->Width)+2;
            if(zx_>GM_MAXZONEX) zx_=GM_MAXZONEX;
            int zy=GM_ZONE(-MapY);
            if(zy<0) zy=0;
            int zy_=zy+GM_ZONE(ImgMap->Height)+2;
            if(zy_>GM_MAXZONEY) zx_=GM_MAXZONEY;
            for(int x=zx;x<zx_;x++)
            {
                for(int y=zy;y<zy_;y++)
                {
                    int proc=mask.GetProcent(x,y);
                    if(!proc) continue;
                    int px=MapX+x*GM_ZONE_LEN+GM_ZONE_LEN/2;
                    int py=MapY+y*GM_ZONE_LEN+GM_ZONE_LEN/2;
                    int w=GM_ZONE_LEN*proc/100/2;
                    //map->Rectangle(px,py+GM_ZONE_LEN-GM_ZONE_LEN*proc/100,px+GM_ZONE_LEN,py+GM_ZONE_LEN);
                    map->Ellipse(px+w,py+w,px-w,py-w);
                    if(CbShowProc->Checked) map->TextOutA(px,py,AnsiString(proc));
                }
            }
            map->TextOutA(0,mask_cnt*12,mask.Name);
            mask_cnt++;
        }
    }
    else if(Pages->TabIndex==TAB_CITIES)
    {
    	map->Brush->Color=(TColor)0xEEFFEE;
    	if(CbShowMeetGraph->Checked || CbShowMeetProc->Checked)
        {
            for(int zx=0;zx<GM_MAXZONEX;zx++)
            {
                for(int zy=0;zy<GM_MAXZONEY;zy++)
                {
                    ZoneMeetStat& ms=MeetStatistics[zx][zy];
                    if(ms.Procent<=0) continue;
                    int px=MapX+zx*GM_ZONE_LEN+GM_ZONE_LEN/2;
                    int py=MapY+zy*GM_ZONE_LEN+GM_ZONE_LEN/2;
                    int w=GM_ZONE_LEN*ms.Procent/TbMeetDiv->Position/2;
                    if(w>GM_ZONE_LEN/2) w=GM_ZONE_LEN/2;
                    if(CbShowMeetGraph->Checked) map->Ellipse(px+w,py+w,px-w,py-w);
                    if(CbShowMeetProc->Checked) map->TextOutA(px-20,py-6,AnsiString(ms.Procent).SetLength(7));
                }
            }
        }
        map->Brush->Color=(TColor)0xFFFFFF;
        for(int i=0;i<(int)MeetInfo.size();i++) map->TextOutA(0,i*12,MeetInfo[i]);
        if(MeetX>=0 && MeetX<GM_MAXZONEX && MeetY>=0 && MeetY<GM_MAXZONEY)
        {
        	for(int i=0;i<(int)MeetStatistics[MeetX][MeetY].FullData.size();i++)
            {
            	map->TextOutA(0,(MeetInfo.size()+i)*12,MeetStatistics[MeetX][MeetY].FullData[i]);
            }
        }
    }
}
//---------------------------------------------------------------------------
void __fastcall TMainForm::ImgMapMouseDown(TObject *Sender, TMouseButton Button, TShiftState Shift, int X, int Y)
{
	if(Button==mbLeft)
    {
        IsPressed=true;
        PressX=X;
        PressY=Y;
    }
}
//---------------------------------------------------------------------------
void __fastcall TMainForm::ImgMapMouseUp(TObject *Sender, TMouseButton Button, TShiftState Shift, int X, int Y)
{
	if(Button==mbLeft) IsPressed=false;
}
//---------------------------------------------------------------------------
void __fastcall TMainForm::ImgMapMouseMove(TObject *Sender, TShiftState Shift, int X, int Y)
{
	if(IsPressed)
    {
    	MapX+=X-PressX;
        MapY+=Y-PressY;
        PressX=X;
    	PressY=Y;
        DrawMap();
    }

    CoordX=X-MapX;
    CoordY=Y-MapY;
    LabelCoordX->Caption="CoordX: "+AnsiString(CoordX);
    LabelCoordY->Caption="CoordY: "+AnsiString(CoordY);
    LabelZoneX->Caption="ZoneX: "+AnsiString(GM_ZONE(CoordX));
    LabelZoneY->Caption="ZoneY: "+AnsiString(GM_ZONE(CoordY));

    if(Pages->TabIndex==TAB_CITIES && (GM_ZONE(CoordX)!=MeetX || GM_ZONE(CoordY)!=MeetY))
    {
       	MeetX=GM_ZONE(CoordX);
        MeetY=GM_ZONE(CoordY);
        DrawMap();
    }
}
//---------------------------------------------------------------------------
bool IsCurOnMap=false;
void __fastcall TMainForm::ImgMapMouseEnter(TObject *Sender)
{
	IsCurOnMap=true;
}
//---------------------------------------------------------------------------

void __fastcall TMainForm::ImgMapMouseLeave(TObject *Sender)
{
    IsCurOnMap=false;
}
//---------------------------------------------------------------------------
void __fastcall TMainForm::FormResize(TObject *Sender)
{
	ImgMap->Picture=NULL;
	DrawMap();
}
//---------------------------------------------------------------------------
void __fastcall TMainForm::BtnLoadMasksClick(TObject *Sender)
{
	TOpenDialog* dlg=new TOpenDialog((TComponent*)Sender);
	dlg->Filter="List file|*.lst";
	dlg->Options=(TOpenOptions)((1<<(int)ofNoChangeDir)|524292);
	if(!dlg->Execute())
    {
    	delete dlg;
        return;
    }

    LbMasks->Items->Clear();
    Masks.clear();
    LoadMasks(dlg->FileName);
    delete dlg;
}
//---------------------------------------------------------------------------
AnsiString MasksPath;
void TMainForm::LoadMasks(AnsiString path)
{
	LbMasks->Items->LoadFromFile(path);
    if(!LbMasks->Items->Count) return;

	for(int i=path.Length();i>0;i--)
    {
    	if(path[i]!='\\') continue;
        path.SetLength(i);
        break;
    }
    MasksPath=path;

    //MasksListPath=path;
    for(int i=0;i<LbMasks->Items->Count;i++)
    {
    	AnsiString line=LbMasks->Items->Strings[i];
        AnsiString name;
        int num=GetMaskNum(line,name);

        if(!num)
        {
        	if(name.IsEmpty() || name[1]!='#') LbMasks->Items->Strings[i]="# "+name+"   # Error: Incorrect Name!";
            continue;
        }
        if(GetMask(num))
        {
      	  	LbMasks->Items->Strings[i]="# "+name+"   # Error: Already Loaded!";
            continue;
        }

        Graphics::TBitmap* bmp=new Graphics::TBitmap();
        try{bmp->LoadFromFile(path+name+".bmp");}catch(...){delete bmp;bmp=NULL;}

        if(bmp && bmp->PixelFormat==pf24bit && bmp->Width==GM_MAXZONEX && bmp->Height==GM_MAXZONEY)
        {
           	Masks.push_back(ZoneMask(name,num,bmp));
        }
        else
        {
        	if(bmp) delete bmp;
            LbMasks->Items->Strings[i]="# "+name+"   # Error: File not found!";;
        }
    }

    DrawMap();
}
//---------------------------------------------------------------------------
void __fastcall TMainForm::BtnSaveMasksClick(TObject *Sender)
{
	TOpenDialog* dlg=new TSaveDialog((TComponent*)Sender);
	dlg->Filter="List file|*.lst";
	dlg->Options=(TOpenOptions)((1<<(int)ofNoChangeDir)|524292);
	if(!dlg->Execute())
    {
    	delete dlg;
        return;
    }

    AnsiString fname=dlg->FileName;
    if(!fname.AnsiPos(".lst")) fname=fname+".lst";

    AnsiString path=dlg->FileName;
    for(int i=path.Length();i>0;i--)
    {
    	if(path[i]!='\\') continue;
        path.SetLength(i);
        break;
    }

    LbMasks->Items->SaveToFile(fname);
    for(DWORD i=0;i<Masks.size();i++)
    {
    	ZoneMask& mask=Masks[i];
        mask.Bmp->SaveToFile(path+mask.Name+".bmp");
    }
}
//---------------------------------------------------------------------------
void __fastcall TMainForm::LbMasksDblClick(TObject *Sender)
{
	if(LbMasks->ItemIndex<0) return;
	AnsiString line=LbMasks->Items->Strings[LbMasks->ItemIndex];
    EditMask->Text=line;
    if(line.IsEmpty() || line[1]=='#') return;

    AnsiString name;
    ZoneMask* mask=GetMask(GetMaskNum(line,name));
    if(mask)
    {
    	if(!CbMaskMultiselect->Checked && !mask->ToDraw)
        {
           for(DWORD i=0;i<Masks.size();i++) Masks[i].ToDraw=false;
        }
    	mask->ToDraw=!mask->ToDraw;
        DrawMap();
    }
}
//---------------------------------------------------------------------------
int TMainForm::GetMaskNum(AnsiString& line, AnsiString& name)
{
	if(line.IsEmpty()) return 0;
    for(int i=1;i<=line.Length();i++)
    {
    	char c=line[i];
        if((c>='a' && c<='z') || (c>='A' && c<='Z') || (c>='0' && c<='9') || c=='_') continue;
        return 0;
    }
    return 1;
}
//---------------------------------------------------------------------------
TMainForm::ZoneMask* TMainForm::GetMask(int num)
{
	for(DWORD i=0;i<Masks.size();i++)
    {
        if(Masks[i].Num==num) return &Masks[i];
    }
    return NULL;
}
//---------------------------------------------------------------------------
void __fastcall TMainForm::FormMouseWheel(TObject *Sender, TShiftState Shift, int WheelDelta, TPoint &MousePos,
      bool &Handled)
{
	if(IsCurOnMap) Handled=true;
    else return;

	if(Pages->TabIndex==TAB_MASKS && Masks.size() && CoordX>=0 && CoordX<GM_MAXX && CoordY>=0 && CoordY<GM_MAXY)
    {
    	for(int i=(int)Masks.size()-1;i>=0;i--)
    	{
   		 	ZoneMask& mask=Masks[i];
   		    if(!mask.ToDraw) continue;
            int zx=GM_ZONE(CoordX);
            int zy=GM_ZONE(CoordY);
            int proc=mask.GetProcent(zx,zy);
            if(WheelDelta>0) proc+=TbMaskProcStep->Position;
            else proc-=TbMaskProcStep->Position;
            if(proc<0) proc=0;
    		if(proc>100) proc=100;
            mask.SetProcent(zx,zy,proc);
         	mask.Bmp->SaveToFile(MasksPath+mask.Name+".bmp");
            DrawMap();
            break;
		}
    }
}
//---------------------------------------------------------------------------
int TMainForm::ZoneMask::GetProcent(int zx, int zy)
{
 	int color=Bmp->Canvas->Pixels[zx][zy];
    color>>=16;
    color&=0xFF;
    return (0xFF-color)*100/0xFF;
}
//---------------------------------------------------------------------------
void TMainForm::ZoneMask::SetProcent(int zx, int zy, int proc)
{
	if(proc<0) proc=0;
    if(proc>100) proc=100;
	int color=((0xFF*(100-proc)/100)<<16)|0x00FFFF;
    Bmp->Canvas->Pixels[zx][zy]=(TColor)color;
}
//---------------------------------------------------------------------------
void __fastcall TMainForm::CbShowProcClick(TObject *Sender)
{
	DrawMap();
}
//---------------------------------------------------------------------------
void __fastcall TMainForm::PagesChange(TObject *Sender)
{
	DrawMap();
}
//---------------------------------------------------------------------------
void __fastcall TMainForm::BtnEditLineClick(TObject *Sender)
{
/*	if(!LbMasks->Items->Count) LbMasks->Items->Add("# New line");
	if(LbMasks->ItemIndex<0) return;
	AnsiString cur_name=LbMasks->Items->Strings[LbMasks->ItemIndex];
    AnsiString new_name=EditMask->Text;
    ZoneMask* cur_mask=GetMask(cur_name);

    if(IsValidMaskName(new_name))
    {
    //Change name of selected mask
        if(cur_mask) cur_mask->Name=new_name;
    //Create new
        else
        {
        	if(GetMask(new_name)) return;
        	Graphics::TBitmap* bmp=new Graphics::TBitmap();
            bmp->SetSize(GM_MAXZONEX,GM_MAXZONEY);
            bmp->PixelFormat=pf24bit;
            Masks.push_back(ZoneMask(new_name,bmp));
        }
    }
    else if(!new_name.IsEmpty() && new_name[1]=='#')
    {
    //Add new line if this last
    	if(LbMasks->ItemIndex==LbMasks->Items->Count-1)
        {
        	LbMasks->Items->Add("# New line");
        	return;
        }
    //Erase selected mask
        else if(cur_mask) Masks.erase(cur_mask);
    }
    else
    {
        return;
    }
    LbMasks->Items->Strings[LbMasks->ItemIndex]=new_name;
    DrawMap();*/
}
//---------------------------------------------------------------------------
AnsiString CitiesPath;
void TMainForm::LoadCities(AnsiString path)
{
	CitiesPath=path;
    for(int i=1;i<1000;i++)
    {
    	AnsiString app="Area "+AnsiString(i);
        if(GetString(app.c_str(),"map_0","",path.c_str()).IsEmpty()) continue;

        City city;
        city.Number=i;
        city.Name=GetString(app.c_str(),"name",app.c_str(),CitiesPath.c_str());
        city.StoreOnMove=GetInt(app.c_str(),"store_on_move",1,CitiesPath.c_str());
        city.Sneak=GetInt(app.c_str(),"sneak",0,CitiesPath.c_str());
        city.Size=GetInt(app.c_str(),"size",24,CitiesPath.c_str());
        city.EntranceCount=GetInt(app.c_str(),"entrance",1,CitiesPath.c_str());
        city.Encaunter=GetInt(app.c_str(),"encaunter",0,CitiesPath.c_str());
        city.AutoGarbage=GetInt(app.c_str(),"auto_garbage",1,CitiesPath.c_str());
        city.MeetChance=GetInt(app.c_str(),"meet_chance",1000,CitiesPath.c_str());
        city.MobGenerate=GetInt(app.c_str(),"mob_generate",100,CitiesPath.c_str());
        city.EnteredProto=GetInt(app.c_str(),"entered_count_proto",0,CitiesPath.c_str());
        city.EnteredCopy=GetInt(app.c_str(),"entered_count_copy",0,CitiesPath.c_str());
        city.EnteredGroups=GetInt(app.c_str(),"entered_groups",1,CitiesPath.c_str());
        city.EnteredPlayers=GetInt(app.c_str(),"entered_players",0,CitiesPath.c_str());
        for(int i=0;;i++)
        {
        	AnsiString key=AnsiString("map_")+i;
        	AnsiString map=GetString(app.c_str(),key.c_str(),"",CitiesPath.c_str());
            if(map.IsEmpty()) break;
            city.Maps.push_back(map);
        }
       	AnsiString key="zones";
       	AnsiString zones=GetString(app.c_str(),key.c_str(),"",CitiesPath.c_str());
        if(zones.IsEmpty()) break;
        for()
        {
        	city.Zones.push_back(zone);
        }

        Cities.push_back(city);
    }
    RefreshCitiesList();
}
//---------------------------------------------------------------------------
void TMainForm::RefreshCitiesList()
{
	LbCities->Items->Clear();
    for(int i=0;i<(int)Cities.size();i++)
    {
    	City& city=Cities[i];
    	if(city.IsCity() && CbShowCity->Checked) LbCities->Items->Add(city.Name);
        else if(city.IsEncaunter() && CbShowEncaunters->Checked) LbCities->Items->Add(city.Name);
        else if(!city.IsCity() && !city.IsEncaunter() && CbShowOtherCities->Checked) LbCities->Items->Add(city.Name);
    }
    UpdateEncauntersMeet(NULL);
}
//---------------------------------------------------------------------------
void __fastcall TMainForm::CbShowCityClick(TObject *Sender)
{
	RefreshCitiesList();
}
//---------------------------------------------------------------------------
TMainForm::City* TMainForm::GetCity(AnsiString ident)
{
	for(int i=0;i<(int)Cities.size();i++) if(Cities[i].Name==ident) return &Cities[i];
    return NULL;
}
//---------------------------------------------------------------------------
TMainForm::City* TMainForm::GetCity(int num)
{
	for(int i=0;i<(int)Cities.size();i++) if(Cities[i].Number==num) return &Cities[i];
    return NULL;
}
//---------------------------------------------------------------------------
void TMainForm::FillCityInfo(City* city)
{
	if(!city)
    {
    	while(VleCityInfo->RowCount>2) VleCityInfo->DeleteRow(2);
    	VleCityInfo->Cells[1][1]="None";
    }
    else
    {
        EditCityInfo("Number",AnsiString(city->Number),false);
        EditCityInfo("Name",city->Name,false);
        EditCityInfo("Store on Move",AnsiString(city->StoreOnMove),false);
        EditCityInfo("Sneak",AnsiString(city->Sneak),false);
        EditCityInfo("Size",AnsiString(city->Size),false);
        for(int i=0;i<(int)city->Maps.size();i++) EditCityInfo(AnsiString("Map ")+i,city->Maps[i],false);
        EditCityInfo("Entrance Count",AnsiString(city->EntranceCount),false);
        EditCityInfo("Is Encaunter",AnsiString(city->Encaunter),false);
        EditCityInfo("Auto Garbage",AnsiString(city->AutoGarbage),false);
        EditCityInfo("Meet Chance",AnsiString(city->MeetChance),false);
        EditCityInfo("Mob Generate",AnsiString(city->MobGenerate),false);
        EditCityInfo("Entered Proto",AnsiString(city->EnteredProto),false);
        EditCityInfo("Entered Copy",AnsiString(city->EnteredCopy),false);
        EditCityInfo("Entered Groups",AnsiString(city->EnteredGroups),false);
        EditCityInfo("Entered Players",AnsiString(city->EnteredPlayers),false);
        for(int i=0;i<(int)city->Zones.size();i++) EditCityInfo(AnsiString("Zone ")+i,city->Zones[i],false);
        UpdateEncauntersMeet(city);
    }
}
//---------------------------------------------------------------------------
AnsiString CityInfoDefStr;
void __fastcall TMainForm::VleCityInfoSelectCell(TObject *Sender, int ACol, int ARow, bool &CanSelect)
{
	if(ACol==0) CanSelect=false;
	else CityInfoDefStr=VleCityInfo->Cells[ACol][ARow];
}
//---------------------------------------------------------------------------
void TMainForm::EditCityInfo(AnsiString key, AnsiString value, bool edit)
{
	int row_id;
	VleCityInfo->FindRow(key,row_id);
    if(row_id<1) row_id=VleCityInfo->InsertRow(key,value,true);
    else VleCityInfo->Cells[1][row_id]=value;

    City* city=GetCity(VleCityInfo->Cells[1][1].ToInt());
    AnsiString app="Area "+VleCityInfo->Cells[1][1];
    bool update_meet=false;

#define INFO_EDIT_STR(var,key) do{if(edit){city->var=value;SetString(app.c_str(),key,city->var.c_str(),CitiesPath.c_str());}}while(0)
#define INFO_EDIT_INT(var,key,min) do{if(value.ToIntDef(-1)<(min)) value=#min; if(edit){city->var=value.ToInt();SetInt(app.c_str(),key,city->var,CitiesPath.c_str());}}while(0)
#define INFO_EDIT_YESNO(var,key) do{value=(value=="0"||value=="no"?"no":"yes"); if(edit){city->var=(value=="yes"?1:0);SetInt(app.c_str(),key,city->var,CitiesPath.c_str());}}while(0)
    if(key=="Name") INFO_EDIT_STR(Name,"name");
    else if(key=="Store on Move") INFO_EDIT_YESNO(StoreOnMove,"store_on_move");
    else if(key=="Sneak") INFO_EDIT_INT(Sneak,"sneak",0);
    else if(key=="Size") INFO_EDIT_INT(Size,"size",1);
    else if(key=="Entrance Count") INFO_EDIT_INT(EntranceCount,"entrance",1);
    else if(key=="Is Encaunter"){INFO_EDIT_YESNO(Encaunter,"encaunter");update_meet=true;}
    else if(key=="Auto Garbage") INFO_EDIT_YESNO(AutoGarbage,"auto_garbage");
    else if(key=="Meet Chance"){INFO_EDIT_INT(MeetChance,"meet_chance",0); update_meet=true;}
    else if(key=="Mob Generate") INFO_EDIT_INT(MobGenerate,"mob_generate",0);
    else if(key=="Entered Proto") INFO_EDIT_INT(EnteredProto,"entered_count_proto",0);
    else if(key=="Entered Copy") INFO_EDIT_INT(EnteredCopy,"entered_count_copy",0);
    else if(key=="Entered Groups") INFO_EDIT_INT(EnteredGroups,"entered_groups",0);
    else if(key=="Entered Players") INFO_EDIT_INT(EnteredPlayers,"entered_players",0);
	//else if(key.AnsiPos("Map")>0) INFO_EDIT_STR(EnteredPlayers,"entered_players",0);
//Read Only
    else
    {
    	if(edit) value=CityInfoDefStr;
    }

    VleCityInfo->Cells[1][row_id]=value;
    if(edit && update_meet) UpdateEncauntersMeet(city);
}
//---------------------------------------------------------------------------
void __fastcall TMainForm::LbCitiesDblClick(TObject *Sender)
{
	FillCityInfo(NULL);
	if(LbCities->ItemIndex<0) return;
    City* city=GetCity(LbCities->Items->Strings[LbCities->ItemIndex]);
    if(!city) return;
    FillCityInfo(city);
}
//---------------------------------------------------------------------------
void __fastcall TMainForm::VleCityInfoKeyPress(TObject *Sender, char &Key)
{
	if(Key==VK_RETURN) EditCityInfo(VleCityInfo->Cells[0][VleCityInfo->Row],VleCityInfo->Cells[1][VleCityInfo->Row],true);
}
//---------------------------------------------------------------------------
void __fastcall TMainForm::VleCityInfoValidate(TObject *Sender, int ACol, int ARow, const AnsiString KeyName,
      const AnsiString KeyValue)
{
	EditCityInfo(KeyName,KeyValue,true);
}
//---------------------------------------------------------------------------
void TMainForm::UpdateEncauntersMeet(City* base)
{
	MeetInfo.clear();
	MeetInfo.push_back("Info:");
	for(int zx=0;zx<GM_MAXZONEX;zx++)
    {
    	for(int zy=0;zy<GM_MAXZONEY;zy++)
    	{
        	ZoneMeetStat& meet=MeetStatistics[zx][zy];
            meet.Clear();
            meet.FullData.push_back(AnsiString("Zone ")+zx+" "+zy+".");
            CityPtrVec enc;
            IntVec enc_chance;
            int sum_chance=0;

        	for(int i=0;i<(int)Cities.size();i++)
    		{
            	City& city=Cities[i];
                if(!city.IsEncaunter()) continue;

                for(int j=0;j<(int)city.Zones.size();j++)
                {
                	AnsiString& mask_name=city.Zones[j];
                    ZoneMask* mask=GetMask(mask_name);
                    if(!mask)
                    {
                    	MeetInfo.push_back("Zone <"+mask_name+"> for encaunter <"+city.Name+"> not found.");
                        continue;
                    }
                    int proc=mask->GetProcent(zx,zy);
                    if(!proc) continue;
                    int chance=city.MeetChance*proc/100;
                    enc.push_back(&city);
                    enc_chance.push_back(chance);
                    sum_chance+=chance;
                    break;
                }
    		}

            for(int i=0;i<(int)enc.size();i++)
            {
            	City* city=enc[i];
                int chance=enc_chance[i];
            	double proc=double(chance)/double(sum_chance)*double(100);
                //if(_isnan(
                if(city==base) meet.Procent=proc;
                meet.FullData.push_back(city->Name+" - "+AnsiString(proc)+"%");
            }
    	}
    }
    DrawMap();
}
//---------------------------------------------------------------------------
void __fastcall TMainForm::TbMeetDivChange(TObject *Sender)
{
	DrawMap();
    LabelMeetDiv->Caption=AnsiString(TbMeetDiv->Position);
}
//---------------------------------------------------------------------------
void TMainForm::LoadWorld(AnsiString path)
{
	TStringList* f=new TStringList();
    f->LoadFromFile(path);
	World.clear();
    int zx,zy,num,gen;
    for(int i=0;i<f->Count;i++)
    {
    	if(std::sscanf(f->Strings[i].c_str(),"%d %d %d %d",&num,&zx,&zy,&gen)==4)
        {
        	WorldCity w;
            w.CityNum=num;
            w.ZoneX=zx;
            w.ZoneY=zy;
        	World.push_back(w);
        }
        else break;
    }
    delete f;
}
//---------------------------------------------------------------------------
