#ifndef JSONMAPPINGJOB_H
#define JSONMAPPINGJOB_H

#include "job.h"
#include "json.hpp"

#include <vector>
#include <memory>

class JSONObjectParser;
class Buffer;

class JSONMappingJob : public Job
{
public:
    JSONMappingJob(std::unique_ptr<std::vector<nlohmann::json>> extracted_records,
                   const std::map <std::string, JSONObjectParser>& parsers, size_t key_count);
    // json obj moved, mappings referenced
    virtual ~JSONMappingJob();

    virtual void run ();

    size_t numMapped() const;
    size_t numNotMapped() const;
    size_t numCreated() const;

    std::map<std::string, std::shared_ptr<Buffer>>& buffers () { return buffers_; }

    std::map<unsigned int, std::pair<size_t, size_t> > categoryMappedCounts() const;

private:
    std::map<unsigned int, std::pair<size_t,size_t>> category_mapped_counts_; // mapped, not mapped
    size_t num_mapped_ {0}; // number of parsed where a parse was successful
    size_t num_not_mapped_ {0}; // number of parsed where no parse was successful
    size_t num_created_ {0}; // number of created objects from parsing

    std::unique_ptr<std::vector<nlohmann::json>> extracted_records_;
    const std::map <std::string, JSONObjectParser>& parsers_;
    size_t key_count_;

    std::map<std::string, std::shared_ptr<Buffer>> buffers_;
};

#endif // JSONMAPPINGJOB_H
