"""One-time pass that brings the whole project in line with .clang-format and .clang-tidy.

1. Refuses to run if projects/ has uncommitted changes, so the formatting lands
   in a commit of its own (use --force to skip this check).
2. Runs clang-format on every C++ file under projects/.
3. Runs clang-tidy and lists the remaining naming findings. These are not fixed
   automatically: clang can't import the MSVC-built modules, so clang-tidy --fix
   would rename declarations without updating their uses in other files.

Usage:
	python tools/apply_conventions.py [--clang-format PATH] [--clang-tidy PATH] [--force]
"""

import argparse
import os
import subprocess
import sys

import code_checks


def projects_dirty() -> bool:
	result = subprocess.run(["git", "status", "--porcelain", "--", code_checks.CHECKED_DIR], cwd=code_checks.ROOT, capture_output=True, text=True, check=True)
	return bool(result.stdout.strip())


def main() -> int:
	parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
	parser.add_argument("--clang-format", default="clang-format", help="clang-format executable (name on PATH or full path)")
	parser.add_argument("--clang-tidy", default="clang-tidy", help="clang-tidy executable (name on PATH or full path)")
	parser.add_argument("--force", action="store_true", help="run even if projects/ has uncommitted changes")
	parser.add_argument("-j", "--jobs", type=int, default=os.cpu_count() or 1, help="parallel jobs (default: CPU count)")
	args = parser.parse_args()

	if not args.force and projects_dirty():
		print(f"error: {code_checks.CHECKED_DIR}/ has uncommitted changes; commit or stash them first (or pass --force)", file=sys.stderr)
		return 1

	try:
		clang_format = code_checks.find_tool(args.clang_format)
		clang_tidy = code_checks.find_tool(args.clang_tidy)
	except FileNotFoundError as error:
		print(f"error: {error}", file=sys.stderr)
		return 1

	files = code_checks.project_sources()
	code_checks.run_format(clang_format, files, fix=True, jobs=args.jobs)
	print()
	tidy_clean = code_checks.run_tidy(clang_tidy, files, None, args.jobs)

	print()
	print("Next steps:")
	print("  1. Build every configuration and platform in Visual Studio; QualifierAlignment rewrites code, not just whitespace.")
	print("  2. Commit the formatting on its own and add its hash to .git-blame-ignore-revs.")
	if not tidy_clean:
		print("  3. Rename or exempt the clang-tidy findings above, build again, commit.")
	return 0


if __name__ == "__main__":
	sys.exit(main())
