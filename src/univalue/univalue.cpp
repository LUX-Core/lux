// Copyright 2015 Bitcoin Core Developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <stdint.h>
#include <ctype.h>
#include <iomanip>
#include <sstream>
#include <stdexcept> // std::runtime_error
#include "univalue.h"

#include "utilstrencodings.h" // ParseXX

using namespace std;

const UniValue NullUniValue;

void UniValue::clear()
{
    typ = VNULL;
    val.clear();
    keys.clear();
    values.clear();
}

bool UniValue::setNull()
{
    clear();
    return true;
}

bool UniValue::setBool(bool val_)
{
    clear();
    typ = VBOOL;
    if (val_)
        val = "1";
    return true;
}

static bool validNumStr(const string& s)
{
    string tokenVal;
    unsigned int consumed;
    enum jtokentype tt = getJsonToken(tokenVal, consumed, s.c_str());
    return (tt == JTOK_NUMBER);
}

bool UniValue::setNumStr(const string& val_)
{
    if (!validNumStr(val_))
        return false;

    clear();
    typ = VNUM;
    val = val_;
    return true;
}

bool UniValue::setInt(uint64_t val)
{
    string s;
    ostringstream oss;

    oss << val;

    return setNumStr(oss.str());
}

bool UniValue::setInt(int64_t val)
{
    string s;
    ostringstream oss;

    oss << val;

    return setNumStr(oss.str());
}

bool UniValue::setFloat(double val)
{
    string s;
    ostringstream oss;

    oss << std::setprecision(16) << val;

    bool ret = setNumStr(oss.str());
    typ = VNUM;
    return ret;
}

bool UniValue::setStr(const string& val_)
{
    clear();
    typ = VSTR;
    val = val_;
    return true;
}

bool UniValue::setArray()
{
    clear();
    typ = VARR;
    return true;
}

bool UniValue::setObject()
{
    clear();
    typ = VOBJ;
    return true;
}

bool UniValue::push_back(const UniValue& val)
{
    if (typ != VARR)
        return false;

    values.push_back(val);
    return true;
}

bool UniValue::push_backV(const std::vector<UniValue>& vec)
{
    if (typ != VARR)
        return false;

    values.insert(values.end(), vec.begin(), vec.end());

    return true;
}

bool UniValue::pushKV(const std::string& key, const UniValue& val)
{
    if (typ != VOBJ)
        return false;

    keys.push_back(key);
    values.push_back(val);
    return true;
}

bool UniValue::pushKVs(const UniValue& obj)
{
    if (typ != VOBJ || obj.typ != VOBJ)
        return false;

    for (unsigned int i = 0; i < obj.keys.size(); i++) {
        keys.push_back(obj.keys[i]);
        values.push_back(obj.values.at(i));
    }

    return true;
}

int UniValue::findKey(const std::string& key) const
{
    for (unsigned int i = 0; i < keys.size(); i++) {
        if (keys[i] == key)
            return (int) i;
    }

    return -1;
}

bool UniValue::checkObject(const std::map<std::string,UniValue::VType>& t)
{
    for (std::map<std::string,UniValue::VType>::const_iterator it = t.begin();
         it != t.end(); it++) {
        int idx = findKey(it->first);
        if (idx < 0)
            return false;

        if (values.at(idx).getType() != it->second)
            return false;
    }

    return true;
}

const UniValue& UniValue::operator[](const std::string& key) const
{
    if (typ != VOBJ)
        return NullUniValue;

    int index = findKey(key);
    if (index < 0)
        return NullUniValue;

    return values.at(index);
}

const UniValue& UniValue::operator[](unsigned int index) const
{
    if (typ != VOBJ && typ != VARR)
        return NullUniValue;
    if (index >= values.size())
        return NullUniValue;

    return values.at(index);
}

const char *uvTypeName(UniValue::VType t)
{
    switch (t) {
    case UniValue::VNULL: return "null";
    case UniValue::VBOOL: return "bool";
    case UniValue::VOBJ: return "object";
    case UniValue::VARR: return "array";
    case UniValue::VSTR: return "string";
    case UniValue::VNUM: return "number";
    case UniValue::VREAL: return "number";
    }

    // not reached
    return NULL;
}

const UniValue& find_value(const UniValue& obj, const std::string& name)
{
    for (unsigned int i = 0; i < obj.keys.size(); i++)
        if (obj.keys[i] == name)
            return obj.values.at(i);

    return NullUniValue;
}

std::vector<std::string> UniValue::getKeys() const
{
    if (typ != VOBJ)
        throw std::runtime_error("JSON value is not an object as expected");
    return keys;
}

std::vector<UniValue> UniValue::getValues() const
{
    if (typ != VOBJ && typ != VARR)
        throw std::runtime_error("JSON value is not an object or array as expected");
    return values;
}

bool UniValue::get_bool() const
{
    if (typ != VBOOL)
        throw std::runtime_error("JSON value is not a boolean as expected");
    return getBool();
}

std::string UniValue::get_str() const
{
    if (typ != VSTR)
        throw std::runtime_error("JSON value is not a string as expected");
    return getValStr();
}

int UniValue::get_int() const
{
    if (typ != VNUM)
        throw std::runtime_error("JSON value is not an integer as expected");
    int32_t retval;
    if (!ParseInt32(getValStr(), &retval))
        throw std::runtime_error("JSON integer out of range");
    return retval;
}

int64_t UniValue::get_int64() const
{
    if (typ != VNUM)
        throw std::runtime_error("JSON value is not an integer as expected");
    int64_t retval;
    if (!ParseInt64(getValStr(), &retval))
        throw std::runtime_error("JSON integer out of range");
    return retval;
}

double UniValue::get_real() const
{
    if (typ != VREAL && typ != VNUM)
        throw std::runtime_error("JSON value is not a number as expected");
    double retval;
    if (!ParseDouble(getValStr(), &retval))
        throw std::runtime_error("JSON double out of range");
    return retval;
}

const UniValue& UniValue::get_obj() const
{
    if (typ != VOBJ)
        throw std::runtime_error("JSON value is not an object as expected");
    return *this;
}

const UniValue& UniValue::get_array() const
{
    if (typ != VARR)
        throw std::runtime_error("JSON value is not an array as expected");
    return *this;
}

