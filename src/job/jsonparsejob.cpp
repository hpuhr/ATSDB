#include "jsonparsejob.h"
#include "logger.h"

using namespace nlohmann;

JSONParseJob::JSONParseJob(std::vector<std::string>&& objects)
    : Job ("JSONParseJob"), objects_(objects)
{

}

JSONParseJob::~JSONParseJob()
{

}

void JSONParseJob::run ()
{
    loginf << "JSONParseJob: run: start with " << objects_.size() << " objects";

    started_ = true;
    assert (!json_objects_);
    json_objects_.reset(new std::vector<nlohmann::json>());

    for (auto& str_it : objects_)
    {
        try
        {
            json_objects_->push_back(json::parse(str_it));
        }
        catch (nlohmann::detail::parse_error& e)
        {
            logwrn << "JSONParseJob: run: parse error " << e.what() << " in '" << str_it << "'";
            ++parse_errors_;
            continue;
        }
        ++objects_parsed_;
    }

    loginf << "JSONParseJob: run: done with " << objects_parsed_ << " objects, errors " << parse_errors_;
    done_ = true;
}

size_t JSONParseJob::objectsParsed() const
{
    return objects_parsed_;
}

size_t JSONParseJob::parseErrors() const
{
    return parse_errors_;
}

std::unique_ptr<std::vector<nlohmann::json>>& JSONParseJob::jsonObjects()
{
    return json_objects_;
}


