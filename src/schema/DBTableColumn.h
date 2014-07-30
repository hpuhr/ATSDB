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
 * DBTableColumn.h
 *
 *  Created on: Aug 13, 2012
 *      Author: sk
 */

#ifndef DBTABLECOLUMN_H_
#define DBTABLECOLUMN_H_

#include <string>
#include "Configurable.h"

/**
 * @brief Database table column definition
 *
 * Holds some parameters which define a database table column
 */
class DBTableColumn : public Configurable
{
public:
  /// @brief Constructor
  DBTableColumn(std::string class_id, std::string instance_id, Configurable *parent, std::string db_table_name);
  /// @brief Destructor
  virtual ~DBTableColumn();

  /// @brief Sets the column name
  void setName (std::string name) { name_=name; }
  /// @brief Returns the column name
  std::string getName() { return name_; }

  /// @brief Sets the data type
  void setType (std::string type) { type_=type; }
  /// @brief Returns the data type
  std::string getType() { return type_; }

  /// @brief Sets key flag
  void setIsKey (bool is_key) { is_key_=is_key;}
  /// @brief Returns key flag
  bool isKey () { return is_key_; }

  /// @brief Returns if column has an assigned unit
  bool hasUnit () { return unit_dimension_.size() != 0;}
  /// @brief Returns unit dimension
  std::string &getUnitDimension () { return unit_dimension_; }
  /// @brief Returns unit
  std::string &getUnitUnit () { return unit_unit_; }

  /// @brief Returns database table name which holds this column
  std::string getDBTableName () { return db_table_name_; }

  bool hasSpecialNull () { return special_null_.size() > 0; }
  void setSpecialNull (std::string special_null) { special_null_ = special_null; }
  std::string getSpecialNull () { return special_null_; }

  void createSubConfigurables () {}

protected:
  /// Name of the column
  std::string name_;
  /// Data type
  std::string type_;
  /// Key flag
  bool is_key_;
  /// Unit dimension
  std::string unit_dimension_;
  /// Unit
  std::string unit_unit_;
  /// Database table name which holds this column
  std::string db_table_name_;
  /// Special value signifying null value
  std::string special_null_;
};

#endif /* DBTABLECOLUMN_H_ */
