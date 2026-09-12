#!/usr/bin/env python3
"""Copy the IPC2978 sources into a consumer without touching its build files."""
import argparse
from pathlib import Path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--include-dir', required=True, type=Path)
    parser.add_argument('--source-dir', required=True, type=Path)
    parser.add_argument('--include-prefix', default='')
    parser.add_argument('--clang-tests-dir', type=Path)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    headers = sorted((root / 'include').glob('*.hpp'))
    header_names = {path.name for path in headers}

    def copy(source, destination, clang_test=False):
        text = source.read_text()
        if args.include_prefix and not clang_test:
            for name in header_names:
                text = text.replace(f'#include "{name}"', f'#include "{args.include_prefix}{name}"')
        if clang_test:
            text = text.replace('// #define IS_THIS_CLANG_REPO', '#define IS_THIS_CLANG_REPO')
        destination.parent.mkdir(parents=True, exist_ok=True)
        if not destination.exists() or destination.read_text() != text:
            destination.write_text(text)

    for source in headers:
        copy(source, args.include_dir / source.name)
    for source in sorted((root / 'src').glob('*.cpp')):
        copy(source, args.source_dir / source.name)
    # Remove only obsolete headers previously shipped by this library.
    for name in ('rapidhash.h', 'expected.hpp'):
        (args.include_dir / name).unlink(missing_ok=True)
    if args.clang_tests_dir:
        copy(root / 'tests/ClangTest.cpp', args.clang_tests_dir / 'IPC2978Test.cpp', clang_test=True)
        for name in ('TestProcess.hpp', 'TestBuildSystem.hpp'):
            copy(root / 'tests' / name, args.clang_tests_dir / name, clang_test=True)


if __name__ == '__main__':
    main()
