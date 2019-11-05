#include "createartasassociationstask.h"
#include "createartasassociationstaskwidget.h"
#include "atsdb.h"
#include "dbinterface.h"
#include "taskmanager.h"
#include "jobmanager.h"
#include "dbobject.h"
#include "dbobjectmanager.h"
#include "dbovariableset.h"
#include "dbovariable.h"
#include "metadbovariable.h"
#include "stringconv.h"
#include "dbodatasource.h"

#include <sstream>

#include <QMessageBox>
#include <QApplication>

using namespace std;
using namespace Utils;

CreateARTASAssociationsTask::CreateARTASAssociationsTask(const std::string& class_id, const std::string& instance_id,
                                                         TaskManager* task_manager)
    : Configurable (class_id, instance_id, task_manager)
{
    registerParameter ("current_data_source_name", &current_data_source_name_, "");

    // tracker vars
    registerParameter ("tracker_ds_id_var_str", &tracker_ds_id_var_str_, "ds_id");
    registerParameter ("tracker_tri_var_str", &tracker_tri_var_str_, "tris_compound");
    registerParameter ("tracker_track_num_var_str", &tracker_track_num_var_str_, "track_num");
    registerParameter ("tracker_track_begin_var_str", &tracker_track_begin_var_str_, "track_created");
    registerParameter ("tracker_track_end_var_str", &tracker_track_end_var_str_, "track_end");

    // meta vars
    registerParameter ("key_var_str", &key_var_str_, "rec_num");
    registerParameter ("hash_var_str", &hash_var_str_, "hash_code");
    registerParameter ("tod_var_str", &tod_var_str_, "tod");
}

CreateARTASAssociationsTask::~CreateARTASAssociationsTask()
{
    if (widget_)
    {
        delete widget_;
        widget_ = nullptr;
    }

}

CreateARTASAssociationsTaskWidget* CreateARTASAssociationsTask::widget()
{
    if (!widget_)
    {
        widget_ = new CreateARTASAssociationsTaskWidget (*this);
    }

    assert (widget_);
    return widget_;
}

bool CreateARTASAssociationsTask::canRun ()
{
    DBObjectManager& object_man = ATSDB::instance().objectManager();

    if (!object_man.existsObject("Tracker"))
        return false;

    DBObject& tracker_object = object_man.object("Tracker");

    // tracker stuff
    if (!tracker_object.loadable())
        return false;

    if (!tracker_object.count())
        return false;

    bool ds_found {false};
    for (auto ds_it = tracker_object.dsBegin(); ds_it != tracker_object.dsEnd(); ++ds_it)
    {
        if (ds_it->second.name() == current_data_source_name_)
        {
            ds_found = true;
            break;
        }
    }
    if (!ds_found)
        return false;

    if (!tracker_object.hasVariable(tracker_tri_var_str_)
            || !tracker_object.hasVariable(tracker_track_num_var_str_)
            || !tracker_object.hasVariable(tracker_track_begin_var_str_)
            || !tracker_object.hasVariable(tracker_track_end_var_str_))
        return false;

    // meta var stuff
    if (!key_var_str_.size()
            || !hash_var_str_.size()
            || !tod_var_str_.size())
        return false;

    if (!object_man.existsMetaVariable(key_var_str_)
            || !object_man.existsMetaVariable(hash_var_str_)
            || !object_man.existsMetaVariable(tod_var_str_))
        return false;

    for (auto& dbo_it : object_man)
    {

        if (!object_man.metaVariable(key_var_str_).existsIn(dbo_it.first)
                || !object_man.metaVariable(hash_var_str_).existsIn(dbo_it.first)
                || !object_man.metaVariable(tod_var_str_).existsIn(dbo_it.first))
            return false;
    }

    return true;
}

void CreateARTASAssociationsTask::run ()
{
    assert (canRun());

    QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));

    std::string msg = "Loading object data.";
    msg_box_ = new QMessageBox;
    assert (msg_box_);
    msg_box_->setWindowTitle("Creating ARTAS Associations");
    //msg_box_->setText(msg.c_str());
    msg_box_->setStandardButtons(QMessageBox::NoButton);

    checkAndSetVariable (tracker_ds_id_var_str_, &tracker_ds_id_var_);
    checkAndSetVariable (tracker_tri_var_str_, &tracker_tri_var_);
    checkAndSetVariable (tracker_track_num_var_str_, &tracker_track_num_var_);
    checkAndSetVariable (tracker_track_begin_var_str_, &tracker_track_begin_var_);
    checkAndSetVariable (tracker_track_end_var_str_, &tracker_track_end_var_);

    checkAndSetMetaVariable (key_var_str_, &key_var_);
    checkAndSetMetaVariable (hash_var_str_, &hash_var_);
    checkAndSetMetaVariable (tod_var_str_, &tod_var_);

    DBObjectManager& object_man = ATSDB::instance().objectManager();

    for (auto& dbo_it : object_man)
    {
        DBOVariableSet read_set = getReadSetFor(dbo_it.first);
        connect (dbo_it.second, &DBObject::newDataSignal, this, &CreateARTASAssociationsTask::newDataSlot);
        connect (dbo_it.second, &DBObject::loadingDoneSignal, this, &CreateARTASAssociationsTask::loadingDoneSlot);

        if (dbo_it.first == "Tracker")
        {
            DBObject& tracker_object = object_man.object("Tracker");

            bool ds_found {false};
            int ds_id{-1};
            for (auto ds_it = tracker_object.dsBegin(); ds_it != tracker_object.dsEnd(); ++ds_it)
            {
                if (ds_it->second.name() == current_data_source_name_)
                {
                    ds_found = true;
                    ds_id = ds_it->first;
                    break;
                }
            }

            assert (ds_found);
            std::string custom_filter_clause {tracker_ds_id_var_str_+" in ("+std::to_string(ds_id)+")"};

            assert (tracker_ds_id_var_);

            //        void DBObject::load (DBOVariableSet& read_set,  std::string custom_filter_clause,
            //                             std::vector <DBOVariable*> filtered_variables, bool use_order,
            //                             DBOVariable* order_variable,
            //                             bool use_order_ascending, const std::string &limit_str)

            dbo_it.second->load (read_set, custom_filter_clause, {tracker_ds_id_var_}, false,
                                 &tod_var_->getFor("Tracker"), false);
        }
        else
            dbo_it.second->load (read_set, false, false, nullptr, false);

        dbo_loading_done_flags_[dbo_it.first] = false;
    }

    updateProgressSlot();
    msg_box_->show();
}

void CreateARTASAssociationsTask::newDataSlot (DBObject& object)
{
    updateProgressSlot();
}

void CreateARTASAssociationsTask::loadingDoneSlot (DBObject& object)
{
    loginf << "CreateARTASAssociationsTask: loadingDoneSlot: object " << object.name();

    disconnect (&object, &DBObject::newDataSignal, this, &CreateARTASAssociationsTask::newDataSlot);
    disconnect (&object, &DBObject::loadingDoneSignal, this, &CreateARTASAssociationsTask::loadingDoneSlot);

    dbo_loading_done_flags_.at(object.name()) = true;

    updateProgressSlot();

    if (dbo_loading_done_)
    {
        assert (!create_job_);

        std::map<std::string, std::shared_ptr<Buffer>> buffers;

        DBObjectManager& object_man = ATSDB::instance().objectManager();

        for (auto& dbo_it : object_man)
        {
            buffers[dbo_it.first] = dbo_it.second->data();
            dbo_it.second->clearData();
        }


        create_job_ = std::make_shared<CreateARTASAssociationsJob> (*this, ATSDB::instance().interface(), buffers);

        connect (create_job_.get(), &CreateARTASAssociationsJob::doneSignal,
                 this, &CreateARTASAssociationsTask::createDoneSlot, Qt::QueuedConnection);
        connect (create_job_.get(), &CreateARTASAssociationsJob::obsoleteSignal,
                 this, &CreateARTASAssociationsTask::createObsoleteSlot,
                 Qt::QueuedConnection);

        JobManager::instance().addDBJob(create_job_);
    }

    updateProgressSlot();
}

void CreateARTASAssociationsTask::createDoneSlot ()
{
    loginf << "CreateARTASAssociationsTask: createDoneSlot";

    create_job_done_ = true;
    updateProgressSlot();
    create_job_ = nullptr;
}

void CreateARTASAssociationsTask::createObsoleteSlot ()
{
    create_job_ = nullptr;
}


void CreateARTASAssociationsTask::updateProgressSlot()
{
    dbo_loading_done_ = true;

    assert (msg_box_);
    stringstream ss;

    ss << "DBObject Loading:\n";

    for (auto& done_it : dbo_loading_done_flags_)
    {
        if (done_it.second)
        {
            ss << "  " << done_it.first << ": Done\n";
        }
        else
        {
            ss << "  " << done_it.first << ": In Progress\n";
            dbo_loading_done_ = false;
        }
    }

    ss << "\nCreating Associations: ";

    if (create_job_done_)
    {
        ss << "Done\n";

        assert (create_job_);

        ss << "Created Associations: " << create_job_->foundHashes() << "\n";
        ss << "Missing Hashes at beginning: " << create_job_->missingHashesAtBeginning() << "\n";
        ss << "Missing Hashes not at beginning: " << create_job_->missingHashes() << "\n";
        ss << "Duplicate Hashes: " << create_job_->foundDuplicates() << "\n\n";

        for (auto& dbo_it : ATSDB::instance().objectManager())
        {
            ss << dbo_it.first << ": Associated " << dbo_it.second->associations().size()
               << "/" << dbo_it.second->count();

            if (dbo_it.second->count())
                ss << " (" << String::percentToString(
                          100.0*dbo_it.second->associations().size()/dbo_it.second->count()) << "%)\n";
            else
                ss << "\n";
        }

        assert (msg_box_);

        QApplication::restoreOverrideCursor();

        msg_box_->setText(ss.str().c_str());
        msg_box_->setStandardButtons(QMessageBox::Ok);
        msg_box_->exec();

        delete msg_box_;
        msg_box_ = nullptr;

        if (widget_)
            widget_->runDoneSlot();
    }
    else
    {
        if (create_job_)
            ss << "In Progress\n";
        else {
            ss << "Waiting\n";
        }
        msg_box_->setText(ss.str().c_str());
    }
}

std::string CreateARTASAssociationsTask::currentDataSourceName() const
{
    return current_data_source_name_;
}

void CreateARTASAssociationsTask::currentDataSourceName(const std::string& current_data_source_name)
{
    loginf << "CreateARTASAssociationsTask: currentDataSourceName: " << current_data_source_name;

    current_data_source_name_ = current_data_source_name;
}

DBOVariable *CreateARTASAssociationsTask::trackerDsIdVar() const
{
    return tracker_ds_id_var_;
}

std::string CreateARTASAssociationsTask::trackerDsIdVarStr() const
{
    return tracker_ds_id_var_str_;
}

void CreateARTASAssociationsTask::trackerDsIdVarStr(const std::string& tracker_ds_id_var_str)
{
    loginf << "CreateARTASAssociationsTask: trackerDsIdVarStr: '" << tracker_ds_id_var_str << "'";
    tracker_ds_id_var_str_ = tracker_ds_id_var_str;
}


std::string CreateARTASAssociationsTask::trackerTRIVarStr() const
{
    return tracker_tri_var_str_;
}

void CreateARTASAssociationsTask::trackerTRIVarStr(const std::string& tracker_tri_var_str)
{
    loginf << "CreateARTASAssociationsTask: trackerTRIVarStr: '" << tracker_tri_var_str << "'";

    tracker_tri_var_str_ = tracker_tri_var_str;
}

std::string CreateARTASAssociationsTask::trackerTrackNumVarStr() const
{
    return tracker_track_num_var_str_;
}

void CreateARTASAssociationsTask::trackerTrackNumVarStr(const std::string& tracker_track_num_var_str)
{
    tracker_track_num_var_str_ = tracker_track_num_var_str;
}

std::string CreateARTASAssociationsTask::trackerTrackBeginVarStr() const
{
    return tracker_track_begin_var_str_;
}

void CreateARTASAssociationsTask::trackerTrackBeginVarStr(const std::string& tracker_track_begin_var_str)
{
    tracker_track_begin_var_str_ = tracker_track_begin_var_str;
}

std::string CreateARTASAssociationsTask::trackerTrackEndVarStr() const
{
    return tracker_track_end_var_str_;
}

void CreateARTASAssociationsTask::trackerTrackEndVarStr(const std::string& tracker_track_end_var_str)
{
    tracker_track_end_var_str_ = tracker_track_end_var_str;
}

std::string CreateARTASAssociationsTask::keyVarStr() const
{
    return key_var_str_;
}

void CreateARTASAssociationsTask::keyVarStr(const std::string& key_var_str)
{
    loginf << "CreateARTASAssociationsTask: keyVarStr: '" << key_var_str << "'";

    key_var_str_ = key_var_str;
}

std::string CreateARTASAssociationsTask::hashVarStr() const
{
    return hash_var_str_;
}

void CreateARTASAssociationsTask::hashVarStr(const std::string& hash_var_str)
{
    loginf << "CreateARTASAssociationsTask: hashVarStr: '" << hash_var_str << "'";

    hash_var_str_ = hash_var_str;
}

std::string CreateARTASAssociationsTask::todVarStr() const
{
    return tod_var_str_;
}

void CreateARTASAssociationsTask::todVarStr(const std::string& tod_var_str)
{
    loginf << "CreateARTASAssociationsTask: todVarStr: '" << tod_var_str << "'";

    tod_var_str_ = tod_var_str;
}

MetaDBOVariable *CreateARTASAssociationsTask::keyVar() const
{
    return key_var_;
}

MetaDBOVariable *CreateARTASAssociationsTask::hashVar() const
{
    return hash_var_;
}

MetaDBOVariable *CreateARTASAssociationsTask::todVar() const
{
    return tod_var_;
}

void CreateARTASAssociationsTask::checkAndSetVariable (std::string& name_str, DBOVariable** var)
{
    DBObjectManager& object_man = ATSDB::instance().objectManager();
    DBObject& object = object_man.object("Tracker");

    if (!object.hasVariable(name_str))
    {
        loginf << "CreateARTASAssociationsTask: checkAndSetVariable: var " << name_str << " does not exist";
        name_str = "";
        var = nullptr;
    }
    else
    {
        *var = &object.variable(name_str);
        loginf << "CreateARTASAssociationsTask: checkAndSetVariable: var " << name_str << " set";
        assert (var);
        assert((*var)->existsInDB());
    }
}

void CreateARTASAssociationsTask::checkAndSetMetaVariable (std::string& name_str, MetaDBOVariable** var)
{
    DBObjectManager& object_man = ATSDB::instance().objectManager();

    if (!object_man.existsMetaVariable(name_str))
    {
        loginf << "CreateARTASAssociationsTask: checkAndSetMetaVariable: var " << name_str << " does not exist";
        name_str = "";
        var = nullptr;
    }
    else
    {
        *var = &object_man.metaVariable(name_str);
        loginf << "CreateARTASAssociationsTask: checkAndSetMetaVariable: var " << name_str << " set";
        assert (var);
    }
}

DBOVariableSet CreateARTASAssociationsTask::getReadSetFor (const std::string& dbo_name)
{
    DBOVariableSet read_set;

    assert (key_var_);
    assert (key_var_->existsIn(dbo_name));
    read_set.add(key_var_->getFor(dbo_name));

    assert (hash_var_);
    assert (hash_var_->existsIn(dbo_name));
    read_set.add(hash_var_->getFor(dbo_name));

    assert (tod_var_);
    assert (tod_var_->existsIn(dbo_name));
    read_set.add(tod_var_->getFor(dbo_name));

    if (dbo_name == "Tracker")
    {
        assert (tracker_tri_var_);
        read_set.add(*tracker_tri_var_);

        assert (tracker_track_num_var_);
        read_set.add(*tracker_track_num_var_);

        assert (tracker_track_begin_var_);
        read_set.add(*tracker_track_begin_var_);

        assert (tracker_track_end_var_);
        read_set.add(*tracker_track_end_var_);
    }

    return read_set;
}