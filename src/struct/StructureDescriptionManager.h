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
 * StructureDescriptionManager.h
 *
 *  Created on: Jul 26, 2012
 *      Author: sk
 */

#ifndef STRUCTUREDESCRIPTIONMANAGER_H_
#define STRUCTUREDESCRIPTIONMANAGER_H_

#include "Singleton.h"
#include "StructureDescription.h"
#include "Global.h"
#include "Configuration.h"

/**
 * @brief Global StructureDescription manager
 *
 * Creates all existing StructureDescriptions and provides access functions.
 */
class StructureDescriptionManager : public Singleton
{
public:
  /// @brief Destructor
  virtual ~StructureDescriptionManager();

  /// @brief Returns StructureDescription for a given DBO type
  StructureDescription *getStructureDescription (DB_OBJECT_TYPE type);
  /// @brief Returns container with all StructureDesciptions
  std::map <DB_OBJECT_TYPE, StructureDescription *> &getStructureDescriptions () { return structure_descriptions_; }

private:
  /// @brief Constructor
  StructureDescriptionManager();
  /// @brief Container with all StructureDescriptions
  std::map <DB_OBJECT_TYPE, StructureDescription *> structure_descriptions_;

public:
  static StructureDescriptionManager& getInstance()
  {
    static StructureDescriptionManager instance;
    return instance;
  }
};

#endif /* STRUCTUREDESCRIPTIONMANAGER_H_ */
