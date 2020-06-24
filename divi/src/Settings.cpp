#include <Settings.h>
#include <string>
#include <utilstrencodings.h>

std::string Settings::GetArg(const std::string& strArg, const std::string& strDefault)
{
    if (mapArgs_.count(strArg))
        return mapArgs_[strArg];
    return strDefault;
}

int64_t Settings::GetArg(const std::string& strArg, int64_t nDefault)
{
    if (mapArgs_.count(strArg))
        return atoi64(mapArgs_[strArg]);
    return nDefault;
}

bool InterpretBool(const std::string& strValue)
{
    if (strValue.empty())
        return true;
    return (atoi(strValue) != 0);
}

bool Settings::GetBoolArg(const std::string& strArg, bool fDefault)
{
    if (mapArgs_.count(strArg))
        return InterpretBool(mapArgs_[strArg]);
    return fDefault;
}

bool Settings::SoftSetArg(const std::string& strArg, const std::string& strValue)
{
    if (mapArgs_.count(strArg))
        return false;
    mapArgs_[strArg] = strValue;
    return true;
}

bool Settings::SoftSetBoolArg(const std::string& strArg, bool fValue)
{
    if (fValue)
        return SoftSetArg(strArg, std::string("1"));
    else
        return SoftSetArg(strArg, std::string("0"));
}

void Settings::ForceRemoveArg(const std::string &strArg)
{
    mapArgs_.erase(strArg);
}

bool Settings::ParameterIsSet (const std::string& key)
{
    return mapArgs_.count(key);
}

std::string Settings::GetParameter(const std::string& key)
{
    if(ParameterIsSet(key))
    {
        return mapArgs_[key];
    }
    else
    {
        return "";
    }
}

void Settings::SetParameter (const std::string& key, const std::string& value)
{
    mapArgs_[key] = value;
}

void Settings::ClearParameter () 
{
    mapArgs_.clear();
}

bool Settings::ParameterIsSetForMultiArgs (const std::string& key)
{
    return mapMultiArgs_.count(key);
}

bool Settings::InterpretBool(const std::string& strValue)
{
    if (strValue.empty())
        return true;
    return (atoi(strValue) != 0);
}

void Settings::InterpretNegativeSetting(std::string& strKey, std::string& strValue)
{
    if (strKey.length()>3 && strKey[0]=='-' && strKey[1]=='n' && strKey[2]=='o') {
        strKey = "-" + strKey.substr(3);
        strValue = InterpretBool(strValue) ? "0" : "1";
    }
}

void Settings::ParseParameters(int argc, const char* const argv[])
{
    ClearParameter();
    mapMultiArgs_.clear();

    for (int i = 1; i < argc; i++) {
        std::string str(argv[i]);
        std::string strValue;
        size_t is_index = str.find('=');
        if (is_index != std::string::npos) {
            strValue = str.substr(is_index + 1);
            str = str.substr(0, is_index);
        }
#ifdef WIN32
        boost::to_lower(str);
        if (boost::algorithm::starts_with(str, "/"))
            str = "-" + str.substr(1);
#endif

        if (str[0] != '-')
            break;

        // Interpret --foo as -foo.
        // If both --foo and -foo are set, the last takes effect.
        if (str.length() > 1 && str[1] == '-')
            str = str.substr(1);
        InterpretNegativeSetting(str, strValue);

        SetParameter(str, strValue);
        mapMultiArgs_[str].push_back(strValue);
    }
}