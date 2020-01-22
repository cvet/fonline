#include "MsgFiles.h"
#include "FileSystem.h"
#include "GenericUtils.h"
#include "Log.h"
#include "StringUtils.h"
#include "Testing.h"

string TextMsgFileName[TEXTMSG_COUNT] = {
    "FOTEXT.MSG",
    "FODLG.MSG",
    "FOOBJ.MSG",
    "FOGAME.MSG",
    "FOGM.MSG",
    "FOCOMBAT.MSG",
    "FOQUEST.MSG",
    "FOHOLO.MSG",
    "FOINTERNAL.MSG",
    "FOLOCATIONS.MSG",
};

FOMsg::FOMsg()
{
    Clear();
}

FOMsg::FOMsg(const FOMsg& other)
{
    Clear();
    for (auto it = other.strData.begin(), end = other.strData.end(); it != end; ++it)
        AddStr(it->first, it->second);
}

FOMsg::~FOMsg()
{
    Clear();
}

FOMsg& FOMsg::operator=(const FOMsg& other)
{
    Clear();
    for (auto it = other.strData.begin(), end = other.strData.end(); it != end; ++it)
        AddStr(it->first, it->second);
    return *this;
}

FOMsg& FOMsg::operator+=(const FOMsg& other)
{
    for (auto it = other.strData.begin(), end = other.strData.end(); it != end; ++it)
    {
        EraseStr(it->first);
        AddStr(it->first, it->second);
    }
    return *this;
}

void FOMsg::AddStr(uint num, const string& str)
{
    strData.insert(std::make_pair(num, str));
}

void FOMsg::AddBinary(uint num, const uchar* binary, uint len)
{
    CharVec str;
    str.resize(len * 2 + 1);
    size_t str_cur = 0;
    for (uint i = 0; i < len; i++)
    {
        Str::HexToStr(binary[i], &str[str_cur]);
        str_cur += 2;
    }

    AddStr(num, (char*)&str[0]);
}

string FOMsg::GetStr(uint num)
{
    uint str_count = (uint)strData.count(num);
    auto it = strData.find(num);

    switch (str_count)
    {
    case 0:
        return MSG_ERROR_MESSAGE;
    case 1:
        break;
    default:
        for (int i = 0, j = GenericUtils::Random(0, str_count) - 1; i < j; i++)
            ++it;
        break;
    }

    return it->second;
}

string FOMsg::GetStr(uint num, uint skip)
{
    uint str_count = (uint)strData.count(num);
    auto it = strData.find(num);

    if (skip >= str_count)
        return MSG_ERROR_MESSAGE;
    for (uint i = 0; i < skip; i++)
        ++it;

    return it->second;
}

uint FOMsg::GetStrNumUpper(uint num)
{
    auto it = strData.upper_bound(num);
    if (it == strData.end())
        return 0;
    return it->first;
}

uint FOMsg::GetStrNumLower(uint num)
{
    auto it = strData.lower_bound(num);
    if (it == strData.end())
        return 0;
    return it->first;
}

int FOMsg::GetInt(uint num)
{
    uint str_count = (uint)strData.count(num);
    auto it = strData.find(num);

    switch (str_count)
    {
    case 0:
        return -1;
    case 1:
        break;
    default:
        for (int i = 0, j = GenericUtils::Random(0, str_count) - 1; i < j; i++)
            ++it;
        break;
    }

    return _str(it->second).toInt();
}

uint FOMsg::GetBinary(uint num, UCharVec& data)
{
    data.clear();

    if (!Count(num))
        return 0;

    string str = GetStr(num);
    uint len = (uint)str.length() / 2;
    data.resize(len);
    for (uint i = 0; i < len; i++)
        data[i] = Str::StrToHex(&str[i * 2]);
    return len;
}

uint FOMsg::Count(uint num)
{
    return (uint)strData.count(num);
}

void FOMsg::EraseStr(uint num)
{
    while (true)
    {
        auto it = strData.find(num);
        if (it != strData.end())
            strData.erase(it);
        else
            break;
    }
}

uint FOMsg::GetSize()
{
    return (uint)strData.size();
}

bool FOMsg::IsIntersects(const FOMsg& other)
{
    for (auto it = strData.begin(), end = strData.end(); it != end; ++it)
        if (other.strData.count(it->first))
            return true;
    return false;
}

void FOMsg::GetBinaryData(UCharVec& data)
{
    // Fill raw data
    uint count = (uint)strData.size();
    data.resize(sizeof(count));
    memcpy(&data[0], &count, sizeof(count));
    for (auto it = strData.begin(), end = strData.end(); it != end; ++it)
    {
        uint num = it->first;
        const string& str = it->second;
        uint str_len = (uint)str.length();

        data.resize(data.size() + sizeof(num) + sizeof(str_len) + str_len);
        memcpy(&data[data.size() - (sizeof(num) + sizeof(str_len) + str_len)], &num, sizeof(num));
        memcpy(&data[data.size() - (sizeof(str_len) + str_len)], &str_len, sizeof(str_len));
        if (str_len)
            memcpy(&data[data.size() - str_len], (void*)str.c_str(), str_len);
    }

    // Compress
    Compressor::Compress(data);
}

bool FOMsg::LoadFromBinaryData(const UCharVec& data)
{
    Clear();

    // Uncompress
    UCharVec data_copy = data;
    if (!Compressor::Uncompress(data_copy, 10))
        return false;

    // Read count of strings
    const uchar* buf = &data_copy[0];
    uint count;
    memcpy(&count, buf, sizeof(count));
    buf += sizeof(count);

    // Read strings
    uint num;
    uint str_len;
    string str;
    for (uint i = 0; i < count; i++)
    {
        memcpy(&num, buf, sizeof(num));
        buf += sizeof(num);

        memcpy(&str_len, buf, sizeof(str_len));
        buf += sizeof(str_len);

        str.resize(str_len);
        if (str_len)
            memcpy(&str[0], buf, str_len);
        buf += str_len;

        AddStr(num, str);
    }

    return true;
}

bool FOMsg::LoadFromString(const string& str)
{
    bool fail = false;

    istringstream istr(str);
    string line;
    while (std::getline(istr, line, '\n'))
    {
        uint num = 0;
        size_t offset = 0;
        for (int i = 0; i < 3; i++)
        {
            size_t first = line.find('{', offset);
            size_t last = line.find('}', first);
            if (first == string::npos || last == string::npos)
            {
                if (i == 2 && first != string::npos)
                {
                    string additional_line;
                    while (last == string::npos && std::getline(istr, additional_line, '\n'))
                    {
                        line += "\n" + additional_line;
                        last = line.find('}', first);
                    }
                }

                if (first == string::npos || last == string::npos)
                {
                    if (i > 0 || first != string::npos)
                        fail = true;
                    break;
                }
            }

            string str = line.substr(first + 1, last - first - 1);
            offset = last + 1;
            if (i == 0 && !num)
                num = (_str(str).isNumber() ? _str(str).toInt() : _str(str).toHash());
            else if (i == 1 && num)
                num += (!str.empty() ? (_str(str).isNumber() ? _str(str).toInt() : _str(str).toHash()) : 0);
            else if (i == 2 && num)
                AddStr(num, str);
            else
                fail = true;
        }
    }

    return !fail;
}

void FOMsg::LoadFromMap(const StrMap& kv)
{
    for (auto& kv_ : kv)
    {
        uint num = _str(kv_.first).toUInt();
        if (num)
            AddStr(num, kv_.second);
    }
}

void FOMsg::Clear()
{
    strData.clear();
}

int FOMsg::GetMsgType(const string& type_name)
{
    if (_str(type_name).compareIgnoreCase("text"))
        return TEXTMSG_TEXT;
    else if (_str(type_name).compareIgnoreCase("dlg"))
        return TEXTMSG_DLG;
    else if (_str(type_name).compareIgnoreCase("item"))
        return TEXTMSG_ITEM;
    else if (_str(type_name).compareIgnoreCase("obj"))
        return TEXTMSG_ITEM;
    else if (_str(type_name).compareIgnoreCase("game"))
        return TEXTMSG_GAME;
    else if (_str(type_name).compareIgnoreCase("gm"))
        return TEXTMSG_GM;
    else if (_str(type_name).compareIgnoreCase("combat"))
        return TEXTMSG_COMBAT;
    else if (_str(type_name).compareIgnoreCase("quest"))
        return TEXTMSG_QUEST;
    else if (_str(type_name).compareIgnoreCase("holo"))
        return TEXTMSG_HOLO;
    else if (_str(type_name).compareIgnoreCase("internal"))
        return TEXTMSG_INTERNAL;
    return -1;
}

LanguagePack::LanguagePack()
{
    memzero(NameStr, sizeof(NameStr));
    IsAllMsgLoaded = false;
}

void LanguagePack::LoadFromFiles(FileManager& file_mngr, const string& lang_name)
{
    RUNTIME_ASSERT(lang_name.length() == 4);
    memcpy(NameStr, lang_name.c_str(), 4);
    NameStr[4] = 0;
    bool fail = false;

    FileCollection msg_files = file_mngr.FilterFiles("msg");
    while (msg_files.MoveNext())
    {
        File msg_file = msg_files.GetCurFile();

        // Check pattern '...Texts/lang/file'
        StrVec dirs = _str(msg_file.GetPath()).split('/');
        if (dirs.size() >= 3 && dirs[dirs.size() - 3] == "Texts" && dirs[dirs.size() - 2] == lang_name)
        {
            for (int i = 0; i < TEXTMSG_COUNT; i++)
            {
                string msg_name = _str(TextMsgFileName[i]).eraseFileExtension();
                if (_str(msg_name).compareIgnoreCase(msg_file.GetName()))
                {
                    if (!Msg[i].LoadFromString(msg_file.GetCStr()))
                    {
                        WriteLog("Invalid MSG file '{}'.\n", msg_file.GetPath());
                        fail = true;
                    }
                    break;
                }
            }
        }
    }

    if (Msg[TEXTMSG_GAME].GetSize() == 0)
        WriteLog("Unable to load '{}' from file.\n", TextMsgFileName[TEXTMSG_GAME]);

    IsAllMsgLoaded = (Msg[TEXTMSG_GAME].GetSize() > 0 && !fail);
}

void LanguagePack::LoadFromCache(CacheStorage& cache, const string& lang_name)
{
    RUNTIME_ASSERT(lang_name.length() == 4);
    memcpy(NameStr, lang_name.c_str(), 4);
    NameStr[4] = 0;

    int errors = 0;
    for (int i = 0; i < TEXTMSG_COUNT; i++)
    {
        uint buf_len;
        uchar* buf = cache.GetCache(GetMsgCacheName(i), buf_len);
        if (buf)
        {
            UCharVec data;
            data.resize(buf_len);
            memcpy(&data[0], buf, buf_len);
            SAFEDELA(buf);

            if (!Msg[i].LoadFromBinaryData(data))
                errors++;
        }
        else
        {
            errors++;
        }
    }

    if (errors)
        WriteLog("Cached language '{}' not found.\n", NameStr);

    IsAllMsgLoaded = (errors == 0);
}

string LanguagePack::GetMsgCacheName(int msg_num)
{
    return _str("${}-{}.cache", NameStr, TextMsgFileName[msg_num]);
}
