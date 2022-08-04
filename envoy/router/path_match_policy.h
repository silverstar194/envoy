#pragma once

#include "envoy/config/typed_config.h"

#include "source/common/common/logger.h"

#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"

namespace Envoy {
namespace Router {

/**
 * Used to decide if path match is needed based on the target route.
 * Subclassing Logger::Loggable so that implementations can log details.
 */
class PathMatchPredicate : Logger::Loggable<Logger::Id::router> {
public:
  PathMatchPredicate() = default;
  virtual ~PathMatchPredicate() = default;

  /**
   * Used to determine if the current url matches the predicate pattern.
   *
   * @param url current url of route
   * @return valid if route url matches the predicate pattern.
   */
  virtual bool match(absl::string_view url) const PURE;

  /**
   * @return the match pattern of the predicate.
   */
  virtual std::string pattern() const PURE;

  /**
   * @return the name of the current predicate.
   */
  virtual absl::string_view name() const PURE;
};

using PathMatchPredicateSharedPtr = std::shared_ptr<PathMatchPredicate>;

/**
 * Factory for PathMatchPredicateFactory.
 */
class PathMatchPredicateFactory : public Envoy::Config::TypedFactory {
public:
  virtual ~PathMatchPredicateFactory() = default;

  /**
   * @param config contains the proto stored in TypedExtensionConfig.typed_config for the predicate.
   * @return an PathMatchPredicateSharedPtr.
   */
  virtual PathMatchPredicateSharedPtr
  createPathMatchPredicate(const Protobuf::Message& config) PURE;

  /**
   * @return the name of the match pattern predicate to be created.
   */
  virtual std::string name() const override PURE;

  /**
   * @return the category of the match pattern predicate to be created.
   */
  std::string category() const override { return "envoy.path.match"; }
};

} // namespace Router
} // namespace Envoy
