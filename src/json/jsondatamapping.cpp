#include "jsondatamapping.h"
#include "dbovariable.h"
#include "dbobjectmanager.h"
#include "dbobject.h"
#include "atsdb.h"
#include "jsonobjectparser.h"
#include "jsondatamappingwidget.h"

JSONDataMapping::JSONDataMapping (const std::string& class_id, const std::string& instance_id, JSONObjectParser& parent)
    : Configurable (class_id, instance_id, &parent)
{
    registerParameter("json_key", &json_key_, "");

    registerParameter("db_object_name", &db_object_name_, "");
    registerParameter("dbovariable_name", &dbovariable_name_, "");

    DBObjectManager& obj_man = ATSDB::instance().objectManager();

    if (!obj_man.existsObject(db_object_name_))
        logwrn << "JSONDataMapping: ctor: dbobject '" << db_object_name_ << "' does not exist";
    else if (!obj_man.object(db_object_name_).hasVariable(dbovariable_name_))
        logwrn << "JSONDataMapping: ctor: dbobject " << db_object_name_ << " variable '" << dbovariable_name_
               << "' does not exist";
    else
        variable_ = &obj_man.object(db_object_name_).variable(dbovariable_name_);

    registerParameter("mandatory", &mandatory_, false);

    registerParameter("json_value_format_str", &json_value_format_str_, "");
    if (!variable_)
        logwrn << "JSONDataMapping: ctor: can not set format since variable is missing";
    else
        json_value_format_.reset(new Format (variable_->dataType(), json_value_format_str_));

    registerParameter("dimension", &dimension_, "");
    registerParameter("unit", &unit_, "");

    sub_keys_ = Utils::String::split(json_key_, '.');
    has_sub_keys_ = sub_keys_.size() > 1;
    num_sub_keys_ = sub_keys_.size();

    logdbg << "JSONDataMapping: ctor: key " << json_key_ << " num subkeys " << sub_keys_.size();

    createSubConfigurables ();
}

JSONDataMapping& JSONDataMapping::operator=(JSONDataMapping&& other)
{
    json_key_ = other.json_key_;
    db_object_name_ = other.db_object_name_;
    dbovariable_name_ = other.dbovariable_name_;
    variable_ = other.variable_;

    mandatory_ = other.mandatory_;
    json_value_format_str_ = other.json_value_format_str_;
    json_value_format_ = std::move(other.json_value_format_);

    dimension_ = other.dimension_;
    unit_ = other.unit_;

    has_sub_keys_ = other.has_sub_keys_;
    sub_keys_ = std::move(other.sub_keys_);
    num_sub_keys_ = other.num_sub_keys_;

    other.configuration().updateParameterPointer ("json_key", &json_key_);
    other.configuration().updateParameterPointer ("db_object_name", &db_object_name_);
    other.configuration().updateParameterPointer ("dbovariable_name", &dbovariable_name_);
    other.configuration().updateParameterPointer ("mandatory", &mandatory_);
    other.configuration().updateParameterPointer ("json_value_format_str", &json_value_format_str_);
    other.configuration().updateParameterPointer ("dimension", &dimension_);
    other.configuration().updateParameterPointer ("unit", &unit_);

    widget_ = std::move(other.widget_);
    if (widget_)
        widget_->setMapping(*this);
    other.widget_ = nullptr;

    return static_cast<JSONDataMapping&>(Configurable::operator=(std::move(other)));
}

DBOVariable &JSONDataMapping::variable() const
{
    assert (variable_);
    return *variable_;
}

bool JSONDataMapping::mandatory() const
{
    return mandatory_;
}

void JSONDataMapping::mandatory(bool mandatory)
{
    mandatory_ = mandatory;
}

Format JSONDataMapping::jsonValueFormat() const
{
    assert (json_value_format_);
    return *json_value_format_.get();
}

//void JSONDataMapping::jsonValueFormat(const Format &json_value_format)
//{
//    json_value_format_ = json_value_format;
//}

std::string JSONDataMapping::dbObjectName() const
{
    return db_object_name_;
}

std::string JSONDataMapping::dboVariableName() const
{
    return dbovariable_name_;
}

std::string JSONDataMapping::jsonKey() const
{
    return json_key_;
}

void JSONDataMapping::jsonKey(const std::string &json_key)
{
    json_key_ = json_key;

    sub_keys_ = Utils::String::split(json_key_, '.');
    has_sub_keys_ = sub_keys_.size() > 1;
    num_sub_keys_ = sub_keys_.size();
}

JSONDataMappingWidget* JSONDataMapping::widget ()
{
    if (!widget_)
    {
        widget_.reset(new JSONDataMappingWidget (*this));
        assert (widget_);
    }

    return widget_.get(); // needed for qt integration, not pretty
}
