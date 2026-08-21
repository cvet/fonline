from __future__ import annotations

import argparse
import copy
import hashlib
import json
import re
import sys
from collections import Counter
from pathlib import Path, PurePosixPath
from urllib.parse import quote

import docs_cli
import docs_description_translations
import docs_localization


SCHEMA_VERSION = 1
DEFAULT_MANIFEST = "BuildTools/VideoInterface.json"
DEFAULT_MODEL = "Docs/generated/video.json"
DEFAULT_OUTPUT_DIR = "Docs/en/reference/video"
RUSSIAN_OUTPUT_DIR = "Docs/ru/reference/video"
LEGACY_OUTPUT_DIR = "Docs/generated/video"
GENERATED_BY = "BuildTools/docs_video.py"
REPOSITORY = "cvet/fonline"
SOURCE_REF = "master"
PAGE_DEFINITIONS = (
    ("index.md", "generated-video-index", "Generated Video Reference"),
    ("formats.md", "generated-video-formats", "Video Resource Formats"),
    ("delivery.md", "generated-video-delivery", "Video Resource Delivery"),
    ("decoding.md", "generated-video-decoding", "Video Decoding Contract"),
    ("fullscreen.md", "generated-video-fullscreen", "Fullscreen Video Contract"),
    ("embedded.md", "generated-video-embedded", "Embedded Video Contract"),
    ("validation.md", "generated-video-validation", "Video Validation Contract"),
)
CANONICAL_OUTPUT_PATHS = tuple(
    f"{DEFAULT_OUTPUT_DIR}/{filename}" for filename, _, _ in PAGE_DEFINITIONS
)
RUSSIAN_OUTPUT_PATHS = tuple(
    f"{RUSSIAN_OUTPUT_DIR}/{filename}" for filename, _, _ in PAGE_DEFINITIONS
)
LEGACY_OUTPUT_PATHS = tuple(
    f"{LEGACY_OUTPUT_DIR}/{filename}" for filename, _, _ in PAGE_DEFINITIONS
)
OUTPUT_PATHS = CANONICAL_OUTPUT_PATHS + RUSSIAN_OUTPUT_PATHS + LEGACY_OUTPUT_PATHS
COLLECTION_KINDS = {
    "formats": "format",
    "delivery_rules": "delivery",
    "decoding_rules": "decoding",
    "fullscreen_rules": "fullscreen",
    "embedded_rules": "embedded",
    "validation_rules": "validation",
}
ENTRY_ID_PATTERN = re.compile(
    r"^video\.(format|delivery|decoding|fullscreen|embedded|validation)\."
    r"[A-Za-z0-9][A-Za-z0-9.-]*$"
)
VALID_STABILITY = {"stable", "experimental", "deprecated", "internal"}


def _required_string(value: object, label: str) -> str:
    if not isinstance(value, str) or not value.strip():
        raise ValueError(f"{label} must be a non-empty string")
    return value


def _string_list(
    value: object, label: str, *, allow_empty: bool = False
) -> list[str]:
    if not isinstance(value, list) or (not value and not allow_empty):
        qualifier = "an" if allow_empty else "a non-empty"
        raise ValueError(f"{label} must be {qualifier} array of strings")
    if any(not isinstance(item, str) or not item.strip() for item in value):
        raise ValueError(f"{label} must contain only non-empty strings")
    if len(value) != len(set(value)):
        raise ValueError(f"{label} must not contain duplicates")
    return list(value)


def _relative_path(value: object, label: str) -> tuple[str, PurePosixPath]:
    source = _required_string(value, label)
    relative = PurePosixPath(source)
    if "\\" in source or relative.is_absolute() or ".." in relative.parts:
        raise ValueError(f"{label} must be a repository-relative forward-slash path")
    return source, relative


def _source_path(root: Path, value: object, label: str) -> str:
    source, relative = _relative_path(value, label)
    if not root.joinpath(*relative.parts).is_file():
        raise ValueError(f"{label} does not exist: {source}")
    return source


def _source_directory(root: Path, value: object, label: str) -> str:
    source, relative = _relative_path(value, label)
    if not root.joinpath(*relative.parts).is_dir():
        raise ValueError(f"{label} does not exist: {source}")
    return source


def _source_refs(
    root: Path, value: object, label: str
) -> list[dict[str, object]]:
    if not isinstance(value, list) or not value:
        raise ValueError(f"{label} must be a non-empty array")
    refs: list[dict[str, object]] = []
    for index, raw in enumerate(value):
        item_label = f"{label}[{index}]"
        if not isinstance(raw, dict):
            raise ValueError(f"{item_label} must be an object")
        path = _source_path(root, raw.get("path"), f"{item_label}.path")
        anchors = _string_list(raw.get("anchors"), f"{item_label}.anchors")
        source_text = (root / path).read_text(encoding="utf-8", errors="replace")
        for anchor in anchors:
            if anchor not in source_text:
                raise ValueError(
                    f"{item_label} anchor is missing from {path}: {anchor}"
                )
        refs.append({"path": path, "anchors": anchors})
    return refs


def _validate_scope(raw: object) -> dict[str, object]:
    if not isinstance(raw, dict) or raw.get("surface") != "video":
        raise ValueError("scope.surface must be video")
    stability = _required_string(raw.get("stability"), "scope.stability")
    if stability not in VALID_STABILITY:
        raise ValueError(f"unsupported scope.stability: {stability}")
    since = raw.get("since")
    if since is not None and (not isinstance(since, str) or not since.strip()):
        raise ValueError("scope.since must be null or a non-empty string")
    _required_string(raw.get("support_note"), "scope.support_note")
    _string_list(raw.get("included"), "scope.included")
    _string_list(raw.get("excluded"), "scope.excluded")
    return copy.deepcopy(raw)


def _validate_sources(root: Path, raw: object) -> dict[str, object]:
    if not isinstance(raw, dict):
        raise ValueError("sources must be an object")
    file_fields = (
        "video_clip",
        "video_clip_header",
        "client",
        "client_header",
        "client_global_scripts",
        "sprite_manager",
        "settings",
        "raw_copy_baker",
        "third_party",
    )
    result: dict[str, object] = {
        field: _source_path(root, raw.get(field), f"sources.{field}")
        for field in file_fields
    }
    result["native_test_directory"] = _source_directory(
        root, raw.get("native_test_directory"), "sources.native_test_directory"
    )
    return result


def _raw_copy_extensions(settings_text: str) -> list[str]:
    match = re.search(
        r"FIXED_SETTING\(vector<string>,\s*Baking,\s*RawCopyFileExtensions,"
        r"(?P<values>.*?)\);",
        settings_text,
        re.DOTALL,
    )
    if match is None:
        raise ValueError("unable to derive Baking.RawCopyFileExtensions")
    return re.findall(r'"([A-Za-z0-9_]+)"', match.group("values"))


def _require_markers(text: str, label: str, markers: tuple[str, ...]) -> None:
    missing = [marker for marker in markers if marker not in text]
    if missing:
        raise ValueError(f"unable to derive {label}; missing: {', '.join(missing)}")


def _derive_outputs(root: Path, sources: dict[str, object]) -> dict[str, object]:
    video = (root / str(sources["video_clip"])).read_text(encoding="utf-8")
    client = (root / str(sources["client"])).read_text(encoding="utf-8")
    scripts = (root / str(sources["client_global_scripts"])).read_text(
        encoding="utf-8"
    )
    sprite = (root / str(sources["sprite_manager"])).read_text(encoding="utf-8")
    settings = (root / str(sources["settings"])).read_text(encoding="utf-8")
    third_party = (root / str(sources["third_party"])).read_text(encoding="utf-8")

    _require_markers(
        video,
        "video decoder",
        (
            '#include "theora/theoradec.h"',
            "ogg_sync_pageout",
            "th_decode_headerin",
            "th_decode_packetin",
            "vector<uint8_t> RawVideoData",
            "RawVideoData = std::move(video_data)",
            "vector<ucolor> RenderedTextureData",
            "TH_PF_420",
            "TH_PF_422",
            "TH_PF_444",
            "pixel.comp.a = 0xFF",
        ),
    )
    _require_markers(
        client,
        "fullscreen playback",
        (
            "strex(video_name).split('|')",
            "Resources.ReadFile(names.front())",
            "SndMngr.PlayMusic(names[1]",
            "SprMngr.DrawTexture(_video->Tex, false)",
            "OnRenderIface.Fire()",
            "ProcessVideo()",
        ),
    )
    _require_markers(
        scripts,
        "embedded playback",
        (
            "Client_Game_CreateVideoPlayback",
            "VideoClip clip {file.GetData()}",
            "Client_Game_DrawVideoPlayback",
            "only in RenderIface event",
        ),
    )
    _require_markers(
        sprite,
        "fullscreen target drawing",
        ("if (!region_from && !region_to)", "width_to_i", "height_to_i"),
    )
    _require_markers(
        third_party,
        "Theora client dependency",
        ("# Theora", "AddStaticThirdPartyLibrary(Theora", "APPEND_TO FO_CLIENT_LIBS"),
    )

    read_chunk = re.search(r"read_bytes = std::min\((\d+), read_bytes\)", video)
    stream_count = re.search(r"static constexpr size_t COUNT = (\d+)", video)
    if read_chunk is None or stream_count is None:
        raise ValueError("unable to derive Ogg read chunk or logical stream limit")

    raw_copy_extensions = _raw_copy_extensions(settings)
    if "ogv" not in raw_copy_extensions:
        raise ValueError("ogv is missing from Baking.RawCopyFileExtensions")

    test_root = root / str(sources["native_test_directory"])
    native_test_files = sorted(
        path.relative_to(root).as_posix()
        for path in test_root.glob("Test_*.cpp")
        if re.search(r"(video|theora)", path.name, re.IGNORECASE)
    )

    return {
        "resource_extension": "ogv",
        "container": "Ogg",
        "codec": "Theora",
        "read_chunk_bytes": int(read_chunk.group(1)),
        "max_logical_streams": int(stream_count.group(1)),
        "pixel_formats": ["TH_PF_420", "TH_PF_422", "TH_PF_444"],
        "output_channels": 4,
        "output_alpha": int("FF", 16),
        "whole_resource_buffered": True,
        "container_audio_decoded": "vorbis" in video.lower(),
        "fullscreen_path_separator": "|",
        "fullscreen_stretches_to_target": True,
        "script_draw_event": "RenderIface",
        "runtime_side": "client",
        "native_test_files": native_test_files,
    }


def _validate_entries(
    root: Path, collection: str, raw: object, identities: set[str]
) -> list[dict[str, object]]:
    kind = COLLECTION_KINDS[collection]
    if not isinstance(raw, list) or not raw:
        raise ValueError(f"{collection} must be a non-empty array")
    result: list[dict[str, object]] = []
    for index, entry in enumerate(raw):
        label = f"{collection}[{index}]"
        if not isinstance(entry, dict):
            raise ValueError(f"{label} must be an object")
        identity = _required_string(entry.get("id"), f"{label}.id")
        if (
            ENTRY_ID_PATTERN.fullmatch(identity) is None
            or not identity.startswith(f"video.{kind}.")
        ):
            raise ValueError(f"invalid {label}.id: {identity}")
        if identity in identities:
            raise ValueError(f"duplicate video entry id: {identity}")
        identities.add(identity)
        _required_string(entry.get("name"), f"{label}.name")
        stability = _required_string(entry.get("stability"), f"{label}.stability")
        if stability not in VALID_STABILITY:
            raise ValueError(f"unsupported {label}.stability: {stability}")
        _required_string(entry.get("requirement"), f"{label}.requirement")
        _required_string(entry.get("rationale"), f"{label}.rationale")
        if "extension" in entry:
            _required_string(entry.get("extension"), f"{label}.extension")
        enriched = copy.deepcopy(entry)
        enriched["source"] = _source_refs(
            root, entry.get("source"), f"{label}.source"
        )
        result.append(enriched)
    return result


def _validate_outputs(raw: object, expected: dict[str, object]) -> dict[str, object]:
    if not isinstance(raw, dict):
        raise ValueError("outputs must be an object")
    result = copy.deepcopy(raw)
    for field, value in expected.items():
        if result.get(field) != value:
            raise ValueError(f"outputs.{field} must match the live source: {value}")
    return result


def generate_video_model(
    root: Path, manifest_relative_path: str = DEFAULT_MANIFEST
) -> dict[str, object]:
    manifest_path = root / manifest_relative_path
    try:
        raw = json.loads(manifest_path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exception:
        raise ValueError(
            f"unable to read video manifest {manifest_relative_path}: {exception}"
        ) from exception
    if not isinstance(raw, dict) or raw.get("schema_version") != SCHEMA_VERSION:
        raise ValueError(f"video manifest schema_version must be {SCHEMA_VERSION}")

    description = _required_string(raw.get("description"), "description")
    scope = _validate_scope(raw.get("scope"))
    sources = _validate_sources(root, raw.get("sources"))
    outputs = _validate_outputs(raw.get("outputs"), _derive_outputs(root, sources))
    identities: set[str] = set()
    collections = {
        collection: _validate_entries(
            root, collection, raw.get(collection), identities
        )
        for collection in COLLECTION_KINDS
    }
    stability_counts = Counter(
        str(entry["stability"])
        for entries in collections.values()
        for entry in entries
    )
    collection_counts = {
        f"{collection.removesuffix('s')}_count": len(entries)
        for collection, entries in collections.items()
    }
    model: dict[str, object] = {
        "schema_version": SCHEMA_VERSION,
        "generated_by": GENERATED_BY,
        "source_manifest": manifest_relative_path,
        "repository": REPOSITORY,
        "source_ref": SOURCE_REF,
        "description": description,
        "scope": scope,
        "sources": sources,
        "outputs": outputs,
        **collections,
        "summary": {
            "entry_count": sum(len(entries) for entries in collections.values()),
            **collection_counts,
            "entries_by_stability": dict(sorted(stability_counts.items())),
        },
    }
    canonical = json.dumps(
        model, ensure_ascii=False, sort_keys=True, separators=(",", ":")
    )
    model["contract_digest"] = hashlib.sha256(
        canonical.encode("utf-8")
    ).hexdigest()
    return model


def render_video_model(
    root: Path, manifest_relative_path: str = DEFAULT_MANIFEST
) -> str:
    return (
        json.dumps(
            generate_video_model(root, manifest_relative_path),
            ensure_ascii=False,
            indent=2,
        )
        + "\n"
    )


def _source_link(model: dict[str, object], source: str) -> str:
    url = (
        f"https://github.com/{model['repository']}/blob/"
        f"{model['source_ref']}/{quote(source)}"
    )
    return f"[{source}]({url})"


def _source_ref_links(
    model: dict[str, object], refs: list[dict[str, object]]
) -> str:
    return ", ".join(
        _source_link(model, str(ref["path"]))
        for ref in refs
        if isinstance(ref, dict)
    )


def _header(document_id: str, title: str) -> list[str]:
    return [
        "---",
        f"title: {title}",
        f"document_id: {document_id}",
        "locale: en",
        "generated: true",
        "---",
        "",
        f"# {title}",
        "",
        "> Generated reference. Do not edit directly. Update "
        "`BuildTools/VideoInterface.json`, then run "
        "`python BuildTools/docs_video.py --write`.",
        "",
        "[Index](index.md) | [Formats](formats.md) | [Delivery](delivery.md) | "
        "[Decoding](decoding.md) | [Fullscreen](fullscreen.md) | "
        "[Embedded](embedded.md) | [Validation](validation.md) | "
        "[Canonical JSON](../../../generated/video.json) | "
        "[Guide](../../how-to/content/video.md)",
        "",
    ]


def _entry_anchor(entry: dict[str, object]) -> str:
    return (
        f'<a id="{docs_cli._anchor("entry", str(entry["id"]))}"></a>'
        f'<code>{docs_cli._text(entry["id"])}</code>'
    )


def _render_legacy_page(
    canonical_path: str,
    title: str,
    canonical_content: str,
) -> str:
    filename = PurePosixPath(canonical_path).name
    english_path = f"../../en/reference/video/{filename}"
    russian_path = f"../../ru/reference/video/{filename}"
    lines = [
        f"# {title}",
        "",
        "> Legacy route.",
        "",
        "The canonical generated reference moved to locale-specific paths.",
        "",
        f"[English]({english_path}) | [Russian]({russian_path})",
        "",
    ]
    for line in canonical_content.splitlines():
        heading = re.fullmatch(r"(#{2,3}) (.+)", line)
        if heading:
            lines.extend(
                [
                    f"{heading.group(1)} {heading.group(2)}",
                    "",
                    f"Continue with the [canonical reference]({english_path}).",
                    "",
                ]
            )
        for anchor in re.findall(r'<a id="([^"]+)"></a>', line):
            lines.extend(
                [
                    f'<a id="{anchor}"></a>',
                    f"- [`{anchor}`]({english_path}#{anchor})",
                    "",
                ]
            )
    return "\n".join(lines).rstrip() + "\n"


def _entry_rows(
    model: dict[str, object], collection: str
) -> list[tuple[str, str, str, str, str]]:
    return [
        (
            _entry_anchor(entry),
            docs_cli._text(entry["name"]),
            docs_cli._text(entry["requirement"]),
            docs_cli._text(entry["rationale"]),
            _source_ref_links(model, entry["source"]),
        )
        for entry in model[collection]
        if isinstance(entry, dict)
    ]


def _render_index(model: dict[str, object]) -> str:
    lines = _header(*PAGE_DEFINITIONS[0][1:])
    scope = model["scope"]
    outputs = model["outputs"]
    summary = model["summary"]
    assert isinstance(scope, dict)
    assert isinstance(outputs, dict)
    assert isinstance(summary, dict)
    lines.extend(
        [
            "This reference describes the revision-pinned Engine video primitive. "
            "It is experimental: a game cinematic system, subtitles, policy, "
            "asset ownership, and acceptance evidence remain project concerns.",
            "",
            "## Contract status",
            "",
        ]
    )
    docs_cli._table(
        lines,
        ("Field", "Value"),
        [
            ("Stability", docs_cli._code(scope["stability"])),
            ("Support policy", docs_cli._text(scope["support_note"])),
            ("Source manifest", _source_link(model, str(model["source_manifest"]))),
            ("Contract digest", docs_cli._code(model["contract_digest"])),
            (
                "Resource",
                docs_cli._code(
                    f".{outputs['resource_extension']} / "
                    f"{outputs['container']} / {outputs['codec']}"
                ),
            ),
            (
                "Whole resource buffered",
                docs_cli._code(outputs["whole_resource_buffered"]),
            ),
            (
                "Container audio decoded",
                docs_cli._code(outputs["container_audio_decoded"]),
            ),
            ("Runtime side", docs_cli._code(outputs["runtime_side"])),
            ("Focused native video tests", str(len(outputs["native_test_files"]))),
        ],
    )
    docs_cli._table(
        lines,
        ("Reference", "Entries", "Purpose"),
        [
            (
                "[Formats](formats.md)",
                str(summary["format_count"]),
                "Container, codec, and pixel-format requirements.",
            ),
            (
                "[Delivery](delivery.md)",
                str(summary["delivery_rule_count"]),
                "Raw-copy, exact paths, runtime ownership, and memory.",
            ),
            (
                "[Decoding](decoding.md)",
                str(summary["decoding_rule_count"]),
                "Ogg/Theora decode, frame clock, color, and failure rules.",
            ),
            (
                "[Fullscreen](fullscreen.md)",
                str(summary["fullscreen_rule_count"]),
                "Replacement, queue, input, music, drawing, and status.",
            ),
            (
                "[Embedded](embedded.md)",
                str(summary["embedded_rule_count"]),
                "Script-owned playback and RenderIface drawing.",
            ),
            (
                "[Validation](validation.md)",
                str(summary["validation_rule_count"]),
                "Visible acceptance gates and known coverage gaps.",
            ),
        ],
    )
    lines.extend(["## Boundary", "", "Included:", ""])
    lines.extend(f"- {item}" for item in scope["included"])
    lines.extend(["", "Excluded:", ""])
    lines.extend(f"- {item}" for item in scope["excluded"])
    lines.append("")
    return "\n".join(lines)


def _render_rules(
    model: dict[str, object], page_index: int, collection: str
) -> str:
    lines = _header(*PAGE_DEFINITIONS[page_index][1:])
    docs_cli._table(
        lines,
        ("Stable ID", "Rule", "Requirement", "Why", "Source"),
        _entry_rows(model, collection),
    )
    return "\n".join(lines)


def _render_validation(model: dict[str, object]) -> str:
    lines = _header(*PAGE_DEFINITIONS[6][1:])
    docs_cli._table(
        lines,
        ("Stable ID", "Rule", "Requirement", "Why", "Source"),
        _entry_rows(model, "validation_rules"),
    )
    lines.extend(
        [
            "## Validation commands",
            "",
            "```powershell",
            "python BuildTools\\docs_video.py --check",
            "python -m unittest BuildTools.tests.test_docs_video",
            "cmake --build <build-dir> --config RelWithDebInfo --target RunUnitTests",
            "```",
            "",
            "There is no focused native video decoder/playback fixture. A visible "
            "client must prove first frame, sustained motion, completion, skip, "
            "queue transitions, resizing, paired music behavior, cleanup, and "
            "multi-cycle looping for every exact asset and claimed platform.",
            "",
        ]
    )
    return "\n".join(lines)


RUSSIAN_TITLES = {
    "Generated Video Reference": "Сгенерированный справочник video",
    "Video Resource Formats": "Форматы видеоресурсов",
    "Video Resource Delivery": "Доставка видеоресурсов",
    "Video Decoding Contract": "Контракт декодирования видео",
    "Fullscreen Video Contract": "Контракт полноэкранного видео",
    "Embedded Video Contract": "Контракт встроенного видео",
    "Video Validation Contract": "Контракт проверки видео",
}

RUSSIAN_REPLACEMENTS = {
    "> Generated reference. Do not edit directly. Update `BuildTools/VideoInterface.json`, then run `python BuildTools/docs_video.py --write`.":
        "> Сгенерированный справочник. Не редактируйте его источник напрямую. Обновите `BuildTools/VideoInterface.json`, затем выполните `python BuildTools/docs_video.py --write`.",
    "[Index](index.md) | [Formats](formats.md) | [Delivery](delivery.md) | [Decoding](decoding.md) | [Fullscreen](fullscreen.md) | [Embedded](embedded.md) | [Validation](validation.md) | [Canonical JSON](../../../generated/video.json) | [Guide](../../how-to/content/video.md)":
        "[Индекс](index.md) | [Форматы](formats.md) | [Доставка](delivery.md) | [Декодирование](decoding.md) | [Полный экран](fullscreen.md) | [Встроенное](embedded.md) | [Проверка](validation.md) | [Канонический JSON](../../../generated/video.json) | [Руководство](../../how-to/content/video.md)",
    "This reference describes the revision-pinned Engine video primitive. It is experimental: a game cinematic system, subtitles, policy, asset ownership, and acceptance evidence remain project concerns.":
        "Этот справочник описывает привязанный к ревизии видеопримитив Engine. Он имеет статус experimental: игровая система роликов, субтитры, политика, владение ресурсами и приёмочные доказательства остаются задачами проекта.",
    "## Contract status": "## Статус контракта",
    "Stability": "Стабильность",
    "Support policy": "Политика поддержки",
    "Source manifest": "Исходный манифест",
    "Contract digest": "Digest контракта",
    "Resource": "Ресурс",
    "Whole resource buffered": "Весь ресурс буферизуется",
    "Container audio decoded": "Аудио контейнера декодируется",
    "Runtime side": "Сторона выполнения",
    "Focused native video tests": "Сфокусированные нативные video-тесты",
    "[Formats](formats.md)": "[Форматы](formats.md)",
    "Container, codec, and pixel-format requirements.": "Требования к контейнеру, кодеку и пиксельным форматам.",
    "[Delivery](delivery.md)": "[Доставка](delivery.md)",
    "Raw-copy, exact paths, runtime ownership, and memory.": "Raw copy, точные пути, владение среды выполнения и память.",
    "[Decoding](decoding.md)": "[Декодирование](decoding.md)",
    "Ogg/Theora decode, frame clock, color, and failure rules.": "Декодирование Ogg/Theora, часы кадров, цвет и правила ошибок.",
    "[Fullscreen](fullscreen.md)": "[Полный экран](fullscreen.md)",
    "Replacement, queue, input, music, drawing, and status.": "Замена, очередь, ввод, музыка, рисование и состояние.",
    "[Embedded](embedded.md)": "[Встроенное](embedded.md)",
    "Script-owned playback and RenderIface drawing.": "Управляемое скриптом воспроизведение и рисование RenderIface.",
    "[Validation](validation.md)": "[Проверка](validation.md)",
    "Visible acceptance gates and known coverage gaps.": "Гейты визуальной приёмки и известные пробелы покрытия.",
    "## Boundary": "## Граница",
    "Included:": "Включено:",
    "Excluded:": "Исключено:",
    "## Validation commands": "## Команды проверки",
    "There is no focused native video decoder/playback fixture. A visible client must prove first frame, sustained motion, completion, skip, queue transitions, resizing, paired music behavior, cleanup, and multi-cycle looping for every exact asset and claimed platform.":
        "Сфокусированный нативный fixture декодера/воспроизведения видео отсутствует. Видимый клиент должен доказать первый кадр, устойчивое движение, завершение, пропуск, переходы очереди, изменение размера, поведение связанной музыки, очистку и многоцикловое зацикливание для каждого точного ресурса и заявленной платформы.",
}

RUSSIAN_TABLE_HEADERS = {
    "| Field | Value |": "| Поле | Значение |",
    "| Reference | Entries | Purpose |": "| Справочник | Записи | Назначение |",
    "| Stable ID | Rule | Requirement | Why | Source |":
        "| Стабильный ID | Правило | Требование | Причина | Источник |",
}


def _render_russian_page(
    document_id: str,
    canonical_path: str,
    english_content: str,
    russian_base_content: str,
) -> str:
    content = russian_base_content.replace("locale: en", "locale: ru", 1)
    for english, russian in sorted(
        {**RUSSIAN_TITLES, **RUSSIAN_REPLACEMENTS}.items(),
        key=lambda item: -len(item[0]),
    ):
        content = content.replace(english, russian)
    for english, russian in RUSSIAN_TABLE_HEADERS.items():
        content = content.replace(english, russian)
    front_matter_end = content.find("\n---\n", 4)
    if front_matter_end < 0:
        raise ValueError("generated video page has no front matter")
    insert_at = front_matter_end + len("\n---\n")
    marker = docs_localization.translation_metadata_line(
        document_id,
        canonical_path,
        docs_localization.normalized_sha256(english_content),
    )
    return content[:insert_at] + "\n" + marker + "\n" + content[insert_at:]


def generate_reference_pages(
    model: dict[str, object],
    russian_model: dict[str, object] | None = None,
) -> dict[str, str]:
    if (
        model.get("schema_version") != SCHEMA_VERSION
        or model.get("generated_by") != GENERATED_BY
    ):
        raise ValueError("unsupported generated video model")
    identities = [
        entry.get("id")
        for collection in COLLECTION_KINDS
        for entry in model.get(collection, [])
        if isinstance(entry, dict)
    ]
    if (
        any(not isinstance(identity, str) or not identity for identity in identities)
        or len(identities) != len(set(identities))
    ):
        raise ValueError("every video entry must have a unique non-empty ID")
    canonical_pages = {
        f"{DEFAULT_OUTPUT_DIR}/index.md": _render_index(model),
        f"{DEFAULT_OUTPUT_DIR}/formats.md": _render_rules(model, 1, "formats"),
        f"{DEFAULT_OUTPUT_DIR}/delivery.md": _render_rules(
            model, 2, "delivery_rules"
        ),
        f"{DEFAULT_OUTPUT_DIR}/decoding.md": _render_rules(
            model, 3, "decoding_rules"
        ),
        f"{DEFAULT_OUTPUT_DIR}/fullscreen.md": _render_rules(
            model, 4, "fullscreen_rules"
        ),
        f"{DEFAULT_OUTPUT_DIR}/embedded.md": _render_rules(
            model, 5, "embedded_rules"
        ),
        f"{DEFAULT_OUTPUT_DIR}/validation.md": _render_validation(model),
    }
    localized_model = model if russian_model is None else russian_model
    russian_base_pages = {
        f"{DEFAULT_OUTPUT_DIR}/index.md": _render_index(localized_model),
        f"{DEFAULT_OUTPUT_DIR}/formats.md": _render_rules(
            localized_model, 1, "formats"
        ),
        f"{DEFAULT_OUTPUT_DIR}/delivery.md": _render_rules(
            localized_model, 2, "delivery_rules"
        ),
        f"{DEFAULT_OUTPUT_DIR}/decoding.md": _render_rules(
            localized_model, 3, "decoding_rules"
        ),
        f"{DEFAULT_OUTPUT_DIR}/fullscreen.md": _render_rules(
            localized_model, 4, "fullscreen_rules"
        ),
        f"{DEFAULT_OUTPUT_DIR}/embedded.md": _render_rules(
            localized_model, 5, "embedded_rules"
        ),
        f"{DEFAULT_OUTPUT_DIR}/validation.md": _render_validation(localized_model),
    }
    pages = dict(canonical_pages)
    for (filename, document_id, title), canonical_path in zip(
        PAGE_DEFINITIONS, CANONICAL_OUTPUT_PATHS, strict=True
    ):
        pages[f"{RUSSIAN_OUTPUT_DIR}/{filename}"] = _render_russian_page(
            document_id,
            canonical_path,
            canonical_pages[canonical_path],
            russian_base_pages[canonical_path],
        )
        pages[f"{LEGACY_OUTPUT_DIR}/{filename}"] = _render_legacy_page(
            canonical_path,
            title,
            canonical_pages[canonical_path],
        )
    if set(pages) != set(OUTPUT_PATHS):
        raise ValueError("generated video page set does not match OUTPUT_PATHS")
    return {
        path: content.rstrip() + "\n"
        for path, content in sorted(pages.items())
    }


def render_reference_pages(
    root: Path,
    manifest_relative_path: str = DEFAULT_MANIFEST,
) -> dict[str, str]:
    model = generate_video_model(root, manifest_relative_path)
    russian_model = docs_description_translations.apply_translations(
        root,
        "video",
        model,
    )
    return generate_reference_pages(model, russian_model)


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        description="Generate the FOnline video model and reference"
    )
    parser.add_argument(
        "--root", type=Path, default=Path(__file__).resolve().parents[1]
    )
    parser.add_argument("--manifest", default=DEFAULT_MANIFEST)
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--write", action="store_true")
    mode.add_argument("--check", action="store_true")
    args = parser.parse_args(argv)
    root = args.root.resolve()
    try:
        model_content = render_video_model(root, args.manifest)
        model = json.loads(model_content)
        russian_model = docs_description_translations.apply_translations(
            root,
            "video",
            model,
        )
        pages = generate_reference_pages(model, russian_model)
    except (OSError, ImportError, json.JSONDecodeError, ValueError) as exception:
        print(f"Unable to generate video documentation: {exception}", file=sys.stderr)
        return 1
    outputs = {DEFAULT_MODEL: model_content, **pages}
    if args.write:
        for relative_path, content in outputs.items():
            output_path = root / relative_path
            output_path.parent.mkdir(parents=True, exist_ok=True)
            output_path.write_text(content, encoding="utf-8", newline="\n")
        print(f"Wrote video model and {len(pages)} reference pages")
        return 0
    stale = [
        path
        for path, content in outputs.items()
        if not (root / path).is_file()
        or (root / path).read_text(encoding="utf-8") != content
    ]
    if stale:
        print(
            "Generated video documentation is missing or stale: "
            + ", ".join(stale)
            + "; run python BuildTools/docs_video.py --write",
            file=sys.stderr,
        )
        return 1
    summary = model["summary"]
    print(
        "Generated video documentation is current: "
        f"{summary['entry_count']} entries, "
        f"{summary['fullscreen_rule_count']} fullscreen rules, "
        f"{summary['embedded_rule_count']} embedded rules"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
