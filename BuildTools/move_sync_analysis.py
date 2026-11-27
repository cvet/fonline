"""Analyse movement synchronization traces written with ``Network.MoveSyncTrace``.

The server and every client write one ``MOVESYNC`` log line per movement synchronization event
(``WriteMoveSyncTrace`` in ``Source/Common/Movement.cpp``). This tool merges those lines from several
logs, cuts them into scenarios and compares three views of one critter in each scenario: where the
server holds it, where its own client draws it and where every other client sees it. The report holds
divergence, lag and correction metrics plus an optional merged timeline per scenario.

Scenarios are delimited by ``ev=mark`` lines with ``label=begin:<name>`` / ``label=end:<name>`` that an
embedding project writes from its scripts in the same line format. Without marks, ``--actor`` treats the
whole trace as one scenario of that critter.

Clocks: every process on one machine reads the same monotonic clock, so ``--clock t`` (the default,
microseconds) measures cross-process lag directly. Logs from several machines share only ``--clock st``
(synchronized time, milliseconds); a client's ``st`` trails the server by the delivery of its last time
synchronization, so cross-process delays measured with it are estimates.
"""

from __future__ import annotations

import argparse
import bisect
import json
import statistics
import sys
from collections.abc import Callable, Iterable, Mapping
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any


MARKER = "MOVESYNC"
BEGIN_PREFIX = "begin:"
END_PREFIX = "end:"
ENGINE_SIDES = ("srv", "cl")
GEOMETRIES = ("hexagonal", "square")
WHOLE_TRACE_LABEL = "trace"

# How much history before a scenario is analysed with it: each view's position at the start and the plan
# the server was walking towards a stop live in events written before the begin mark
HISTORY_US = 120_000_000

# Events that move a critter in each view, and the field that carries its new hex
SERVER_POSITION_FIELDS = {
    "step": "hex",
    "stop": "hex",
    "move_req": "server_hex",
    "move_start": "start",
    "stopmove_req": "final_hex",
    "finish_req": "server_hex",
}
CLIENT_POSITION_FIELDS = {
    "in": "hex",
    "step": "hex",
    "arrive": "hex",
    "pos_recv": "hex",
    "teleport_recv": "hex",
    "stop_send": "hex",
    "finish_send": "hex",
}

Hex = tuple[int, int]
Distance = Callable[[Hex, Hex], int]


@dataclass
class Event:
    side: str
    ev: str
    t: int
    st: int
    fields: dict[str, str]
    source: str
    line_no: int

    def time(self, clock: str) -> int:
        # Both clocks are brought to microseconds so the rest of the analysis does not know which one is used
        return self.t if clock == "t" else self.st * 1000

    def int_field(self, name: str, default: int = 0) -> int:
        value = self.fields.get(name)

        if value is None:
            return default

        try:
            return int(value)
        except ValueError:
            return default

    def hex_field(self, name: str) -> Hex | None:
        return parse_hex(self.fields.get(name))


@dataclass
class Scenario:
    label: str
    actor: int
    begin: int
    end: int


@dataclass
class ScenarioWindow:
    """What an extension sees: the scenario, its events (with history) and the clock in use."""

    scenario: Scenario
    events: list[Event]
    clock: str
    settle_us: int


@dataclass
class Timeline:
    """Step function of a position: (time, hex) points in ascending time."""

    points: list[tuple[int, Hex | None]] = field(default_factory=list)
    times: list[int] = field(default_factory=list)

    def add(self, time: int, hex_pos: Hex | None) -> None:
        self.points.append((time, hex_pos))

    def finish(self) -> None:
        # The sort is stable, so two events of one moment keep their log order
        self.points.sort(key=lambda point: point[0])
        self.times = [time for time, _ in self.points]

    def at(self, time: int) -> Hex | None:
        index = bisect.bisect_right(self.times, time)
        return self.points[index - 1][1] if index > 0 else None

    def within(self, begin: int, end: int) -> list[tuple[int, Hex | None]]:
        return [point for point in self.points if begin <= point[0] <= end]


ScenarioExtension = Callable[[ScenarioWindow, dict[str, Any]], Any]


def hexagonal_distance(a: Hex, b: Hex) -> int:
    """Port of GeometryHelper::GetDistance for FO_GEOMETRY HEXAGONAL."""
    x1, y1 = a
    x2, y2 = b
    dx = abs(x1 - x2)

    if x1 % 2 == 0:
        if y2 <= y1:
            rx = y1 - y2 - dx // 2
            return dx + max(rx, 0)

        rx = y2 - y1 - (dx + 1) // 2
        return dx + max(rx, 0)

    if y2 >= y1:
        rx = y2 - y1 - dx // 2
        return dx + max(rx, 0)

    rx = y1 - y2 - (dx + 1) // 2
    return dx + max(rx, 0)


def square_distance(a: Hex, b: Hex) -> int:
    """Port of GeometryHelper::GetDistance for FO_GEOMETRY SQUARE."""
    return max(abs(a[0] - b[0]), abs(a[1] - b[1]))


def distance_for(geometry: str) -> Distance:
    if geometry == "hexagonal":
        return hexagonal_distance
    if geometry == "square":
        return square_distance

    raise ValueError(f"unknown geometry: {geometry}")


def parse_hex(value: str | None) -> Hex | None:
    if not value or "," not in value:
        return None

    left, right = value.split(",", 1)

    try:
        return int(left), int(right)
    except ValueError:
        return None


def parse_line(line: str, source: str, line_no: int, sides: Iterable[str] = ENGINE_SIDES) -> Event | None:
    # The logger prefixes date, time and sometimes a thread name, so the marker is searched, not anchored
    index = line.find(MARKER + " ")

    if index < 0:
        return None

    fields: dict[str, str] = {}

    for token in line[index + len(MARKER) + 1:].split():
        if "=" not in token:
            continue

        key, value = token.split("=", 1)
        fields[key] = value

    side = fields.pop("side", "")
    ev = fields.pop("ev", "")

    if side not in set(sides) or not ev:
        return None

    try:
        t = int(fields.pop("t"))
        st = int(fields.pop("st"))
    except (KeyError, ValueError):
        return None

    return Event(side, ev, t, st, fields, source, line_no)


def parse_lines(lines: Iterable[str], source: str, sides: Iterable[str] = ENGINE_SIDES) -> list[Event]:
    allowed = tuple(sides)
    events = []

    for line_no, line in enumerate(lines, start=1):
        event = parse_line(line, source, line_no, allowed)

        if event is not None:
            events.append(event)

    return events


def parse_logs(paths: Iterable[Path], sides: Iterable[str] = ENGINE_SIDES) -> list[Event]:
    allowed = tuple(sides)
    events: list[Event] = []

    for path in paths:
        text = path.read_text(encoding="utf-8", errors="replace")
        events.extend(parse_lines(text.splitlines(), path.name, allowed))

    return events


def sorted_events(events: list[Event], clock: str) -> list[Event]:
    # Within one source the line order is more reliable than equal timestamps, so it is the second key
    return sorted(events, key=lambda event: (event.time(clock), event.source, event.line_no))


def find_scenarios(events: list[Event], clock: str) -> list[Scenario]:
    """Scenarios are delimited by begin:<name> and end:<name> marks of the acting client."""
    open_marks: dict[str, Event] = {}
    scenarios: list[Scenario] = []

    for event in sorted_events(events, clock):
        if event.side != "cl" or event.ev != "mark":
            continue

        label = event.fields.get("label", "")

        if label.startswith(BEGIN_PREFIX):
            open_marks[label[len(BEGIN_PREFIX):]] = event
        elif label.startswith(END_PREFIX):
            name = label[len(END_PREFIX):]
            begin = open_marks.pop(name, None)

            if begin is not None:
                scenarios.append(Scenario(name, begin.int_field("viewer"), begin.time(clock), event.time(clock)))

    return scenarios


def whole_trace_scenario(events: list[Event], actor: int, clock: str) -> Scenario:
    times = [event.time(clock) for event in events]
    return Scenario(WHOLE_TRACE_LABEL, actor, min(times), max(times))


def server_timeline(events: list[Event], critter: int, clock: str) -> Timeline:
    timeline = Timeline()

    for event in events:
        if event.side != "srv" or event.int_field("cr") != critter:
            continue

        field_name = SERVER_POSITION_FIELDS.get(event.ev)

        # The server reports a teleport only as a send; the position is the same for every recipient
        if event.ev == "send" and event.fields.get("kind") == "teleport":
            field_name = "hex"

        if field_name is None:
            continue

        hex_pos = event.hex_field(field_name)

        if hex_pos is not None:
            timeline.add(event.time(clock), hex_pos)

    timeline.finish()
    return timeline


def client_timeline(events: list[Event], viewer: int, critter: int, clock: str) -> Timeline:
    timeline = Timeline()

    for event in events:
        if event.side != "cl" or event.int_field("viewer") != viewer or event.int_field("cr") != critter:
            continue

        # A critter that left the view is unknown to the client: its last hex is stale memory, not an opinion
        if event.ev == "out":
            timeline.add(event.time(clock), None)
            continue

        field_name = CLIENT_POSITION_FIELDS.get(event.ev)

        if field_name is None:
            continue

        hex_pos = event.hex_field(field_name)

        if hex_pos is not None:
            timeline.add(event.time(clock), hex_pos)

    timeline.finish()
    return timeline


def step_sequence(events: list[Event], side: str, critter: int, clock: str, begin: int, end: int, viewer: int | None = None) -> list[tuple[int, Hex]]:
    sequence = []

    for event in events:
        if event.side != side or event.ev != "step" or event.int_field("cr") != critter:
            continue
        if viewer is not None and event.int_field("viewer") != viewer:
            continue

        time = event.time(clock)
        hex_pos = event.hex_field("hex")

        if hex_pos is not None and begin <= time <= end:
            sequence.append((time, hex_pos))

    sequence.sort(key=lambda point: point[0])
    return sequence


def frame_stalls_ms(events: list[Event], critter: int, clock: str, begin: int, end: int, viewer: int, distance: Distance) -> list[float]:
    """Client frame stalls: adjacent steps of one plan between which the client skipped more than one hex.

    Prediction follows the clock, so a skipped hex means no frame was drawn for longer than a step. Network
    lag does not look like this, it arrives as a plan correction, so every received plan, position or
    teleport restarts the count; the client's own sent plan sets the reference point instead. Each value is
    how many milliseconds the gap lasted."""
    stalls: list[float] = []
    previous: tuple[int, Hex, int] | None = None

    for event in events:
        if event.side != "cl" or event.int_field("cr") != critter or event.int_field("viewer") != viewer:
            continue

        time = event.time(clock)

        if time < begin or time > end:
            continue
        if event.ev == "move_send":
            # The client predicts its own move from the start hex from the moment it sends it, so even the
            # first step that skips hexes is a stall
            start = event.hex_field("start")
            previous = (time, start, 0) if start is not None else None
            continue
        if event.ev != "step":
            previous = None
            continue

        hex_pos = event.hex_field("hex")
        elapsed = event.int_field("elapsed_ms", -1)

        if hex_pos is None:
            continue

        if previous is not None and elapsed >= previous[2] and distance(previous[1], hex_pos) > 1:
            stalls.append((time - previous[0]) / 1000.0)

        previous = (time, hex_pos, elapsed)

    return stalls


def server_stalls_ms(events: list[Event], critter: int, clock: str, begin: int, end: int) -> list[float]:
    """Server movement stalls: several steps of one plan processed in one pass. The stall length is how far
    the first of them lagged behind the plan time, which is what a server whose worker got no time shows."""
    steps = [event for event in events if event.side == "srv" and event.ev in {"move_start", "lease_req", "step"}
             and event.int_field("cr") == critter and event.time(clock) <= end]
    stalls: list[float] = []
    catch_up: dict[int, int] = {}
    index = 0

    while index < len(steps):
        first = steps[index]
        uid = first.int_field("uid")

        if first.ev in {"move_start", "lease_req"}:
            added = first.int_field("offset_ms" if first.ev == "move_start" else "catchup_ms")
            catch_up[uid] = catch_up.get(uid, 0) + max(added, 0)
            index += 1
            continue

        batch_end = index + 1

        while (batch_end < len(steps) and steps[batch_end].ev == "step" and steps[batch_end].int_field("uid") == uid
               and steps[batch_end].int_field("runtime_ms") == first.int_field("runtime_ms")):
            batch_end += 1

        # A late plan fast-forwarded on purpose also runs several steps in one pass; that is protocol work,
        # not worker starvation, and must not excuse a failed gate
        delayed = first.int_field("runtime_ms") - first.int_field("elapsed_ms") - catch_up.pop(uid, 0)

        if batch_end - index > 1 and first.time(clock) >= begin and delayed > 0:
            stalls.append(float(delayed))

        index = batch_end

    return stalls


def stale_steps_after_teleport(events: list[Event], critter: int, clock: str, begin: int, end: int, viewer: int) -> int:
    """Steps a client drew along the old plan after a teleport. The server stops a critter before moving it,
    so no step may follow until the next plan or position; each such step walks an abandoned path."""
    stale = 0
    after_teleport = False

    for event in events:
        if event.side != "cl" or event.int_field("cr") != critter or event.int_field("viewer") != viewer:
            continue

        time = event.time(clock)

        if time < begin or time > end:
            continue
        if event.ev == "teleport_recv":
            after_teleport = True
        elif event.ev in {"move_recv", "move_send", "pos_recv", "in", "out"}:
            after_teleport = False
        elif event.ev == "step" and after_teleport:
            stale += 1

    return stale


def align_lag_ms(leader: list[tuple[int, Hex]], follower: list[tuple[int, Hex]], lookahead: int = 4) -> list[float]:
    """How many milliseconds later the following view entered the same hex as the leading one.

    The alignment is sequential and greedy: each leader hex is searched at most a few steps past the last
    follower match, so a loop over the same hexes is not matched with another pass. A negative value means
    the follower was there first."""
    lags: list[float] = []
    cursor = 0

    for leader_time, hex_pos in leader:
        for offset in range(lookahead):
            index = cursor + offset

            if index >= len(follower):
                break

            follower_time, follower_hex = follower[index]

            if follower_hex == hex_pos:
                lags.append((follower_time - leader_time) / 1000.0)
                cursor = index + 1
                break

    return lags


def uplink_delays_ms(sends: list[Event], requests: list[Event], clock: str) -> list[float]:
    """Milliseconds from a client's move order to its processing on the server: transit plus worker queue.
    Pairs are matched in order and by start hex, since one player's orders are sent and read in order."""
    delays: list[float] = []
    cursor = 0

    for send in sends:
        start = send.hex_field("start")

        for index in range(cursor, len(requests)):
            request = requests[index]

            if request.hex_field("client_start") == start and request.time(clock) >= send.time(clock):
                delays.append((request.time(clock) - send.time(clock)) / 1000.0)
                cursor = index + 1
                break

    return delays


def stop_overshoot_hexes(events: list[Event], critter: int, clock: str, stops: list[Event]) -> list[float]:
    """For each stop, how many server steps followed the hex where the client stopped, along the server's
    current or just finished plan. Zero when the server had not reached that hex or it is not on its path."""
    moves = [event for event in events if event.side == "srv" and event.int_field("cr") == critter and event.ev in {"move_start", "step"}]
    overshoots: list[float] = []

    for stop in stops:
        stop_time = stop.time(clock)
        path: list[Hex] = []
        uid = None

        for event in moves:
            if event.time(clock) > stop_time:
                break

            if event.ev == "move_start":
                uid = event.int_field("uid")
                start = event.hex_field("start")
                path = [start] if start is not None else []
            elif event.int_field("uid") == uid:
                hex_pos = event.hex_field("hex")

                if hex_pos is not None:
                    path.append(hex_pos)

        client_hex = stop.hex_field("client_hex")

        if client_hex is None or client_hex not in path:
            overshoots.append(0.0)
            continue

        last_index = len(path) - 1 - path[::-1].index(client_hex)
        overshoots.append(float(len(path) - 1 - last_index))

    return overshoots


def speed_change_delays_ms(server_changes: list[Event], client_changes: list[Event], clock: str) -> list[float]:
    """Milliseconds by which a client applied a speed change after the server. Each server change is paired
    with the first unused reception of the same new speed after it."""
    delays: list[float] = []
    used: set[int] = set()

    for change in server_changes:
        for index, received in enumerate(client_changes):
            if index in used or received.int_field("speed") != change.int_field("speed"):
                continue
            if received.time(clock) < change.time(clock):
                continue

            used.add(index)
            delays.append((received.time(clock) - change.time(clock)) / 1000.0)
            break

    return delays


def max_divergence(first: Timeline, second: Timeline, begin: int, end: int, distance: Distance) -> int:
    """Largest distance between two views over the interval where both know a position."""
    times = sorted({time for time, _ in first.within(begin, end)} | {time for time, _ in second.within(begin, end)} | {begin, end})
    result = 0

    for time in times:
        a = first.at(time)
        b = second.at(time)

        if a is not None and b is not None:
            result = max(result, distance(a, b))

    return result


def divergence_profile(view: Timeline, reference: Timeline, begin: int, end: int, distance: Distance) -> dict[str, Any]:
    """Divergence from the server over time, not only at its worst: accumulated hex-seconds, seconds apart
    by at least one and at least three hexes, and when the views last disagreed. Counted where both views
    know a position; the position between events is a step function."""
    times = sorted({time for time, _ in view.within(begin, end)} | {time for time, _ in reference.within(begin, end)} | {begin})
    hex_seconds = 0.0
    apart = 0.0
    apart_far = 0.0
    last_apart: int | None = None

    for index, time in enumerate(times):
        a = view.at(time)
        b = reference.at(time)
        following = times[index + 1] if index + 1 < len(times) else end

        if a is None or b is None or following <= time:
            continue

        gap = distance(a, b)
        seconds = (following - time) / 1_000_000.0

        if gap > 0:
            hex_seconds += gap * seconds
            apart += seconds
            last_apart = following

        if gap >= 3:
            apart_far += seconds

    return {"hexSeconds": round(hex_seconds, 2), "secondsApart": round(apart, 2), "secondsApart3": round(apart_far, 2),
            "lastApartMs": None if last_apart is None else round((last_apart - begin) / 1000.0)}


def frame_health(window: list[Event], viewer: int) -> dict[str, Any]:
    """Client frame rate from its once-a-second frames lines: lowest, median and the longest frame. Movement
    is processed once a frame, so a client drawing five frames a second measures its host, not the protocol."""
    rates = []
    longest = 0

    for event in window:
        if event.side != "cl" or event.ev != "frames" or event.int_field("viewer") != viewer:
            continue

        duration = event.int_field("ms")

        if duration > 0:
            rates.append(event.int_field("n") * 1000.0 / duration)

        longest = max(longest, event.int_field("max_ms"))

    if not rates:
        return {"count": 0}

    return {"count": len(rates), "fpsMin": round(min(rates), 1), "fpsMedian": round(statistics.median(rates), 1), "frameMaxMs": longest}


def stop_overridden_hexes(actor_window: list[Event], server_final: Hex | None, distance: Distance) -> int | None:
    """How far the server overrode a player who stopped: the distance between where the last stop order put
    the critter and where the server left it. None when the player's last order was not a stop."""
    last_stop: Hex | None = None

    for event in actor_window:
        if event.ev == "stop_send":
            last_stop = event.hex_field("hex")
        elif event.ev in {"move_send", "dir_plan"}:
            last_stop = None

    if last_stop is None or server_final is None:
        return None

    return distance(last_stop, server_final)


def lease_holds(window: list[Event], clock: str, end: int) -> dict[str, Any]:
    """How often and how long an observer held another critter at the end of its held-direction lease while
    waiting for a renewal: a standing critter is the visible cost of a stalled player link."""
    count = 0
    total_us = 0
    since: int | None = None

    for event in window:
        if event.ev in {"move_recv", "pos_recv", "teleport_recv", "out"} and since is not None:
            total_us += event.time(clock) - since
            since = None

        if event.ev != "hold":
            continue

        if event.int_field("on") == 1 and since is None:
            since = event.time(clock)
            count += 1
        elif event.int_field("on") == 0 and since is not None:
            total_us += event.time(clock) - since
            since = None

    if since is not None:
        total_us += max(end - since, 0)

    return {"count": count, "ms": round(total_us / 1000.0, 1)}


def input_response_ms(inputs: list[Event], sends: list[Event], clock: str, horizon_ms: float = 500.0) -> list[float]:
    """Time from an input change to the client sending a move or a stop: the latency a player feels. A change
    followed by nothing within horizon_ms (a stick turn that does not change the move) is not counted."""
    send_times = sorted(event.time(clock) for event in sends)
    delays = []

    for event in inputs:
        time = event.time(clock)
        index = bisect.bisect_left(send_times, time)

        if index < len(send_times) and send_times[index] - time <= horizon_ms * 1000:
            delays.append((send_times[index] - time) / 1000.0)

    return delays


def step_reversals(steps: list[tuple[int, Hex]]) -> int:
    """How often a view stepped onto a hex and straight back (A, B, A): visible jitter in place."""
    return sum(1 for index in range(2, len(steps)) if steps[index][1] == steps[index - 2][1] and steps[index][1] != steps[index - 1][1])


def server_position_jumps(reference: Timeline, begin: int, end: int, distance: Distance) -> list[int]:
    """Server position jumps of more than one hex (stop reconciliation, bridges, teleports); every such jump
    is a correction an observer will see."""
    jumps = []
    previous: Hex | None = reference.at(begin)

    for _, hex_pos in reference.within(begin, end):
        if hex_pos is None:
            continue

        if previous is not None and distance(previous, hex_pos) > 1:
            jumps.append(distance(previous, hex_pos))

        previous = hex_pos

    return jumps


def summarize(values: list[float]) -> dict[str, Any]:
    if not values:
        return {"count": 0}

    ordered = sorted(values)
    return {
        "count": len(ordered),
        "min": round(ordered[0], 1),
        "median": round(statistics.median(ordered), 1),
        "max": round(ordered[-1], 1),
    }


def count_by(events: Iterable[Event], field_name: str) -> dict[str, int]:
    counts: dict[str, int] = {}

    for event in events:
        key = event.fields.get(field_name, "?")
        counts[key] = counts.get(key, 0) + 1

    return dict(sorted(counts.items()))


def hex_text(hex_pos: Hex | None) -> str | None:
    return None if hex_pos is None else f"{hex_pos[0]},{hex_pos[1]}"


def analyze_scenario(events: list[Event], scenario: Scenario, clock: str, settle_us: int = 0, distance: Distance = hexagonal_distance) -> dict[str, Any]:
    actor = scenario.actor
    begin = scenario.begin
    end = scenario.end + settle_us
    window = [event for event in events if begin <= event.time(clock) <= end]

    observers = sorted({event.int_field("viewer") for event in window
                        if event.side == "cl" and event.int_field("cr") == actor and event.int_field("viewer") not in (actor, 0)})

    srv_line = server_timeline(events, actor, clock)
    actor_line = client_timeline(events, actor, actor, clock)

    srv_window = [event for event in window if event.side == "srv" and event.int_field("cr") == actor]
    actor_window = [event for event in window if event.side == "cl" and event.int_field("viewer") == actor and event.int_field("cr") == actor]

    finish_events = [event for event in srv_window if event.ev == "finish_req"]
    stopmove_events = [event for event in srv_window if event.ev == "stopmove_req"]
    move_reqs = [event for event in srv_window if event.ev == "move_req"]
    bridges = [event.int_field("bridge") for event in move_reqs]

    # A correction of the own critter is any received plan or position about it; a snap is one that moved it
    # away from where the client drew it. The player sees the second, not the first
    corrections = [event for event in actor_window if event.ev in {"move_recv", "pos_recv", "teleport_recv"}]
    snaps = []

    for event in corrections:
        if event.ev == "pos_recv" and event.int_field("jump") > 0:
            snaps.append(event.int_field("jump"))
        elif event.ev == "move_recv":
            start = event.hex_field("start")
            prev = event.hex_field("prev_hex")

            if start is not None and prev is not None and start != prev:
                snaps.append(distance(start, prev))

    actor_corrections_px = [float(event.int_field("err_px")) for event in corrections if event.ev == "pos_recv" and "err_px" in event.fields]
    # A project-written input line has no cr field, only the viewer
    input_changes = [event for event in window if event.side == "cl" and event.ev == "input" and event.int_field("viewer") == actor]

    actor_steps = step_sequence(events, "cl", actor, clock, begin, end, viewer=actor)
    server_steps = step_sequence(events, "srv", actor, clock, begin, end)

    server_final = srv_line.at(end)
    actor_final = actor_line.at(end)

    observer_reports: dict[str, Any] = {}
    finals = [server_final, actor_final]
    speed_changes = [event for event in srv_window if event.ev == "speed_change"]

    for observer in observers:
        observer_line = client_timeline(events, observer, actor, clock)
        observer_window = [event for event in window if event.side == "cl" and event.int_field("viewer") == observer and event.int_field("cr") == actor]
        observer_steps = step_sequence(events, "cl", actor, clock, begin, end, viewer=observer)
        observer_final = observer_line.at(end)
        observer_snaps = [event.int_field("jump") for event in observer_window if event.ev == "pos_recv" and event.int_field("jump") > 0]
        observer_corrections_px = [float(event.int_field("err_px")) for event in observer_window if event.ev == "pos_recv" and "err_px" in event.fields]
        observer_speed_recvs = [event for event in observer_window if event.ev == "speed_recv"]

        observer_reports[str(observer)] = {
            "finalHex": hex_text(observer_final),
            "steps": len(observer_steps),
            "planUpdates": sum(1 for event in observer_window if event.ev == "move_recv"),
            "positionUpdates": sum(1 for event in observer_window if event.ev == "pos_recv"),
            "teleports": sum(1 for event in observer_window if event.ev == "teleport_recv"),
            "snaps": len(observer_snaps),
            "snapMaxJump": max(observer_snaps, default=0),
            # A hex jump overstates a correction that a sub-hex offset moves into the next hex invisibly, so the
            # drawn distance in pixels is reported beside it
            "correctionPx": summarize(observer_corrections_px),
            "behindServerMs": summarize(align_lag_ms(server_steps, observer_steps)),
            "maxDivergenceFromServer": max_divergence(srv_line, observer_line, begin, end, distance),
            "divergence": divergence_profile(observer_line, srv_line, begin, end, distance),
            "reversals": step_reversals(observer_steps),
            "leaseHolds": lease_holds(observer_window, clock, end),
            "frameHealth": frame_health(window, observer),
            "frameStallMaxMs": round(max(frame_stalls_ms(events, actor, clock, begin, end, observer, distance), default=0.0), 1),
            "inViewAtEnd": observer_final is not None,
            "staleStepsAfterTeleport": stale_steps_after_teleport(events, actor, clock, begin, end, observer),
            "viewChanges": sum(1 for event in observer_window if event.ev in {"in", "out"}),
            "speedChanges": len(observer_speed_recvs),
            "speedChangeDelayMs": summarize(speed_change_delays_ms(speed_changes, observer_speed_recvs, clock)),
        }
        finals.append(observer_final)

    actor_speed_recvs = [event for event in actor_window if event.ev == "speed_recv"]

    # The server and the actor must know the position; an observer whose view the actor left states nothing
    # and takes no part in convergence
    required = finals[:2]
    known_finals = [value for value in finals if value is not None]
    converged = all(value is not None for value in required) and len(set(known_finals)) == 1

    actor_divergence = divergence_profile(actor_line, srv_line, begin, end, distance)
    last_apart = [report["divergence"]["lastApartMs"] for report in observer_reports.values()]
    last_apart.append(actor_divergence["lastApartMs"])
    known_apart = [value for value in last_apart if value is not None]

    return {
        "label": scenario.label,
        "actorId": actor,
        "observers": observers,
        "durationMs": round((scenario.end - scenario.begin) / 1000.0, 1),
        "converged": converged,
        # When every view last disagreed with the server, counted from the scenario start
        "settledAtMs": max(known_apart) if known_apart else 0,
        "server": {
            "finalHex": hex_text(server_final),
            "steps": len(server_steps),
            "moveRequests": len(move_reqs),
            "bridges": summarize([float(value) for value in bridges]),
            "bridgedRequests": sum(1 for value in bridges if value > 0),
            "truncatedPaths": sum(1 for event in move_reqs if event.int_field("truncated") == 1),
            "finishOutcomes": count_by(finish_events, "outcome"),
            "finishRemainingMs": summarize([float(event.int_field("remaining_ms", -1)) for event in finish_events if event.int_field("remaining_ms", -1) >= 0]),
            "stopReconciled": sum(1 for event in stopmove_events if event.int_field("reconciled") == 1),
            "stopReconcileFailed": sum(1 for event in stopmove_events if event.int_field("reconciled") == 0),
            # Hexes the server walked past the player's stop while the stop was in flight: a plan it played
            # without the player and now walks back, which observers see as a run forward and back
            "stopOvershootHexes": summarize(stop_overshoot_hexes(events, actor, clock, stopmove_events)),
            "stopReasons": count_by([event for event in srv_window if event.ev == "stop"], "reason"),
            "correctionsSentToActor": sum(1 for event in srv_window if event.ev == "send" and event.int_field("own") == 1),
            "speedChanges": len(speed_changes),
            # Progress a speed change discarded: the plan is rebased from its last stored progress, not from the
            # live time, so the difference is a server position rollback
            "speedChangeDiscardedMs": summarize([float(event.int_field("runtime_ms") - event.int_field("elapsed_ms")) for event in speed_changes]),
            "behindActorMs": summarize(align_lag_ms(actor_steps, server_steps)),
            # Time from the client's order to its processing: at equal network lag, growth here is the server
            # worker queue, during which the server keeps playing an already abandoned plan
            "uplinkMs": summarize(uplink_delays_ms([event for event in actor_window if event.ev == "move_send"], move_reqs, clock)),
            "stallMaxMs": round(max(server_stalls_ms(events, actor, clock, begin, end), default=0.0), 1),
            "positionJumps": summarize([float(value) for value in server_position_jumps(srv_line, begin, end, distance)]),
            "reversals": step_reversals(server_steps),
        },
        "actor": {
            "finalHex": hex_text(actor_final),
            "steps": len(actor_steps),
            "moveSends": sum(1 for event in actor_window if event.ev == "move_send"),
            "stopSends": sum(1 for event in actor_window if event.ev == "stop_send"),
            "finishSends": sum(1 for event in actor_window if event.ev == "finish_send"),
            "speedChanges": len(actor_speed_recvs),
            "speedChangeDelayMs": summarize(speed_change_delays_ms(speed_changes, actor_speed_recvs, clock)),
            "corrections": len(corrections),
            "snaps": len(snaps),
            "snapMaxJump": max(snaps, default=0),
            "correctionPx": summarize(actor_corrections_px),
            "inputChanges": len(input_changes),
            "inputResponseMs": summarize(input_response_ms(input_changes, [event for event in actor_window if event.ev in {"move_send", "stop_send"}], clock)),
            # Held-direction plans the client built itself, by reason: start, turn, extend, resume
            "dirPlans": count_by([event for event in actor_window if event.ev == "dir_plan"], "reason"),
            "maxDivergenceFromServer": max_divergence(actor_line, srv_line, begin, end, distance),
            "divergence": actor_divergence,
            "reversals": step_reversals(actor_steps),
            "frameHealth": frame_health(window, actor),
            "frameStallMaxMs": round(max(frame_stalls_ms(events, actor, clock, begin, end, actor, distance), default=0.0), 1),
            "staleStepsAfterTeleport": stale_steps_after_teleport(events, actor, clock, begin, end, actor),
            "stopOverriddenHexes": stop_overridden_hexes(actor_window, srv_line.at(end), distance),
        },
        "observerViews": observer_reports,
    }


def analyze(events: list[Event], clock: str = "t", settle_us: int = 0, geometry: str = "hexagonal",
            scenarios: list[Scenario] | None = None, extensions: Mapping[str, ScenarioExtension] | None = None) -> dict[str, Any]:
    """Analyse every scenario; an extension receives each scenario window and report and returns extra data."""
    distance = distance_for(geometry)
    ordered = sorted_events(events, clock)
    times = [event.time(clock) for event in ordered]
    selected = find_scenarios(ordered, clock) if scenarios is None else scenarios
    reports = []

    # Each scenario reads its own event window, not the whole run: one log may hold hundreds of scenarios
    for scenario in selected:
        low = bisect.bisect_left(times, scenario.begin - HISTORY_US)
        high = bisect.bisect_right(times, scenario.end + settle_us)
        window = ordered[low:high]
        report = analyze_scenario(window, scenario, clock, settle_us, distance)

        if extensions:
            scenario_window = ScenarioWindow(scenario, window, clock, settle_us)
            report["extensions"] = {name: extension(scenario_window, report) for name, extension in sorted(extensions.items())}

        reports.append(report)

    return {
        "clock": clock,
        "geometry": geometry,
        "events": len(ordered),
        "scenarios": reports,
        "summary": {
            "scenarios": len(reports),
            "converged": sum(1 for report in reports if report["converged"]),
            "actorSnaps": sum(report["actor"]["snaps"] for report in reports),
        },
    }


def viewpoint(event: Event, actor: int) -> str:
    if event.side == "srv":
        return "server"
    if event.side != "cl":
        return event.side

    viewer = event.int_field("viewer")
    return "actor" if viewer == actor else f"obs:{viewer}"


def render_timeline(events: list[Event], scenario: Scenario, clock: str, settle_us: int = 0) -> list[str]:
    """Merged scenario timeline: one row per event, time from the scenario start, view and fields."""
    lines = [f"== {scenario.label} (actor {scenario.actor}) =="]
    end = scenario.end + settle_us

    for event in sorted_events(events, clock):
        time = event.time(clock)

        if time < scenario.begin or time > end:
            continue
        if event.side == "srv" and event.int_field("cr") not in (scenario.actor, 0) and event.ev != "send":
            continue
        if event.side == "srv" and event.ev == "send" and event.int_field("cr") != scenario.actor:
            continue
        if event.side == "cl" and event.ev != "mark" and event.int_field("cr") != scenario.actor:
            continue

        details = " ".join(f"{key}={value}" for key, value in event.fields.items() if key not in {"viewer", "cr"})
        lines.append(f"{(time - scenario.begin) / 1000.0:+10.1f} ms  {viewpoint(event, scenario.actor):<10} {event.ev:<14} {details}".rstrip())

    return lines


def render_markdown(report: dict[str, Any]) -> list[str]:
    lines = ["# Movement sync analysis", "", f"Clock: `{report['clock']}`, geometry: `{report['geometry']}`, events: {report['events']}", ""]
    lines.append("| Scenario | Converged | Actor snaps | Server behind actor, ms (med/max) | Observer behind server, ms (med/max) | Arrivals | Bridges max |")
    lines.append("|---|---|---|---|---|---|---|")

    for scenario in report["scenarios"]:
        server = scenario["server"]
        behind = server["behindActorMs"]
        observer_lags = [view["behindServerMs"] for view in scenario["observerViews"].values()]
        observer_text = ", ".join(f"{lag.get('median', '-')}/{lag.get('max', '-')}" for lag in observer_lags) or "-"
        arrivals = ", ".join(f"{key}:{value}" for key, value in sorted(server["finishOutcomes"].items())) or "-"
        lines.append(
            f"| {scenario['label']} | {'yes' if scenario['converged'] else '**NO**'} | {scenario['actor']['snaps']} | "
            f"{behind.get('median', '-')}/{behind.get('max', '-')} | {observer_text} | {arrivals} | {server['bridges'].get('max', '-')} |"
        )

    return lines


def create_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(prog="move_sync_analysis.py", description="Analyse MOVESYNC movement synchronization traces written with Network.MoveSyncTrace.")
    parser.add_argument("logs", nargs="+", type=Path, help="log files holding MOVESYNC lines (server and clients)")
    parser.add_argument("--geometry", choices=GEOMETRIES, required=True, help="world grid geometry of the project (FO_GEOMETRY)")
    parser.add_argument("--clock", choices=("t", "st"), default="t", help="t: shared monotonic clock (one machine); st: synchronized time (several machines)")
    parser.add_argument("--actor", type=int, help="analyse the whole trace as one scenario of this critter id instead of mark-delimited scenarios")
    parser.add_argument("--settle-ms", type=int, default=0, help="extend each scenario window by this many milliseconds after its end mark")
    parser.add_argument("--extra-side", action="append", default=[], help="also read MOVESYNC lines of this project-written side into timelines (repeatable)")
    parser.add_argument("--timeline", action="append", default=[], help="print the merged timeline of a scenario label (repeatable, or 'all')")
    parser.add_argument("--json", type=Path, help="write the full report as JSON")
    parser.add_argument("--markdown", type=Path, help="write the Markdown summary table")
    parser.add_argument("--require-converged", action="store_true", help="exit 1 when any scenario ends with the views disagreeing")
    return parser


def main(argv: list[str] | None = None) -> int:
    for stream in (sys.stdout, sys.stderr):
        reconfigure = getattr(stream, "reconfigure", None)

        if reconfigure is not None:
            reconfigure(encoding="utf-8", errors="replace")

    args = create_parser().parse_args(argv)
    sides = (*ENGINE_SIDES, *(side for side in args.extra_side if side not in ENGINE_SIDES))

    try:
        events = parse_logs(args.logs, sides)
    except OSError as exception:
        print(f"cannot read log: {exception}", file=sys.stderr)
        return 2

    if not events:
        print("no MOVESYNC lines found; was Network.MoveSyncTrace enabled?", file=sys.stderr)
        return 2

    ordered = sorted_events(events, args.clock)
    scenarios = [whole_trace_scenario(ordered, args.actor, args.clock)] if args.actor is not None else find_scenarios(ordered, args.clock)

    if not scenarios:
        print("no scenario marks (ev=mark label=begin:<name> / end:<name>) found; pass --actor to analyse the whole trace", file=sys.stderr)
        return 2

    settle_us = max(args.settle_ms, 0) * 1000
    report = analyze(events, args.clock, settle_us, args.geometry, scenarios)

    for scenario in scenarios:
        if "all" in args.timeline or scenario.label in args.timeline:
            print("\n".join(render_timeline(ordered, scenario, args.clock, settle_us)))
            print()

    markdown = render_markdown(report)
    print("\n".join(markdown))

    if args.json:
        args.json.parent.mkdir(parents=True, exist_ok=True)
        args.json.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    if args.markdown:
        args.markdown.parent.mkdir(parents=True, exist_ok=True)
        args.markdown.write_text("\n".join(markdown) + "\n", encoding="utf-8")

    if args.require_converged and report["summary"]["converged"] != report["summary"]["scenarios"]:
        failed = ", ".join(scenario["label"] for scenario in report["scenarios"] if not scenario["converged"])
        print(f"views did not converge in: {failed}", file=sys.stderr)
        return 1

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
