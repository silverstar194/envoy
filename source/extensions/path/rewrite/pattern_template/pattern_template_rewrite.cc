#include "source/extensions/path/rewrite/pattern_template/pattern_template_rewrite.h"

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "source/common/http/path_utility.h"
#include "source/extensions/path/match/pattern_template/pattern_template_match.h"
#include "source/extensions/path/uri_template_lib/uri_template_internal.h"

#include "absl/status/statusor.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "re2/re2.h"

namespace Envoy {
namespace Extensions {
namespace UriTemplate {
namespace Rewrite {

#ifndef SWIG
// Silence warnings about missing initializers for members of LazyRE2.
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#endif

absl::Status
PatternTemplateRewriter::isCompatiblePathMatcher(Router::PathMatcherSharedPtr path_matcher,
                                                 bool active_matcher) const {
  if (!active_matcher || path_matcher->name() != Extensions::UriTemplate::Match::NAME) {
    return absl::InvalidArgumentError(fmt::format("unable to use {} extension without {} extension",
                                                  Extensions::UriTemplate::Rewrite::NAME,
                                                  Extensions::UriTemplate::Match::NAME));
  }

  // This is needed to match up variable values.
  // Validation between extensions as they share rewrite pattern variables.
  if (!isValidSharedVariableSet(rewrite_pattern_, path_matcher->pattern()).ok()) {
    return absl::InvalidArgumentError(
        fmt::format("mismatch between variables in path_match_policy {} and path_rewrite_policy {}",
                    path_matcher->pattern(), rewrite_pattern_));
  }

  return absl::OkStatus();
}

absl::StatusOr<std::string>
PatternTemplateRewriter::rewritePath(absl::string_view pattern,
                                     absl::string_view matched_path) const {
  absl::StatusOr<std::string> regex_pattern = convertPathPatternSyntaxToRegex(matched_path);
  if (!regex_pattern.ok()) {
    return absl::InvalidArgumentError("Unable to parse matched_path");
  }
  std::string regex_pattern_str = *std::move(regex_pattern);

  absl::StatusOr<RewriteSegments> rewrite_pattern =
      parseRewritePattern(rewrite_pattern_, regex_pattern_str);

  if (!rewrite_pattern.ok()) {
    return absl::InvalidArgumentError("Unable to parse path rewrite pattern");
  }

  const RewriteSegments& rewrite_pattern_segments = *std::move(rewrite_pattern);
  RE2 regex = RE2(Internal::toStringPiece(regex_pattern_str));
  if (!regex.ok()) {
    return absl::InternalError(regex.error());
  }

  // First capture is the whole matched regex pattern.
  int capture_num = regex.NumberOfCapturingGroups() + 1;
  std::vector<re2::StringPiece> captures(capture_num);
  if (!regex.Match(Internal::toStringPiece(pattern), /*startpos=*/0,
                   /*endpos=*/pattern.size(), RE2::ANCHOR_BOTH, captures.data(), captures.size())) {
    return absl::InvalidArgumentError("Pattern not match");
  }

  std::string new_path;
  for (const RewriteSegment& segment : rewrite_pattern_segments) {
    auto* literal = absl::get_if<std::string>(&segment);
    auto* capture_index = absl::get_if<int>(&segment);
    if (literal != nullptr) {
      absl::StrAppend(&new_path, *literal);
    } else if (capture_index != nullptr) {
      if (*capture_index < 1 || *capture_index >= capture_num) {
        return absl::InvalidArgumentError("Invalid variable index");
      }
      absl::StrAppend(&new_path, absl::string_view(captures[*capture_index].as_string()));
    }
  }

  return {new_path};
}

} // namespace Rewrite
} // namespace UriTemplate
} // namespace Extensions
} // namespace Envoy
