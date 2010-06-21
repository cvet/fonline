//---------------------------------------------------------------------------

#include <vcl.h>
#pragma hdrstop

#include "template.h"
#include "pbar.h"

extern TfrmMDI * frmMDI;

void MapChange::SetChange(ChangeType change)
    {
        type = change;
        if (!pTileSet)
        {
            pTileSet    = new CTileSet(pHeader);
            pTileSet->CreateNewTileSet();
        }
        if (!pObjSet) pObjSet     = new CObjSet(pHeader);

        switch (change)
        {
            case LM_NONE:
                break;
            case LM_FLOOR:
                ChangeFloor();
                // Для экономии освобождаем память
                pObjSet->ClearObjSet();
                break;
            case LM_ROOF:
                ChangeRoof();
                // Для экономии освобождаем память
                pObjSet->ClearObjSet();
                break;
            case LM_OBJ:
                ChangeObjSet();
                break;
        }
    }

    void MapChange::Restore()
    {
        switch(type)
        {
            case LM_NONE:
                break;
            case LM_FLOOR:
                RestoreFloor();
                break;
            case LM_ROOF:
                RestoreRoof();
                break;
            case LM_OBJ:
                RestoreObjSet();
                break;
        }
    }

MapChange::~MapChange(void)
{
    _RELEASE(pTileSet);
    _RELEASE(pObjSet);
};


void MapChange::ChangeFloor(void)
{
    CTileSet * tile = pMap->GetTileSet();
    int x1, y1, x2, y2;
    x1 = tile->SelectedFloorX1;
    y1 = tile->SelectedFloorY1;
    x2 = tile->SelectedFloorX2;
    y2 = tile->SelectedFloorY2;

    tile->CopySelectedTo(pTileSet, x1, y1, COPY_FLOOR);
    pTileSet->SelectTiles(SELECT_FLOOR, x1, y1, x2, y2);
    ChangeHint        = "Отменить изменение FLOOR";
}

void MapChange::RestoreFloor(void)
{
    CTileSet * tile = pMap->GetTileSet();
    int x1, y1;
    x1 = pTileSet->SelectedFloorX1;
    y1 = pTileSet->SelectedFloorY1;

    pTileSet->CopySelectedTo(tile, x1, y1, COPY_FLOOR);
}

void MapChange::ChangeRoof(void)
{
    CTileSet * tile = pMap->GetTileSet();
    int x1, y1, x2, y2;
    x1 = tile->SelectedRoofX1;
    y1 = tile->SelectedRoofY1;
    x2 = tile->SelectedRoofX2;
    y2 = tile->SelectedRoofY2;

    tile->CopySelectedTo(pTileSet, x1, y1, COPY_ROOF);
    pTileSet->SelectTiles(SELECT_ROOF, x1 - 2, y1 - 3, x2 - 2, y2 - 3);
    ChangeHint        = "Отменить изменение ROOF";
}

void MapChange::RestoreRoof(void)
{
    CTileSet * tile = pMap->GetTileSet();
    int x1, y1;
    x1 = pTileSet->SelectedRoofX1;
    y1 = pTileSet->SelectedRoofY1;

    pTileSet->CopySelectedTo(tile, x1, y1, COPY_ROOF);
}

void MapChange::ChangeObjSet(void)
{
    CObjSet * obj = pMap->GetObjSet();
    pObjSet->ClearObjSet();
    obj->CopyTo(pObjSet, true);
    ChangeHint        = "Отменить изменение OBJECT";
}

void MapChange::RestoreObjSet(void)
{
    CObjSet * obj = pMap->GetObjSet();
    obj->ClearObjSet();
    pObjSet->CopyTo(obj, true);
    obj->ClearSelection();
}

MapChange::MapChange(CMap * map)
{
    pMap        = map;
    pHeader     = pMap->GetHeader();
    type        = LM_NONE;
    pTileSet    = NULL;
    pObjSet     = NULL;
};

CMap::CMap()
{
    pHeader            = NULL;
    level = 0;
    for (int i = 0; i < 3 ; i++)
    {
        pTileSet[i]    = NULL;
        pObjSet[i]     = NULL;
    }

    for (int i = 0; i < MAX_LM; i++)
        Changes[i] = NULL;
    pos     = -1;
    count   = 0;
    SaveState   = false;
//    ChangeHint = "";
}

CMap::~CMap()
{
    _RELEASE(pHeader);
    for (int i =0; i < 3; i++)
    {
        _RELEASE(pTileSet[i]);
        _RELEASE(pObjSet[i]);
    }

    for (int i = 0; i < MAX_LM; i++)
    {
        _RELEASE(Changes[i]);
    }
}

void CMap::SaveChange(ChangeType LM)
{
    if (pos == MAX_LM - 1)
    {
        MapChange * first = Changes[0];
        // Сдвигаем массив
        for (int i = 1; i < MAX_LM; i++)
            Changes[i-1] = Changes[i];
        Changes[MAX_LM - 1] = first;
    }else pos++;

    MapChange * lm = Changes[pos];
    if (!lm)
    {
        // Создаем чандж для этой карты.
        lm              = new MapChange(this);
        Changes[pos]    = lm;
    }
    // Сохраняем изменения.
    lm->SetChange(LM);
    for (int j = pos + 1; j < count; j++)
        Changes[j]->SetChange(LM_NONE);
    count     = pos + 1;
    SaveState = true;
    frmMDI->spbUndo->Enabled = true;
    frmMDI->spbUndo->Hint   = GetChangeHint();
}

void CMap::RestoreChange(RestoreType type)
{
    if (!count) return;
    switch (type)
    {
        case RT_BACK:
            if (pos < 0 || pos == count) return;
            Changes[pos]->Restore();
            pos--;
            break;
        case RT_FORWARD:
            if (pos >= count-1) return;
            pos++;
            Changes[pos]->Restore();
        break;
    }
    if (pos == -1)
    {
        frmMDI->spbUndo->Enabled = false;
        frmMDI->spbUndo->Hint = "";
    }
    frmMDI->spbUndo->Hint   = GetChangeHint();
}

void CMap::SaveToFile(String FileName)
{
    HANDLE h_out = CreateFile(FileName.c_str(),
		GENERIC_WRITE,
		FILE_SHARE_WRITE,
		NULL,
		CREATE_ALWAYS,
		FILE_ATTRIBUTE_NORMAL,
		NULL);
    if (h_out == INVALID_HANDLE_VALUE)
    {
		MessageDlg("Ошибка при сохранении карты. Не могу открыть файл", mtError, TMsgDlgButtons() << mbOK, 0);
		return;
	}
	bool res;
	pHeader->SaveToFile(h_out);
	pTileSet[level]->SaveToFile(h_out);
	res = pObjSet[level]->SaveToFile(h_out);
    if (!res)
    {
		MessageDlg("Ошибка при сохранении карты. Не удалось сохранить объекты!", mtError, TMsgDlgButtons() << mbOK, 0);
    }
    CloseHandle(h_out);
    SaveState = false;
}

/*void CMap::SetSize(WORD new_w, WORD new_h)
{
    if (new_w > FO_MAP_WIDTH || new_h >= FO_MAP_HEIGHT) return;
    pHeader->SetWidth(new_w);
    pHeader->SetHeight(new_h);
    pTileSet->Resize(new_w, new_h);
} */

void CMap::CreateNewMap(WORD width, WORD height)
{
    FileName = "noname.map";
    DirName  = pUtil->DataDir;
    FullFileName = DirName + "\\maps\\" + FileName;
    
    // Создаем заголовки
    pHeader = new CHeader();
    pHeader->SetVersion(FO_MAP_VERSION);
    pHeader->SetWidth(width);
    pHeader->SetHeight(height);
    level = 0;
    // создаем тайлы
    pTileSet[level] = new CTileSet(pHeader);
    // Выделяем память. Зануляем.
    pTileSet[level]->CreateNewTileSet();
    // Создаем мно-во объектов.
    pObjSet[level] = new CObjSet(pHeader);

    WorldX = (GetWidth() - 10) * 48;
    WorldY = 0;
    SaveState = true;
}

void CMap::SetFullFileName(String name)
{
    FullFileName = name;
    pUtil->GetFileName(name, &FileName, &DirName, &Name, &Ext);
}

// Name: LoadFromFilr
// Desc: Загрузка карты Fonline из Filename
bool CMap::LoadFromFile(String name)
{
    SetFullFileName(name);
    ULONG i;
    HANDLE h_map = CreateFile(name.c_str(),
	                              GENERIC_READ,
                               FILE_SHARE_READ,
                                          NULL,
                                 OPEN_EXISTING,
                         FILE_ATTRIBUTE_NORMAL,
                                          NULL);
    if (h_map == INVALID_HANDLE_VALUE)
    {
        pLog->LogX("Cannot open file.");
        MessageDlg("Не могу открыть файл с картой", mtError, TMsgDlgButtons() << mbOK, 0);
        return false;
    }

    frmMDI->OpenPBarForm();

    // Грузим заголовок
    frmMDI->frmPBar->NewTask("Loading map's data", "Load header ...", 0, 0, &frmMDI->iPos);
    Application->ProcessMessages();
    // Заголовок
    pHeader = new CHeader();
    // Загружаем заголовок
    pHeader->LoadFromFile(h_map);
    if (pHeader->bError)
    {
        pLog->LogX("Cannot load map's header.");
        MessageDlg("Не могу загрузить заголовок карты", mtError, TMsgDlgButtons() << mbOK, 0);
        CloseHandle(h_map);
        return false;
    }

    // Кол-во уровней на карте
    int lev = pHeader->GetLevels();
    for ( int i = 0; i < lev; i++)
    {
        // Грузим тайлы.
        frmMDI->frmPBar->NewTask("Map level " + String(i + 1), "Load tileset ...", 0, 0, &frmMDI->iPos);
        Application->ProcessMessages();
        pTileSet[i] = new CTileSet(pHeader);
        pTileSet[i]->LoadFromFile(h_map);
        if (pTileSet[i]->bError)
        {
            pLog->LogX("Cannot load map's tileset.");
            MessageDlg("Не могу загрузить область с тайлами", mtError, TMsgDlgButtons() << mbOK, 0);
            CloseHandle(h_map);
            return false;
        }
    }

    int nTotalObjs = 0;
    int nTotalRead = 0;

    for (int i = 0; i < lev; i++)
    {
         // Грузим объекты
        pObjSet[i] = new CObjSet(pHeader);
        // Если грузим карту оригинального фола
        if (pHeader->GetVersion() != FO_MAP_VERSION && i == 0)
        {
            // Позиционируем указатель файла на начало области с объектами
            if ((nTotalObjs = pObjSet[i]->FindStartObjects(h_map)) < 0 || pObjSet[i]->bError)
            {
                pLog->LogX("Cannot load map's object set.");
                MessageDlg("Не могу загрузить область с объектами", mtError, TMsgDlgButtons() << mbOK, 0);
                CloseHandle(h_map);
                return false;
            }
            frmMDI->frmPBar->NewTask("Map level " + String(i + 1), "Load objectset ...", 0, nTotalObjs, &frmMDI->iPos);
            Application->ProcessMessages();
        }

        // Считываем облась объектов из файла
        nTotalRead += pObjSet[i]->LoadFromFile(h_map);
        if (pObjSet[i]->bError)
        {
            pLog->LogX("Cannot load map's object set.");
            MessageDlg("Не могу загрузить область с объектами", mtError, TMsgDlgButtons() << mbOK, 0);
            CloseHandle(h_map);
            return false;
        }
    }

    // Для Fonline переменная nTotalObjs не определена
    if (nTotalRead != nTotalObjs && pHeader->GetVersion() != FO_MAP_VERSION)
    {
        pLog->LogX("Cannot load map's object set.");
        MessageDlg("Не могу загрузить область с объектами", mtError, TMsgDlgButtons() << mbOK, 0);
        CloseHandle(h_map);
        return false;
    }

    CloseHandle(h_map);

    for (int i = 0; i < lev; i++)
    {
        frmMDI->frmPBar->NewTask("Load Map's Data level "+ String(i+1), "Load pro files ...", 0, pObjSet[i]->GetObjCount(true), &
                                                                                               frmMDI->iPos);
        Application->ProcessMessages();
        pObjSet[i]->ReLoadPROs();
    }

    for ( int i = 0; i < lev; i++)
    {

        frmMDI->frmPBar->NewTask("Load Map's Data level "+ String(i+1), "Load frm files ...", 0, pHeader->GetWidth() *
                                                                        pHeader->GetHeight() +
                                                                        pObjSet[i]->GetObjCount(true), &frmMDI->iPos);
        Application->ProcessMessages();

        pTileSet[i]->ReLoadFRMs();
        pObjSet[i]->ReLoadFRMs();
    }
    Application->ProcessMessages();
    frmMDI->DeletePBarForm();

    if (pHeader->GetLevels() > 1)
    {
        frmSelLevel->Label1->Caption = "Данная карта содержит " + String(pHeader->GetLevels()) +
                       " уровней. Выберите уровень для открытия.";
        frmSelLevel->Edit1->Text = "1";
        frmSelLevel->UpDown1->Min = 1;
        frmSelLevel->UpDown1->Position = 1;
        frmSelLevel->UpDown1->Max = pHeader->GetLevels();
        frmSelLevel->ShowModal();

        level = frmSelLevel->UpDown1->Position-1;
    } else level = 0;

    for (BYTE i =0; i < pHeader->GetLevels(); i++ )
    {
        if (i == level) continue;
        if (pObjSet[i])   _RELEASE(pObjSet[i]);
        if (pTileSet[i])  _RELEASE(pTileSet[i]);
    }

    // Заголовок загрузили. Теперь дадим новое имя файлу.
    if (pHeader->GetLevels() > 1)
    {
        FileName =  String(String(Name.c_str()) + "_level" + IntToStr(level + 1) + "." + Ext);
        FullFileName = DirName + FileName;
    }

    pHeader->SetVersion(FO_MAP_VERSION);

    WorldX = (GetWidth() - 10) * 48;
    WorldY = 0;

    return true;
}

//---------------------------------------------------------------------------

#pragma package(smart_init)
