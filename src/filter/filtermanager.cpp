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
 * FilterManager.cpp
 *
 *  Created on: Jan 15, 2012
 *      Author: sk
 */

#include "configurationmanager.h"
#include "dbfilter.h"
#include "dbobject.h"
#include "dbobjectmanager.h"
#include "dbovariable.h"
#include "atsdb.h"
#include "filtermanager.h"
#include "logger.h"
#include "dbinterface.h"
#include "dbconnection.h"
#include "filtermanagerwidget.h"
//#include "SensorFilter.h"


FilterManager::FilterManager(const std::string &class_id, const std::string &instance_id, ATSDB *atsdb)
: Configurable (class_id, instance_id, atsdb, "conf/config_filter.xml"), widget_(nullptr)
{
    logdbg  << "FilterManager: constructor";

    registerParameter ("db_id", &db_id_, "");
}

FilterManager::~FilterManager()
{
    for (unsigned int cnt=0; cnt < filters_.size(); cnt++)
        delete filters_.at(cnt);
    filters_.clear();

    if (widget_)
    {
        delete widget_;
        widget_ = nullptr;
    }
}

void FilterManager::generateSubConfigurable (const std::string &class_id, const std::string &instance_id)
{
    if (class_id == "DBFilter")
    {
        DBFilter *filter = new DBFilter (class_id, instance_id, this);
        filters_.push_back (filter);
    }
    // TODO
//    else if (class_id.compare ("SensorFilter") == 0)
//    {
//        SensorFilter *filter = new SensorFilter (class_id, instance_id, this);
//        filters_.push_back (filter);
//    }
    else
        throw std::runtime_error ("FilterManager: generateSubConfigurable: unknown class_id "+class_id );
}
void FilterManager::checkSubConfigurables ()
{

    if (filters_.size() == 0)
    {
//        loginf << "FilterManager: checkSubConfigurables: generating sensor filters";
//        // sensor filters
//        const std::map <std::string, DBObject*> &objects =  DBObjectManager::getInstance().getDBObjects ();
//        std::map <std::string, DBObject*>::const_iterator it;

//        for (it = objects.begin(); it != objects.end(); it++)
//        {
//            if (!it->second->hasCurrentDataSource())
//                continue;

//            std::string instance_id = "Sensors"+it->second->getName();
//            Configuration &sensorfilter_configuration = addNewSubConfiguration ("SensorFilter", instance_id);
//            sensorfilter_configuration.addParameterString ("dbo_type", it->first);
//            generateSubConfigurable ("SensorFilter", instance_id);
//        }

    }
}

std::string FilterManager::getSQLCondition (const std::string &dbo_name, std::vector <DBOVariable*>& filtered_variables)
{
    assert (ATSDB::instance().objectManager().object(dbo_name).loadable());

    std::stringstream ss;

    bool first=true;

    for (unsigned int cnt=0; cnt < filters_.size(); cnt++)
    {
        if (filters_.at(cnt)->getActive())
        {
            ss << filters_.at(cnt)->getConditionString (dbo_name, first, filtered_variables);
        }
    }

    logdbg  << "FilterManager: getSQLCondition: name " << dbo_name << " '" << ss.str() << "'";
    return ss.str();
}


unsigned int FilterManager::getNumFilters ()
{
    return filters_.size();
}

DBFilter *FilterManager::getFilter (unsigned int index)
{
    assert (index < filters_.size());

    return filters_.at(index);
}

void FilterManager::reset ()
{
    for (unsigned int cnt=0; cnt < filters_.size(); cnt++)
    {
        filters_.at(cnt)->reset();
    }
}

void FilterManager::deleteFilterSlot (DBFilter *filter)
{
    std::vector <DBFilter*>::iterator it;

    it = find (filters_.begin(), filters_.end(), filter);
    if (it == filters_.end())
        throw std::runtime_error ("FilterManager: deleteFilter: called with unknown filter");
    else
    {
        filters_.erase (it);
        delete filter;
    }

    emit changedFiltersSignal();
}

FilterManagerWidget *FilterManager::widget ()
{
    if (!widget_)
    {
        widget_ = new FilterManagerWidget (*this);
        connect (this, SIGNAL(changedFiltersSignal()), widget_, SLOT(updateFiltersSlot()));
    }

    assert (widget_);
    return widget_;
}

void FilterManager::databaseOpenedSlot ()
{
    createSubConfigurables ();

    std::string tmpstr = ATSDB::instance().interface().connection().identifier();
    replace(tmpstr.begin(), tmpstr.end(), ' ', '_');

    if (db_id_.compare (tmpstr) != 0)
    {
        loginf  << "FilterManager: constructor: different db id, resetting filters";
        reset();
        db_id_ = tmpstr;
    }

    emit changedFiltersSignal();

    if (widget_)
        widget_->databaseOpenedSlot();
}
