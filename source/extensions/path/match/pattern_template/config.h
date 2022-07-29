#pragma once

#include "envoy/extensions/path/match/pattern_template/v3/pattern_template_match.pb.h"
#include "envoy/extensions/path/match/pattern_template/v3/pattern_template_match.pb.validate.h"
#include "envoy/router/path_match_policy.h"

#include "source/common/protobuf/message_validator_impl.h"
#include "source/common/protobuf/utility.h"

namespace Envoy {
namespace Extensions {
namespace PatternTemplate {
namespace Match {

class PatternTemplateMatchPredicateFactory : public Router::PathMatchPredicateFactory {
public:
  Router::PathMatchPredicateSharedPtr
  createPathMatchPredicate(const Protobuf::Message& config) override {
    // TODO(silverstar194): Implement createPathMatchPredicate
   const Protobuf::Message& temp = config;
   config = temp;
   throw absl::UnimplementedError("createPathMatchPredicate not implemented");
  }

  ProtobufTypes::MessagePtr createEmptyConfigProto() override {
    return std::make_unique<
        envoy::extensions::path::match::pattern_template::v3::PatternTemplateMatchConfig>();
  }

  std::string name() const override {
    return "envoy.path.match.pattern_template.v3.pattern_template_match_predicate";
  }

  std::string category() const override { return "envoy.path.match"; }
};

} // namespace Match
} // namespace PatternTemplate
} // namespace Extensions
} // namespace Envoy
