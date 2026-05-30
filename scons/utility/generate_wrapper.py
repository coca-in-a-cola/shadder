#!/usr/bin/env python3
"""Generate a namespace wrapper header that brings global symbols into a namespace."""

import re
from pathlib import Path


_CLASS_LIKE = re.compile(
    r'\b(?:class|struct|enum(?:\s+class)?|union)\s+(\w+)'
)
_USING_ALIAS = re.compile(
    r'\busing\s+(\w+)\s*='
)
_TYPEDEF = re.compile(
    r'\btypedef\b.*\s+(\w+)\s*;'
)
_CONSTEXPR = re.compile(
    r'\bconstexpr\s+(?:\w+\s+)+\s*(\w+)\s*='
)
_FUNC = re.compile(
    r'(\w+)\s*\([^)]*\)\s*(?:const|override|final)?\s*;'
)


def _strip_comments(text: str) -> str:
    text = re.sub(r'//.*', '', text)
    text = re.sub(r'/\*.*?\*/', '', text, flags=re.DOTALL)
    return text


def _strip_template_headers(text: str) -> str:
    return re.sub(r'template\s*<[^>]*>\s*\n?', '', text)


def _keep_global_scope(text: str) -> str:
    result = []
    i = 0
    depth = 0
    n = len(text)

    while i < n:
        c = text[i]

        if c == '/' and i + 1 < n:
            if text[i + 1] == '/':
                end = text.find('\n', i)
                if end == -1:
                    end = n
                if depth == 0:
                    result.append(text[i:end])
                i = end
                continue
            if text[i + 1] == '*':
                end = text.find('*/', i + 2)
                if end == -1:
                    end = n - 2
                else:
                    end += 2
                if depth == 0:
                    result.append(text[i:end])
                i = end
                continue

        if c == '"':
            end = i + 1
            while end < n and text[end] != '"':
                if text[end] == '\\':
                    end += 1
                end += 1
            if end < n:
                end += 1
            if depth == 0:
                result.append(text[i:end])
            i = end
            continue

        if c == '\'':
            end = i + 1
            while end < n and text[end] != '\'':
                if text[end] == '\\':
                    end += 1
                end += 1
            if end < n:
                end += 1
            if depth == 0:
                result.append(text[i:end])
            i = end
            continue

        if c == '{':
            if depth == 0:
                result.append('{')
            depth += 1
        elif c == '}':
            depth -= 1
            if depth == 0:
                result.append('}')
        else:
            if depth == 0:
                result.append(c)

        i += 1

    return ''.join(result)


def extract_symbols(filepath: Path, symbols: set) -> set:
    text = filepath.read_text(encoding='utf-8', errors='ignore')
    text = _strip_comments(text)
    text = _strip_template_headers(text)
    text = _keep_global_scope(text)

    for match in _CLASS_LIKE.finditer(text):
        symbols.add(match.group(1))
    for match in _USING_ALIAS.finditer(text):
        symbols.add(match.group(1))
    for match in _TYPEDEF.finditer(text):
        symbols.add(match.group(1))
    for match in _CONSTEXPR.finditer(text):
        symbols.add(match.group(1))
    for match in _FUNC.finditer(text):
        name = match.group(1)
        if name not in ('main',):
            symbols.add(name)

    return symbols


def generate_wrapper(header_paths, output: Path, namespace: str):
    symbols = set()
    for h in header_paths:
        extract_symbols(Path(h), symbols)

    symbols = sorted(symbols)

    with output.open('w', encoding='utf-8', newline='\n') as f:
        f.write('#pragma once\n')
        f.write('\n')
        f.write('#include "index.h"\n')
        f.write('\n')
        f.write(f'namespace {namespace} {{\n')
        for sym in symbols:
            f.write(f'    using ::{sym};\n')
        f.write('}\n')


if __name__ == '__main__':
    import argparse

    parser = argparse.ArgumentParser(
        description='Generate a namespace wrapper header'
    )
    parser.add_argument('output', type=Path, help='Output wrapper header path')
    parser.add_argument('namespace', help='Namespace name')
    parser.add_argument('headers', nargs='+', help='C++ header files to scan')
    args = parser.parse_args()
    generate_wrapper(args.headers, args.output, args.namespace)
