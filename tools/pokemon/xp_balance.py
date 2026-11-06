#!/usr/bin/env python3
"""Offline Commander-paced Pokemon XP balancing tool.

Does not change production firmware, profiles, inventory or game rewards.
Assumes a single active Pokemon earns XP from each validated game.
"""
import argparse
import math

def next_xp(level, base=30, step=6):
    if not 1 <= level < 100:
        raise ValueError("level must be 1..99")
    return base + step * level

def total_xp(base=30, step=6):
    return sum(next_xp(level, base, step) for level in range(1, 100))

def games_for_lvl100(win_rate, base=30, step=6, loss_xp=250, win_xp=350):
    if not 0 <= win_rate <= 1:
        raise ValueError("win rate must be 0..1")
    expected = loss_xp * (1 - win_rate) + win_xp * win_rate
    return math.ceil(total_xp(base, step) / expected)

def self_test():
    assert total_xp(100, 25) == 133650  # current firmware formula
    assert games_for_lvl100(0, 100, 25, 50, 75) == 2673
    assert games_for_lvl100(1, 100, 25, 50, 75) == 1782
    assert total_xp(30, 6) == 32670
    assert games_for_lvl100(0) == 131
    assert games_for_lvl100(1) == 94
    assert games_for_lvl100(0.25) == 119
    assert games_for_lvl100(0.5) == 109

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--base", type=int, default=30)
    parser.add_argument("--step", type=int, default=6)
    parser.add_argument("--loss-xp", type=int, default=250)
    parser.add_argument("--win-xp", type=int, default=350)
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()
    if args.self_test:
        self_test()
        print("Balance self-test passed")
        return
    if min(args.base, args.step, args.loss_xp, args.win_xp) < 0 or min(args.loss_xp,args.win_xp) == 0:
        parser.error("XP settings must be nonnegative; rewards must both be positive")
    print(f"Total XP level 1 -> 100: {total_xp(args.base, args.step):,}")
    print("Win rate | Expected games | At 1 game/week | At 2 games/week")
    for wr in (0, 0.25, 0.5, 0.75, 1):
        n=games_for_lvl100(wr,args.base,args.step,args.loss_xp,args.win_xp)
        print(f"{wr:>6.0%}   | {n:>14} | {n/52:>13.1f} yr | {n/104:>13.1f} yr")

if __name__ == "__main__":
    main()
