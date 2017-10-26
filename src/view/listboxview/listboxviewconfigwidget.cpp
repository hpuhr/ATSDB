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

#include <QVBoxLayout>
#include <QPushButton>
#include <QCheckBox>
#include <QLineEdit>
#include <QLabel>
#include <QMessageBox>

#include "dbobjectmanager.h"
#include "dbovariableorderedsetwidget.h"
#include "listboxview.h"
#include "listboxviewconfigwidget.h"
#include "listboxviewdatasource.h"
#include "logger.h"
#include "stringconv.h"

using namespace Utils;

ListBoxViewConfigWidget::ListBoxViewConfigWidget( ListBoxView* view, QWidget* parent )
    :   QWidget( parent ), view_( view )
{
    QVBoxLayout *vlayout = new QVBoxLayout;

    assert (view_);

    variable_set_widget_ = view_->getDataSource()->getSet()->widget();
    vlayout->addWidget (variable_set_widget_);

    presentation_check_ = new QCheckBox("Use Presentation");
    presentation_check_->setChecked(view_->usePresentation());
    connect(presentation_check_, SIGNAL( clicked() ), this, SLOT( toggleUsePresentation() ));
    vlayout->addWidget(presentation_check_);

    vlayout->addStretch();

    overwrite_check_ = new QCheckBox("Overwrite Exported File");
    overwrite_check_->setChecked(view_->overwriteCSV());
    connect(overwrite_check_, SIGNAL( clicked() ), this, SLOT( toggleUseOverwrite() ));
    vlayout->addWidget(overwrite_check_);

    export_button_ = new QPushButton ("Export");
    connect(export_button_, SIGNAL(clicked(bool)), this, SLOT(exportSlot()));
    vlayout->addWidget(export_button_);

    setLayout( vlayout );
}

ListBoxViewConfigWidget::~ListBoxViewConfigWidget()
{
}

void ListBoxViewConfigWidget::toggleUsePresentation()
{
  assert (presentation_check_);
  bool checked = presentation_check_->checkState() == Qt::Checked;
  logdbg  << "ListBoxViewConfigWidget: toggleUsePresentation: setting use presentation to " << checked;
  view_->usePresentation(checked);
}

void ListBoxViewConfigWidget::toggleUseOverwrite()
{
    assert (overwrite_check_);
    bool checked = overwrite_check_->checkState() == Qt::Checked;
    logdbg  << "ListBoxViewConfigWidget: toggleUseOverwrite: setting overwrite to " << checked;
    view_->overwriteCSV (checked);

}

void ListBoxViewConfigWidget::exportSlot()
{
    logdbg << "ListBoxViewConfigWidget: exportSlot";
    assert (overwrite_check_);
    assert (export_button_);

    export_button_->setDisabled(true);
    emit exportSignal(overwrite_check_->checkState() == Qt::Checked);
}

void ListBoxViewConfigWidget::exportDoneSlot(bool cancelled)
{
    assert (export_button_);

    export_button_->setDisabled(false);

    if (!cancelled)
    {
        QMessageBox msgBox;
        msgBox.setText("Export complete.");
        msgBox.exec();
    }
}
