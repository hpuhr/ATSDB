#ifndef SECTOR_H
#define SECTOR_H

#include "json.hpp"

class Sector
{
public:
    Sector(const std::string& name, const std::string& layer_name, std::vector<std::pair<double,double>> points);
    Sector(const std::string& name, const std::string& layer_name, const std::string& json_str);

    std::string name() const;
    void name(const std::string& name);

    std::string layerName() const;
    std::string jsonData () const;

    unsigned int size () { return points_.size(); }

protected:
    std::string name_;
    const std::string layer_name_;

    std::vector<std::pair<double,double>> points_;
};

#endif // SECTOR_H
