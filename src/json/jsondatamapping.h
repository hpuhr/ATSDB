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

#ifndef JSONDATAMAPPING_H
#define JSONDATAMAPPING_H

#include "json.hpp"
#include "logger.h"
#include "format.h"
#include "nullablevector.h"
#include "jsonutils.h"
#include "configurable.h"
#include "jsondatamappingwidget.h"

#include <memory>

class DBOVariable;
class JSONObjectParser;

class JSONDataMapping : public Configurable
{
public:
    JSONDataMapping (const std::string& class_id, const std::string& instance_id, JSONObjectParser& parent);
    JSONDataMapping() = default;
    JSONDataMapping(JSONDataMapping&& other) { *this = std::move(other); }

    virtual ~JSONDataMapping();

    /// @brief Move constructor
    JSONDataMapping& operator=(JSONDataMapping&& other);

    // return bool mandatory missing
    template<typename T>
    bool findAndSetValue(const nlohmann::json& j, NullableVector<T>& array_list, size_t row_cnt) const;

    bool hasDimension () const { return dimension_.size() > 0; }
    /// @brief Returns dimension contained in the column
    std::string& dimensionRef () { return dimension_; }
    const std::string& dimension () const { return dimension_; }
    /// @brief Returns unit
    std::string& unitRef () { return unit_; }
    const std::string& unit () const { return unit_; }

    const std::string& jsonKey() const;
    void jsonKey(const std::string &json_key);

    bool active() const;
    void active(bool active);

    bool hasVariable () { return variable_ != nullptr; }
    DBOVariable& variable() const;

    bool mandatory() const;
    void mandatory(bool mandatory);

    Format jsonValueFormat() const;
    Format& jsonValueFormatRef();
    //void jsonValueFormat(const Format &json_value_format);

    std::string dbObjectName() const;

    void dboVariableName(const std::string& name);
    std::string dboVariableName() const;

    virtual void generateSubConfigurable (const std::string& class_id, const std::string& instance_id) {}

    //JSONDataMappingWidget* widget ();

    void initializeIfRequired ();

    std::string& formatDataTypeRef();

    bool initialized() const;

    std::string comment() const;
    void comment(const std::string &comment);

    bool appendValue() const;
    void appendValue(bool appendValue);

    bool inArray() const;
    void inArray(bool inArray);

private:
    bool initialized_ {false};

    bool active_ {false};
    std::string json_key_;

    std::string db_object_name_;
    std::string dbovariable_name_;
    DBOVariable* variable_ {nullptr};

    bool mandatory_ {false};

    std::string comment_;

    std::string format_data_type_;
    Format json_value_format_;
    //std::unique_ptr<Format> json_value_format_;

    /// Unit dimension
    std::string dimension_;
    /// Unit
    std::string unit_;

    bool in_array_ {false};
    bool append_value_ {false};

    bool has_sub_keys_ {false};
    std::vector<std::string> sub_keys_;
    size_t num_sub_keys_;
    std::vector<std::string>::iterator last_key_;
    std::vector<std::string>::iterator second_to_last_key_;

    std::unique_ptr<JSONDataMappingWidget> widget_;

    void initialize ();

protected:
    virtual void checkSubConfigurables () {}

    const nlohmann::json* findKey (const nlohmann::json& j) const;
    const nlohmann::json* findParentKey (const nlohmann::json& j) const;

    // generic template functions
    template<typename T>
    void setValue(const nlohmann::json* val_ptr, NullableVector<T>& array_list, size_t row_cnt) const;

    template<typename T>
    void appendValue(const nlohmann::json* val_ptr, NullableVector<T>& array_list, size_t row_cnt) const;
    // bool template functions
    void setValue(const nlohmann::json* val_ptr, NullableVector<bool>& array_list, size_t row_cnt) const;
    void appendValue(const nlohmann::json* val_ptr, NullableVector<bool>& array_list, size_t row_cnt) const;
    // char template functions
    void setValue(const nlohmann::json* val_ptr, NullableVector<char>& array_list, size_t row_cnt) const;
    void appendValue(const nlohmann::json* val_ptr, NullableVector<char>& array_list, size_t row_cnt) const;
    // string template functions
    void setValue(const nlohmann::json* val_ptr, NullableVector<std::string>& array_list, size_t row_cnt) const;
    void appendValue(const nlohmann::json* val_ptr, NullableVector<std::string>& array_list, size_t row_cnt) const;
};

Q_DECLARE_METATYPE(JSONDataMapping*)

#endif // JSONDATAMAPPING_H
