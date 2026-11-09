from __future__ import annotations

import json
from pathlib import Path
import sys

import pytest


BUILDTOOLS_DIR = Path(__file__).resolve().parents[1]
FIXTURE_DIR = Path(__file__).resolve().parent / "fixtures" / "move_sync"

sys.path.insert(0, str(BUILDTOOLS_DIR))
import move_sync_analysis as msa  # noqa: E402


ACTOR = 101
OBSERVER = 202


def srv(ev: str, t_ms: float, **fields: object) -> str:
    return _line("srv", ev, t_ms, None, fields)


def cl(viewer: int, ev: str, t_ms: float, **fields: object) -> str:
    return _line("cl", ev, t_ms, viewer, fields)


def _line(side: str, ev: str, t_ms: float, viewer: int | None, fields: dict[str, object]) -> str:
    # The prefix imitates the logger stamp: the parser must find the marker in the middle of a line
    parts = [f"[24/09/26] [12:00:00] MOVESYNC side={side} ev={ev} t={int(t_ms * 1000)} st={int(t_ms)}"]

    if viewer is not None:
        parts.append(f"viewer={viewer}")

    parts.extend(f"{key}={value}" for key, value in fields.items())
    return " ".join(parts)


def walk_trace(server_lag_ms: float, observer_lag_ms: float, hexes: list[tuple[int, int]], step_ms: float = 300.0) -> list[str]:
    """The acting client walks the hexes, the server repeats them later and the observer later still."""
    start = hexes[0]
    end = hexes[-1]
    lines = [cl(ACTOR, "mark", 0.0, label="begin:walk")]
    lines.append(cl(ACTOR, "move_send", 1.0, cr=ACTOR, start=f"{start[0]},{start[1]}", end=f"{end[0]},{end[1]}", steps=len(hexes) - 1))
    lines.append(srv("move_req", 1.0 + server_lag_ms, cr=ACTOR, client_start=f"{start[0]},{start[1]}", server_hex=f"{start[0]},{start[1]}", bridge=0, truncated=0))
    lines.append(cl(OBSERVER, "move_recv", 1.0 + server_lag_ms + observer_lag_ms, cr=ACTOR, own=0, start=f"{start[0]},{start[1]}", end=f"{end[0]},{end[1]}", prev_hex=f"{start[0]},{start[1]}"))

    for index, hex_pos in enumerate(hexes[1:], start=1):
        actor_time = 1.0 + index * step_ms
        text = f"{hex_pos[0]},{hex_pos[1]}"
        lines.append(cl(ACTOR, "step", actor_time, cr=ACTOR, own=1, hex=text))
        lines.append(srv("step", actor_time + server_lag_ms, cr=ACTOR, hex=text))
        lines.append(cl(OBSERVER, "step", actor_time + server_lag_ms + observer_lag_ms, cr=ACTOR, own=0, hex=text))

    last = 1.0 + (len(hexes) - 1) * step_ms
    end_text = f"{end[0]},{end[1]}"
    lines.append(cl(ACTOR, "arrive", last, cr=ACTOR, own=1, end=end_text, hex=end_text))
    lines.append(cl(ACTOR, "finish_send", last, cr=ACTOR, end=end_text, hex=end_text))
    lines.append(srv("finish_req", last + server_lag_ms, cr=ACTOR, reported_end=end_text, client_hex=end_text, server_hex=end_text, remaining_ms=0, allowed_ms=90, outcome="accepted"))
    lines.append(cl(OBSERVER, "arrive", last + server_lag_ms + observer_lag_ms, cr=ACTOR, own=0, end=end_text, hex=end_text))
    lines.append(cl(ACTOR, "mark", last + 2000.0, label="end:walk"))
    return lines


def analyze_lines(lines: list[str], **options: object) -> dict:
    return msa.analyze(msa.parse_lines(lines, "all.log"), **options)["scenarios"][0]


def test_hexagonal_metric_has_six_neighbours_and_is_a_metric() -> None:
    # Every hex has exactly six neighbours at distance 1 and the metric is symmetric and triangular, which
    # catches a column-parity mistake a few point samples would miss
    points = [(x, y) for x in range(4, 12) for y in range(4, 12)]

    for a in points:
        assert msa.hexagonal_distance(a, a) == 0
        neighbours = [b for b in points if msa.hexagonal_distance(a, b) == 1]

        if 5 <= a[0] <= 10 and 5 <= a[1] <= 10:
            assert len(neighbours) == 6, a

        for b in points:
            assert msa.hexagonal_distance(a, b) == msa.hexagonal_distance(b, a)

    for a in points[::7]:
        for b in points[::5]:
            for c in points[::11]:
                assert msa.hexagonal_distance(a, c) <= msa.hexagonal_distance(a, b) + msa.hexagonal_distance(b, c)


def test_straight_run_along_x_counts_every_column() -> None:
    assert msa.hexagonal_distance((10, 10), (16, 10)) == 6


def test_square_metric_has_eight_neighbours() -> None:
    neighbours = [(x, y) for x in range(4, 7) for y in range(4, 7) if msa.square_distance((5, 5), (x, y)) == 1]

    assert len(neighbours) == 8
    assert msa.square_distance((0, 0), (3, 7)) == 7


def test_unknown_geometry_is_rejected() -> None:
    with pytest.raises(ValueError, match="unknown geometry"):
        msa.distance_for("triangular")


def test_line_with_logger_prefix_parses() -> None:
    event = msa.parse_line(srv("move_req", 12.5, cr=ACTOR, server_hex="10,11", bridge=2), "server.log", 3)

    assert event is not None
    assert (event.side, event.ev, event.t, event.st) == ("srv", "move_req", 12500, 12)
    assert event.hex_field("server_hex") == (10, 11)
    assert event.int_field("bridge") == 2
    assert event.int_field("cr") == ACTOR


def test_thread_tagged_line_parses() -> None:
    event = msa.parse_line("[07/11/26] [12:30:07] [ServerPool-0] MOVESYNC side=srv ev=step t=5 st=1 cr=7 uid=1 hex=3,4", "s", 1)

    assert event is not None
    assert event.hex_field("hex") == (3, 4)


def test_foreign_and_broken_lines_are_skipped() -> None:
    assert msa.parse_line("[x] Process_Move: async fix pathfinding failed", "s", 1) is None
    assert msa.parse_line("MOVESYNC side=srv ev=step t=oops st=1", "s", 1) is None
    assert msa.parse_line("MOVESYNC side=srv ev=step st=1", "s", 1) is None
    assert msa.parse_line("MOVESYNC side=weird ev=step t=1 st=1", "s", 1) is None
    assert msa.parse_line("MOVESYNC side=srv t=1 st=1", "s", 1) is None


def test_extra_side_is_read_only_when_requested() -> None:
    line = "MOVESYNC side=relay ev=flow t=10 st=0 link=a"

    assert msa.parse_line(line, "s", 1) is None
    event = msa.parse_line(line, "s", 1, (*msa.ENGINE_SIDES, "relay"))
    assert event is not None
    assert msa.viewpoint(event, ACTOR) == "relay"


def test_marks_pair_into_scenarios() -> None:
    events = msa.parse_lines([
        cl(ACTOR, "mark", 0, label="begin:a"),
        cl(ACTOR, "mark", 50, label="end:a"),
        cl(ACTOR, "mark", 60, label="begin:b"),
        cl(ACTOR, "mark", 90, label="end:b"),
        cl(ACTOR, "mark", 95, label="begin:unfinished"),
        cl(ACTOR, "mark", 99, label="end:never_begun"),
    ], "actor.log")
    scenarios = msa.find_scenarios(events, "t")

    assert [(scenario.label, scenario.actor) for scenario in scenarios] == [("a", ACTOR), ("b", ACTOR)]
    assert (scenarios[0].begin, scenarios[0].end) == (0, 50000)


def test_synchronized_clock_uses_milliseconds() -> None:
    events = msa.parse_lines([cl(ACTOR, "mark", 2.0, label="begin:a"), cl(ACTOR, "mark", 9.0, label="end:a")], "actor.log")
    scenario = msa.find_scenarios(events, "st")[0]

    assert (scenario.begin, scenario.end) == (2000, 9000)


def test_clean_walk_converges_and_measures_lag() -> None:
    hexes = [(10, 10), (11, 10), (12, 10), (13, 10), (14, 10)]
    scenario = analyze_lines(walk_trace(200.0, 100.0, hexes))

    assert scenario["converged"]
    assert scenario["observers"] == [OBSERVER]
    assert scenario["server"]["finalHex"] == "14,10"
    assert scenario["actor"]["finalHex"] == "14,10"
    assert scenario["server"]["behindActorMs"]["median"] == 200.0
    assert scenario["observerViews"][str(OBSERVER)]["behindServerMs"]["median"] == 100.0
    assert scenario["server"]["finishOutcomes"] == {"accepted": 1}
    assert scenario["actor"]["snaps"] == 0
    # The server trails by 200 ms at a 300 ms step, so at worst it is one hex behind
    assert scenario["actor"]["maxDivergenceFromServer"] == 1


def test_uplink_delay_pairs_each_order_with_its_processing() -> None:
    scenario = analyze_lines(walk_trace(250.0, 0.0, [(10, 10), (11, 10), (12, 10)]))

    assert scenario["server"]["uplinkMs"]["median"] == 250.0


def test_long_lag_shows_as_multi_hex_divergence() -> None:
    hexes = [(10, 10), (11, 10), (12, 10), (13, 10), (14, 10), (15, 10), (16, 10)]
    scenario = analyze_lines(walk_trace(700.0, 0.0, hexes))

    assert scenario["actor"]["maxDivergenceFromServer"] == 3
    assert scenario["actor"]["divergence"]["secondsApart3"] > 0
    assert scenario["converged"]


def test_correction_snap_and_disagreeing_observer_are_reported() -> None:
    lines = walk_trace(100.0, 50.0, [(10, 10), (11, 10), (12, 10)])
    end_mark = lines.pop()
    # The server put the critter two hexes back while the observer stayed at the end of the path
    lines.append(srv("send", 1500.0, cr=ACTOR, to="Actor", own=1, kind="pos", hex="10,10"))
    lines.append(srv("stop", 1500.0, cr=ACTOR, reason=8, hex="10,10", broadcast="custom"))
    lines.append(cl(ACTOR, "pos_recv", 1600.0, cr=ACTOR, own=1, hex="10,10", prev_hex="12,10", jump=2, was_moving=0))
    lines.append(end_mark)
    scenario = analyze_lines(lines)

    assert scenario["actor"]["snaps"] == 1
    assert scenario["actor"]["snapMaxJump"] == 2
    assert scenario["server"]["correctionsSentToActor"] == 1
    assert scenario["server"]["finalHex"] == "10,10"
    assert scenario["server"]["positionJumps"]["max"] == 2.0
    assert scenario["observerViews"][str(OBSERVER)]["finalHex"] == "12,10"
    assert not scenario["converged"]


def test_pixel_correction_and_project_input_lines_are_measured() -> None:
    lines = walk_trace(20.0, 10.0, [(10, 10), (11, 10)])
    end_mark = lines.pop()
    # A stop that a sub-hex offset moves into the next hex: a hex jump, a few pixels on screen; a line without
    # err_px stays out of the pixel summary
    lines.append(cl(OBSERVER, "pos_recv", 900.0, cr=ACTOR, own=0, hex="11,10", prev_hex="10,10", jump=1, err_px=6, was_moving=1))
    lines.append(cl(OBSERVER, "pos_recv", 950.0, cr=ACTOR, own=0, hex="11,10", prev_hex="11,10", jump=0, was_moving=0))
    lines.append(cl(ACTOR, "pos_recv", 960.0, cr=ACTOR, own=1, hex="11,10", prev_hex="11,10", jump=0, err_px=31, was_moving=0))
    lines.append(cl(ACTOR, "input", 0.5, state="W", pattern_ms=0))
    lines.append(cl(ACTOR, "input", 75.0, state="-", pattern_ms=70))
    lines.append(cl(OBSERVER, "input", 80.0, state="D", pattern_ms=0))
    lines.append(cl(ACTOR, "dir_plan", 6.0, cr=ACTOR, reason="start", dir=90, speed=134, steps=6, slid=0, hex="10,10"))
    lines.append(cl(ACTOR, "dir_plan", 400.0, cr=ACTOR, reason="extend", dir=90, speed=134, steps=6, slid=0, hex="11,10"))
    lines.append(cl(ACTOR, "dir_plan", 600.0, cr=ACTOR, reason="extend", dir=90, speed=134, steps=6, slid=1, hex="11,10"))
    lines.append(end_mark)
    scenario = analyze_lines(lines)
    observer = scenario["observerViews"][str(OBSERVER)]

    assert observer["snaps"] == 1
    assert observer["correctionPx"] == {"count": 1, "min": 6.0, "median": 6.0, "max": 6.0}
    assert scenario["actor"]["correctionPx"]["max"] == 31.0
    assert scenario["actor"]["inputChanges"] == 2
    assert scenario["actor"]["inputResponseMs"]["min"] == 0.5
    assert scenario["actor"]["dirPlans"] == {"extend": 2, "start": 1}


def test_server_steps_processed_in_one_pass_are_a_stall() -> None:
    # Three steps of one plan with one processing time: the server worker got no time for 560 ms. On-time
    # steps are not a stall, nor are equal times of steps of different plans
    lines = [cl(ACTOR, "mark", 0.0, label="begin:stall")]
    lines.append(srv("step", 100.0, cr=ACTOR, uid=1, hex="11,10", elapsed_ms=100, runtime_ms=110))
    lines.append(srv("step", 700.0, cr=ACTOR, uid=1, hex="12,10", elapsed_ms=300, runtime_ms=860))
    lines.append(srv("step", 700.0, cr=ACTOR, uid=1, hex="13,10", elapsed_ms=500, runtime_ms=860))
    lines.append(srv("step", 700.0, cr=ACTOR, uid=1, hex="14,10", elapsed_ms=700, runtime_ms=860))
    lines.append(srv("step", 800.0, cr=ACTOR, uid=3, hex="15,10", elapsed_ms=100, runtime_ms=120))
    lines.append(srv("step", 900.0, cr=ACTOR, uid=5, hex="16,10", elapsed_ms=100, runtime_ms=120))
    lines.append(cl(ACTOR, "mark", 1000.0, label="end:stall"))

    assert analyze_lines(lines)["server"]["stallMaxMs"] == 560.0


def test_intentional_catch_up_is_not_a_stall() -> None:
    # A plan started 500 ms late is fast-forwarded on purpose; its steps share one pass without starvation
    lines = [cl(ACTOR, "mark", 0.0, label="begin:catchup")]
    lines.append(srv("move_start", 100.0, cr=ACTOR, uid=1, start="10,10", end="14,10", offset_ms=500))
    lines.append(srv("step", 100.0, cr=ACTOR, uid=1, hex="11,10", elapsed_ms=200, runtime_ms=500))
    lines.append(srv("step", 100.0, cr=ACTOR, uid=1, hex="12,10", elapsed_ms=400, runtime_ms=500))
    lines.append(cl(ACTOR, "mark", 1000.0, label="end:catchup"))

    assert analyze_lines(lines)["server"]["stallMaxMs"] == 0.0


def test_server_walking_past_a_delayed_stop_is_measured() -> None:
    # The stop travelled while the server played its plan: it walked three hexes past the stop hex. The second
    # stop reaches a server that lags behind the player, which is lag, not an overshoot
    lines = [cl(ACTOR, "mark", 0.0, label="begin:stop")]
    lines.append(srv("move_start", 10.0, cr=ACTOR, uid=1, start="10,10", end="16,10"))

    for index, x in enumerate(range(11, 15)):
        lines.append(srv("step", 100.0 + index * 100.0, cr=ACTOR, uid=1, hex=f"{x},10"))

    lines.append(srv("stopmove_req", 500.0, cr=ACTOR, client_hex="11,10", server_hex="14,10", reconciled=1, final_hex="11,10", after_end=0))
    lines.append(srv("move_start", 600.0, cr=ACTOR, uid=3, start="11,10", end="17,10"))
    lines.append(srv("step", 700.0, cr=ACTOR, uid=3, hex="12,10"))
    lines.append(srv("stopmove_req", 750.0, cr=ACTOR, client_hex="14,10", server_hex="12,10", reconciled=1, final_hex="14,10", after_end=0))
    lines.append(cl(ACTOR, "mark", 1000.0, label="end:stop"))
    scenario = analyze_lines(lines)

    assert scenario["server"]["stopOvershootHexes"] == {"count": 2, "min": 0.0, "median": 1.5, "max": 3.0}
    assert scenario["server"]["stopReconciled"] == 2


def test_speed_change_delay_is_measured_per_view() -> None:
    lines = walk_trace(100.0, 50.0, [(10, 10), (11, 10), (12, 10), (13, 10)])
    end_mark = lines.pop()
    lines.append(srv("speed_change", 400.0, cr=ACTOR, old_speed=70, speed=113, elapsed_ms=260, runtime_ms=300))
    lines.append(cl(ACTOR, "speed_recv", 550.0, cr=ACTOR, own=1, old_speed=70, speed=113))
    lines.append(cl(OBSERVER, "speed_recv", 700.0, cr=ACTOR, own=0, old_speed=70, speed=113))
    lines.append(end_mark)
    scenario = analyze_lines(lines)

    assert scenario["server"]["speedChanges"] == 1
    assert scenario["server"]["speedChangeDiscardedMs"]["max"] == 40.0
    assert scenario["actor"]["speedChangeDelayMs"]["median"] == 150.0
    assert scenario["observerViews"][str(OBSERVER)]["speedChangeDelayMs"]["median"] == 300.0


def test_frame_stall_is_a_skipped_hex_not_a_late_plan() -> None:
    # The actor drew no frame for a second and skipped four hexes; the observer picked up a late plan in the
    # middle, which is a catch-up, not a stall, and a jump after a received position restarts the count
    lines = [
        cl(ACTOR, "mark", 0.0, label="begin:stall"),
        cl(ACTOR, "move_send", 1.0, cr=ACTOR, start="10,10", end="16,10", steps=6),
        cl(ACTOR, "step", 251.0, cr=ACTOR, own=1, hex="11,10", elapsed_ms=250),
        cl(ACTOR, "step", 1251.0, cr=ACTOR, own=1, hex="15,10", elapsed_ms=1250),
        cl(ACTOR, "pos_recv", 1300.0, cr=ACTOR, own=1, hex="12,10", prev_hex="15,10", jump=3, was_moving=1),
        cl(ACTOR, "step", 1320.0, cr=ACTOR, own=1, hex="16,10", elapsed_ms=1320),
        cl(OBSERVER, "move_recv", 900.0, cr=ACTOR, own=0, start="10,10", end="16,10", prev_hex="10,10", offset_ms=800),
        cl(OBSERVER, "step", 920.0, cr=ACTOR, own=0, hex="13,10", elapsed_ms=820),
        cl(OBSERVER, "step", 1150.0, cr=ACTOR, own=0, hex="14,10", elapsed_ms=1050),
        cl(ACTOR, "mark", 3000.0, label="end:stall"),
    ]
    scenario = analyze_lines(lines)

    assert scenario["actor"]["frameStallMaxMs"] == 1000.0
    assert scenario["observerViews"][str(OBSERVER)]["frameStallMaxMs"] == 0.0


def test_first_predicted_step_counts_from_the_send() -> None:
    lines = [
        cl(ACTOR, "mark", 0.0, label="begin:late_first_step"),
        cl(ACTOR, "move_send", 100.0, cr=ACTOR, start="10,10", end="14,10", steps=4),
        cl(ACTOR, "step", 600.0, cr=ACTOR, own=1, hex="13,10", elapsed_ms=700),
        cl(ACTOR, "mark", 2000.0, label="end:late_first_step"),
    ]

    assert analyze_lines(lines)["actor"]["frameStallMaxMs"] == 500.0


def test_square_geometry_changes_the_stall_verdict() -> None:
    # A diagonal step is one cell on a square grid but two hexes on a hexagonal one
    lines = [
        cl(ACTOR, "mark", 0.0, label="begin:diagonal"),
        cl(ACTOR, "move_send", 1.0, cr=ACTOR, start="10,10", end="12,8", steps=2),
        cl(ACTOR, "step", 301.0, cr=ACTOR, own=1, hex="11,9", elapsed_ms=300),
        cl(ACTOR, "mark", 1000.0, label="end:diagonal"),
    ]

    assert analyze_lines(lines, geometry="square")["actor"]["frameStallMaxMs"] == 0.0
    assert analyze_lines(lines, geometry="hexagonal")["actor"]["frameStallMaxMs"] == 300.0


def test_observer_that_lost_sight_does_not_break_convergence() -> None:
    lines = walk_trace(100.0, 50.0, [(10, 10), (11, 10), (12, 10), (13, 10)])
    end_mark = lines.pop()
    lines = [line for line in lines if not ("ev=step" in line and f"viewer={OBSERVER}" in line and "hex=13,10" in line)]
    lines = [line for line in lines if not ("ev=arrive" in line and f"viewer={OBSERVER}" in line)]
    lines.append(cl(OBSERVER, "out", 800.0, cr=ACTOR, own=0, hex="12,10"))
    lines.append(end_mark)
    scenario = analyze_lines(lines)
    view = scenario["observerViews"][str(OBSERVER)]

    assert scenario["converged"]
    assert not view["inViewAtEnd"]
    assert view["viewChanges"] == 1


def test_own_critter_re_entering_on_login_sets_the_actor_position() -> None:
    lines = [
        srv("stop", 0.0, cr=ACTOR, reason=1, hex="20,20", broadcast="all"),
        cl(ACTOR, "arrive", 0.0, cr=ACTOR, own=1, end="15,20", hex="15,20"),
        cl(ACTOR, "mark", 10.0, label="begin:relogin"),
        cl(ACTOR, "in", 500.0, cr=ACTOR, own=1, hex="20,20", moving=0),
        cl(ACTOR, "mark", 900.0, label="end:relogin"),
    ]
    scenario = analyze_lines(lines)

    assert scenario["actor"]["finalHex"] == "20,20"
    assert scenario["converged"]


def test_steps_of_the_old_plan_after_a_teleport_are_counted() -> None:
    lines = [
        cl(ACTOR, "mark", 0.0, label="begin:teleport"),
        cl(OBSERVER, "move_recv", 10.0, cr=ACTOR, own=0, start="10,10", end="20,10", prev_hex="10,10", offset_ms=0),
        cl(OBSERVER, "step", 200.0, cr=ACTOR, own=0, hex="11,10", elapsed_ms=190),
        cl(OBSERVER, "teleport_recv", 300.0, cr=ACTOR, own=0, hex="5,15", prev_hex="11,10", was_moving=1),
        cl(OBSERVER, "step", 320.0, cr=ACTOR, own=0, hex="12,10", elapsed_ms=310),
        cl(OBSERVER, "step", 560.0, cr=ACTOR, own=0, hex="13,10", elapsed_ms=550),
        cl(OBSERVER, "move_recv", 600.0, cr=ACTOR, own=0, start="5,15", end="9,15", prev_hex="13,10", offset_ms=0),
        cl(OBSERVER, "step", 800.0, cr=ACTOR, own=0, hex="6,15", elapsed_ms=190),
        cl(ACTOR, "teleport_recv", 300.0, cr=ACTOR, own=1, hex="5,15", prev_hex="11,10", was_moving=1),
        cl(ACTOR, "pos_recv", 301.0, cr=ACTOR, own=1, hex="5,15", prev_hex="5,15", jump=0, was_moving=1),
        cl(ACTOR, "step", 500.0, cr=ACTOR, own=1, hex="6,15", elapsed_ms=190),
        cl(ACTOR, "mark", 2000.0, label="end:teleport"),
    ]
    scenario = analyze_lines(lines)

    assert scenario["observerViews"][str(OBSERVER)]["staleStepsAfterTeleport"] == 2
    assert scenario["actor"]["staleStepsAfterTeleport"] == 0


def test_position_before_the_window_carries_into_a_standing_scenario() -> None:
    lines = [
        srv("stop", 0.0, cr=ACTOR, reason=1, hex="7,7", broadcast="all"),
        cl(ACTOR, "arrive", 0.0, cr=ACTOR, own=1, end="7,7", hex="7,7"),
        cl(OBSERVER, "arrive", 0.0, cr=ACTOR, own=0, end="7,7", hex="7,7"),
        cl(ACTOR, "mark", 10.0, label="begin:idle"),
        cl(OBSERVER, "step", 20.0, cr=ACTOR, own=0, hex="7,7"),
        cl(ACTOR, "mark", 30.0, label="end:idle"),
    ]
    scenario = analyze_lines(lines)

    assert scenario["converged"]
    assert scenario["server"]["finalHex"] == "7,7"


def test_lease_hold_and_frame_health_are_reported() -> None:
    lines = walk_trace(50.0, 50.0, [(10, 10), (11, 10)])
    end_mark = lines.pop()
    lines.append(cl(OBSERVER, "hold", 400.0, cr=ACTOR, own=0, on=1, elapsed_ms=300))
    lines.append(cl(OBSERVER, "hold", 650.0, cr=ACTOR, own=0, on=0, elapsed_ms=300))
    lines.append(cl(OBSERVER, "frames", 900.0, n=30, ms=1000, max_ms=80))
    lines.append(cl(OBSERVER, "frames", 1900.0, n=60, ms=1000, max_ms=20))
    lines.append(end_mark)
    view = analyze_lines(lines)["observerViews"][str(OBSERVER)]

    assert view["leaseHolds"] == {"count": 1, "ms": 250.0}
    assert view["frameHealth"] == {"count": 2, "fpsMin": 30.0, "fpsMedian": 45.0, "frameMaxMs": 80}


def test_stop_overridden_by_the_server_is_measured() -> None:
    lines = [
        cl(ACTOR, "mark", 0.0, label="begin:override"),
        cl(ACTOR, "move_send", 1.0, cr=ACTOR, start="10,10", end="16,10", steps=6),
        cl(ACTOR, "stop_send", 300.0, cr=ACTOR, hex="11,10", offset="0,0"),
        srv("step", 350.0, cr=ACTOR, uid=1, hex="13,10"),
        cl(ACTOR, "mark", 1000.0, label="end:override"),
    ]

    assert analyze_lines(lines)["actor"]["stopOverriddenHexes"] == 2


def test_extensions_receive_the_window_and_the_report() -> None:
    events = msa.parse_lines(walk_trace(100.0, 50.0, [(10, 10), (11, 10)]), "all.log")
    seen: list[str] = []

    def count_lines(window: msa.ScenarioWindow, report: dict) -> int:
        seen.append(window.scenario.label)
        return sum(1 for event in window.events if event.side == "cl" and report["converged"])

    report = msa.analyze(events, extensions={"clientLines": count_lines})

    assert seen == ["walk"]
    assert report["scenarios"][0]["extensions"]["clientLines"] > 0


def test_timeline_lists_only_the_actor_across_views() -> None:
    lines = walk_trace(100.0, 50.0, [(10, 10), (11, 10)])
    lines.insert(3, srv("step", 5.0, cr=999, hex="1,1"))
    events = msa.sorted_events(msa.parse_lines(lines, "all.log"), "t")
    scenario = msa.find_scenarios(events, "t")[0]
    timeline = msa.render_timeline(events, scenario, "t")

    assert any("server" in row and "move_req" in row for row in timeline)
    assert any(f"obs:{OBSERVER}" in row and "move_recv" in row for row in timeline)
    assert not any("hex=1,1" in row for row in timeline)


def test_engine_fixture_logs_produce_the_expected_report(tmp_path: Path) -> None:
    json_path = tmp_path.resolve() / "report.json"
    logs = [str(path) for path in sorted(FIXTURE_DIR.glob("*.log"))]

    assert msa.main([*logs, "--geometry", "hexagonal", "--json", str(json_path), "--require-converged"]) == 0

    report = json.loads(json_path.read_text(encoding="utf-8"))
    walk, blocked = report["scenarios"]

    assert report["summary"] == {"scenarios": 2, "converged": 2, "actorSnaps": 1}
    assert (walk["label"], walk["actorId"], walk["observers"]) == ("walk", 1001, [1002])
    assert walk["server"]["finishOutcomes"] == {"not_moving": 1}
    assert walk["server"]["behindActorMs"]["median"] == 5.0
    assert walk["observerViews"]["1002"]["behindServerMs"]["median"] == 40.0
    assert walk["server"]["uplinkMs"]["median"] == 40.0
    assert walk["actor"]["frameHealth"]["count"] == 2
    assert blocked["actor"]["snaps"] == 1
    assert blocked["actor"]["correctionPx"]["max"] == 32.0
    assert blocked["server"]["stopReasons"] == {"8": 1}
    assert blocked["server"]["correctionsSentToActor"] == 1
    assert blocked["server"]["finalHex"] == blocked["actor"]["finalHex"] == blocked["observerViews"]["1002"]["finalHex"] == "105,100"


def test_report_is_deterministic_across_log_order(tmp_path: Path) -> None:
    root = tmp_path.resolve()
    logs = [str(path) for path in sorted(FIXTURE_DIR.glob("*.log"))]

    assert msa.main([*logs, "--geometry", "hexagonal", "--json", str(root / "a.json")]) == 0
    assert msa.main([*reversed(logs), "--geometry", "hexagonal", "--json", str(root / "b.json")]) == 0

    first = json.loads((root / "a.json").read_text(encoding="utf-8"))
    second = json.loads((root / "b.json").read_text(encoding="utf-8"))
    assert first == second


def test_whole_trace_scenario_needs_no_marks(tmp_path: Path, capsys: pytest.CaptureFixture[str]) -> None:
    root = tmp_path.resolve()
    unmarked = root / "server.log"
    text = (FIXTURE_DIR / "server.log").read_text(encoding="utf-8")
    unmarked.write_text(text, encoding="utf-8")

    assert msa.main([str(unmarked), "--geometry", "hexagonal"]) == 2
    assert "pass --actor" in capsys.readouterr().err
    assert msa.main([str(unmarked), "--geometry", "hexagonal", "--actor", "1001", "--json", str(root / "r.json")]) == 0

    scenario = json.loads((root / "r.json").read_text(encoding="utf-8"))["scenarios"][0]
    assert (scenario["label"], scenario["server"]["finalHex"], scenario["server"]["steps"]) == ("trace", "105,100", 5)


def test_log_without_trace_lines_exits_2(tmp_path: Path, capsys: pytest.CaptureFixture[str]) -> None:
    log = tmp_path.resolve() / "client.log"
    log.write_text("[07/11/26] [12:00:00] Apply config Game.fomain\n", encoding="utf-8")

    assert msa.main([str(log), "--geometry", "square"]) == 2
    assert "Network.MoveSyncTrace" in capsys.readouterr().err


def test_missing_log_exits_2(tmp_path: Path) -> None:
    assert msa.main([str(tmp_path.resolve() / "absent.log"), "--geometry", "square"]) == 2


def test_geometry_is_required() -> None:
    with pytest.raises(SystemExit) as error:
        msa.main([str(FIXTURE_DIR / "server.log")])

    assert error.value.code == 2


def test_require_converged_fails_a_diverged_scenario(tmp_path: Path, capsys: pytest.CaptureFixture[str]) -> None:
    lines = walk_trace(100.0, 50.0, [(10, 10), (11, 10), (12, 10)])
    end_mark = lines.pop()
    lines.append(srv("stop", 1500.0, cr=ACTOR, reason=8, hex="10,10", broadcast="all"))
    lines.append(end_mark)
    log = tmp_path.resolve() / "all.log"
    log.write_text("\n".join(lines) + "\n", encoding="utf-8")
    markdown = tmp_path.resolve() / "out" / "summary.md"

    assert msa.main([str(log), "--geometry", "hexagonal", "--markdown", str(markdown)]) == 0
    assert msa.main([str(log), "--geometry", "hexagonal", "--require-converged"]) == 1
    assert "did not converge in: walk" in capsys.readouterr().err
    assert "| walk | **NO** |" in markdown.read_text(encoding="utf-8")


def test_timeline_option_prints_the_selected_scenario(capsys: pytest.CaptureFixture[str]) -> None:
    logs = [str(path) for path in sorted(FIXTURE_DIR.glob("*.log"))]

    assert msa.main([*logs, "--geometry", "hexagonal", "--timeline", "blocked"]) == 0

    output = capsys.readouterr().out
    assert "== blocked (actor 1001) ==" in output
    assert "== walk" not in output
    assert "pos_recv" in output
