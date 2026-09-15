# Station production gameplay integration — 2026-09-15

## Player behaviour

An owned rail station can host one processing facility. Its station window
provides a recipe dropdown, material requirements, internal buffers, last-month
batch count, output destination and a removal button. Construction costs 100,000
Cr; each facility can process up to 100 batches per month. Available recipes use
the existing world-phase rules and actual cargo types enabled in the game.

Use a normal delivery order for input trains and a separate outgoing train for
output. Facility inputs are accepted without a matching nearby OpenTTD industry.
They enter the facility before any remaining cargo goes to industries or towns;
they are not credited again to a logistics hub or Megacity demand. Monthly
production consumes those buffers using the existing recipes and research bonus.

With a company logistics hub on that world, output goes to the planetary
stockpile, preserving the established hub policy. Otherwise it becomes ordinary
waiting station cargo through OpenTTD's station production path, including cargo
routing supply, ratings and animations. Output remains buffered when cargo
packets cannot be allocated and is retried on subsequent monthly updates.

## Lifecycle and authority

- Build and remove use normal server-authoritative commands; query mode validates
  without changing facilities, buffers or station acceptance.
- Invalid, foreign, duplicate, non-rail and wrong-phase placements are rejected.
- Removing the upgrade salvages internal inputs and outputs to the owner's
  planetary stockpile. Cargo already published at the station remains normal
  station cargo. Removing the final rail platform also retires the upgrade.
- Station deletion removes the attachment; company acquisition transfers facility
  ownership and bankruptcy removes the associated records.
- The existing PROD save chunk stores the station link, recipe, buffers and
  counters; published output is stored by ordinary station cargo persistence.
- Game initialization clears production and corporate manager state, preventing
  old facilities, hubs, inventory or research from leaking into another game.
- Default recipes refresh after the cargo-label map is rebuilt, so starting or
  loading a different cargo configuration does not reuse the previous catalog.

## Evidence boundaries

The migrated demo retains vanilla cargo aliases. Several conceptual Commonwealth
materials therefore share a cargo. This change supplies station processing
without claiming new raw-extraction industries, distinct active-pack cargo
acceptance, or a completed graphical/cross-world walkthrough. UAT-13 remains the
content acceptance gate; UAT-14 now has a player procedure and starts Not run.

Native UI automation could not run here: `orca-ide: command not found`.

Automated validation: the combined production-gameplay and transport selection
passes 181 assertions in 7 cases; the full suite passes 306/306 cases. The
follow-up [critical bug review](CRITICAL_BUG_REVIEW_2026-09-15.md) records a real
rail-removal lifecycle defect found by those tests and its fix. Last-platform
removal now salvages buffers immediately; partial removal updates the anchor.
Actual delivery/conversion/onward loading and save/reload tests pass after
correcting their language, town-generator and cargo-staging fixtures.
