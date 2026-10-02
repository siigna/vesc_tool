#!/usr/bin/env python3
"""Mutation testing: breaks the code on purpose and checks that a test notices.

    tests/mutate.py                 every mutation
    tests/mutate.py string signa    only those whose name contains a fragment
    tests/mutate.py --list          what is defined, without running anything

A test that cannot fail is worse than no test, because it reads as coverage.
Four such checks were found by hand in this tree -- a config round-trip that
stayed green with both write-through calls deleted, a disconnected-pin check
that passed with the validity window removed, a message-capture check that
collected nothing, and a stability check that advanced no clock. Each was
found by severing the code and seeing nothing go red, and each was found only
because somebody happened to doubt it that day.

So the severings live here instead, as files under tests/mutations/, one per
mutation, applied by this script. Adding a check to the suite means adding the
mutation that proves it bites.

Each mutation file looks like this, and the from/to blocks are exact text:

    file: configparams.cpp
    suite: ui
    test: configSurvivesBinaryRoundTrip
    why: a narrower tx width shifts the whole rest of the stream
    --- from
            vb.vbAppendUint16(p.valInt);
    --- to
            vb.vbAppendUint8(p.valInt);

Nothing is committed, and every file is restored afterwards -- including on a
Ctrl-C, and verified by hash before the script exits. If a restore ever fails
the script says so loudly and exits non-zero; `git checkout` the named file.
"""

import hashlib
import os
import re
import shutil
import signal
import subprocess
import sys
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
MUTDIR = os.path.join(ROOT, "tests", "mutations")


def sha(path):
    with open(path, "rb") as f:
        return hashlib.sha256(f.read()).hexdigest()


class Mutation:
    def __init__(self, path):
        self.path = path
        self.name = os.path.splitext(os.path.basename(path))[0]
        text = open(path).read()

        parts = re.split(r"^--- (from|to)\s*$", text, flags=re.M)
        if len(parts) != 5 or parts[1] != "from" or parts[3] != "to":
            raise ValueError("%s: expected a '--- from' block then a "
                             "'--- to' block" % self.name)

        head, self.old, self.new = parts[0], parts[2], parts[4]
        # The blocks are exact text, minus the single newline that separates
        # them from their own header line.
        self.old = self.old[:-1] if self.old.endswith("\n") else self.old
        self.new = self.new[:-1] if self.new.endswith("\n") else self.new

        # A field's value may continue on following indented lines, so a long
        # "why" reads as a sentence in the file rather than one long line.
        fields = {}
        key = None
        for line in head.splitlines():
            m = re.match(r"^(\w+):[ \t]*(.*)$", line)
            if m:
                key = m.group(1)
                fields[key] = m.group(2).strip()
            elif key and line[:1] in (" ", "\t"):
                fields[key] = (fields[key] + " " + line.strip()).strip()
            elif not line.strip():
                key = None
        for required in ("file", "test"):
            if required not in fields:
                raise ValueError("%s: no '%s:' field" % (self.name, required))

        self.file = fields["file"]
        self.test = fields["test"]
        self.suite = fields.get("suite", "ui")
        # Some editors write through from more than one place -- ParamEditInt
        # has a plain box and a percentage box, ParamEditDouble a spin box and
        # a slider. Severing one and leaving the other would let the test pass
        # through the surviving path, so a mutation may declare how many sites
        # it expects and all of them are cut. The count is declared rather
        # than inferred: a pattern that silently starts matching a new site is
        # how a mutation stops meaning what it says.
        self.occurrences = int(fields.get("occurrences", "1"))
        # Which tier of the suite the test lives in. A GL-tier slot named
        # without this skips in the offscreen tier, which is not a failure, so
        # the mutation would read as uncaught.
        self.tier = fields.get("tier", "offscreen")
        self.why = fields.get("why", "")

    def target(self):
        return os.path.join(ROOT, self.file)


def load(fragments):
    if not os.path.isdir(MUTDIR):
        sys.exit("no %s" % MUTDIR)

    names = sorted(n for n in os.listdir(MUTDIR) if n.endswith(".mut"))
    muts = [Mutation(os.path.join(MUTDIR, n)) for n in names]

    if fragments:
        muts = [m for m in muts
                if any(f in m.name or f in m.test for f in fragments)]

    return muts


def build(suite):
    # A mutation that does not compile would leave the previous binary in
    # place, the test would pass, and the mutation would be reported as not
    # caught -- the one wrong answer this script must never give. So a build
    # failure is its own outcome.
    d = os.path.join(ROOT, "tests", suite)

    # In a fresh checkout there is no Makefile yet, and `make` would fail in a
    # way that reads as "this mutation does not compile" on the very first
    # mutation. Running qmake here also means the CI step is one command with
    # no nested shell.
    if not os.path.exists(os.path.join(d, "Makefile")):
        pro = os.path.join(d, "%s.pro" % suite)

        if not os.path.exists(pro):
            return False, "no Makefile and no %s.pro in tests/%s" % (suite,
                                                                     suite)

        q = subprocess.run(["qmake", "%s.pro" % suite], cwd=d,
                           stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)

        if q.returncode != 0:
            return False, q.stderr.decode("utf-8", "replace")[-2000:]

    r = subprocess.run(["make", "-j%d" % (os.cpu_count() or 8)], cwd=d,
                       stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
    return r.returncode == 0, r.stderr.decode("utf-8", "replace")[-2000:]


def run_test(suite, test, tier="offscreen"):
    d = os.path.join(ROOT, "tests", suite)
    argv = ["./run.sh"]

    if tier in ("gl", "light"):
        argv.append("--" + tier)

    argv.append(test)
    r = subprocess.run(argv, cwd=d,
                       stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    out = r.stdout.decode("utf-8", "replace")
    failed = re.search(r"^FAIL!", out, flags=re.M) is not None
    # A test filter that matches nothing exits 0 with no PASS line, which
    # would otherwise read as "the mutation was not caught".
    ran = re.search(r"^(PASS|FAIL!)\s+:\s+\w+::%s" % re.escape(test),
                    out, flags=re.M) is not None
    # A tier that could not run at all -- the GL tier without xvfb-run -- must
    # not look like a test that passed.
    skipped_tier = "were NOT tested" in out
    return failed, ran and not skipped_tier, out


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    flags = [a for a in sys.argv[1:] if a.startswith("--")]
    muts = load(args)

    if not muts:
        sys.exit("no mutations matched %s" % (args or "anything"))

    if "--list" in flags:
        for m in muts:
            print("%-44s %s::%s%s" % (m.name, m.suite, m.test,
                  "" if m.tier == "offscreen" else " [%s tier]" % m.tier))
            if m.why:
                print("%-44s   %s" % ("", m.why))
        return 0

    backups = {}

    def restore_all():
        bad = []
        for path, (tmp, want) in list(backups.items()):
            try:
                shutil.copyfile(tmp, path)
                if sha(path) != want:
                    bad.append(path)
            except OSError:
                bad.append(path)
            else:
                os.unlink(tmp)
                del backups[path]
        return bad

    def on_signal(_sig, _frm):
        bad = restore_all()
        print("\ninterrupted; sources restored" if not bad else
              "\ninterrupted AND RESTORE FAILED: git checkout %s"
              % " ".join(bad), file=sys.stderr)
        sys.exit(130)

    signal.signal(signal.SIGINT, on_signal)
    signal.signal(signal.SIGTERM, on_signal)

    suites = sorted({m.suite for m in muts})

    print("baseline: the suites must be green before anything is broken, or "
          "a red test proves nothing")
    for suite in suites:
        ok, err = build(suite)
        if not ok:
            print(err, file=sys.stderr)
            sys.exit("tests/%s does not build as committed" % suite)

    for m in muts:
        failed, ran, out = run_test(m.suite, m.test, m.tier)
        if failed or not ran:
            print(out[-1500:], file=sys.stderr)
            sys.exit("%s::%s is not green as committed (%s)"
                     % (m.suite, m.test, "it failed" if failed else "it did "
                        "not run -- is the name right?"))
    print("  ok, %d test(s) green\n" % len(muts))

    results = []

    for m in muts:
        path = m.target()

        if not os.path.exists(path):
            results.append((m, "NO SUCH FILE", m.file))
            continue

        src = open(path).read()
        hits = src.count(m.old)

        if hits != m.occurrences:
            results.append((m, "PATTERN x%d, EXPECTED x%d"
                            % (hits, m.occurrences), m.file))
            continue

        if path not in backups:
            fd, tmp = tempfile.mkstemp(prefix="mutate-")
            os.close(fd)
            shutil.copyfile(path, tmp)
            backups[path] = (tmp, sha(path))

        with open(path, "w") as f:
            f.write(src.replace(m.old, m.new))

        built, err = build(m.suite)

        if not built:
            results.append((m, "DOES NOT BUILD", err.strip().splitlines()[-1]
                            if err.strip() else ""))
        else:
            failed, ran, _out = run_test(m.suite, m.test, m.tier)
            if not ran:
                results.append((m, "TEST DID NOT RUN", m.test))
            elif failed:
                results.append((m, "caught", ""))
            else:
                results.append((m, "NOT CAUGHT", "the check cannot fail"))

        shutil.copyfile(backups[path][0], path)
        print("  %-44s %s" % (m.name, results[-1][1]))

    bad = restore_all()

    # Leave the committed binaries behind, not the last mutant's.
    for suite in suites:
        build(suite)

    print()
    missed = [m for m, verdict, _ in results if verdict != "caught"]

    for m, verdict, detail in results:
        if verdict != "caught":
            print("%s: %s%s" % (m.name, verdict,
                                " -- %s" % detail if detail else ""))
            if m.why:
                print("    expected %s::%s to fail: %s"
                      % (m.suite, m.test, m.why))

    if bad:
        print("RESTORE FAILED, run: git checkout %s" % " ".join(bad),
              file=sys.stderr)
        return 2

    print("%d mutation(s), %d caught, %d not"
          % (len(results), len(results) - len(missed), len(missed)))
    return 1 if missed else 0


if __name__ == "__main__":
    sys.exit(main())
