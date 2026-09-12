> Legacy route.

# Entity Model

This guide moved to its locale-aware canonical path:

- [English](en/explanation/entity-and-property-model/index.md)
- [Русский](ru/explanation/entity-and-property-model/index.md)

The headings below preserve existing fragment links while the old route remains a durable Markdown pointer.

## Ownership model

Continue with [Ownership model](en/explanation/entity-and-property-model/index.md#ownership-model).

## Runtime entity types

Continue with [Runtime entity types](en/explanation/entity-and-property-model/index.md#runtime-entity-types).

## Entity base class

Continue with [Entity base class](en/explanation/entity-and-property-model/index.md#entity-base-class).

## Generated property wrappers

Continue with [Generated property wrappers](en/explanation/entity-and-property-model/index.md#generated-property-wrappers).

## Property runtime

Continue with [Property runtime](en/explanation/entity-and-property-model/index.md#property-runtime).

A property name may be reused for a primitive of a different serialized kind while its old values migrate into a separate property. Document loads distinguish boolean, integer and floating-point payloads; text loads distinguish explicit `True`/`False`, integral and fractional/exponent forms. When a payload uniquely matches the current primitive kind rather than the migration target's kind, it stays in the current property. Otherwise the declared stored-name migration still applies. This preserves typed save/load round trips, including a current boolean and an explicitly populated legacy integer in the same document, without discarding either field. RefType fields use the same resolution.

When a name is reused with the same serialized kind, such as `int16` replacing `int32`, the embedding project can qualify its rule: `MigrationRule Property Actor Step LegacyStep BeforeVersion DataVersion 100`. The version property and positive cutoff are project metadata; the engine has no game-specific version or migration logic. The condition applies to the original stored field name: a document version below the cutoff uses its declared alias chain, while the cutoff and later versions use the current literal property. Other unconditional aliases keep their original chain behavior, including aliases that pass through the reused name. Explicit legacy fields remain independent current fields.

Database document loads treat a missing version as zero so existing unversioned saves still migrate. Text loads serve authored prototypes, maps and editor exports: an absent version means current authored data, preserving both numeric and boolean current slots, including `SaveToText`/`ApplyFromText` round trips that omit a zero version. Importing old text through a qualified rule requires an explicit old version, including `0`. Both readers inspect the complete sibling input before property assignment, so input order cannot change resolution. RefType fields use their own sibling document/text version. A present version must be an integer and is consulted only for a matching conditional rule.

Saved document and text layouts remain unchanged; existing versioned documents need no new marker. The optional qualifier extends baked migration records from four to seven tokens, which requires metadata file layout version 4 and a rebake. Four-token rules remain unconditional, including the primitive-kind disambiguation above. Same-kind reuse without a version qualifier remains ambiguous and retains migration precedence. Load validation still rejects two inputs that resolve to the same property, including an old-version alias plus an explicit legacy-name integer.

Stored-name migrations resolve aliases before duplicate detection: a document or text input may name a property or RefType field only once, even when its old and current names differ. Duplicate aliases fail loading instead of making the final value depend on input order. RefType layouts require stored fields; registering a `Virtual` field fails before publishing the layout.

## Base properties and overlays

Continue with [Base properties and overlays](en/explanation/entity-and-property-model/index.md#base-properties-and-overlays).

## Prototypes

Continue with [Prototypes](en/explanation/entity-and-property-model/index.md#prototypes).

## Inner entities and holders

Continue with [Inner entities and holders](en/explanation/entity-and-property-model/index.md#inner-entities-and-holders).

## Events and time events

Continue with [Events and time events](en/explanation/entity-and-property-model/index.md#events-and-time-events).

## Serialization relationships

Continue with [Serialization relationships](en/explanation/entity-and-property-model/index.md#serialization-relationships).

## Tests to inspect

Continue with [Tests to inspect](en/explanation/entity-and-property-model/index.md#tests-to-inspect).

## Change routing

Continue with [Change routing](en/explanation/entity-and-property-model/index.md#change-routing).

## Validation checklist

Continue with [Validation checklist](en/explanation/entity-and-property-model/index.md#validation-checklist).
