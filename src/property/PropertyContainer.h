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
 * PropertyContainer.h
 *
 *  Created on: Dec 8, 2011
 *      Author: sk
 */

#ifndef PROPERTYCONTAINER_H_
#define PROPERTYCONTAINER_H_

#include "PropertyList.h"
#include "ArrayTemplate.h"

/**
 * @brief Dynamic container with an ArrayTemplate for each Property
 *
 * Its contents are defined by a PropertyList. For each Property an ArrayTemplate is stored, generated by the MemoryManager.
 * Provides access functions and functions for management of the data contents. Created with a PropertyList, adding of
 * Properties is possible, but deleting a Property is not possible.
 *
 * An ArrayTemplate is identified by an index in their appropriate ArrayTemplateManager.
 *
 * Deep copy of the data contents is not possible, but shallow copy is possible. Implements a binary search algorithm based
 * based on a monotonous key.
 *
 * Used by a Buffer. NOT thread-safe.
 */
class PropertyContainer
{
protected:
  /// Property list with all data items
  PropertyList member_list_;
  /// Number of elements in all ArrayTemplates
  unsigned int size_;
  /// Container with all ArrayTemplates
  std::vector <ArrayTemplateBase *> array_templates_;
  /// Information if ArrayTemplates are shallow copies (not deleted if shallow copies)
  std::vector<bool> array_template_is_shallow_copy_;
  /// Indexes of ArrayTemplate in their appropriate ArrayTemplateManager
  std::vector <unsigned int> array_templates_indexes_;
  /// Current index
  unsigned int index_;
  /// Maximal index, is size_-1
  unsigned int max_index_;
  /// Maximal used index
  unsigned int max_used_index_;
  /// Base pointers to the start of the ArrayTemplate data arrays
  std::vector<void *> base_addresses_;
  /// Addresses for current index
  std::vector<void *> addresses_;
  /// Size increments for fast index increment/decrement
  std::vector<int> size_increments_;

  /// @brief Creates ArrayTemplates for all Properties in the member list
  void createArrayTemplates ();
  /// @brief Deletes all ArrayTemplates
  void clear ();

  /// @brief Searches for a key (internal)
  int binarySearch(unsigned int key_pos, int index_start, int index_last, unsigned int key);

public:
  /// @brief Constructor
  PropertyContainer(PropertyList member_list, bool create_array_templates=true);
  /// @brief Destructor
  virtual ~PropertyContainer();

  /// @brief Sets the index to a given one
  void setIndex (unsigned int index);
  /// @brief Returns void pointer to a given data item for the current index
  void *get (unsigned int property);
  /// @brief Returns void pointer to a given data item at a given index
  void *get (unsigned int index, unsigned int property);

  /// @brief Increments the index
  void incrementIndex ();
  /// @brief Decrements the index
  void decrementIndex ();
  /// @brief Returns flag indicating if index is at maximal value
  bool isMaxIndex ();
  /// @brief Returns flag indicating if index is at minimal value
  bool isMinIndex ();

  /// @brief Adds a property and allocates an ArrayTemplate for it
  void addProperty (std::string id, PROPERTY_DATA_TYPE type);

  /// @brief Returns a shallow copy, must be deleted by caller
  PropertyContainer *getShallowCopy ();

  /// @brief Sets all values of a given Property to Nan (Not a number)
  void setPropertyValuesNan (unsigned int property_index);

  /// @brief Returns key value for the first position
  unsigned int getMinKey (unsigned int key_pos);
  /// @brief Returns key value for the maximum used position
  unsigned int getMaxKey (unsigned int key_pos);
  /// @brief Searches and returns key position for a given key value (and index), returns -1 if not found
  int getIndexForKey (unsigned int key_pos, unsigned int key);

  /// @brief Returns the maximal size
  unsigned int getSize ()
  {
    return size_;
  };

  /// @brief Returns property list
  PropertyList *getPropertyList ()
  {
    return &member_list_;
  }

  /// @brief Returns container with void pointers to the values of the data item at the current index
  std::vector<void *>* getAdresses ()
      {
    return &addresses_;
      }
};

#endif /* PROPERTYCONTAINER_H_ */
