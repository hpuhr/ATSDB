#include "eval/requirement/config.h"
#include "eval/requirement/group.h"
#include "logger.h"

#include <QFormLayout>
#include <QLineEdit>

using namespace std;

namespace EvaluationRequirement
{

Config::Config(
        const std::string& class_id, const std::string& instance_id,
        Group& group, EvaluationStandard& standard, EvaluationManager& eval_man)
    : Configurable(class_id, instance_id, &group), EvaluationStandardTreeItem(&group),
      group_(group), standard_(standard), eval_man_(eval_man)
{
    registerParameter("name", &name_, "");
    registerParameter("short_name", &short_name_, "");

    assert (name_.size());
    assert (short_name_.size());

    createSubConfigurables();
}

Config::~Config()
{

}


void Config::generateSubConfigurable(const std::string& class_id,
                                                          const std::string& instance_id)
{
    assert(false);
}

std::string Config::name() const
{
    return name_;
}

void Config::checkSubConfigurables()
{
}

void Config::addGUIElements(QFormLayout* layout)
{
    assert (layout);

    QLineEdit* name_edit = new QLineEdit (name_.c_str());
    connect(name_edit, &QLineEdit::textEdited, this, &Config::changedNameSlot);

    layout->addRow("Name", name_edit);

    QLineEdit* short_name_edit = new QLineEdit (short_name_.c_str());
    connect(short_name_edit, &QLineEdit::textEdited, this, &Config::changedShortNameSlot);

    layout->addRow("Short Name", short_name_edit);
}

EvaluationStandardTreeItem* Config::child(int row)
{
    return nullptr;
}

int Config::childCount() const
{
    return 0;
}

int Config::columnCount() const
{
    return 1;
}

QVariant Config::data(int column) const
{
    assert (column == 0);

    return name_.c_str();
}

int Config::row() const
{
    return 0;
}

void Config::changedNameSlot(const QString& value)
{
    string value_str = value.toStdString();

    loginf << "EvaluationRequirementConfig: changedNameSlot: name '" << value_str << "'";
}

void Config::changedShortNameSlot(const QString& value)
{
    string value_str = value.toStdString();

    loginf << "EvaluationRequirementConfig: changedShortNameSlot: name '" << value_str << "'";
}

void Config::name(const std::string& name)
{
    name_ = name;
}

void Config::shortName(const std::string& short_name)
{
    short_name_ = short_name;
}

std::string Config::shortName() const
{
    return short_name_;
}

}