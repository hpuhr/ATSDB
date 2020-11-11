/*
 * This file is part of OpenATS COMPASS.
 *
 * COMPASS is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * COMPASS is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with COMPASS. If not, see <http://www.gnu.org/licenses/>.
 */

#include "evaluationmanager.h"
#include "evaluationmanagerwidget.h"
#include "evaluationdatawidget.h"
#include "evaluationstandard.h"
#include "evaluationstandardwidget.h"
#include "eval/requirement/group.h"
#include "eval/requirement/config.h"
#include "compass.h"
#include "dbinterface.h"
#include "dbobject.h"
#include "dbobjectmanager.h"
#include "dbobjectmanagerloadwidget.h"
#include "dbobjectinfowidget.h"
#include "sector.h"
#include "metadbovariable.h"
#include "dbovariable.h"
#include "buffer.h"
#include "filtermanager.h"
#include "datasourcesfilter.h"
#include "datasourcesfilterwidget.h"
#include "viewabledataconfig.h"
#include "viewmanager.h"
#include "stringconv.h"
#include "dbovariableorderedset.h"
#include "dbconnection.h"

#include "json.hpp"

#include <QTabWidget>
#include <QApplication>
#include <QCoreApplication>
#include <QThread>

#include "boost/date_time/posix_time/posix_time.hpp"

#include <memory>
#include <fstream>

using namespace Utils;
using namespace std;
using namespace nlohmann;

EvaluationManager::EvaluationManager(const std::string& class_id, const std::string& instance_id, COMPASS* compass)
    : Configurable(class_id, instance_id, compass, "eval.json"), compass_(*compass), data_(*this), results_gen_(*this)
{
    registerParameter("dbo_name_ref", &dbo_name_ref_, "RefTraj");
    registerParameter("active_sources_ref", &active_sources_ref_, json::object());

    registerParameter("dbo_name_tst", &dbo_name_tst_, "Tracker");
    registerParameter("active_sources_tst", &active_sources_tst_, json::object());

    registerParameter("current_standard", &current_standard_, "");

    registerParameter("configs", &configs_, json::object());

    registerParameter("use_grp_in_sector", &use_grp_in_sector_, json::object());
    registerParameter("use_requirement", &use_requirement_, json::object());

    // remove utn stuff
    // shorts
    registerParameter("remove_short_targets", &remove_short_targets_, true);
    registerParameter("remove_short_targets_min_updates", &remove_short_targets_min_updates_, 10);
    registerParameter("remove_short_targets_min_duration", &remove_short_targets_min_duration_, 60.0);
    // psr
    registerParameter("remove_psr_only_targets", &remove_psr_only_targets_, true);
    // ma
    registerParameter("remove_modeac_onlys", &remove_modeac_onlys_, false);
    registerParameter("remove_mode_a_codes", &remove_mode_a_codes_, false);
    registerParameter("remove_mode_a_code_values", &remove_mode_a_code_values_, "7000,7777");
    // ta
    registerParameter("remove_mode_target_addresses", &remove_target_addresses_, false);
    registerParameter("remove_target_address_vales", &remove_target_address_values_, "");
    // dbo
    registerParameter("remove_not_detected_dbos", &remove_not_detected_dbos_, false);
    registerParameter("remove_not_detected_dbo_values", &remove_not_detected_dbo_values_, json::object());

    createSubConfigurables();
}

void EvaluationManager::init(QTabWidget* tab_widget)
{
    loginf << "EvaluationManager: init";

    assert (!initialized_);
    assert (tab_widget);

    updateReferenceDBO();
    updateTestDBO();

    initialized_ = true;

    tab_widget->addTab(widget(), "Evaluation");

    if (!COMPASS::instance().objectManager().hasAssociations())
        widget()->setDisabled(true);
}

bool EvaluationManager::canLoadData ()
{
    assert (initialized_);
    return COMPASS::instance().objectManager().hasAssociations() && hasCurrentStandard();
}

void EvaluationManager::loadData ()
{
    loginf << "EvaluationManager: loadData";

    assert (initialized_);

    reference_data_loaded_ = false;
    test_data_loaded_ = false;
    data_loaded_ = false;

    evaluated_ = false;

    DBObjectManager& object_man = COMPASS::instance().objectManager();

    // load adsb mops versions in a hacky way
    if (object_man.object("ADSB").hasData() && !has_adsb_mops_versions_)
    {
        adsb_mops_versions_ = COMPASS::instance().interface().queryADSBMOPSVersions();
        has_adsb_mops_versions_ = true;
    }

    // set use filters
    object_man.useFilters(true);
    object_man.loadWidget()->updateUseFilters();

    // clear data
    data_.clear();

    // set if load for dbos
    for (auto& obj_it : object_man)
    {
        obj_it.second->infoWidget()->setLoad(obj_it.first == dbo_name_ref_
                                             || obj_it.first == dbo_name_tst_);
    }

    // set ref data sources filters
    FilterManager& fil_man = COMPASS::instance().filterManager();

    fil_man.disableAllFilters();

    if (dbo_name_ref_ != dbo_name_tst_)
    {
        {
            DataSourcesFilter* ref_filter = fil_man.getDataSourcesFilter(dbo_name_ref_);
            ref_filter->setActive(true);

            for (auto& fil_ds_it : ref_filter->dataSources())
            {
                assert (data_sources_ref_.count(fil_ds_it.first));
                fil_ds_it.second.setActive(data_sources_ref_.at(fil_ds_it.first).isActive());
            }

            ref_filter->widget()->update();
        }

        {
            DataSourcesFilter* tst_filter = fil_man.getDataSourcesFilter(dbo_name_tst_);
            tst_filter->setActive(true);

            for (auto& fil_ds_it : tst_filter->dataSources())
            {
                assert (data_sources_tst_.count(fil_ds_it.first));
                fil_ds_it.second.setActive(data_sources_tst_.at(fil_ds_it.first).isActive());
            }

            tst_filter->widget()->update();
        }
    }
    else // same ref / tst dbo
    {
        DataSourcesFilter* filter = fil_man.getDataSourcesFilter(dbo_name_ref_);
        filter->setActive(true);

        for (auto& fil_ds_it : filter->dataSources())
        {
            assert (data_sources_tst_.count(fil_ds_it.first));
            fil_ds_it.second.setActive(data_sources_ref_.at(fil_ds_it.first).isActive()
                                       || data_sources_tst_.at(fil_ds_it.first).isActive());
        }

        filter->widget()->update();
    }


    // reference data
    {
        assert (object_man.existsObject(dbo_name_ref_));
        DBObject& dbo_ref = object_man.object(dbo_name_ref_);

        connect(&dbo_ref, &DBObject::newDataSignal, this, &EvaluationManager::newDataSlot);
        connect(&dbo_ref, &DBObject::loadingDoneSignal, this, &EvaluationManager::loadingDoneSlot);
    }

    // test data

    if (dbo_name_ref_ != dbo_name_tst_) // otherwise already connected
    {
        assert (object_man.existsObject(dbo_name_tst_));
        DBObject& dbo_tst = object_man.object(dbo_name_tst_);

        connect(&dbo_tst, &DBObject::newDataSignal, this, &EvaluationManager::newDataSlot);
        connect(&dbo_tst, &DBObject::loadingDoneSignal, this, &EvaluationManager::loadingDoneSlot);

    }

    needs_additional_variables_ = true;

    object_man.loadSlot();

    needs_additional_variables_ = false;

    if (widget_)
        widget_->updateButtons();
}

bool EvaluationManager::canEvaluate ()
{
    assert (initialized_);
    return data_loaded_ && hasCurrentStandard();
}

void EvaluationManager::newDataSlot(DBObject& object)
{
    loginf << "EvaluationManager: newDataSlot: obj " << object.name() << " buffer size " << object.data()->size();
}
void EvaluationManager::loadingDoneSlot(DBObject& object)
{
    loginf << "EvaluationManager: loadingDoneSlot: obj " << object.name() << " buffer size " << object.data()->size();

    DBObjectManager& object_man = COMPASS::instance().objectManager();

    if (object.name() == dbo_name_ref_)
    {
        DBObject& dbo_ref = object_man.object(dbo_name_ref_);

        disconnect(&dbo_ref, &DBObject::newDataSignal, this, &EvaluationManager::newDataSlot);
        disconnect(&dbo_ref, &DBObject::loadingDoneSignal, this, &EvaluationManager::loadingDoneSlot);

        data_.addReferenceData(dbo_ref, object.data());

        reference_data_loaded_ = true;
    }

    if (object.name() == dbo_name_tst_)
    {
        DBObject& dbo_tst = object_man.object(dbo_name_tst_);

        if (dbo_name_ref_ != dbo_name_tst_) // otherwise already disconnected
        {
            disconnect(&dbo_tst, &DBObject::newDataSignal, this, &EvaluationManager::newDataSlot);
            disconnect(&dbo_tst, &DBObject::loadingDoneSignal, this, &EvaluationManager::loadingDoneSlot);
        }

        data_.addTestData(dbo_tst, object.data());

        test_data_loaded_ = true;
    }

    bool data_loaded_tmp = reference_data_loaded_ && test_data_loaded_;

    loginf << "EvaluationManager: loadingDoneSlot: data loaded " << data_loaded_;

    if (data_loaded_tmp)
    {
        loginf << "EvaluationManager: loadingDoneSlot: finalizing";

        boost::posix_time::ptime start_time = boost::posix_time::microsec_clock::local_time();

        data_.finalize();

        boost::posix_time::time_duration time_diff =  boost::posix_time::microsec_clock::local_time() - start_time;

        loginf << "EvaluationManager: loadingDoneSlot: finalize done "
               << String::timeStringFromDouble(time_diff.total_milliseconds() / 1000.0, true);
    }

    data_loaded_ = data_loaded_tmp;

    if (widget_)
        widget_->updateButtons();
}

void EvaluationManager::evaluate ()
{
    loginf << "EvaluationManager: evaluate";

    assert (initialized_);
    assert (data_loaded_);
    assert (hasCurrentStandard());

    results_gen_.evaluate(data_, currentStandard());

    evaluated_ = true;

    if (widget_)
    {
        widget_->updateButtons();
        widget_->expandResults();
    }
}

bool EvaluationManager::canGenerateReport ()
{
    assert (initialized_);
    return evaluated_;
}

void EvaluationManager::generateReport ()
{
    loginf << "EvaluationManager: generateReport";

    assert (initialized_);
    assert (data_loaded_);
    assert (evaluated_);

    assert (pdf_gen_);
    pdf_gen_->dialog().exec();

    if (widget_)
        widget_->updateButtons();
}

void EvaluationManager::close()
{
    initialized_ = false;
}

bool EvaluationManager::needsAdditionalVariables ()
{
    return needs_additional_variables_;
}

void EvaluationManager::addVariables (const std::string dbo_name, DBOVariableSet& read_set)
{
    loginf << "EvaluationManager: addVariables: dbo_name " << dbo_name;

    DBObjectManager& object_man = COMPASS::instance().objectManager();

    read_set.add(object_man.metaVariable("rec_num").getFor(dbo_name));
    read_set.add(object_man.metaVariable("ds_id").getFor(dbo_name));
    read_set.add(object_man.metaVariable("tod").getFor(dbo_name));
    read_set.add(object_man.metaVariable("pos_lat_deg").getFor(dbo_name));
    read_set.add(object_man.metaVariable("pos_long_deg").getFor(dbo_name));
    read_set.add(object_man.metaVariable("target_addr").getFor(dbo_name));
    read_set.add(object_man.metaVariable("modec_code_ft").getFor(dbo_name));
    read_set.add(object_man.metaVariable("mode3a_code").getFor(dbo_name));

    // TODO add required variables from standard requirements

    if (dbo_name_ref_ == dbo_name && dbo_name_ref_ == "Tracker")
        read_set.add(object_man.object("Tracker").variable("tracked_alt_baro_ft"));

//    if (dbo_name == "ADSB")
//    {
//        DBObject& obj = object_man.object("ADSB");

//        read_set.add(obj.variable("mops_version"));
//        read_set.add(obj.variable("nac_p"));
//        read_set.add(obj.variable("nucp_nic"));
//        read_set.add(obj.variable("sil"));
//    }

    //        read_set.add(object_man.metaVariable("groundspeed_kt").getFor(dbo_name_ref_));
    //        read_set.add(object_man.metaVariable("heading_deg").getFor(dbo_name_ref_));
}

EvaluationManager::~EvaluationManager()
{
    sector_layers_.clear();
}

void EvaluationManager::generateSubConfigurable(const std::string& class_id,
                                                const std::string& instance_id)
{
    if (class_id == "EvaluationStandard")
    {
        EvaluationStandard* standard = new EvaluationStandard(class_id, instance_id, *this);
        logdbg << "EvaluationManager: generateSubConfigurable: adding standard " << standard->name();

        assert(standards_.find(standard->name()) == standards_.end());

        standards_[standard->name()].reset(standard);
    }
    else if (class_id == "EvaluationResultsReportPDFGenerator")
    {
        assert (!pdf_gen_);
        pdf_gen_.reset(new EvaluationResultsReport::PDFGenerator(class_id, instance_id, *this));
        assert (pdf_gen_);
    }
    else
        throw std::runtime_error("EvaluationManager: generateSubConfigurable: unknown class_id " +
                                 class_id);
}

EvaluationManagerWidget* EvaluationManager::widget()
{
    if (!widget_)
        widget_.reset(new EvaluationManagerWidget(*this));

    assert(widget_);
    return widget_.get();
}

void EvaluationManager::checkSubConfigurables()
{
    if (!pdf_gen_)
        generateSubConfigurable("EvaluationResultsReportPDFGenerator", "EvaluationResultsReportPDFGenerator0");

    assert (pdf_gen_);
}

bool EvaluationManager::hasSectorLayer (const std::string& layer_name)
{
    assert (sectors_loaded_);

    auto iter = std::find_if(sector_layers_.begin(), sector_layers_.end(),
                             [&layer_name](const shared_ptr<SectorLayer>& x) { return x->name() == layer_name;});

    return iter != sector_layers_.end();
}

//void EvaluationManager::renameSectorLayer (const std::string& name, const std::string& new_name)
//{
//    // TODO
//}

std::shared_ptr<SectorLayer> EvaluationManager::sectorLayer (const std::string& layer_name)
{
    assert (sectors_loaded_);
    assert (hasSectorLayer(layer_name));

    auto iter = std::find_if(sector_layers_.begin(), sector_layers_.end(),
                             [&layer_name](const shared_ptr<SectorLayer>& x) { return x->name() == layer_name;});
    assert (iter != sector_layers_.end());

    return *iter;
}

void EvaluationManager::loadSectors()
{
    loginf << "EvaluationManager: loadSectors";

    assert (!sectors_loaded_);

    sector_layers_ = COMPASS::instance().interface().loadSectors();

    sectors_loaded_ = true;
}

unsigned int EvaluationManager::getMaxSectorId ()
{
    assert (sectors_loaded_);

    unsigned int id = 0;
    for (auto& sec_lay_it : sector_layers_)
        for (auto& sec_it : sec_lay_it->sectors())
            if (sec_it->id() > id)
                id = sec_it->id();

    return id;
}

void EvaluationManager::createNewSector (const std::string& name, const std::string& layer_name,
                                         std::vector<std::pair<double,double>> points)
{
    loginf << "EvaluationManager: createNewSector: name " << name << " layer_name " << layer_name
           << " num points " << points.size();

    assert (sectors_loaded_);
    assert (!hasSector(name, layer_name));

    unsigned int id = getMaxSectorId()+1;

    shared_ptr<Sector> sector = make_shared<Sector> (id, name, layer_name, points);

    // add to existing sectors
    if (!hasSectorLayer(layer_name))
        sector_layers_.push_back(make_shared<SectorLayer>(layer_name));

    assert (hasSectorLayer(layer_name));

    sectorLayer(layer_name)->addSector(sector);

    assert (hasSector(name, layer_name));
    sector->save();
}

bool EvaluationManager::hasSector (const string& name, const string& layer_name)
{
    assert (sectors_loaded_);

    if (!hasSectorLayer(layer_name))
        return false;

    return sectorLayer(layer_name)->hasSector(name);
}

bool EvaluationManager::hasSector (unsigned int id)
{
    assert (sectors_loaded_);

    for (auto& sec_lay_it : sector_layers_)
    {
        auto& sectors = sec_lay_it->sectors();
        auto iter = std::find_if(sectors.begin(), sectors.end(),
                                 [id](const shared_ptr<Sector>& x) { return x->id() == id;});
        if (iter != sectors.end())
            return true;
    }
    return false;
}

std::shared_ptr<Sector> EvaluationManager::sector (const string& name, const string& layer_name)
{
    assert (sectors_loaded_);
    assert (hasSector(name, layer_name));

    return sectorLayer(layer_name)->sector(name);
}

std::shared_ptr<Sector> EvaluationManager::sector (unsigned int id)
{
    assert (sectors_loaded_);
    assert (hasSector(id));

    for (auto& sec_lay_it : sector_layers_)
    {
        auto& sectors = sec_lay_it->sectors();
        auto iter = std::find_if(sectors.begin(), sectors.end(),
                                 [id](const shared_ptr<Sector>& x) { return x->id() == id;});
        if (iter != sectors.end())
            return *iter;
    }

    logerr << "EvaluationManager: sector: id " << id << " not found";
    assert (false);
}

void EvaluationManager::moveSector(unsigned int id, const std::string& old_layer_name, const std::string& new_layer_name)
{
    assert (sectors_loaded_);
    assert (hasSector(id));

    shared_ptr<Sector> tmp_sector = sector(id);

    assert (hasSectorLayer(old_layer_name));
    sectorLayer(old_layer_name)->removeSector(tmp_sector);

    if (!hasSectorLayer(new_layer_name))
        sector_layers_.push_back(make_shared<SectorLayer>(new_layer_name));

    assert (hasSectorLayer(new_layer_name));
    sectorLayer(new_layer_name)->addSector(tmp_sector);

    assert (hasSector(tmp_sector->name(), new_layer_name));
    tmp_sector->save();
}

std::vector<std::shared_ptr<SectorLayer>>& EvaluationManager::sectorsLayers()
{
    assert (sectors_loaded_);

    return sector_layers_;
}

void EvaluationManager::saveSector(unsigned int id)
{
    assert (sectors_loaded_);
    assert (hasSector(id));

    saveSector(sector(id));
}

void EvaluationManager::saveSector(std::shared_ptr<Sector> sector)
{
    assert (sectors_loaded_);
    assert (hasSector(sector->name(), sector->layerName()));
    COMPASS::instance().interface().saveSector(sector);

    emit sectorsChangedSignal();
}

void EvaluationManager::deleteSector(shared_ptr<Sector> sector)
{
    assert (sectors_loaded_);
    assert (hasSector(sector->name(), sector->layerName()));

    string layer_name = sector->layerName();

    assert (hasSectorLayer(layer_name));

    sectorLayer(layer_name)->removeSector(sector);

    // remove sector layer if empty
    if (!sectorLayer(layer_name)->size())
    {
        auto iter = std::find_if(sector_layers_.begin(), sector_layers_.end(),
                                 [&layer_name](const shared_ptr<SectorLayer>& x) { return x->name() == layer_name;});

        assert (iter != sector_layers_.end());
        sector_layers_.erase(iter);
    }

    COMPASS::instance().interface().deleteSector(sector);

    emit sectorsChangedSignal();
}

void EvaluationManager::deleteAllSectors()
{
    assert (sectors_loaded_);
    sector_layers_.clear();

    COMPASS::instance().interface().deleteAllSectors();

    emit sectorsChangedSignal();
}


void EvaluationManager::importSectors (const std::string& filename)
{
    loginf << "EvaluationManager: importSectors: filename '" << filename << "'";

    assert (sectors_loaded_);

    sector_layers_.clear();
    COMPASS::instance().interface().clearSectorsTable();

    std::ifstream input_file(filename, std::ifstream::in);

    try
    {
        json j = json::parse(input_file);

        if (!j.contains("sectors"))
        {
            logerr << "EvaluationManager: importSectors: file does not contain sectors";
            return;
        }

        json& sectors = j["sectors"];

        if (!sectors.is_array())
        {
            logerr << "EvaluationManager: importSectors: file sectors is not an array";
            return;
        }

        unsigned int id;
        string name;
        string layer_name;
        string json_str;

        for (auto& j_sec_it : sectors.get<json::array_t>())
        {
            if (!j_sec_it.contains("id")
                    || !j_sec_it.contains("name")
                    || !j_sec_it.contains("layer_name")
                    || !j_sec_it.contains("points"))
            {
                logerr << "EvaluationManager: importSectors: ill-defined sectors skipped, json '" << j_sec_it.dump(4)
                       << "'";
                continue;
            }

            id = j_sec_it.at("id");
            name = j_sec_it.at("name");
            layer_name = j_sec_it.at("layer_name");

            shared_ptr<Sector> new_sector = make_shared<Sector>(id, name, layer_name, j_sec_it.dump());

            if (!hasSectorLayer(layer_name))
                sector_layers_.push_back(make_shared<SectorLayer>(layer_name));

            sectorLayer(layer_name)->addSector(new_sector);

            assert (hasSector(name, layer_name));

            new_sector->save();

            loginf << "EvaluationManager: importSectors: loaded sector '" << name << "' in layer '"
                   << layer_name << "' num points " << sector(name, layer_name)->size();
        }
    }
    catch (json::exception& e)
    {
        logerr << "EvaluationManager: importSectors: could not load file '"
               << filename << "'";
        throw e;
    }

    emit sectorsChangedSignal();
}

void EvaluationManager::exportSectors (const std::string& filename)
{
    loginf << "EvaluationManager: exportSectors: filename '" << filename << "'";

    assert (sectors_loaded_);

    json j;

    j["sectors"] = json::array();
    json& sectors = j["sectors"];

    unsigned int cnt = 0;

    for (auto& sec_lay_it : sector_layers_)
    {
        for (auto& sec_it : sec_lay_it->sectors())
        {
            sectors[cnt] = sec_it->jsonData();
            ++cnt;
        }
    }

    std::ofstream output_file;
    output_file.open(filename, std::ios_base::out);

    output_file << j.dump(4);

}

std::string EvaluationManager::dboNameRef() const
{
    return dbo_name_ref_;
}

void EvaluationManager::dboNameRef(const std::string& name)
{
    loginf << "EvaluationManager: dboNameRef: name " << name;

    dbo_name_ref_ = name;

    updateReferenceDBO();
}

bool EvaluationManager::hasValidReferenceDBO ()
{
    if (!dbo_name_ref_.size())
        return false;

    if (!COMPASS::instance().objectManager().existsObject(dbo_name_ref_))
        return false;

    DBObject& object = COMPASS::instance().objectManager().object(dbo_name_ref_);

    if (!object.hasCurrentDataSourceDefinition())
        return false;

    return object.hasDataSources();
}


std::set<int> EvaluationManager::activeDataSourcesRef()
{
    set<int> active_sources;

    for (auto& ds_it : data_sources_ref_)
        if (ds_it.second.isActive())
            active_sources.insert(ds_it.first);

    return active_sources;
}

std::string EvaluationManager::dboNameTst() const
{
    return dbo_name_tst_;
}

void EvaluationManager::dboNameTst(const std::string& name)
{
    loginf << "EvaluationManager: dboNameTst: name " << name;

    dbo_name_tst_ = name;

    updateTestDBO();
}

bool EvaluationManager::hasValidTestDBO ()
{
    if (!dbo_name_tst_.size())
        return false;

    if (!COMPASS::instance().objectManager().existsObject(dbo_name_tst_))
        return false;

    DBObject& object = COMPASS::instance().objectManager().object(dbo_name_tst_);

    if (!object.hasCurrentDataSourceDefinition())
        return false;

    return object.hasDataSources();
}

std::set<int> EvaluationManager::activeDataSourcesTst()
{
    set<int> active_sources;

    for (auto& ds_it : data_sources_tst_)
        if (ds_it.second.isActive())
            active_sources.insert(ds_it.first);

    return active_sources;
}


bool EvaluationManager::dataLoaded() const
{
    return data_loaded_;
}

bool EvaluationManager::evaluated() const
{
    return evaluated_;
}

EvaluationData& EvaluationManager::getData()
{
    return data_;
}

bool EvaluationManager::hasCurrentStandard()
{
    return current_standard_.size() && hasStandard(current_standard_);
}

std::string EvaluationManager::currentStandardName() const
{
    return current_standard_;
}

void EvaluationManager::currentStandardName(const std::string& current_standard)
{
    current_standard_ = current_standard;

    if (current_standard_.size())
        assert (hasStandard(current_standard_));

    emit currentStandardChangedSignal();

    if (widget_)
        widget_->updateButtons();
}

EvaluationStandard& EvaluationManager::currentStandard()
{
    assert (hasCurrentStandard());

    return *standards_.at(current_standard_).get();
}

bool EvaluationManager::hasStandard(const std::string& name)
{
    return standards_.count(name);
}

void EvaluationManager::addStandard(const std::string& name)
{
    loginf << "EvaluationManager: addStandard: name " << name;

    assert (!hasStandard(name));

    std::string instance = "EvaluationStandard" + name + "0";

    Configuration& config = addNewSubConfiguration("EvaluationStandard", instance);
    config.addParameterString("name", name);

    generateSubConfigurable("EvaluationStandard", instance);

    emit standardsChangedSignal();

    currentStandardName(name);
}

void EvaluationManager::deleteCurrentStandard()
{
    loginf << "EvaluationManager: deleteCurrentStandard: name " << current_standard_;

    assert (hasCurrentStandard());

    standards_.erase(current_standard_);

    emit standardsChangedSignal();

    currentStandardName("");
}

std::vector<std::string> EvaluationManager::currentRequirementNames()
{
    std::vector<std::string> names;

    if (hasCurrentStandard())
    {
        for (auto& req_grp_it : currentStandard())
        {
            for (auto& req_it : *req_grp_it.second)
            {
                if (find(names.begin(), names.end(), req_it->name()) == names.end())
                    names.push_back(req_it->name());
            }
        }
    }

    return names;
}

EvaluationResultsGenerator& EvaluationManager::resultsGenerator()
{
    return results_gen_;
}

bool EvaluationManager::sectorsLoaded() const
{
    return sectors_loaded_;
}

void EvaluationManager::updateReferenceDBO()
{
    loginf << "EvaluationManager: updateReferenceDBO";
    
    data_sources_ref_.clear();
    active_sources_ref_.clear();
    
    if (!hasValidReferenceDBO())
        return;

    DBObject& object = COMPASS::instance().objectManager().object(dbo_name_ref_);

    if (object.hasDataSources())
        updateReferenceDataSources();

    if (object.hasActiveDataSourcesInfo())
        updateReferenceDataSourcesActive();
}

void EvaluationManager::updateReferenceDataSources()
{
    loginf << "EvaluationManager: updateReferenceDataSources";

    assert (hasValidReferenceDBO());

    DBObject& object = COMPASS::instance().objectManager().object(dbo_name_ref_);

    for (auto ds_it = object.dsBegin(); ds_it != object.dsEnd(); ++ds_it)
    {
        if (data_sources_ref_.find(ds_it->first) == data_sources_ref_.end())
        {
            if (!active_sources_ref_.contains(to_string(ds_it->first)))
                active_sources_ref_[to_string(ds_it->first)] = true; // init with default true

            // needed for old compiler
            json::boolean_t& active = active_sources_ref_[to_string(ds_it->first)].get_ref<json::boolean_t&>();

            data_sources_ref_.emplace(std::piecewise_construct,
                                      std::forward_as_tuple(ds_it->first),  // args for key
                                      std::forward_as_tuple(ds_it->first, ds_it->second.name(),
                                                            active));
        }
    }
}

void EvaluationManager::updateReferenceDataSourcesActive()
{
    loginf << "EvaluationManager: updateReferenceDataSourcesActive";

    assert (hasValidReferenceDBO());

    DBObject& object = COMPASS::instance().objectManager().object(dbo_name_ref_);

    assert (object.hasActiveDataSourcesInfo());

    for (auto& srcit : data_sources_ref_)
        srcit.second.setActiveInData(false);

    for (auto& it : object.getActiveDataSources())
    {
        assert(data_sources_ref_.find(it) != data_sources_ref_.end());
        ActiveDataSource& src = data_sources_ref_.at(it);
        src.setActiveInData(true);
    }

    for (auto& srcit : data_sources_ref_)
    {
        if (!srcit.second.isActiveInData())
            srcit.second.setActive(false);
    }
}

void EvaluationManager::updateTestDBO()
{
    loginf << "EvaluationManager: updateTestDBO";

    data_sources_tst_.clear();
    active_sources_tst_.clear();

    if (!hasValidTestDBO())
        return;

    DBObject& object = COMPASS::instance().objectManager().object(dbo_name_tst_);

    if (object.hasDataSources())
        updateTestDataSources();

    if (object.hasActiveDataSourcesInfo())
        updateTestDataSourcesActive();
}

void EvaluationManager::updateTestDataSources()
{
    loginf << "EvaluationManager: updateTestDataSources";

    assert (hasValidTestDBO());

    DBObject& object = COMPASS::instance().objectManager().object(dbo_name_tst_);

    for (auto ds_it = object.dsBegin(); ds_it != object.dsEnd(); ++ds_it)
    {
        if (data_sources_tst_.find(ds_it->first) == data_sources_tst_.end())
        {
            if (!active_sources_tst_.contains(to_string(ds_it->first)))
                active_sources_tst_[to_string(ds_it->first)] = true; // init with default true

            // needed for old compiler
            json::boolean_t& active = active_sources_tst_[to_string(ds_it->first)].get_ref<json::boolean_t&>();

            data_sources_tst_.emplace(std::piecewise_construct,
                                      std::forward_as_tuple(ds_it->first),  // args for key
                                      std::forward_as_tuple(ds_it->first, ds_it->second.name(),
                                                            active));
        }
    }
}

void EvaluationManager::updateTestDataSourcesActive()
{
    loginf << "EvaluationManager: updateTestDataSourcesActive";

    assert (hasValidTestDBO());

    DBObject& object = COMPASS::instance().objectManager().object(dbo_name_tst_);

    assert (object.hasActiveDataSourcesInfo());

    for (auto& srcit : data_sources_tst_)
        srcit.second.setActiveInData(false);

    for (auto& it : object.getActiveDataSources())
    {
        assert(data_sources_tst_.find(it) != data_sources_tst_.end());
        ActiveDataSource& src = data_sources_tst_.at(it);
        src.setActiveInData(true);
    }

    for (auto& srcit : data_sources_tst_)
    {
        if (!srcit.second.isActiveInData())
            srcit.second.setActive(false);
    }
}

void EvaluationManager::setViewableDataConfig (const nlohmann::json::object_t& data)
{
    if (viewable_data_cfg_)
    {
        COMPASS::instance().viewManager().unsetCurrentViewPoint();
        viewable_data_cfg_ = nullptr;
    }

    viewable_data_cfg_.reset(new ViewableDataConfig(data));

    COMPASS::instance().viewManager().setCurrentViewPoint(viewable_data_cfg_.get());
}

void EvaluationManager::showUTN (unsigned int utn)
{
    loginf << "EvaluationManager: showUTN: utn " << utn;

    nlohmann::json data = getBaseViewableDataConfig();
    data["filters"]["UTNs"]["utns"] = to_string(utn);

    loginf << "EvaluationManager: showUTN: showing";
    setViewableDataConfig(data);
}

std::unique_ptr<nlohmann::json::object_t> EvaluationManager::getViewableForUTN (unsigned int utn)
{
    nlohmann::json::object_t data = getBaseViewableDataConfig();
    data["filters"]["UTNs"]["utns"] = to_string(utn);

    return std::unique_ptr<nlohmann::json::object_t>{new nlohmann::json::object_t(move(data))};
}

std::unique_ptr<nlohmann::json::object_t> EvaluationManager::getViewableForEvaluation (
        const std::string& req_grp_id, const std::string& result_id)
{
    nlohmann::json::object_t data = getBaseViewableNoDataConfig();

    data["evaluation_results"]["show_results"] = true;
    data["evaluation_results"]["req_grp_id"] = req_grp_id;
    data["evaluation_results"]["result_id"] = result_id;

    return std::unique_ptr<nlohmann::json::object_t>{new nlohmann::json::object_t(move(data))};
}

std::unique_ptr<nlohmann::json::object_t> EvaluationManager::getViewableForEvaluation (
        unsigned int utn, const std::string& req_grp_id, const std::string& result_id)
{
    nlohmann::json::object_t data = getBaseViewableDataConfig();
    data["filters"]["UTNs"]["utns"] = to_string(utn);

    data["evaluation_results"]["show_results"] = true;
    data["evaluation_results"]["req_grp_id"] = req_grp_id;
    data["evaluation_results"]["result_id"] = result_id;

    return std::unique_ptr<nlohmann::json::object_t>{new nlohmann::json::object_t(move(data))};
}

void EvaluationManager::showResultId (const std::string& id)
{
    loginf << "EvaluationManager: showResultId: id '" << id << "'";

    assert (widget_);
    widget_->showResultId(id);
}

//void EvaluationManager::setUseTargetData (unsigned int utn, bool value)
//{
//    loginf << "EvaluationManager: setUseTargetData: utn " << utn << " use " << value;

//    data_.setUseTargetData(utn, value);
//    updateResultsToUseChangeOf(utn);
//}

void EvaluationManager::updateResultsToChanges ()
{
    if (evaluated_)
    {
        results_gen_.updateToChanges();

        if (widget_)
        {
            widget_->expandResults();
            widget_->reshowLastResultId();
        }
    }
}

void EvaluationManager::showFullUTN (unsigned int utn)
{
    nlohmann::json::object_t data;
    data["filters"]["UTNs"]["utns"] = to_string(utn);

    setViewableDataConfig(data);
}

void EvaluationManager::showSurroundingData (unsigned int utn)
{
    nlohmann::json::object_t data;

    assert (data_.hasTargetData(utn));

    const EvaluationTargetData& target_data = data_.targetData(utn);

    float time_begin = target_data.timeBegin();
    time_begin -= 60.0;

    if (time_begin < 0)
        time_begin = 0;

    float time_end = target_data.timeEnd();
    time_end += 60.0;

    if (time_end > 24*60*60)
        time_end = 24*60*60;

    //    "Time of Day": {
    //    "Time of Day Maximum": "05:56:32.297",
    //    "Time of Day Minimum": "05:44:58.445"
    //    },

    data["filters"]["Time of Day"]["Time of Day Maximum"] = String::timeStringFromDouble(time_end);
    data["filters"]["Time of Day"]["Time of Day Minimum"] = String::timeStringFromDouble(time_begin);

    //    "Target Address": {
    //    "Target Address Values": "FEFE10"
    //    },
    if (target_data.targetAddresses().size())
        data["filters"]["Target Address"]["Target Address Values"] = target_data.targetAddressesStr()+",NULL";

    //    "Mode 3/A Code": {
    //    "Mode 3/A Code Values": "7000"
    //    }

    if (target_data.modeACodes().size())
        data["filters"]["Mode 3/A Codes"]["Mode 3/A Codes Values"] = target_data.modeACodesStr()+",NULL";

    //    "filters": {
    //    "Barometric Altitude": {
    //    "Barometric Altitude Maxmimum": "43000",
    //    "Barometric Altitude Minimum": "500"
    //    },

    //    if (target_data.hasModeC())
    //    {
    //        float alt_min = target_data.modeCMin();
    //        alt_min -= 300;
    //        float alt_max = target_data.modeCMax();
    //        alt_max += 300;
    //    }

    //    "Position": {
    //    "Latitude Maximum": "50.78493920733",
    //    "Latitude Minimum": "44.31547147615",
    //    "Longitude Maximum": "20.76559892354",
    //    "Longitude Minimum": "8.5801592186"
    //    }

    if (target_data.hasPos())
    {
        data["filters"]["Position"]["Latitude Maximum"] = to_string(target_data.latitudeMax()+0.2);
        data["filters"]["Position"]["Latitude Minimum"] = to_string(target_data.latitudeMin()-0.2);
        data["filters"]["Position"]["Longitude Maximum"] = to_string(target_data.longitudeMax()+0.2);
        data["filters"]["Position"]["Longitude Minimum"] = to_string(target_data.longitudeMin()-0.2);
    }

    setViewableDataConfig(data);
}

json::boolean_t& EvaluationManager::useGroupInSectorLayer(const std::string& sector_layer_name,
                                                          const std::string& group_name)
{
    assert (hasCurrentStandard());

    // standard_name->sector_layer_name->req_grp_name->bool use
    if (!use_grp_in_sector_.contains(current_standard_)
            || !use_grp_in_sector_.at(current_standard_).contains(sector_layer_name)
            || !use_grp_in_sector_.at(current_standard_).at(sector_layer_name).contains(group_name))
        use_grp_in_sector_[current_standard_][sector_layer_name][group_name] = true;

    return use_grp_in_sector_[current_standard_][sector_layer_name][group_name].get_ref<json::boolean_t&>();
}

void EvaluationManager::useGroupInSectorLayer(const std::string& sector_layer_name,
                                              const std::string& group_name, bool value)
{
    assert (hasCurrentStandard());

    loginf << "EvaluationManager: useGroupInSector: standard_name " << current_standard_
           << " sector_layer_name " << sector_layer_name << " group_name " << group_name << " value " << value;

    use_grp_in_sector_[current_standard_][sector_layer_name][group_name] = value;
}

json::boolean_t& EvaluationManager::useRequirement(const std::string& standard_name, const std::string& group_name,
                                                   const std::string& req_name)
{
    // standard_name->req_grp_name->req_grp_name->bool use
    if (!use_requirement_.contains(standard_name)
            || !use_requirement_.at(standard_name).contains(group_name)
            || !use_requirement_.at(standard_name).at(group_name).contains(req_name))
        use_requirement_[standard_name][group_name][req_name] = true;

    return use_requirement_[standard_name][group_name][req_name].get_ref<json::boolean_t&>();
}

EvaluationResultsReport::PDFGenerator& EvaluationManager::pdfGenerator() const
{
    assert (pdf_gen_);
    return *pdf_gen_;
}

bool EvaluationManager::useUTN (unsigned int utn)
{
    logdbg << "EvaluationManager: useUTN: utn " << utn;

    if (!current_config_name_.size())
        current_config_name_ = COMPASS::instance().interface().connection().shortIdentifier();

    string utn_str = to_string(utn);

    if (!configs_[current_config_name_]["utns"].contains(utn_str)
            || !configs_[current_config_name_]["utns"].at(utn_str).contains("use"))
        return true;
    else
        return configs_[current_config_name_]["utns"][utn_str]["use"];
}

void EvaluationManager::useUTN (unsigned int utn, bool value, bool update_td, bool update_res)
{
    logdbg << "EvaluationManager: useUTN: utn " << utn << " value " << value
           << " update_td " << update_td;

    if (!current_config_name_.size())
        current_config_name_ = COMPASS::instance().interface().connection().shortIdentifier();

    string utn_str = to_string(utn);
    configs_[current_config_name_]["utns"][utn_str]["use"] = value;

    if (update_td)
        data_.setUseTargetData(utn, value);

    if (update_res && update_results_)
        updateResultsToChanges();
}

void EvaluationManager::useAllUTNs (bool value)
{
    update_results_ = false;

    for (auto& target_it : data_)
        useUTN(target_it.utn_, value, true);

    update_results_ = true;
    updateResultsToChanges();
}

void EvaluationManager::clearUTNComments ()
{
    update_results_ = false;

    for (auto& target_it : data_)
        utnComment(target_it.utn_, "", true);

    update_results_ = true;
}


void EvaluationManager::filterUTNs ()
{
    loginf << "EvaluationManager: filterUTNs";

    QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));

    update_results_ = false;

    DBObjectManager& dbo_man = COMPASS::instance().objectManager();

    map<string, set<unsigned int>> associated_utns;

    if (remove_not_detected_dbos_) // prepare associations
    {
        if (dbo_man.hasAssociations())
        {
            for (auto& dbo_it : dbo_man)
            {
                if (remove_not_detected_dbo_values_.contains(dbo_it.first)
                        && remove_not_detected_dbo_values_.at(dbo_it.first) == true)
                {
                    associated_utns[dbo_it.first] = dbo_it.second->associations().getAllUTNS();
                }
            }
        }
    }

    bool use;
    string comment;

    std::set<std::pair<int,int>> remove_mode_as = removeModeACodeData();
    std::set<unsigned int> remove_tas = removeTargetAddressData();

    for (auto& target_it : data_)
    {
        if (!target_it.use())
            continue;

        use = true; // must be true here
        comment = "";

        if (remove_short_targets_
                && (target_it.numUpdates() < remove_short_targets_min_updates_
                    || target_it.timeDuration() < remove_short_targets_min_duration_))
        {
            use = false;
            comment = "Short track";
        }

        if (use && remove_psr_only_targets_)
        {
            if (!target_it.callsigns().size()
                    && !target_it.targetAddresses().size()
                    && !target_it.modeACodes().size()
                    && !target_it.hasModeC())
            {
                use = false;
                comment = "Primary only";
            }
        }

        if (use && remove_modeac_onlys_)
        {
            if (!target_it.callsigns().size()
                    && !target_it.targetAddresses().size())
            {
                use = false;
                comment = "Mode A/C only";
            }
        }

        if (use && remove_mode_a_codes_)
        {
            for (auto t_ma : target_it.modeACodes())
            {
                for (auto& r_ma_p : remove_mode_as)
                {
                    if (r_ma_p.second == -1) // single
                    {
                        if (t_ma == r_ma_p.first)
                        {
                            use = false;
                            comment = "Mode A";
                            break;
                        }
                    }
                    else // pair
                    {
                        if (t_ma >= r_ma_p.first && t_ma <= r_ma_p.second)
                        {
                            use = false;
                            comment = "Mode A";
                            break;
                        }
                    }
                }

                if (!use) // already removed
                    break;
            }
        }

        if (use && remove_target_addresses_)
        {
            for (auto t_ta : target_it.targetAddresses())
            {
                if (remove_tas.count(t_ta))
                {
                    use = false;
                    comment = "Target Address";
                    break;
                }
            }
        }

        if (use && remove_not_detected_dbos_) // prepare associations
        {
            if (dbo_man.hasAssociations())
            {
                for (auto& dbo_it : dbo_man)
                {
                    if (remove_not_detected_dbo_values_.contains(dbo_it.first)
                            && remove_not_detected_dbo_values_.at(dbo_it.first) == true // removed if not detected
                            && associated_utns.count(dbo_it.first) // have associations
                            && !associated_utns.at(dbo_it.first).count(target_it.utn_)) // not detected
                    {
                        use = false; // remove it
                        comment = "Not Detected by "+dbo_it.first;
                        break;
                    }
                }
            }
        }

        if (!use)
        {
            logdbg << "EvaluationManager: filterUTNs: removing " << target_it.utn_ << " comment '" << comment << "'";
            useUTN (target_it.utn_, use, true);
            utnComment(target_it.utn_, comment, false);
        }
    }

    update_results_ = true;
    updateResultsToChanges();

    QApplication::restoreOverrideCursor();
}

std::string EvaluationManager::utnComment (unsigned int utn)
{
    logdbg << "EvaluationManager: utnComment: utn " << utn;

    if (!current_config_name_.size())
        current_config_name_ = COMPASS::instance().interface().connection().shortIdentifier();

    string utn_str = to_string(utn);

    if (!configs_[current_config_name_]["utns"].contains(utn_str)
            || !configs_[current_config_name_]["utns"].at(utn_str).contains("comment"))
        return "";
    else
        return configs_[current_config_name_]["utns"][utn_str]["comment"];
}

void EvaluationManager::utnComment (unsigned int utn, std::string value, bool update_td)
{
    logdbg << "EvaluationManager: utnComment: utn " << utn << " value '" << value << "'"
           << " update_td " << update_td;

    if (!current_config_name_.size())
        current_config_name_ = COMPASS::instance().interface().connection().shortIdentifier();

    string utn_str = to_string(utn);
    configs_[current_config_name_]["utns"][utn_str]["comment"] = value;

    if (update_td)
        data_.setTargetDataComment(utn, value);
}

bool EvaluationManager::removeShortTargets() const
{
    return remove_short_targets_;
}

void EvaluationManager::removeShortTargets(bool value)
{
    loginf << "EvaluationManager: removeShortTargets: value " << value;

    remove_short_targets_ = value;
}

unsigned int EvaluationManager::removeShortTargetsMinUpdates() const
{
    return remove_short_targets_min_updates_;
}

void EvaluationManager::removeShortTargetsMinUpdates(unsigned int value)
{
    loginf << "EvaluationManager: removeShortTargetsMinUpdates: value " << value;

    remove_short_targets_min_updates_ = value;
}

double EvaluationManager::removeShortTargetsMinDuration() const
{
    return remove_short_targets_min_duration_;
}

void EvaluationManager::removeShortTargetsMinDuration(double value)
{
    loginf << "EvaluationManager: removeShortTargetsMinDuration: value " << value;

    remove_short_targets_min_duration_ = value;
}

bool EvaluationManager::removePsrOnlyTargets() const
{
    return remove_psr_only_targets_;
}

void EvaluationManager::removePsrOnlyTargets(bool value)
{
    loginf << "EvaluationManager: removePsrOnlyTargets: value " << value;

    remove_psr_only_targets_ = value;
}

std::string EvaluationManager::removeModeACodeValues() const
{
    return remove_mode_a_code_values_;
}

std::set<std::pair<int,int>> EvaluationManager::removeModeACodeData() const // single ma,-1 or range ma1,ma2
{
    std::set<std::pair<int,int>> data;

    vector<string> parts = String::split(remove_mode_a_code_values_, ',');

    for (auto& part_it : parts)
    {
        if (part_it.find("-") != std::string::npos) // range
        {
            vector<string> sub_parts = String::split(part_it, '-');

            if (sub_parts.size() != 2)
            {
                logwrn << "EvaluationManager: removeModeACodeData: not able to parse range '" << part_it << "'";
                continue;
            }

            int val1 = String::intFromOctalString(sub_parts.at(0));
            int val2 = String::intFromOctalString(sub_parts.at(1));

            data.insert({val1, val2});
        }
        else // single value
        {
            int val1 = String::intFromOctalString(part_it);
            data.insert({val1, -1});
        }
    }

    return data;
}

void EvaluationManager::removeModeACodeValues(const std::string& value)
{
    loginf << "EvaluationManager: removeModeACodeValues: value '" << value << "'";

    remove_mode_a_code_values_ = value;
}

std::string EvaluationManager::removeTargetAddressValues() const
{
    return remove_target_address_values_;
}

std::set<unsigned int> EvaluationManager::removeTargetAddressData() const
{
    std::set<unsigned int>  data;

    vector<string> parts = String::split(remove_target_address_values_, ',');

    for (auto& part_it : parts)
    {
        int val1 = String::intFromHexString(part_it);
        data.insert(val1);
    }

    return data;
}

void EvaluationManager::removeTargetAddressValues(const std::string& value)
{
    loginf << "EvaluationManager: removeTargetAddressValues: value '" << value << "'";

    remove_target_address_values_ = value;
}

bool EvaluationManager::removeModeACOnlys() const
{
    return remove_modeac_onlys_;
}

void EvaluationManager::removeModeACOnlys(bool value)
{
    loginf << "EvaluationManager: removeModeACOnlys: value " << value;
    remove_modeac_onlys_ = value;
}

bool EvaluationManager::removeNotDetectedDBOs() const
{
    return remove_not_detected_dbos_;
}

void EvaluationManager::removeNotDetectedDBOs(bool value)
{
    loginf << "EvaluationManager: removeNotDetectedDBOs: value " << value;

    remove_not_detected_dbos_ = value;
}

bool EvaluationManager::removeNotDetectedDBO(const std::string& dbo_name) const
{
    if (!remove_not_detected_dbo_values_.contains(dbo_name))
        return false;

    return remove_not_detected_dbo_values_.at(dbo_name);
}

void EvaluationManager::removeNotDetectedDBOs(const std::string& dbo_name, bool value)
{
    loginf << "EvaluationManager: removeNotDetectedDBOs: dbo " << dbo_name << " value " << value;

    remove_not_detected_dbo_values_[dbo_name] = value;
}

bool EvaluationManager::hasADSBMOPSVersions() const
{
    return has_adsb_mops_versions_;
}

bool EvaluationManager::hasADSBMOPSVersions(unsigned int ta) const
{
    assert (has_adsb_mops_versions_);
    return adsb_mops_versions_.count(ta);
}

std::set<unsigned int> EvaluationManager::adsbMOPSVersions(unsigned int ta) const
{
    assert (has_adsb_mops_versions_);
    assert (adsb_mops_versions_.count(ta));

    return adsb_mops_versions_.at(ta);
}

bool EvaluationManager::splitResultsByMOPS() const
{
    return split_results_by_mops_;
}

bool EvaluationManager::removeTargetAddresses() const
{
    return remove_target_addresses_;
}

void EvaluationManager::removeTargetAddresses(bool value)
{
    loginf << "EvaluationManager: removeTargetAddresses: value " << value;

    remove_target_addresses_ = value;
}

bool EvaluationManager::removeModeACodes() const
{
    return remove_mode_a_codes_;
}

void EvaluationManager::removeModeACodes(bool value)
{
    loginf << "EvaluationManager: removeModeACodes: value " << value;

    remove_mode_a_codes_ = value;
}

nlohmann::json::object_t EvaluationManager::getBaseViewableDataConfig ()
{
    nlohmann::json data;
    //    "db_objects": [
    //    "Tracker"
    //    ],
    // "filters": {
    //    "Tracker Data Sources": {
    //    "active_sources": [
    //    13040,
    //    13041
    //    ]
    //    }
    //    }

    data["db_objects"] = vector<string>{dbo_name_ref_, dbo_name_tst_};

    // ref srcs
    {
        vector<unsigned int> active_ref_srcs;

        for (auto& ds_it : data_sources_ref_)
            active_ref_srcs.push_back(ds_it.first);

        data["filters"][dbo_name_ref_+" Data Sources"]["active_sources"] = active_ref_srcs;
    }

    // tst srcs
    {
        vector<unsigned int> active_tst_srcs;

        for (auto& ds_it : data_sources_tst_)
            active_tst_srcs.push_back(ds_it.first);

        data["filters"][dbo_name_tst_+" Data Sources"]["active_sources"] = active_tst_srcs;
    }

    return data;
}

nlohmann::json::object_t EvaluationManager::getBaseViewableNoDataConfig ()
{
    nlohmann::json data;

    data["db_objects"] = vector<string>{};

    return data;
}
