#ifndef EVALUATIONREQUIREMENTRESULT_H
#define EVALUATIONREQUIREMENTRESULT_H

#include <memory>
#include <vector>

class EvaluationRequirement;
class EvaluationTargetData;
class EvaluationManager;

namespace EvaluationResultsReport {
    class Section;
    class RootItem;
    class SectionContentTable;
}

class EvaluationRequirementResult
{
public:
    EvaluationRequirementResult(std::shared_ptr<EvaluationRequirement> requirement,
                                std::vector<unsigned int> utns, std::vector<const EvaluationTargetData*> targets,
                                EvaluationManager& eval_man);

    std::shared_ptr<EvaluationRequirement> requirement() const;

    virtual void join(const std::shared_ptr<EvaluationRequirementResult> other_base); // joins other result to this one

    virtual std::shared_ptr<EvaluationRequirementResult> copy() = 0; // copies this instance

    virtual void print() = 0;
    virtual void addToReport (std::shared_ptr<EvaluationResultsReport::RootItem> root_item) = 0;

    std::vector<unsigned int> utns() const;
    std::string utnsString() const;

protected:
    std::shared_ptr<EvaluationRequirement> requirement_;

    std::vector<unsigned int> utns_; // utns used to generate result
    std::vector<const EvaluationTargetData*> targets_; // targets used to generate result

    EvaluationManager& eval_man_;

    EvaluationResultsReport::SectionContentTable& getReqOverviewTable (
            std::shared_ptr<EvaluationResultsReport::RootItem> root_item);

    EvaluationResultsReport::Section& getRequirementSection (
            std::shared_ptr<EvaluationResultsReport::RootItem> root_item);
};

#endif // EVALUATIONREQUIREMENTRESULT_H
