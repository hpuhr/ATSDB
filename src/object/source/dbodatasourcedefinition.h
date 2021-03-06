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

#ifndef DBODATASOURCEDEFINITION_H
#define DBODATASOURCEDEFINITION_H

#include <QObject>

#include "configurable.h"

class DBObject;
class DBODataSourceDefinitionWidget;

/**
 * @brief Definition of a data source for a DBObject
 */
class DBODataSourceDefinition : public QObject, public Configurable
{
    Q_OBJECT

  signals:
    void definitionChangedSignal();

  public:
    /// @brief Constructor, registers parameters
    DBODataSourceDefinition(const std::string& class_id, const std::string& instance_id,
                            DBObject* object);
    virtual ~DBODataSourceDefinition();

    const std::string& schema() const { return schema_; }

    const std::string& localKey() const { return local_key_; }
    void localKey(const std::string& local_key);
    const std::string& metaTableName() const { return meta_table_; }
    void metaTable(const std::string& meta_table);
    const std::string& foreignKey() const { return foreign_key_; }
    void foreignKey(const std::string& foreign_key);
    const std::string& nameColumn() const { return name_column_; }
    void nameColumn(const std::string& name_column);

    DBODataSourceDefinitionWidget* widget();

    bool hasLatitudeColumn() const { return latitude_column_.size() > 0; }
    std::string latitudeColumn() const;
    void latitudeColumn(const std::string& latitude_column);

    bool hasLongitudeColumn() const { return longitude_column_.size() > 0; }
    std::string longitudeColumn() const;
    void longitudeColumn(const std::string& longitude_column);

    bool hasShortNameColumn() const { return short_name_column_.size() > 0; }
    std::string shortNameColumn() const;
    void shortNameColumn(const std::string& short_name_column);

    bool hasSacColumn() const { return sac_column_.size() > 0; }
    std::string sacColumn() const;
    void sacColumn(const std::string& sac_column);

    bool hasSicColumn() const { return sic_column_.size() > 0; }
    std::string sicColumn() const;
    void sicColumn(const std::string& sic_column);

    bool hasAltitudeColumn() const { return altitude_column_.size() > 0; }
    std::string altitudeColumn() const;
    void altitudeColumn(const std::string& altitude_column);

    bool hasPrimaryAzimuthStdDevColumn() const { return primary_azimuth_stddev_column_.size() > 0; }
    std::string primaryAzimuthStdDevColumn() const;
    void primaryAzimuthStdDevColumn(const std::string& value);

    // psr
    bool hasPrimaryRangeStdDevColumn() const { return primary_range_stddev_column_.size() > 0; }
    std::string primaryRangeStdDevColumn() const;
    void primaryRangeStdDevColumn(const std::string& value);

    bool hasPrimaryIRMinColumn() const { return primary_ir_min_column_.size() > 0; }
    std::string primaryIRMinColumn() const;
    void primaryIRMinColumn(const std::string& value);

    bool hasPrimaryIRMaxColumn() const { return primary_ir_max_column_.size() > 0; }
    std::string primaryIRMaxColumn() const;
    void primaryIRMaxColumn(const std::string& value);

    // ssr
    bool hasSecondaryAzimuthStdDevColumn() const { return secondary_azimuth_stddev_column_.size() > 0; }
    std::string secondaryAzimuthStdDevColumn() const;
    void secondaryAzimuthStdDevColumn(const std::string& value);

    bool hasSecondaryRangeStdDevColumn() const { return secondary_range_stddev_column_.size() > 0; }
    std::string secondaryRangeStdDevColumn() const;
    void secondaryRangeStdDevColumn(const std::string& value);

    bool hasSecondaryIRMinColumn() const { return secondary_ir_min_column_.size() > 0; }
    std::string secondaryIRMinColumn() const;
    void secondaryIRMinColumn(const std::string& value);

    bool hasSecondaryIRMaxColumn() const { return secondary_ir_max_column_.size() > 0; }
    std::string secondaryIRMaxColumn() const;
    void secondaryIRMaxColumn(const std::string& value);

    // mode s
    bool hasModeSAzimuthStdDevColumn() const { return mode_s_azimuth_stddev_column_.size() > 0; }
    std::string modeSAzimuthStdDevColumn() const;
    void modeSAzimuthStdDevColumn(const std::string& value);

    bool hasModeSRangeStdDevColumn() const { return mode_s_range_stddev_column_.size() > 0; }
    std::string modeSRangeStdDevColumn() const;
    void modeSRangeStdDevColumn(const std::string& value);

    bool hasModeSIRMinColumn() const { return mode_s_ir_min_column_.size() > 0; }
    std::string modeSIRMinColumn() const;
    void modeSIRMinColumn(const std::string& value);

    bool hasModeSIRMaxColumn() const { return mode_s_ir_max_column_.size() > 0; }
    std::string modeSIRMaxColumn() const;
    void modeSIRMaxColumn(const std::string& value);

protected:
    DBObject* object_{nullptr};

    /// DBSchema identifier
    std::string schema_;
    /// Identifier for key in main table
    std::string local_key_;
    /// Identifier for meta table with data sources
    std::string meta_table_;
    /// Identifier for key in meta table with data sources
    std::string foreign_key_;
    /// Identifier for sensor name column in meta table with data sources
    std::string short_name_column_;
    std::string name_column_;
    std::string sac_column_;
    std::string sic_column_;
    std::string latitude_column_;
    std::string longitude_column_;
    std::string altitude_column_;

    /// radar columns
    std::string primary_azimuth_stddev_column_;
    std::string primary_range_stddev_column_;
    std::string primary_ir_min_column_;
    std::string primary_ir_max_column_;

    std::string secondary_azimuth_stddev_column_;
    std::string secondary_range_stddev_column_;
    std::string secondary_ir_min_column_;
    std::string secondary_ir_max_column_;

    std::string mode_s_azimuth_stddev_column_;
    std::string mode_s_range_stddev_column_;
    std::string mode_s_ir_min_column_;
    std::string mode_s_ir_max_column_;

    DBODataSourceDefinitionWidget* widget_{nullptr};
};

Q_DECLARE_METATYPE(DBODataSourceDefinition*)
Q_DECLARE_METATYPE(const DBODataSourceDefinition*)

#endif  // DBODATASOURCEDEFINITION_H
