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

#ifndef DATABASEOPENTASK_H
#define DATABASEOPENTASK_H

#include "configurable.h"
#include "task.h"

#include <QObject>

#include <memory>

class DatabaseOpenTaskWidget;
class TaskManager;

class DatabaseOpenTask : public Task, public Configurable
{
    Q_OBJECT

public slots:
    void databaseOpenedSlot();

public:
    DatabaseOpenTask(const std::string& class_id, const std::string& instance_id,
                     TaskManager& task_manager);

    virtual void generateSubConfigurable (const std::string &class_id, const std::string &instance_id);

    virtual TaskWidget* widget ();
    virtual void deleteWidget ();

    virtual bool checkPrerequisites ();
    virtual bool isRecommended ();
    virtual bool isRequired ();

protected:
    std::unique_ptr<DatabaseOpenTaskWidget> widget_;

    virtual void checkSubConfigurables () {}
};

#endif // DATABASEOPENTASK_H
