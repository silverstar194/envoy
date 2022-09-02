#pragma once

#include <string>

#include "envoy/extensions/path/match/pattern_template/v3/pattern_template_match.pb.h"
#include "envoy/extensions/path/match/pattern_template/v3/pattern_template_match.pb.validate.h"
#include "envoy/extensions/path/rewrite/pattern_template/v3/pattern_template_rewrite.pb.h"
#include "envoy/extensions/path/rewrite/pattern_template/v3/pattern_template_rewrite.pb.validate.h"
#include "envoy/router/path_matcher.h"
#include "envoy/router/path_rewriter.h"

#include "source/common/protobuf/message_validator_impl.h"
#include "source/common/protobuf/utility.h"
#include "source/extensions/path/uri_template_lib/uri_template.h"

#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"

namespace Envoy {
namespace Extensions {
namespace UriTemplate {
namespace Rewrite {

const absl::string_view NAME = "envoy.path.rewrite.pattern_template.pattern_template_rewriter";

/**
 * PatternTemplateRewriter allows rewriting paths based on match pattern variables provided
 * in PatternTemplateMatcher.
 *
 * Example:
 * PatternTemplateMatcher = /foo/bar/{var}
 * PatternTemplateRewriter = /foo/{var}
 *    Will replace segment of path with value of {var}
 *    e.g. /foo/bar/cat -> /foo/cat
 */
class PatternTemplateRewriter : public Router::PathRewriter {
public:
  explicit PatternTemplateRewriter(
      const envoy::extensions::path::rewrite::pattern_template::v3::PatternTemplateRewriteConfig&
          rewrite_config)
      : rewrite_pattern_(rewrite_config.path_template_rewrite()) {}

  // Router::PathRewriter
  absl::string_view pattern() const override { return rewrite_pattern_; }

  /**
   * Concatenates literals and extracts variable values to form the final rewritten path.
   * For example:
   * rewrite_pattern: [capture_index=2, literal="cat"]
   * path: "/bar/var"
   * capture_regex: "(1)/(2)"
   * Rewrite would result in rewrite of "/var/cat".
   */
  absl::StatusOr<std::string> rewritePath(absl::string_view pattern,
                                          absl::string_view matched_path) const override;

  absl::Status isCompatiblePathMatcher(Router::PathMatcherSharedPtr path_matcher,
                                       bool active_matcher) const override;

  absl::string_view name() const override { return NAME; }

private:
  std::string rewrite_pattern_;
};

} // namespace Rewrite
} // namespace UriTemplate
} // namespace Extensions
} // namespace Envoy
