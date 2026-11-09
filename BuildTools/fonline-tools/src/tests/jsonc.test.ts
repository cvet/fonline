import * as assert from 'node:assert/strict';
import { test } from 'node:test';
import { parseJsonc } from '../jsonc';

test('strict JSON is parsed unchanged', () => {
    assert.deepEqual(parseJsonc('{"tasks": [{"label": "Build :: Server [sort:1]"}]}'), {tasks: [{label: 'Build :: Server [sort:1]'}]});
});

test('comments, trailing commas and a byte-order mark are accepted', () => {
    const text = '\uFEFF{\n  // line comment\n  "version": "2.0.0", /* block\n comment */\n  "tasks": [\n    {"label": "A", "args": ["x", "y",],},\n  ],\n}\n';
    assert.deepEqual(parseJsonc(text), {version: '2.0.0', tasks: [{label: 'A', args: ['x', 'y']}]});
});

test('comment markers and commas inside strings are kept', () => {
    const text = '{"command": "echo // not a comment /* nor this */", "label": "a,]", "path": "C:\\\\dir\\\\", "quote": "\\",}"}';
    assert.deepEqual(parseJsonc(text), {command: 'echo // not a comment /* nor this */', label: 'a,]', path: 'C:\\dir\\', quote: '",}'});
});

test('a block comment between tokens does not join them', () => {
    assert.deepEqual(parseJsonc('[1/**/,2]'), [1, 2]);
    assert.throws(() => parseJsonc('[1/**/2]'), SyntaxError);
});

test('invalid content is rejected', () => {
    assert.throws(() => parseJsonc('{"tasks": [,]}'), SyntaxError);
    assert.throws(() => parseJsonc('[1,,]'), SyntaxError);
    assert.throws(() => parseJsonc('{"tasks": [] /* unterminated'), SyntaxError);
    assert.throws(() => parseJsonc('{"label": "unterminated}'), SyntaxError);
    assert.throws(() => parseJsonc(''), SyntaxError);
});
