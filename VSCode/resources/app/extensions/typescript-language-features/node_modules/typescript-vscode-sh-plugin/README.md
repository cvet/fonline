A [TypeScript plugin](https://github.com/Microsoft/TypeScript/wiki/Writing-a-Language-Service-Plugin) that replaces `getEncodedSemanticClassifications` and `getEncodedSyntacticClassifications` to provide more classifications.

The plugin uses new token classifications, consisting of a `TokenType` and with any number of `TokenModifier`.
All new classifications have a value >= 0x100 and consist of the sum of the TokenType enum value and TokenModifier enum values defined as follows:

```ts
const enum TokenType {
	'class' = 0x100,
	'enum' = 0x200,
	'interface' = 0x300,
	'namespace' = 0x400,
	'typeParameter' = 0x500,
	'type' = 0x600,
	'parameter' = 0x700,
	'variable' = 0x800,
	'property' = 0x900,
	'constant' = 0xA00,
	'function' = 0xB00,
	'member' = 0xC00
}

const enum TokenModifier {
	'declaration' = 0x01,
	'static' = 0x02,
	'async' = 0x04
}

```