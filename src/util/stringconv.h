/*
 * This file is part of ATSDB.
 *
 * ATSDB is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * ATSDB is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with ATSDB.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef STRINGMANIPULATION_H_
#define STRINGMANIPULATION_H_

#include "logger.h"
#include "property.h"
#include "global.h"

#include <vector>
#include <iomanip>
#include <map>

#include <boost/regex.hpp>

namespace Utils
{

namespace String
{

inline std::string intToString(int number, int width, char c)
{
    std::ostringstream out;
    out << std::setfill(c) << std::setw(width) << number;
    return out.str();
}

inline std::string doubleToStringPrecision(double number, unsigned int precision)
{
    std::ostringstream out;
    out << std::fixed << std::setprecision(precision) << number;
    return out.str();
}

inline std::string doubleToStringNoScientific(double number)
{
    std::ostringstream out;
    out << std::fixed << number;
    return out.str();
}

inline std::string percentToString(double number)
{
    std::ostringstream out;
    out << std::fixed << std::setprecision(2) << number;
    return out.str();
}

inline unsigned int intFromOctalString (std::string number)
{
    return std::stoi( number, 0, 8 );
}

inline unsigned int intFromHexString (std::string number)
{
    return std::stoi( number, 0, 16 );
}

inline std::vector<std::string> &split(const std::string &s, char delim, std::vector<std::string> &elems)
{
    std::stringstream ss(s);
    std::string item;
    while(std::getline(ss, item, delim))
    {
        elems.push_back(item);
    }
    return elems;
}

inline std::vector<std::string> split(const std::string &s, char delim)
{
    std::vector<std::string> elems;
    return split(s, delim, elems);
}

inline std::string timeStringFromDouble (double seconds)
{
    int hours, minutes;
    std::ostringstream out;

    hours = (int) (seconds / 3600.0);
    minutes = (int) ((double)((int)seconds%3600)/60.0);
    seconds = seconds-hours*3600.0-minutes*60.0;

    out << std::fixed << std::setw(2) << std::setfill('0') << hours << ":" << std::setw(2) << std::setfill('0') << minutes << ":" << std::setw(6)
        << std::setfill('0') << std::setprecision(3) << seconds;
    return out.str();
}

inline double timeFromString (std::string time_str)
{
    std::vector<std::string> chunks = split(time_str, ':');

    double time;

    if (chunks.size() != 3)
        throw std::invalid_argument( "Util: timeFromString: wrong number of chunks" );

    time =  std::stod(chunks[0]) * 3600.0;
    time += std::stod(chunks[1]) * 60.0;
    time += std::stod(chunks[2]);

    return time;
}

inline std::string octStringFromInt (int number)
{
    std::ostringstream out;
    out <<  std::oct << number;
    return out.str();
}

inline std::string octStringFromInt(int number, int width, char c)
{
    std::ostringstream out;
    out << std::oct << std::setfill(c) << std::setw(width) << number;
    return out.str();
}


inline std::string hexStringFromInt (int number)
{
    std::ostringstream out;
    out <<  std::hex << number;
    return out.str();
}

inline std::string hexStringFromInt (int number, int width, char c)
{
    std::ostringstream out;
    out <<  std::hex << std::setfill(c) << std::setw(width) << number;
    return out.str();
}

inline int getAppendedInt (std::string text)
{
    int ret=0;
    boost::regex re("[0-9]+");
    boost::sregex_token_iterator i(text.begin(), text.end(), re, 0);
    boost::sregex_token_iterator j;

    unsigned count = 0;
    while(i != j)
    {
        ret = std::stoi(*i++);
        count++;
    }

    if (count == 0)
        throw std::runtime_error ("Util: getAppendedInt: no int found");

    return ret;
}

inline int getLeadingInt (std::string text)
{
    boost::regex re("[0-9]+");
    boost::sregex_token_iterator i(text.begin(), text.end(), re, 0);
    boost::sregex_token_iterator j;

    if (i != j)
    {
        return std::stoi(*i++);
    }
    else
        throw std::runtime_error ("Util: getLeadingInt: no int found");
}

inline double doubleFromLatitudeString(std::string &latitude_str)
{
    unsigned int len = latitude_str.size();
    assert (len == 12);
    char last_char = latitude_str.at(len-1);
    assert (last_char == 'N' || last_char == 'S');

    double x=0.0;

    x = std::stod(latitude_str.substr(0, 2));
    x += std::stod(latitude_str.substr(2, 2))/60.0;
    x += std::stod(latitude_str.substr(4, 7))/3600.0;

    if (last_char == 'S')
        x *= -1.0;

    return x;
}

inline double doubleFromLongitudeString(std::string &longitude_str)
{
    unsigned int len = longitude_str.size();
    assert (len == 13);
    char last_char = longitude_str.at(len-1);
    assert (last_char == 'E' || last_char == 'W');

    double x=0.0;

    x = std::stod(longitude_str.substr(0, 3));
    x += std::stod(longitude_str.substr(3, 2))/60.0;
    x += std::stod(longitude_str.substr(5, 7))/3600.0;

    if (last_char == 'W')
        x *= -1.0;

    return x;
}

enum class Representation { STANDARD, SECONDS_TO_TIME, DEC_TO_OCTAL, DEC_TO_HEX, FEET_TO_FLIGHTLEVEL};

extern std::map<Representation, std::string> representation_2_string;
extern std::map<std::string, Representation> string_2_representation;

Representation stringToRepresentation (const std::string &representation_str);
std::string representationToString (Representation representation);

inline std::string getValueString (const std::string &value)
{
    return value;
}

typedef std::numeric_limits<double> double_limit;
typedef std::numeric_limits<float> float_limit;

inline std::string getValueString (const float &value)
{
    std::ostringstream out;
    out << std::setprecision (float_limit::max_digits10) << value;
    return out.str();

}

inline std::string getValueString (const double &value)
{
    std::ostringstream out;
    out << std::setprecision (double_limit::max_digits10) << value;
    return out.str();
}

template <typename T> std::string getValueString (T value)
{
    return std::to_string(value);
}

template <typename T> std::string getAsSpecialRepresentationString (T value, Representation representation)
{
    std::ostringstream out;
    try
    {
        if (representation == Utils::String::Representation::SECONDS_TO_TIME)
        {
            return Utils::String::timeStringFromDouble (value);
        }
        else if (representation == Utils::String::Representation::DEC_TO_OCTAL)
        {
            out << std::oct << std::setfill ('0') << std::setw (4) << value;
        }
        else if (representation == Utils::String::Representation::DEC_TO_HEX)
        {
            out << std::uppercase << std::hex << value;
        }
        else if (representation == Utils::String::Representation::FEET_TO_FLIGHTLEVEL)
        {
            out << value/100.0;
        }
        else
        {
            throw std::runtime_error ("Utils: String: getAsSpecialRepresentationString: unknown representation");
        }
    }
    catch(std::exception& e)
    {
        logerr  << "Utils: String: getAsSpecialRepresentationString: exception thrown: " << e.what();
    }
    catch(...)
    {
        logerr  << "Utils: String: getAsSpecialRepresentationString: exception thrown";;
    }

    return out.str();
}

inline std::string getRepresentationStringFromValue (const std::string& value_str, PropertyDataType data_type, Representation representation)
{
    logdbg << "String: getRepresentationStringFromValue: value " << value_str << " data_type " << Property::asString(data_type)
           << " representation " << Utils::String::representationToString(representation);

    if (value_str == NULL_STRING)
        return value_str;

    if (representation == Utils::String::Representation::STANDARD)
        return value_str;

    switch (data_type)
    {
    case PropertyDataType::BOOL:
    {
        bool value = std::stoul(value_str);
        return Utils::String::getAsSpecialRepresentationString (value, representation);
    }
    case PropertyDataType::CHAR:
    {
        char value = std::stoi(value_str);
        return Utils::String::getAsSpecialRepresentationString (value, representation);
    }
    case PropertyDataType::UCHAR:
    {
        unsigned char value = std::stoul(value_str);
        return Utils::String::getAsSpecialRepresentationString (value, representation);
    }
    case PropertyDataType::INT:
    {
        int value = std::stoi(value_str);
        return Utils::String::getAsSpecialRepresentationString (value, representation);
    }
    case PropertyDataType::UINT:
    {
        unsigned int value = std::stoul(value_str);
        return Utils::String::getAsSpecialRepresentationString (value, representation);
    }
    case PropertyDataType::LONGINT:
    {
        long value = std::stol(value_str);
        return Utils::String::getAsSpecialRepresentationString (value, representation);
    }
    case PropertyDataType::ULONGINT:
    {
        unsigned long value = std::stoul(value_str);
        return Utils::String::getAsSpecialRepresentationString (value, representation);
    }
    case PropertyDataType::FLOAT:
    {
        float value = std::stof(value_str);
        return Utils::String::getAsSpecialRepresentationString (value, representation);
    }
    case PropertyDataType::DOUBLE:
    {
        double value = std::stod(value_str);
        return Utils::String::getAsSpecialRepresentationString (value, representation);
    }
    case PropertyDataType::STRING:
        throw std::invalid_argument ("String: getRepresentationStringFromValue: representation of string variable impossible");
    default:
        logerr  <<  "String: getRepresentationStringFromValue:: unknown property type " << Property::asString(data_type);
        throw std::runtime_error ("String: getRepresentationStringFromValue:: unknown property type " + Property::asString(data_type));
    }
}

inline std::string getValueStringFromRepresentation (const std::string& representation_str, Representation representation)
{
    if (representation_str == NULL_STRING)
        return representation_str;

    if (representation == Utils::String::Representation::SECONDS_TO_TIME)
    {
        return getValueString(timeFromString (representation_str));
    }
    else if (representation == Utils::String::Representation::DEC_TO_OCTAL)
    {
        return getValueString(intFromOctalString(representation_str));
    }
    else if (representation == Utils::String::Representation::DEC_TO_HEX)
    {
        return getValueString(intFromHexString(representation_str));
    }
    else if (representation == Utils::String::Representation::FEET_TO_FLIGHTLEVEL)
    {
        return getValueString(std::stod(representation_str)*100.0);
    }
    else
    {
        throw std::runtime_error ("Utils: String: getAsSpecialRepresentationString: unknown representation");
    }
}

std::string multiplyString (const std::string& value_str, double factor, PropertyDataType data_type);
const std::string& getLargerValueString (const std::string& value_a_str, const std::string& value_b_str, PropertyDataType data_type);
const std::string& getSmallerValueString (const std::string& value_a_str, const std::string& value_b_str, PropertyDataType data_type);
}


//template <typename T> std::string formatBinaryString (T val)
//{
//    int length = sizeof(T) * 8;
//    std::string binary(length, '0');
//    for (int i = 0; i < length; i++)
//    {
//        if ((val >> i) & 1)
//            binary[length - 1 - i] = '1';
//    }

//    return binary;
//}

}

#endif /* STRINGMANIPULATION_H_ */
