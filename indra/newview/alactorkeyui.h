/** Capture UI hooks; renderer state is owned by WP0-WP4. */
#pragma once
#include <string>
namespace ALActorKeyUI
{
void initialize();
void drawBadge();
// Presentation-only sink beside the existing WP2 diagnostic log.
void recordSelfTest(const std::string& result);
std::string shadowStatus();
// Complete requests whose frame could not reach the shadow comparison.
void shadowTestUnavailable();
}
