// Parses the JSON-with-comments dialect VS Code accepts for `.vscode/*.json` files:
// a leading byte-order mark, `//` and `/* */` comments, and trailing commas before `}` or `]`.
// Anything else must be strict JSON; invalid input throws a SyntaxError.
export function parseJsonc(text: string): unknown {
    return JSON.parse(removeTrailingCommas(removeComments(text.replace(/^\uFEFF/, ''))));
}

function removeComments(text: string): string {
    let result = '';
    let index = 0;

    while (index < text.length) {
        const char = text[index];

        if (char === '"') {
            const end = findStringEnd(text, index);
            result += text.slice(index, end);
            index = end;
            continue;
        }

        if (char === '/' && text[index + 1] === '/') {
            const lineEnd = text.indexOf('\n', index);
            index = lineEnd === -1 ? text.length : lineEnd;
            continue;
        }

        if (char === '/' && text[index + 1] === '*') {
            const commentEnd = text.indexOf('*/', index + 2);
            if (commentEnd === -1) {
                throw new SyntaxError('Unterminated block comment');
            }
            // Keep a separator so that a comment between two tokens never joins them.
            result += ' ';
            index = commentEnd + 2;
            continue;
        }

        result += char;
        index++;
    }

    return result;
}

function removeTrailingCommas(text: string): string {
    let result = '';
    let index = 0;
    let previous = '';

    while (index < text.length) {
        const char = text[index];

        if (char === '"') {
            const end = findStringEnd(text, index);
            result += text.slice(index, end);
            previous = char;
            index = end;
            continue;
        }

        // Only a comma that follows a value is trailing, so `[,]` stays invalid.
        if (char === ',' && previous !== '' && previous !== '[' && previous !== '{' && previous !== ',') {
            let next = index + 1;
            while (next < text.length && /\s/.test(text[next])) {
                next++;
            }
            if (text[next] === '}' || text[next] === ']') {
                index++;
                continue;
            }
        }

        result += char;
        if (!/\s/.test(char)) {
            previous = char;
        }
        index++;
    }

    return result;
}

// Returns the index just past the closing quote; an unterminated string ends at the text end
// and is then rejected by JSON.parse.
function findStringEnd(text: string, start: number): number {
    let index = start + 1;

    while (index < text.length) {
        const char = text[index];
        if (char === '\\') {
            index += 2;
            continue;
        }
        index++;
        if (char === '"') {
            return index;
        }
    }

    return text.length;
}
