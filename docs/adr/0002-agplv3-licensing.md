# 2. License the project under AGPLv3

* Status: accepted
* Deciders: Yves Vogl
* Date: 2026-07-14
* Related: [ADR 0001 — Use JUCE 8 as the plugin framework](0001-use-juce8-framework.md)

## Context and Problem Statement

The intent for Silentium is to be copyleft open source software — modifications and derivatives should stay open, in the spirit of the GPL family. JUCE 8 was chosen as the plugin framework (ADR 0001), and JUCE's own licensing constrains what license this project can practically use in its open-source configuration. What license should the project ship under?

## Decision Drivers

* Original intent: copyleft open source, in the tradition of GPLv3 — not a permissive (MIT/BSD/Apache) license.
* JUCE 8's no-cost, open-source usage tier is licensed under **AGPLv3** (a change from JUCE 7 and earlier, which used **GPLv3** for the equivalent tier). Using JUCE for free requires complying with AGPLv3 for the combined work.
* The bundled VST3 SDK is GPLv3-licensed; license compatibility between the project's license and GPLv3 needs to hold.
* Avoiding paid commercial JUCE licensing, which isn't warranted for a hobbyist/community open-source project at this stage.

## Considered Options

* **AGPLv3** (matching JUCE 8's open-source tier)
* **GPLv3** (matching JUCE 7's open-source tier, and the VST3 SDK's license)
* **Permissive license (MIT/BSD/Apache-2.0)** + paid/commercial JUCE license

## Decision Outcome

Chosen option: **AGPLv3**, because it is the license JUCE 8's free tier requires, it fulfills the original copyleft intent (AGPLv3 is a strict superset of GPLv3's copyleft, adding the network-use clause), and it remains license-compatible with the GPLv3-licensed VST3 SDK.

### Consequences

* Good, because it satisfies JUCE 8's open-source licensing terms, so the project can use JUCE without a commercial license.
* Good, because it keeps the copyleft intent: modified versions of this project must also be released under AGPLv3 (or a compatible license), including when triggered by the AGPLv3-specific network-use clause — though this clause has limited practical bite for a local audio plugin, which isn't typically operated as a network service.
* Good, because AGPLv3 and GPLv3 are mutually compatible for combined works: GPLv3 §13 explicitly permits linking/combining GPLv3 code with AGPLv3 code, and AGPLv3 §13 grants the reciprocal permission for combining with GPLv3 code. This resolves the potential tension between the AGPLv3 project license and the GPLv3-licensed VST3 SDK.
* Good, because using JUCE's open-source tier legitimately permits disabling the JUCE splash screen requirement that applies to closed-source/unlicensed usage.
* Bad, because AGPLv3 is a stronger copyleft than plain GPLv3 (the network-use clause), which may deter some potential contributors or downstream users who would be comfortable with GPLv3 but not AGPLv3 — accepted as a reasonable trade-off since it flows directly from the JUCE 8 dependency chosen in ADR 0001.
* Neutral, because a future commercial/dual-licensed release remains possible: only the copyright holder(s) could relicense the project's own code commercially — this does not on its own resolve JUCE's licensing (a commercial JUCE license would still be needed for a closed-source relicensed build), and no such relicensing is planned in v1.0 scope.

## Pros and Cons of the Options

### AGPLv3

* Good, because it's what JUCE 8's free tier requires — no commercial JUCE license needed.
* Good, because it matches the copyleft intent and is GPLv3-compatible via the mutual §13 permissions.
* Bad, because it's a stronger copyleft than strictly necessary for a desktop plugin with no inherent "network use" surface.

### GPLv3

* Good, because it would match the VST3 SDK's own license directly and the original "GPLv3" framing of intent.
* Bad, because it does not satisfy JUCE **8**'s open-source tier terms (JUCE 8 requires AGPLv3, not GPLv3, for free/open-source usage) — this option was only viable for JUCE 7 and earlier.

### Permissive license + paid JUCE license

* Good, because it would allow the broadest possible downstream reuse, including closed-source derivatives.
* Bad, because it contradicts the copyleft intent behind the project from the outset.
* Bad, because it requires paying for a commercial JUCE license, which isn't justified for this project at its current stage.
