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

#include "jsonimporttask.h"
#include "jsonimporttaskwidget.h"
#include "taskmanager.h"
#include "dbobject.h"
#include "dbobjectmanager.h"
#include "dbovariable.h"
#include "savedfile.h"
#include "files.h"
#include "stringconv.h"
#include "metadbtable.h"
#include "dbtable.h"
#include "dbtablecolumn.h"
#include "propertylist.h"
#include "buffer.h"
#include "jobmanager.h"
#include "jsonparsejob.h"
#include "jsonmappingjob.h"
#include "atsdb.h"
#include "dbinterface.h"
#include "radarplotpositioncalculatortask.h"
#include "createartasassociationstask.h"

#include <stdexcept>
#include <fstream>
#include <memory>
#include <algorithm>

#include <QDateTime>
#include <QCoreApplication>
#include <QThread>
#include <QMessageBox>
#include <QApplication>

using namespace Utils;
using namespace nlohmann;
using namespace std;

const unsigned int num_objects_chunk = 10000;

const std::string DONE_PROPERTY_NAME = "json_data_imported";

JSONImportTask::JSONImportTask(const std::string& class_id, const std::string& instance_id,
                                   TaskManager& task_manager)
    : Task("JSONImportTask", "Import JSON Data", true, false, task_manager),
      Configurable (class_id, instance_id, &task_manager, "task_import_json.json")
{
    registerParameter("current_filename", &current_filename_, "");
    registerParameter("current_schema", &current_schema_, "");

    createSubConfigurables();
}

JSONImportTask::~JSONImportTask()
{
    if (msg_box_)
    {
        delete msg_box_;
        msg_box_ = nullptr;
    }

    for (auto it : file_list_)
        delete it.second;

    file_list_.clear();
}

void JSONImportTask::generateSubConfigurable (const std::string &class_id, const std::string &instance_id)
{
    if (class_id == "JSONFile")
    {
        SavedFile *file = new SavedFile (class_id, instance_id, this);
        assert (file_list_.count (file->name()) == 0);
        file_list_.insert (std::pair <std::string, SavedFile*> (file->name(), file));
    }
    else if (class_id == "JSONParsingSchema")
    {
        std::string name = configuration().getSubConfiguration(
                    class_id, instance_id).getParameterConfigValueString("name");

        assert (schemas_.find (name) == schemas_.end());

        logdbg << "JSONImporterTask: generateSubConfigurable: generating schema " << instance_id
               << " with name " << name;

        schemas_.emplace(std::piecewise_construct,
                     std::forward_as_tuple(name),  // args for key
                     std::forward_as_tuple(class_id, instance_id, this));  // args for mapped value
    }
    else
        throw std::runtime_error ("JSONImporterTask: generateSubConfigurable: unknown class_id "+class_id );
}


TaskWidget* JSONImportTask::widget()
{
    if (!widget_)
    {
        widget_.reset(new JSONImportTaskWidget (*this));

        connect (&task_manager_, &TaskManager::expertModeChangedSignal,
                 widget_.get(), &JSONImportTaskWidget::expertModeChangedSlot);
    }

    assert (widget_);
    return widget_.get();
}

void JSONImportTask::deleteWidget ()
{
    widget_.reset(nullptr);
}

void JSONImportTask::addFile (const std::string &filename)
{
    loginf << "JSONImporterTask: addFile: filename '" << filename << "'";

    if (file_list_.count (filename) != 0)
        throw std::invalid_argument ("JSONImporterTask: addFile: name '"+filename+"' already in use");

    std::string instancename = filename;
    instancename.erase (std::remove(instancename.begin(), instancename.end(), '/'), instancename.end());

    Configuration &config = addNewSubConfiguration ("JSONFile", "JSONFile"+instancename);
    config.addParameterString("name", filename);
    generateSubConfigurable ("JSONFile", "JSONFile"+instancename);

    current_filename_ = filename; // set as current

    emit statusChangedSignal(name_);

    if (widget_)
        widget_->updateFileListSlot();
}

void JSONImportTask::removeCurrentFilename ()
{
    loginf << "JSONImporterTask: removeCurrentFilename: filename '" << current_filename_ << "'";

    assert (current_filename_.size());
    assert (hasFile(current_filename_));

    if (file_list_.count (current_filename_) != 1)
        throw std::invalid_argument ("JSONImporterTask: addFile: name '"+current_filename_+"' not in use");

    delete file_list_.at(current_filename_);
    file_list_.erase(current_filename_);
    current_filename_ = "";

    emit statusChangedSignal(name_);

    if (widget_)
        widget_->updateFileListSlot();
}

void JSONImportTask::currentFilename (const std::string filename, bool archive)
{

    loginf << "JSONImporterTask: currentFilename: filename '" << filename << "' archive " << archive;

    archive_ = archive;
    current_filename_ = filename;

    emit statusChangedSignal(name_);
}

bool JSONImportTask::hasCurrentSchema ()
{
    if (!current_schema_.size())
        return false;

    return schemas_.count(current_schema_) > 0;
}

JSONParsingSchema& JSONImportTask::currentSchema()
{
    assert (hasCurrentSchema());
    return schemas_.at(current_schema_);
}

void JSONImportTask::removeCurrentSchema ()
{
    assert (hasSchema(current_schema_));
    schemas_.erase(current_schema_);
    assert (!hasSchema(current_schema_));

    current_schema_ = "";

    if (schemas_.size())
        current_schema_ = schemas_.begin()->first;

    loginf << "JSONImporterTask: removeCurrentSchema: set current schema '" << currentSchemaName() << "'";

    emit statusChangedSignal(name_);
}

std::string JSONImportTask::currentSchemaName() const
{
    return current_schema_;
}

void JSONImportTask::currentSchemaName(const std::string& current_schema)
{
    current_schema_ = current_schema;

    emit statusChangedSignal(name_);
}

bool JSONImportTask::checkPrerequisites ()
{
    if (!ATSDB::instance().interface().ready())  // must be connected
        return false;

    if (ATSDB::instance().interface().hasProperty(DONE_PROPERTY_NAME))
        done_ = ATSDB::instance().interface().getProperty(DONE_PROPERTY_NAME) == "1";

    return true;
}

bool JSONImportTask::isRecommended ()
{
    if (!checkPrerequisites())
        return false;

    if (ATSDB::instance().objectManager().hasData())
        return false;

    return true;
}

bool JSONImportTask::isRequired ()
{
    return false;
}

void JSONImportTask::test(bool test)
{
    test_ = test;
}

bool JSONImportTask::canImportFile ()
{
    if (!current_filename_.size())
        return false;

    if (!Files::fileExists(current_filename_))
    {
        loginf << "JSONImporterTask: canImportFile: not possible since file '" << current_filename_ << "does not exist";
        return false;
    }

    if (!current_schema_.size())
        return false;

    if (!schemas_.count(current_schema_))
    {
        current_schema_ = "";
        return false;
    }

    return true;
}

bool JSONImportTask::canRun()
{
    return canImportFile();
}


void JSONImportTask::run()
{
    loginf << "JSONImporterTask: run: filename '" << current_filename_ << "' archive " << archive_ << " test " << test_;

    std::string tmp;

    if (test_)
        tmp = "test import of";
    else
        tmp = "import of ";

    if (archive_)
        tmp += " archive ";
    else
        tmp += " file ";

    task_manager_.appendInfo("JSONImporterTask: "+tmp+" '"+current_filename_+"' started");

    if (widget_)
        widget_->runStarted();

    start_time_ = boost::posix_time::microsec_clock::local_time();

    assert (canImportFile());

    objects_read_ = 0;
    objects_parsed_ = 0;
    objects_parse_errors_ = 0;

    objects_mapped_ = 0;
    objects_not_mapped_ = 0;

    objects_created_ = 0;
    objects_inserted_ = 0;

    all_done_ = false;

    assert (schemas_.count(current_schema_));
    for (auto& map_it : schemas_.at(current_schema_))
        if (!map_it.second.initialized())
            map_it.second.initialize();


    QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));

    read_json_job_ = std::make_shared<ReadJSONFilePartJob> (current_filename_, archive_, num_objects_chunk);

    connect (read_json_job_.get(), &ReadJSONFilePartJob::obsoleteSignal,
             this, &JSONImportTask::readJSONFilePartObsoleteSlot, Qt::QueuedConnection);
    connect (read_json_job_.get(), &ReadJSONFilePartJob::doneSignal,
             this, &JSONImportTask::readJSONFilePartDoneSlot, Qt::QueuedConnection);

    JobManager::instance().addNonBlockingJob(read_json_job_);

    updateMsgBox();

    logdbg << "JSONImporterTask: run done";
}

void JSONImportTask::readJSONFilePartDoneSlot ()
{
    loginf << "JSONImporterTask: readJSONFilePartDoneSlot";

    assert (read_json_job_);

    loginf << "JSONImporterTask: readJSONFilePartDoneSlot: moving objects";
    std::vector <std::string> objects = read_json_job_->objects();
    //assert (!read_json_job_->objects().size());

    bytes_read_ = read_json_job_->bytesRead();
    bytes_to_read_ = read_json_job_->bytesToRead();
    read_status_percent_ = read_json_job_->getStatusPercent();
    objects_read_ += objects.size();
    loginf << "JSONImporterTask: readJSONFilePartDoneSlot: bytes " << bytes_read_ << " to read " << bytes_to_read_
           << " percent " << read_status_percent_;

    //loginf << "got part '" << ss.str() << "'";

    // restart read job
    if (!read_json_job_->fileReadDone())
    {
        loginf << "JSONImporterTask: readJSONFilePartDoneSlot: read continue";
        read_json_job_->resetDone();
        JobManager::instance().addNonBlockingJob(read_json_job_);
    }
    else
    {
        loginf << "JSONImporterTask: readJSONFilePartDoneSlot: done";
        task_manager_.appendInfo("JSONImporterTask: reading done with "+std::to_string(objects_read_)+" records");
        read_status_percent_ = 100.0;
        read_json_job_ = nullptr;
    }

    // start parse job
    loginf << "JSONImporterTask: readJSONFilePartDoneSlot: starting parse job";
    std::shared_ptr<JSONParseJob> json_parse_job = std::make_shared<JSONParseJob> (std::move(objects));

    connect (json_parse_job.get(), &JSONParseJob::obsoleteSignal,
             this, &JSONImportTask::parseJSONObsoleteSlot, Qt::QueuedConnection);
    connect (json_parse_job.get(), &JSONParseJob::doneSignal,
             this, &JSONImportTask::parseJSONDoneSlot,Qt::QueuedConnection);

    JobManager::instance().addNonBlockingJob(json_parse_job);

    json_parse_jobs_.push_back(json_parse_job);

    loginf << "JSONImporterTask: readJSONFilePartDoneSlot: updating message box";
    updateMsgBox();

    logdbg << "JSONImporterTask: readJSONFilePartDoneSlot: done";
}

void JSONImportTask::readJSONFilePartObsoleteSlot ()
{
    logdbg << "JSONImporterTask: readJSONFilePartObsoleteSlot";
}

void JSONImportTask::parseJSONDoneSlot ()
{
    logdbg << "JSONImporterTask: parseJSONDoneSlot";

    JSONParseJob* parse_job = dynamic_cast<JSONParseJob*>(QObject::sender());
    assert (parse_job);

    objects_parsed_ += parse_job->objectsParsed();
    objects_parse_errors_ += parse_job->parseErrors();
    std::unique_ptr<std::vector<nlohmann::json>> json_objects {std::move(parse_job->jsonObjects())};

    json_parse_jobs_.erase(json_parse_jobs_.begin());

    logdbg << "JSONImporterTask: parseJSONDoneSlot: " << json_objects->size() << " parsed objects";

    size_t count = json_objects->size();

    assert (schemas_.count(current_schema_));

    std::shared_ptr<JSONMappingJob> json_map_job = std::make_shared<JSONMappingJob> (
                std::move(json_objects), schemas_.at(current_schema_).parsers(), key_count_);

    connect (json_map_job.get(), &JSONMappingJob::obsoleteSignal,
             this, &JSONImportTask::mapJSONObsoleteSlot, Qt::QueuedConnection);
    connect (json_map_job.get(), &JSONMappingJob::doneSignal,
             this, &JSONImportTask::mapJSONDoneSlot, Qt::QueuedConnection);

    json_map_jobs_.push_back(json_map_job);

    JobManager::instance().addNonBlockingJob(json_map_job);

    key_count_ += count;

    updateMsgBox();

    logdbg << "JSONImporterTask: parseJSONDoneSlot: done";
}

void JSONImportTask::parseJSONObsoleteSlot ()
{
    logdbg << "JSONImporterTask: parseJSONObsoleteSlot";
}

void JSONImportTask::mapJSONDoneSlot ()
{
    loginf << "JSONImporterTask: mapJSONDoneSlot";

    JSONMappingJob* map_job = dynamic_cast<JSONMappingJob*>(QObject::sender());
    assert (map_job);

    loginf << "JSONImporterTask: mapJSONDoneSlot: skipped " << map_job->numNotMapped()
           << " all skipped " << objects_not_mapped_;

    objects_mapped_ += map_job->numMapped(); // TODO done twice?
    objects_not_mapped_ += map_job->numNotMapped();

    objects_created_ += map_job->numCreated();

    std::map <std::string, std::shared_ptr<Buffer>> job_buffers = map_job->buffers();

    json_map_jobs_.erase(json_map_jobs_.begin());

    for (auto& buf_it : job_buffers)
        if (buf_it.second && buf_it.second->size())
            objects_mapped_ += buf_it.second->size();

    if (test_ || !objects_mapped_)
    {
        checkAllDone();
        updateMsgBox();
        return;
    }

    for (auto& buf_it : job_buffers)
    {
        if (buf_it.second && buf_it.second->size())
        {
            std::shared_ptr<Buffer> job_buffer = buf_it.second;

            if (buffers_.count(buf_it.first) == 0)
                buffers_[buf_it.first] = job_buffer;
            else
                buffers_.at(buf_it.first)->seizeBuffer(*job_buffer.get());
        }
    }

    updateMsgBox();

    if (!insert_active_)
    {
        for (auto& buf_it : buffers_)
        {
            if (buf_it.second->size() > 10000)
            {
                loginf << "JSONImporterTask: mapJSONDoneSlot: inserting part of parsed objects";
                insertData ();
                return;
            }
        }
    }

    if (read_json_job_ == nullptr && json_parse_jobs_.size() == 0 && json_map_jobs_.size() == 0)
    {
        loginf << "JSONImporterTask: mapJSONDoneSlot: inserting parsed objects at end";
        insertData ();
    }

    logdbg << "JSONImporterTask: mapJSONDoneSlot: done";
}

void JSONImportTask::mapJSONObsoleteSlot ()
{
    logdbg << "JSONImporterTask: mapJSONObsoleteSlot";
}

void JSONImportTask::insertData ()
{
    loginf << "JSONImporterTask: insertData: inserting into database";

    while (insert_active_)
    {
        QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
        QThread::msleep (1);
    }

    bool has_sac_sic = false;

    assert (schemas_.count(current_schema_));

    for (auto& parser_it : schemas_.at(current_schema_))
    {
        if (buffers_.count(parser_it.second.dbObject().name()) != 0)
        {
            ++insert_active_;

            DBObject& db_object = parser_it.second.dbObject();
            std::shared_ptr<Buffer> buffer = buffers_.at(parser_it.second.dbObject().name());

            has_sac_sic = db_object.hasVariable("sac") && db_object.hasVariable("sic")
                    && buffer->has<char>("sac") && buffer->has<char>("sic");

            logdbg << "JSONImporterTask: insertData: " << db_object.name() << " has sac/sic " << has_sac_sic;

            logdbg << "JSONImporterTask: insertData: " << db_object.name() << " buffer " << buffer->size();

            connect (&db_object, &DBObject::insertDoneSignal, this, &JSONImportTask::insertDoneSlot,
                     Qt::UniqueConnection);
            connect (&db_object, &DBObject::insertProgressSignal, this, &JSONImportTask::insertProgressSlot,
                     Qt::UniqueConnection);


            if (parser_it.second.dataSourceVariableName() != "")
            {
                logdbg << "JSONImporterTask: insertData: adding new data sources";

                std::string data_source_var_name = parser_it.second.dataSourceVariableName();


                // collect existing datasources
                std::set <int> datasources_existing;
                if (db_object.hasDataSources())
                    for (auto ds_it = db_object.dsBegin(); ds_it != db_object.dsEnd(); ++ds_it)
                        datasources_existing.insert(ds_it->first);

                // getting key list and distinct values
                assert (buffer->properties().hasProperty(data_source_var_name));
                assert (buffer->properties().get(data_source_var_name).dataType() == PropertyDataType::INT);

                assert(buffer->has<int>(data_source_var_name));
                NullableVector<int>& data_source_key_list = buffer->get<int> (data_source_var_name);
                std::set<int> data_source_keys = data_source_key_list.distinctValues();

                std::map <int, std::pair<char, char>> sac_sics; // keyvar->(sac,sic)
                // collect sac/sics
                if (has_sac_sic)
                {
                    NullableVector<char>& sac_list = buffer->get<char> ("sac");
                    NullableVector<char>& sic_list = buffer->get<char> ("sic");

                    size_t size = buffer->size();
                    int key_val;
                    for (unsigned int cnt=0; cnt < size; ++cnt)
                    {
                        key_val = data_source_key_list.get(cnt);

                        if (datasources_existing.count(key_val) != 0)
                            continue;

                        if (sac_sics.count(key_val) == 0)
                        {
                            logdbg << "JSONImporterTask: insertData: found new ds " << key_val << " for sac/sic";

                            assert (!sac_list.isNull(cnt) && !sic_list.isNull(cnt));
                            sac_sics[key_val] = std::pair<char, char> (sac_list.get(cnt), sic_list.get(cnt));

                            loginf << "JSONImporterTask: insertData: source " << key_val
                                   << " sac " << static_cast<int>(sac_list.get(cnt))
                                   << " sic " << static_cast<int>(sic_list.get(cnt));
                        }
                    }

                }

                // adding datasources
                std::map <int, std::pair<int,int>> datasources_to_add;

                for (auto ds_key_it : data_source_keys)
                    if (datasources_existing.count(ds_key_it) == 0 && added_data_sources_.count(ds_key_it) == 0)
                    {
                        if (datasources_to_add.count(ds_key_it) == 0)
                        {
                            logdbg << "JSONImporterTask: insertData: adding new data source " << ds_key_it;
                            if (sac_sics.count(ds_key_it) == 0)
                                datasources_to_add[ds_key_it] = {-1,-1};
                            else
                                datasources_to_add[ds_key_it] = {sac_sics.at(ds_key_it).first,
                                                                 sac_sics.at(ds_key_it).second};

                            added_data_sources_.insert(ds_key_it);
                        }
                    }

                if (datasources_to_add.size())
                {
                    db_object.addDataSources(datasources_to_add);
                }
            }

            logdbg << "JSONImporterTask: insertData: " << db_object.name() << " inserting";

            DBOVariableSet set = parser_it.second.variableList();
            db_object.insertData(set, buffer, false);
            objects_inserted_ += buffer->size();

            logdbg << "JSONImporterTask: insertData: " << db_object.name() << " clearing";
            buffers_.erase(parser_it.second.dbObject().name());
        }
        else
            logdbg << "JSONImporterTask: insertData: emtpy buffer for " << parser_it.second.dbObject().name();
    }

    assert (buffers_.size() == 0);

    logdbg << "JSONImporterTask: insertData: done";
}

void JSONImportTask::checkAllDone ()
{
    logdbg << "JSONImporterTask: checkAllDone";

    loginf << "JSONImporterTask: checkAllDone: all done " << all_done_ << " read " << (read_json_job_ == nullptr)
           << " parse jobs " << json_parse_jobs_.empty() << " map jobs " << json_map_jobs_.empty()
           << " insert active " << (insert_active_ == 0);

    if (!all_done_ && read_json_job_ == nullptr && json_parse_jobs_.size() == 0 && json_map_jobs_.size() == 0
            && insert_active_ == 0)
    {
        stop_time_ = boost::posix_time::microsec_clock::local_time();

        boost::posix_time::time_duration diff = stop_time_ - start_time_;

        std::string time_str = String::timeStringFromDouble(diff.total_milliseconds()/1000.0, false);

        loginf << "JSONImporterTask: checkAllDone: read done after " << time_str;

        all_done_ = true;
        assert (buffers_.size() == 0);

        QApplication::restoreOverrideCursor();

        if (!test_)
            emit ATSDB::instance().interface().databaseContentChangedSignal();

        if (widget_)
            widget_->runDone();

        double records_per_second = objects_inserted_/(diff.total_milliseconds()/1000.0);

        task_manager_.appendInfo("JSONImporterTask: inserted "+std::to_string(objects_inserted_)+" records ("
                                 +std::to_string(static_cast<int>(records_per_second))+" rec/s)");

        if (test_)
            task_manager_.appendSuccess("JSONImporterTask: import test done after "+time_str);
        else
        {
            task_manager_.appendSuccess("JSONImporterTask: import done after "+time_str);
            done_ = true;

            // in case data was imported, clear other task done properties
            ATSDB::instance().interface().setProperty(RadarPlotPositionCalculatorTask::DONE_PROPERTY_NAME, "0");
            ATSDB::instance().interface().setProperty(CreateARTASAssociationsTask::DONE_PROPERTY_NAME, "0");

            ATSDB::instance().interface().setProperty(DONE_PROPERTY_NAME, "1");

            emit doneSignal(name_);
        }

        test_ = false; // is set again in case of test import
    }

    logdbg << "JSONImporterTask: checkAllDone: done";
}

void JSONImportTask::updateMsgBox ()
{
    logdbg << "JSONImporterTask: updateMsgBox";

    if (!msg_box_)
    {
        msg_box_ = new QMessageBox ();

        if (test_)
            msg_box_->setWindowTitle("Test Import JSON Data Status");
        else
            msg_box_->setWindowTitle("Import JSON Data Status");

        assert (msg_box_);
    }

    std::string msg;

    if (archive_)
        msg = "Archive '"+current_filename_+"'\n";
    else
        msg = "File '"+current_filename_+"'\n";

    stop_time_ = boost::posix_time::microsec_clock::local_time();

    boost::posix_time::time_duration diff = stop_time_ - start_time_;

    std::string elapsed_time_str = String::timeStringFromDouble(diff.total_milliseconds()/1000.0, false);

    // calculate insert rate
    double objects_per_second = 0.0;
    bool objects_per_second_updated = false;
    if (objects_inserted_ && statistics_calc_objects_inserted_ != objects_inserted_)
    {
        objects_per_second = objects_inserted_/(diff.total_milliseconds()/1000.0);
        objects_per_second_updated = true;

        statistics_calc_objects_inserted_ = objects_inserted_;
        object_rate_str_ = std::to_string(static_cast<int>(objects_per_second));
    }

    // calculate remaining time
    if (objects_per_second_updated && bytes_to_read_ && objects_parsed_ && objects_mapped_)
    {
        double avg_time_per_obj_s = 1.0/objects_per_second;

        double avg_mapped_obj_bytes = static_cast<double>(bytes_read_)/static_cast<double>(objects_mapped_);
        double num_obj_total = static_cast<double>(bytes_to_read_)/avg_mapped_obj_bytes;

        double remaining_obj_num = 0.0;

        if (objects_not_mapped_ < objects_parsed_) // skipped objects ok
        {
            double not_skipped_ratio =
                    static_cast<double>(objects_parsed_-objects_not_mapped_)/static_cast<double>(objects_parsed_);
            remaining_obj_num = (num_obj_total*not_skipped_ratio)-objects_inserted_;

    //        loginf << "UGA avg bytes " << avg_obj_bytes << " num total " << num_obj_total << " not skipped ratio "
    //               << not_skipped_ratio << " all mapped " << num_obj_total*not_skipped_ratio
    //               << " obj ins " << objects_inserted_ << " remain obj " << remaining_obj_num;
        }
        else // unknown number of skipped objects
        {
            remaining_obj_num = num_obj_total-objects_inserted_;
        }

        if (remaining_obj_num < 0.0)
            remaining_obj_num = 0.0;

        double time_remaining_s = remaining_obj_num*avg_time_per_obj_s;
        remaining_time_str_ = String::timeStringFromDouble(time_remaining_s, false);
    }

    msg += "Elapsed Time: "+elapsed_time_str+"\n";

    if (bytes_read_ > 1e9)
        msg += "Data read: "+String::doubleToStringPrecision(static_cast<double>(bytes_read_)*1e-9,2)+" GB";
    else
        msg += "Data read: "+String::doubleToStringPrecision(static_cast<double>(bytes_read_)*1e-6,2)+" MB";

    if (bytes_to_read_)
        msg += " ("+std::to_string(static_cast<int>(read_status_percent_))+"%)\n\n";
    else
        msg += "\n\n";

    msg += "Objects read: "+std::to_string(objects_read_)+"\n";
    msg += "Objects parsed: "+std::to_string(objects_parsed_)+"\n";
    msg += "Objects parse errors: "+std::to_string(objects_parse_errors_)+"\n\n";

    msg += "Objects mapped: "+std::to_string(objects_mapped_)+"\n";
    msg += "Objects not mapped: "+std::to_string(objects_not_mapped_)+"\n\n";

    msg += "Objects created: "+std::to_string(objects_created_)+"\n";
    msg += "Objects inserted: "+std::to_string(objects_inserted_)+"\n\n";

    if (object_rate_str_.size())
        msg += "Object rate: "+object_rate_str_+" e/s";

    if (!all_done_ && remaining_time_str_.size())
        msg += "\nEstimated remaining time: "+remaining_time_str_;

    msg_box_->setText(msg.c_str());

    if (all_done_)
        msg_box_->setStandardButtons(QMessageBox::Ok);
    else
        msg_box_->setStandardButtons(QMessageBox::NoButton);

    msg_box_->show();

    logdbg << "JSONImporterTask: updateMsgBox: done";
}

void JSONImportTask::insertProgressSlot (float percent)
{
    logdbg << "JSONImporterTask: insertProgressSlot: " << String::percentToString(percent) << "%";
}

void JSONImportTask::insertDoneSlot (DBObject& object)
{
    logdbg << "JSONImporterTask: insertDoneSlot";
    --insert_active_;

    checkAllDone();
    updateMsgBox();

    logdbg << "JSONImporterTask: insertDoneSlot: done";
}




