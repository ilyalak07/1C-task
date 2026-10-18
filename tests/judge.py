#!/usr/bin/env python3
"""Локальный судья по диалогу из условия
"""
import argparse
import subprocess
import sys


def fmt(x):
    return str(int(x)) if x == int(x) else str(x)


def main():
    args = sys.argv[1:]
    cut = args.index("--")
    parser = argparse.ArgumentParser()
    parser.add_argument("map")
    parser.add_argument("--broken", default="")
    parser.add_argument("--verbose", action="store_true")
    opts = parser.parse_args(args[:cut])
    solver = args[cut + 1:]

    text = open(opts.map, encoding="utf-8").read()
    profit, tunnels, day_cost, entrance = {}, {}, 0.0, None
    for line in text.splitlines():
        w = line.split()
        if not w:
            continue
        if w[0] == "day_cost":
            day_cost = float(w[1])
        elif w[0] == "entrance":
            entrance = w[1]
        elif w[0] == "vertex":
            profit[w[1]] = float(w[2])
        elif w[0] == "edge":
            tunnels[w[1]] = (w[2], w[3])
    incident = {h: [] for h in profit}
    for t, (a, b) in tunnels.items():
        incident[a].append(t)
        if b != a:
            incident[b].append(t)
    intact = {t: True for t in tunnels}
    for t in filter(None, opts.broken.split(",")):
        intact[t] = False

    proc = subprocess.Popen(solver, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                            text=True, encoding="utf-8", bufsize=1)
    proc.stdin.write(text if text.endswith("\n") else text + "\n")
    proc.stdin.flush()

    def answer(line):
        if opts.verbose:
            print(line, file=sys.stderr)
        proc.stdin.write(line + "\n")
        proc.stdin.flush()

    pos, visited, collected, days = entrance, {entrance}, profit[entrance], 0
    while True:
        command = proc.stdout.readline().split()
        if not command:
            print("WRONG solver stopped without END")
            return 1
        if opts.verbose:
            print("> " + " ".join(command), file=sys.stderr)
        if command[0] == "LOOK":
            for t in incident[pos]:
                a, b = tunnels[t]
                answer(f"PATH {t} {b if a == pos else a} {'OK' if intact[t] else 'BROKEN'}")
        elif command[0] == "MOVE" and command[1] in tunnels and pos in tunnels[command[1]]:
            a, b = tunnels[command[1]]
            if intact[command[1]]:
                pos = b if a == pos else a
                if pos not in visited:
                    visited.add(pos)
                    collected += profit[pos]
                answer("OK")
            else:
                answer("FAIL")
        elif command[0] == "CLEAR" and command[1] in tunnels:
            intact[command[1]] = True
            days += 1
            pos = entrance
            answer("OK")
        elif command[0] == "END":
            result = f"RESULT {fmt(collected)} {days} {fmt(collected - days * day_cost)}"
            answer(result)
            proc.wait()
            print(result)
            return 0
        else:
            print("WRONG bad command: " + " ".join(command))
            proc.kill()
            return 1


if __name__ == "__main__":
    sys.exit(main())
