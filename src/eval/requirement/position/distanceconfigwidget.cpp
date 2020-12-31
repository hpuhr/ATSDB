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

#include "eval/requirement/position/distanceconfigwidget.h"
#include "eval/requirement/position/distanceconfig.h"
#include "textfielddoublevalidator.h"
#include "logger.h"

#include <QLineEdit>
#include <QFormLayout>
#include <QCheckBox>

namespace EvaluationRequirement
{

    PositionDistanceConfigWidget::PositionDistanceConfigWidget(PositionDistanceConfig& config)
        : QWidget(), config_(config)
    {
        form_layout_ = new QFormLayout();

        config_.addGUIElements(form_layout_);

        // max ref time diff
        max_ref_time_diff_edit_ = new QLineEdit(QString::number(config_.maxRefTimeDiff()));
        max_ref_time_diff_edit_->setValidator(new QDoubleValidator(0.0, 30.0, 2, this));
        connect(max_ref_time_diff_edit_, &QLineEdit::textEdited,
                this, &PositionDistanceConfigWidget::maxRefTimeDiffEditSlot);

        form_layout_->addRow("Maximum Reference Time Difference [s]", max_ref_time_diff_edit_);


        // max dist
        max_abs_value_edit_ = new QLineEdit(QString::number(config_.maxAbsValue()));
        max_abs_value_edit_->setValidator(new QDoubleValidator(0.0, 10000.0, 2, this));
        connect(max_abs_value_edit_, &QLineEdit::textEdited,
                this, &PositionDistanceConfigWidget::maxAbsValueEditSlot);

        form_layout_->addRow("Maximum Absolute Value [m]", max_abs_value_edit_);

        // prob
        minimum_prob_edit_ = new QLineEdit(QString::number(config_.minimumProbability()));
        minimum_prob_edit_->setValidator(new QDoubleValidator(0.0001, 1.0, 4, this));
        connect(minimum_prob_edit_, &QLineEdit::textEdited,
                this, &PositionDistanceConfigWidget::minimumProbEditSlot);

        form_layout_->addRow("Minimum Probability [1]", minimum_prob_edit_);

        setLayout(form_layout_);
    }

    void PositionDistanceConfigWidget::maxRefTimeDiffEditSlot(QString value)
    {
        loginf << "PositionDistanceConfigWidget: maxRefTimeDiffEditSlot: value " << value.toStdString();

        bool ok;
        float val = value.toFloat(&ok);

        if (ok)
            config_.maxRefTimeDiff(val);
        else
            loginf << "PositionDistanceConfigWidget: maxRefTimeDiffEditSlot: invalid value";
    }


    void PositionDistanceConfigWidget::maxAbsValueEditSlot(QString value)
    {
        loginf << "PositionDistanceConfigWidget: maxAbsValueEditSlot: value " << value.toStdString();

        bool ok;
        float val = value.toFloat(&ok);

        if (ok)
            config_.maxAbsValue(val);
        else
            loginf << "PositionDistanceConfigWidget: maxDistanceEditSlot: invalid value";
    }

    void PositionDistanceConfigWidget::PositionDistanceConfigWidget::minimumProbEditSlot(QString value)
    {
        loginf << "PositionDistanceConfigWidget: maximumProbEditSlot: value " << value.toStdString();

        bool ok;
        float val = value.toFloat(&ok);

        if (ok)
            config_.minimumProbability(val);
        else
            loginf << "PositionDistanceConfigWidget: maximumProbEditSlot: invalid value";
    }

}