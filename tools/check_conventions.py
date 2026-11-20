"""Check the project's layout and naming conventions that clang-format and clang-tidy can't express.

Rules (ID: what it checks):
  line-length      Lines longer than --max-columns (tabs count as --tab-width columns).
  leading-comma    Multi-line argument/parameter lists use trailing commas. Leading commas are
                   only allowed in constructor initializer and base-class lists.
  operator-at-end  Long expressions break before the operator (| || && ? + ...), not after it.
  break-after-paren
                   A multi-line call or declaration breaks right after '(' instead of leaving
                   the first argument on the '(' line. Conditions of if/while/for/switch are exempt.
  close-paren      When a list breaks after '(', its ')' starts its own line.
  error-return     Keep 'return std::unexpected(Error{' together on one line.
  abbreviations    Type and module names don't run two abbreviations together (DebugUID3D12),
                   using the list in tools/naming_abbreviations.txt.
  if-one-line      'if (...) statement;' is written on two lines. Off by default; enable with
                   --enable if-one-line.

Usage:
	python tools/check_conventions.py [FILES...] [--max-columns N] [--disable RULE ...] [--enable RULE ...]

With no FILES, every C++ file under projects/ is checked. Exit code is 1 if anything is reported.
"""

import argparse
import re
import sys
from dataclasses import dataclass
from pathlib import Path

import code_checks
import naming_abbreviations

ALL_RULES = [
	"line-length",
	"leading-comma",
	"operator-at-end",
	"break-after-paren",
	"close-paren",
	"error-return",
	"abbreviations",
	"if-one-line",
]
DEFAULT_OFF = {"if-one-line"}

# Operators that should start a continuation line rather than end one.
# '*' and '&' are left out: at the end of a line they are usually pointer/reference declarators.
TRAILING_OPERATOR = re.compile(r"(\|\||&&|(?<![|&<>=!])\||\?|(?<![+\-])[+\-](?![+\-])|/|%|\^)\s*$")
CONTROL_KEYWORD = re.compile(r"\b(if|while|for|switch)\s*(constexpr\s*)?$")
TYPE_DECLARATION = re.compile(r"\b(?:class|struct|union|enum(?:\s+class)?)\s+([A-Za-z_]\w*)|\bmodule\s+([A-Za-z_]\w*)")
CAMEL_WORDS = re.compile(r"[A-Z0-9]+(?![a-z])|[A-Z][a-z0-9]*|[a-z0-9]+")
ONE_LINE_IF = re.compile(r"^\s*(?:else\s+)?if\s*\(.*\)\s*[^\s{].*;\s*$")


@dataclass
class Finding:
	path: Path
	line: int
	column: int
	rule: str
	message: str

	def __str__(self) -> str:
		return f"{code_checks.relative(self.path)}:{self.line}:{self.column}: [{self.rule}] {self.message}"


def strip_comments_and_strings(text: str) -> list[str]:
	"""Return the file's lines with comments and string/char literals blanked out (columns preserved)."""
	out = []
	i, n = 0, len(text)
	in_block = False
	buffer = []
	while i < n:
		c = text[i]
		nxt = text[i + 1] if i + 1 < n else ""
		if in_block:
			if c == "*" and nxt == "/":
				buffer.append("  ")
				i += 2
				in_block = False
			else:
				buffer.append("\n" if c == "\n" else " ")
				i += 1
			continue
		if c == "/" and nxt == "*":
			in_block = True
			buffer.append("  ")
			i += 2
		elif c == "/" and nxt == "/":
			while i < n and text[i] != "\n":
				buffer.append(" ")
				i += 1
		elif c in "\"'":
			quote = c
			buffer.append(c)
			i += 1
			while i < n and text[i] != quote and text[i] != "\n":
				if text[i] == "\\" and i + 1 < n:
					buffer.append("  ")
					i += 2
				else:
					buffer.append(" ")
					i += 1
			if i < n and text[i] == quote:
				buffer.append(quote)
				i += 1
		else:
			buffer.append(c)
			i += 1
	out = "".join(buffer).split("\n")
	return [line.rstrip("\r") for line in out]


def display_width(line: str, tab_width: int) -> int:
	return len(line.expandtabs(tab_width))


def check_abbreviation_runs(name: str, abbreviations: set[str]) -> str | None:
	"""Return the offending run if two or more abbreviations are written back to back."""

	def splits(run: str) -> bool:
		if not run:
			return True
		return any(run.startswith(a) and splits(run[len(a) :]) for a in abbreviations)

	for word in CAMEL_WORDS.findall(name):
		if word.isupper() and word not in abbreviations and any(word.startswith(a) and splits(word[len(a) :]) for a in abbreviations):
			return word
	return None


def check_file(path: Path, rules: set[str], max_columns: int, tab_width: int, abbreviations: set[str]) -> list[Finding]:
	text = path.read_text(encoding="utf-8", errors="replace")
	raw_lines = text.replace("\r\n", "\n").split("\n")
	# Drop macro line continuations so a '\' at the end doesn't hide how the line really ends.
	code_lines = [re.sub(r"\\\s*$", "", line).rstrip() for line in strip_comments_and_strings(text.replace("\r\n", "\n"))]
	findings: list[Finding] = []

	def report(line_index: int, column: int, rule: str, message: str) -> None:
		if rule in rules:
			findings.append(Finding(path, line_index + 1, column + 1, rule, message))

	in_initializer_list = False
	# Stack of open parentheses: (line index, layout, is a control-statement condition), where layout is
	# "break" (nothing after '(' on its line), "hug" (the line ends with another opener, as in
	# 'std::unexpected(Error{' or 'SUCCEEDED(Call('), or "inline" (an argument follows on the same line).
	open_parens: list[tuple[int, str, bool]] = []

	for index, (raw, code) in enumerate(zip(raw_lines, code_lines)):
		stripped = code.strip()

		width = display_width(raw.rstrip(), tab_width)
		if width > max_columns:
			report(index, max_columns, "line-length", f"line is {width} columns, limit is {max_columns}")

		if not stripped or stripped.startswith("#"):
			continue

		# Constructor initializer and base-class lists may use leading commas.
		if stripped.startswith(":") and not stripped.startswith("::"):
			in_initializer_list = True
		elif stripped.startswith(","):
			if not in_initializer_list:
				report(index, code.index(","), "leading-comma", "use a trailing comma on the previous line instead of a leading comma")
		else:
			in_initializer_list = False

		if TRAILING_OPERATOR.search(stripped) and not stripped.endswith("operator" + stripped[-1:]):
			report(index, len(code.rstrip()) - 1, "operator-at-end", "break before the operator: move it to the start of the next line")

		if ONE_LINE_IF.match(code):
			report(index, code.index("if"), "if-one-line", "put the statement on the next line")

		if re.search(r"\breturn\s+std::unexpected\(\s*$", stripped):
			next_code = code_lines[index + 1].strip() if index + 1 < len(code_lines) else ""
			if re.match(r"(gg::)?Error\{", next_code):
				report(index, code.index("return"), "error-return", "keep 'return std::unexpected(Error{' on one line")

		for match in TYPE_DECLARATION.finditer(code):
			name = match.group(1) or match.group(2)
			run = check_abbreviation_runs(name, abbreviations)
			if run:
				report(index, match.start(), "abbreviations", f"'{name}' runs abbreviations together ('{run}'); reword the name")

		# Parentheses: track where each '(' was opened and how its line ended.
		for column, char in enumerate(code):
			if char == "(":
				before = code[:column].rstrip()
				is_condition = bool(CONTROL_KEYWORD.search(before))
				rest = code[column + 1 :].strip()
				layout = "break" if not rest else "hug" if rest[-1] in "({" else "inline"
				open_parens.append((index, layout, is_condition))
			elif char == ")" and open_parens:
				opened_line, layout, is_condition = open_parens.pop()
				if opened_line != index and layout == "break" and not is_condition and code[:column].strip():
					report(index, column, "close-paren", "put ')' on its own line (the list breaks after '(')")

		# A '(' left open at the end of this line with an argument after it means the first argument
		# stayed on the '(' line.
		for opened_line, layout, is_condition in open_parens:
			if opened_line == index and layout == "inline" and not is_condition:
				report(index, 0, "break-after-paren", "list continues on the next line: break right after '(' and put one item per line")
				break

	return findings


def main() -> int:
	parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
	parser.add_argument("files", nargs="*", type=Path, help="files to check (default: all C++ files under projects/)")
	parser.add_argument("--max-columns", type=int, default=140, help="maximum line width (default: 140)")
	parser.add_argument("--tab-width", type=int, default=4, help="columns per tab (default: 4)")
	parser.add_argument("--disable", nargs="+", default=[], choices=ALL_RULES, metavar="RULE", help="rules to skip")
	parser.add_argument("--enable", nargs="+", default=[], choices=ALL_RULES, metavar="RULE", help="rules to turn on")
	parser.add_argument("--summary", action="store_true", help="print only a count per rule")
	args = parser.parse_args()

	rules = (set(ALL_RULES) - DEFAULT_OFF - set(args.disable)) | set(args.enable)

	if args.files:
		files = [p.resolve() for p in args.files if code_checks.is_project_source(p.resolve()) and p.is_file()]
	else:
		files = code_checks.project_sources()

	abbreviations = set(naming_abbreviations.read_abbreviations())
	findings = []
	for path in files:
		findings += check_file(path, rules, args.max_columns, args.tab_width, abbreviations)

	if args.summary:
		for rule in ALL_RULES:
			if rule in rules:
				print(f"{rule:18} {sum(1 for f in findings if f.rule == rule)}")
	else:
		for finding in findings:
			print(finding)

	print(f"conventions: {len(findings)} finding(s) in {len({f.path for f in findings})} file(s), {len(files)} file(s) checked")
	return 1 if findings else 0


if __name__ == "__main__":
	sys.exit(main())
