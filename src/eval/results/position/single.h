#ifndef EVALUATIONREQUIREMENPOSITIONMAXDISTANCERESULT_H
#define EVALUATIONREQUIREMENPOSITIONMAXDISTANCERESULT_H

#include "eval/results/single.h"
#include "eval/requirement/position/positionmaxdistance.h"

namespace EvaluationRequirementResult
{

class SinglePositionMaxDistance : public Single
{
public:
    SinglePositionMaxDistance(
            const std::string& result_id, std::shared_ptr<EvaluationRequirement::Base> requirement,
            const SectorLayer& sector_layer,
            unsigned int utn, const EvaluationTargetData* target, EvaluationManager& eval_man,
            int num_pos, int num_no_ref, int num_pos_outside, int num_pos_inside, int num_pos_ok, int num_pos_nok,
            double error_min, double error_max, double error_avg,
            std::vector<EvaluationRequirement::PositionMaxDistanceDetail> details);

    virtual void print() override;
    virtual void addToReport (std::shared_ptr<EvaluationResultsReport::RootItem> root_item) override;

    virtual std::shared_ptr<Joined> createEmptyJoined(const std::string& result_id) override;

    int numPos() const;
    int numNoRef() const;
    int numPosOutside() const;
    int numPosInside() const;
    int numPosOk() const;
    int numPosNOk() const;

    double errorMin() const;
    double errorMax() const;
    double errorAvg() const;

    std::vector<EvaluationRequirement::PositionMaxDistanceDetail>& details();

    virtual bool hasViewableData (
            const EvaluationResultsReport::SectionContentTable& table, const QVariant& annotation) override;
    virtual std::unique_ptr<nlohmann::json::object_t> viewableData(
            const EvaluationResultsReport::SectionContentTable& table, const QVariant& annotation) override;

    virtual bool hasReference (
            const EvaluationResultsReport::SectionContentTable& table, const QVariant& annotation) override;
    virtual std::string reference(
            const EvaluationResultsReport::SectionContentTable& table, const QVariant& annotation) override;


protected:
    int num_pos_ {0};
    int num_no_ref_ {0};
    int num_pos_outside_ {0};
    int num_pos_inside_ {0};
    int num_pos_ok_ {0};
    int num_pos_nok_ {0};

    double error_min_ {0};
    double error_max_ {0};
    double error_avg_ {0};

    bool has_p_max_pos_ {false};
    float p_max_pos_{0};

    std::vector<EvaluationRequirement::PositionMaxDistanceDetail> details_;

    void updatePMaxPos();
    void addTargetToOverviewTable(std::shared_ptr<EvaluationResultsReport::RootItem> root_item);
    void addTargetDetailsToReport(std::shared_ptr<EvaluationResultsReport::RootItem> root_item);
    void reportDetails(EvaluationResultsReport::Section& utn_req_section);
};

}

#endif // EVALUATIONREQUIREMENPOSITIONMAXDISTANCERESULT_H
