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

/*
 * DBSchemaManager.cpp
 *
 *  Created on: Aug 13, 2012
 *      Author: sk
 */

#include "ConfigurationManager.h"
#include "DBSchemaManager.h"
#include "DBSchema.h"
#include "Logger.h"
#include "ATSDB.h"
#include "Buffer.h"

/**
 * Registers current_schema as parameter, creates sub-configurables (schemas), checks if current_schema exists (if defined).
 */
DBSchemaManager::DBSchemaManager()
: Configurable ("DBSchemaManager", "DBSchemaManager0", 0, "conf/config_db_schema.xml")
{
    registerParameter ("current_schema", &current_schema_, (std::string)"");

    createSubConfigurables ();

    if (current_schema_.size() != 0)
        assert (schemas_.find(current_schema_) != schemas_.end());

}

/**
 * Deletes all schemas
 */
DBSchemaManager::~DBSchemaManager()
{
    std::map <std::string, DBSchema *>::iterator it;
    for (it = schemas_.begin(); it != schemas_.end(); it++)
        delete it->second;
    schemas_.clear();

}

void DBSchemaManager::renameCurrentSchema (std::string new_name)
{
    assert (hasCurrentSchema());

    std::map <std::string, DBSchema *>::iterator it;
    it = schemas_.find (current_schema_);
    DBSchema *schema = it->second;
    schemas_.erase (it);
    assert (new_name.size() != 0);
    assert (schemas_.find(new_name) == schemas_.end());
    schemas_[new_name] = schema;
    current_schema_= new_name;
}

void DBSchemaManager::setCurrentSchema (std::string current_schema)
{
    assert (current_schema.size() != 0);
    assert (schemas_.find(current_schema) != schemas_.end());
    current_schema_=current_schema;
}

bool DBSchemaManager::hasCurrentSchema ()
{
    if (current_schema_.size() != 0)
    {
        if (schemas_.find(current_schema_) == schemas_.end())
        {
            logerr << "DBSchemaManager: hasCurrentSchema: schema '" << current_schema_ << "' not found in " << schemas_.size()
                          << " schemas";
            return false;
        }
        else
            return true;
    }
    else
    {
        logerr << "DBSchemaManager: hasCurrentSchema: schema '" << current_schema_ << "' not found in " << schemas_.size()
                      << " schemas";
        return false;
    }
}
std::string DBSchemaManager::getCurrentSchemaName ()
{
    assert (hasCurrentSchema());
    return current_schema_;
}


/**
 * Can create DBSchemas.
 *
 * \exception std::runtime_error if unknown class_id
 */
void DBSchemaManager::generateSubConfigurable (std::string class_id, std::string instance_id)
{
    logdbg  << "DBSchemaManager: generateSubConfigurable: " << class_id_ << " instance " << instance_id_;

    if (class_id.compare("DBSchema") == 0)
    {
        logdbg  << "DBSchema: generateSubConfigurable: generating DBTable";
        DBSchema *schema = new DBSchema ("DBSchema", instance_id, this);
        assert (schema->getName().size() != 0);
        assert (schemas_.find(schema->getName()) == schemas_.end());
        schemas_[schema->getName()]=schema;
    }
    else
        throw std::runtime_error ("DBSchema: generateSubConfigurable: unknown class_id "+class_id);
}


void DBSchemaManager::checkSubConfigurables ()
{
    //  if (schemas_.size() == 0)
    //  {
    //    loadDBSchema ();
    //  }
}

DBSchema *DBSchemaManager::getCurrentSchema ()
{
    assert (current_schema_.size() != 0);
    assert (schemas_.find(current_schema_) != schemas_.end());
    return schemas_[current_schema_];
}

DBSchema *DBSchemaManager::getSchema (std::string name)
{
    assert (schemas_.find(name) != schemas_.end());
    return schemas_[name];
}

bool DBSchemaManager::hasSchema (std::string name)
{
    return schemas_.find(name) != schemas_.end();
}

void DBSchemaManager::addEmptySchema (std::string name)
{
    std::string schema_name="DBSchema"+name+"0";
    Configuration &schema_configuration = addNewSubConfiguration ("DBSchema", schema_name);
    schema_configuration.addParameterString ("name", name);
    generateSubConfigurable ("DBSchema", schema_name);

    if (!hasCurrentSchema())
        setCurrentSchema(name);
}
