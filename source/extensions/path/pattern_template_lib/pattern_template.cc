#include "source/extensions/path/pattern_template_lib/pattern_template.h"

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "source/common/http/path_utility.h"
#include "source/extensions/path/pattern_template_lib/pattern_template_internal.h"
#include "source/extensions/path/pattern_template_lib/proto/pattern_template_rewrite_segments.pb.h"

#include "absl/status/statusor.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "re2/re2.h"

namespace Envoy {
namespace Extensions {
namespace PatternTemplate {

using PatternTemplateInternal::ParsedUrlPattern;

#ifndef SWIG
// Silence warnings about missing initializers for members of LazyRE2.
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#endif

inline re2::StringPiece toStringPiece(absl::string_view text) { return {text.data(), text.size()}; }

absl::StatusOr<std::string> convertURLPatternSyntaxToRegex(absl::string_view url_pattern) {

  absl::StatusOr<ParsedUrlPattern> status =
      PatternTemplateInternal::parseURLPatternSyntax(url_pattern);
  if (!status.ok()) {
    return status.status();
  }
  return PatternTemplateInternal::toRegexPattern(*status);
}

absl::StatusOr<std::vector<RewritePatternSegment>>
parseRewritePatternHelper(absl::string_view pattern) {
  std::vector<RewritePatternSegment> result;

  // Don't allow contiguous '/' patterns.
  static const LazyRE2 invalid_regex = {"^.*//.*$"};
  if (RE2::FullMatch(toStringPiece(pattern), *invalid_regex)) {
    return absl::InvalidArgumentError("Invalid rewrite literal pattern");
  }

  // The pattern should start with a '/' and thus the first segment should
  // always be a literal.
  if (pattern.empty() || pattern[0] != '/') {
    return absl::InvalidArgumentError("Invalid rewrite variable placement");
  }
  while (!pattern.empty()) {
    std::vector<absl::string_view> segments1 = absl::StrSplit(pattern, absl::MaxSplits('{', 1));
    if (!segments1[0].empty()) {
      if (!PatternTemplateInternal::isValidRewriteLiteral(segments1[0])) {
        return absl::InvalidArgumentError("Invalid rewrite literal pattern");
      }
      result.emplace_back(segments1[0], RewriteStringKind::KLiteral);
    }

    if (segments1.size() < 2) {
      // No more variable replacement, done.
      break;
    }

    std::vector<absl::string_view> segments2 =
        absl::StrSplit(segments1[1], absl::MaxSplits('}', 1));
    if (segments2.size() < 2) {
      return absl::InvalidArgumentError("Unmatched variable bracket");
    }
    pattern = segments2[1];

    if (!PatternTemplateInternal::isValidIndent(segments2[0])) {
      return absl::InvalidArgumentError("Invalid variable name");
    }
    result.emplace_back(segments2[0], RewriteStringKind::KVariable);
  }
  return result;
}

absl::StatusOr<envoy::extensions::pattern_template::PatternTemplateRewriteSegments>
parseRewritePattern(absl::string_view pattern, absl::string_view capture_regex) {
  envoy::extensions::pattern_template::PatternTemplateRewriteSegments parsed_pattern;
  RE2 regex = RE2(toStringPiece(capture_regex));
  if (!regex.ok()) {
    return absl::InternalError(regex.error());
  }

  absl::StatusOr<std::vector<RewritePatternSegment>> status = parseRewritePatternHelper(pattern);
  if (!status.ok()) {
    return status.status();
  }
  std::vector<RewritePatternSegment> processed_pattern = *std::move(status);

  const std::map<std::string, int>& capture_index_map = regex.NamedCapturingGroups();

  for (const auto& [str, kind] : processed_pattern) {
    switch (kind) {
    case RewriteStringKind::KLiteral:
      parsed_pattern.add_segments()->set_literal(std::string(str));
      break;
    case RewriteStringKind::KVariable:
      auto it = capture_index_map.find(std::string(str));
      if (it == capture_index_map.end()) {
        return absl::InvalidArgumentError("Nonexisting variable name");
      }
      parsed_pattern.add_segments()->set_var_index(it->second);
      break;
    }
  }

  return parsed_pattern;
}

absl::Status isValidMatchPattern(const std::string path_template_match) {
  return convertURLPatternSyntaxToRegex(path_template_match).status();
}

absl::Status isValidPathTemplateRewritePattern(const std::string& path_template_rewrite) {
  return parseRewritePatternHelper(path_template_rewrite).status();
}

absl::Status isValidSharedVariableSet(const std::string& path_template_rewrite,
                                      const std::string& capture_regex) {
  absl::StatusOr<std::string> status = convertURLPatternSyntaxToRegex(capture_regex).value();
  if (!status.ok()) {
    return status.status();
  }
  return parseRewritePattern(path_template_rewrite, *std::move(status)).status();
}

absl::StatusOr<std::string> rewriteURLTemplatePattern(
    absl::string_view url, absl::string_view capture_regex,
    const envoy::extensions::pattern_template::PatternTemplateRewriteSegments& rewrite_pattern) {
  RE2 regex = RE2(PatternTemplateInternal::toStringPiece(capture_regex));
  if (!regex.ok()) {
    return absl::InternalError(regex.error());
  }

  // First capture is the whole matched regex pattern.
  int capture_num = regex.NumberOfCapturingGroups() + 1;
  std::vector<re2::StringPiece> captures(capture_num);
  if (!regex.Match(PatternTemplateInternal::toStringPiece(url), /*startpos=*/0,
                   /*endpos=*/url.size(), RE2::ANCHOR_BOTH, captures.data(), captures.size())) {
    return absl::InvalidArgumentError("Pattern does not match");
  }

  std::string rewritten_url;

  for (const envoy::extensions::pattern_template::PatternTemplateRewriteSegments::RewriteSegment&
           segment : rewrite_pattern.segments()) {
    if (segment.has_literal()) {
      absl::StrAppend(&rewritten_url, segment.literal());
    } else if (segment.has_var_index()) {
      if (segment.var_index() < 1 || segment.var_index() >= capture_num) {
        return absl::InvalidArgumentError("Invalid variable index");
      }
      absl::StrAppend(&rewritten_url, absl::string_view(captures[segment.var_index()].as_string()));
    }
  }

  return rewritten_url;
}

} // namespace PatternTemplate
} // namespace Extensions
} // namespace Envoy
