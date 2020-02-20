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

#include "atsdb.h"
#include "config.h"
#include "dbobject.h"
#include "dbobjectmanager.h"
#include "dbtableinfo.h"
#include "dbschemamanager.h"
#include "dbinterface.h"
#include "global.h"
#include "logger.h"
#include "filtermanager.h"
#include "jobmanager.h"
#include "taskmanager.h"
#include "viewmanager.h"
#include "projectionmanager.h"

#include <boost/date_time/posix_time/posix_time.hpp>
#include <qobject.h>

using namespace std;

/**
 * Sets init state, creates members, starts the thread using go.
 */
ATSDB::ATSDB()
 : Configurable ("ATSDB", "ATSDB0", 0, "atsdb.json")
{
    logdbg  << "ATSDB: constructor: start";

    simple_config_.reset (new SimpleConfig ("config.json"));

    JobManager::instance().start();

    createSubConfigurables ();

    assert (db_interface_);
    assert (db_schema_manager_);
    assert (dbo_manager_);
    assert (filter_manager_);
    assert (task_manager_);
    assert (view_manager_);

    QObject::connect (db_schema_manager_.get(), &DBSchemaManager::schemaChangedSignal,
                      dbo_manager_.get(),  &DBObjectManager::updateSchemaInformationSlot);
    //QObject::connect (db_schema_manager_, SIGNAL(schemaLockedSignal()), dbo_manager_, SLOT(schemaLockedSlot()));
    QObject::connect (db_interface_.get(), &DBInterface::databaseContentChangedSignal,
                      db_schema_manager_.get(), &DBSchemaManager::databaseContentChangedSlot, Qt::QueuedConnection);
    QObject::connect (db_interface_.get(), &DBInterface::databaseContentChangedSignal,
                      dbo_manager_.get(), &DBObjectManager::databaseContentChangedSlot, Qt::QueuedConnection);
    //QObject::connect(db_interface_, SIGNAL(databaseOpenedSignal()), filter_manager_, SLOT(databaseOpenedSlot()));

    QObject::connect(dbo_manager_.get(), &DBObjectManager::dbObjectsChangedSignal,
                     task_manager_.get(), &TaskManager::dbObjectsChangedSlot);
    QObject::connect(dbo_manager_.get(), &DBObjectManager::schemaChangedSignal,
                     task_manager_.get(), &TaskManager::schemaChangedSlot);

    logdbg  << "ATSDB: constructor: end";
}

void ATSDB::initialize()
{
    assert (!initialized_);
    initialized_=true;

    dbo_manager_->updateSchemaInformationSlot();
}

/**
 * Deletes members.
 */
ATSDB::~ATSDB()
{
    logdbg  << "ATSDB: destructor: start";

    assert (!initialized_);

    assert (!dbo_manager_);
    assert (!db_schema_manager_);
    assert (!db_interface_);
    assert (!filter_manager_);
    assert (!task_manager_);
    assert (!view_manager_);

    logdbg  << "ATSDB: destructor: end";
}

void ATSDB::generateSubConfigurable (const std::string &class_id, const std::string &instance_id)
{
    logdbg  << "ATSDB: generateSubConfigurable: class_id " << class_id << " instance_id " << instance_id;
    if (class_id == "DBInterface")
    {
        assert (!db_interface_);
        db_interface_.reset(new DBInterface (class_id, instance_id, this));
        assert (db_interface_);
    }
    else if (class_id == "DBObjectManager")
    {
        assert (!dbo_manager_);
        dbo_manager_.reset(new DBObjectManager (class_id, instance_id, this));
        assert (dbo_manager_);
    }
    else if (class_id == "DBSchemaManager")
    {
        assert (db_interface_);
        assert (!db_schema_manager_);
        db_schema_manager_.reset(new DBSchemaManager (class_id, instance_id, this, *db_interface_));
        assert (db_schema_manager_);
    }
    else if (class_id == "FilterManager")
    {
        assert (!filter_manager_);
        filter_manager_.reset(new FilterManager (class_id, instance_id, this));
        assert (filter_manager_);
    }
    else if (class_id == "TaskManager")
    {
        assert (!task_manager_);
        task_manager_.reset(new TaskManager (class_id, instance_id, this));
        assert (task_manager_);
    }
    else if (class_id == "ViewManager")
    {
        assert (!view_manager_);
        view_manager_.reset(new ViewManager (class_id, instance_id, this));
        assert (view_manager_);
    }
    else
        throw std::runtime_error ("ATSDB: generateSubConfigurable: unknown class_id "+class_id );
}

void ATSDB::checkSubConfigurables ()
{
    if (!db_interface_)
    {
        addNewSubConfiguration ("DBInterface", "DBInterface0");
        generateSubConfigurable ("DBInterface", "DBInterface0");
        assert (db_interface_);
    }
    if (!dbo_manager_)
    {
        assert (db_interface_);
        addNewSubConfiguration ("DBObjectManager", "DBObjectManager0");
        generateSubConfigurable ("DBObjectManager", "DBObjectManager0");
        assert (dbo_manager_);
    }
    if (!db_schema_manager_)
    {
        addNewSubConfiguration ("DBSchemaManager", "DBSchemaManager0");
        generateSubConfigurable ("DBSchemaManager", "DBSchemaManager0");
        assert (dbo_manager_);
    }
    if (!filter_manager_)
    {
        addNewSubConfiguration ("FilterManager", "FilterManager0");
        generateSubConfigurable ("FilterManager", "FilterManager0");
        assert (filter_manager_);
    }
    if (!task_manager_)
    {
        addNewSubConfiguration ("TaskManager", "TaskManager0");
        generateSubConfigurable ("TaskManager", "TaskManager0");
        assert (task_manager_);
    }
    if (!view_manager_)
    {
        addNewSubConfiguration ("ViewManager", "ViewManager0");
        generateSubConfigurable ("ViewManager", "ViewManager0");
        assert (view_manager_);
    }
}

DBInterface &ATSDB::interface ()
{
    assert (db_interface_);
    assert (initialized_);
    return *db_interface_;
}

DBSchemaManager &ATSDB::schemaManager ()
{
    assert (db_schema_manager_);
    assert (initialized_);
    return *db_schema_manager_;
}

DBObjectManager &ATSDB::objectManager ()
{
    assert (dbo_manager_);
    assert (initialized_);
    return *dbo_manager_;
}

FilterManager &ATSDB::filterManager ()
{
    assert (filter_manager_);
    assert (initialized_);
    return *filter_manager_;
}

TaskManager& ATSDB::taskManager ()
{
    assert (task_manager_);
    assert (initialized_);
    return *task_manager_;
}

ViewManager &ATSDB::viewManager ()
{
    assert (view_manager_);
    assert (initialized_);
    return *view_manager_;
}

SimpleConfig& ATSDB::config ()
{
    assert (initialized_);
    assert (simple_config_);
    return *simple_config_;
}

bool ATSDB::ready ()
{
    if (!db_interface_ || !initialized_)
        return false;

    return db_interface_->ready();
}

///**
// * Calls stop. If data was written uning the StructureReader, this process is finished correctly.
// * State is set to DB_STATE_SHUTDOWN and ouput buffers are cleared.
// */
void ATSDB::shutdown ()
{
    loginf  << "ATSDB: database shutdown";

    if (!initialized_)
    {
        logerr  << "ATSDB: already shut down";
        return;
    }

    JobManager::instance().shutdown();
    ProjectionManager::instance().shutdown();

    assert (db_interface_);
    db_interface_->closeConnection(); // removes connection widgets, needs to be before

    assert (view_manager_);
    view_manager_->close();
    view_manager_ = nullptr;

    assert (dbo_manager_);
    dbo_manager_ = nullptr;

    assert (db_schema_manager_);
    db_schema_manager_ = nullptr;

    assert (task_manager_);
    task_manager_->shutdown();
    task_manager_ = nullptr;

    assert (db_interface_);
    db_interface_ = nullptr;

    assert (filter_manager_);
    filter_manager_ = nullptr;

    initialized_=false;

    loginf  << "ATSDB: shutdown: end";
}

