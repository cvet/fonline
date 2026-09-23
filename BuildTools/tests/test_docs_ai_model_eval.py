from __future__ import annotations

import json
import sys
import tempfile
import unittest
from pathlib import Path


BUILDTOOLS_DIR = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(BUILDTOOLS_DIR))
import docs_ai_model_eval  # noqa: E402


class _FakeResponse:
    def __init__(self, value: dict[str, object]) -> None:
        self._data = json.dumps(value).encode("utf-8")
        self._lines = [self._data + b"\n"]

    def __enter__(self) -> _FakeResponse:
        return self

    def __exit__(self, *args: object) -> None:
        return None

    def read(self) -> bytes:
        return self._data

    def readline(self) -> bytes:
        return self._lines.pop(0) if self._lines else b""


class DocumentationAiModelEvaluationTests(unittest.TestCase):
    def test_candidates_interleave_query_ranks_without_duplicates(self) -> None:
        task = {
            "retrieval_checks": [
                {"top_document_ids": ["a", "shared", "c"]},
                {"top_document_ids": ["b", "shared", "d"]},
            ]
        }

        self.assertEqual(
            docs_ai_model_eval._candidate_document_ids(task, 5),
            ["a", "b", "shared", "c", "d"],
        )

    def test_excerpt_prefers_query_sections_and_obeys_utf8_budget(self) -> None:
        source = (
            "# Guide\n\nIntroduction.\n\n"
            "## Unrelated\n\n" + "noise " * 500 + "\n\n"
            "## Recovery contract\n\nRestore into an isolated destination and validate it.\n"
        )

        excerpt = docs_ai_model_eval._document_excerpt(
            source,
            {"restore", "isolated", "destination"},
            500,
        )

        self.assertIn("# Guide", excerpt)
        self.assertIn("## Recovery contract {#recovery-contract}", excerpt)
        self.assertLessEqual(len(excerpt.encode("utf-8")), 500)

    def test_excerpt_keeps_complete_document_when_it_fits(self) -> None:
        source = "# Guide\n\nIntro.\n\n## First\n\nOne.\n\n## Second\n\nTwo.\n"

        excerpt = docs_ai_model_eval._document_excerpt(source, {"first"}, 1000)

        self.assertIn("## First {#first}", excerpt)
        self.assertIn("## Second {#second}", excerpt)
        self.assertNotIn("omitted unrelated sections", excerpt)

    def test_quick_decision_block_prefers_decision_over_purpose(self) -> None:
        content = (
            "# Guide {#guide}\n\n"
            "## Purpose {#purpose}\n\nBackground.\n\n"
            "## Integration decision {#integration-decision}\n\n"
            "1. Keep the boundary.\n2. Prove it.\n\n"
            "## Details {#details}\n\nLong detail.\n"
        )

        block = docs_ai_model_eval._quick_decision_block(content)

        self.assertIsNotNone(block)
        self.assertIn("## Integration decision", block)
        self.assertIn("2. Prove it.", block)
        self.assertNotIn("Long detail", block)

    def test_quick_decision_block_adds_query_relevant_sections(self) -> None:
        content = (
            "# Guide {#guide}\n\n"
            "## Integration decision {#integration-decision}\n\nKeep the boundary.\n\n"
            "## Background {#background}\n\nUnrelated history.\n\n"
            "## Security boundary {#security-boundary}\n\nSecure the listener.\n\n"
            "## Validation {#validation}\n\nValidate release evidence.\n"
        )

        block = docs_ai_model_eval._quick_decision_block(
            content,
            {"secure", "listener", "validate", "release", "evidence"},
        )

        self.assertIsNotNone(block)
        self.assertIn("## Integration decision", block)
        self.assertIn("## Security boundary", block)
        self.assertIn("## Validation", block)
        self.assertNotIn("Unrelated history", block)

    def test_user_prompt_repeats_quick_decision_blocks(self) -> None:
        prompt = docs_ai_model_eval._build_user_prompt(
            {"id": "fixture", "question": "What is the boundary?"},
            "fixture-ref",
            [
                {
                    "id": "guide",
                    "path": "Docs/en/guide.md",
                    "content": "# Guide {#guide}\n\n## Boundary decision {#boundary-decision}\n\nKeep it explicit.\n",
                }
            ],
        )

        self.assertIn("RETRIEVED QUICK EVIDENCE BLOCKS", prompt)
        self.assertIn('<decision document_id="guide">', prompt)
        self.assertEqual(prompt.count("Keep it explicit."), 2)

    def test_user_prompt_repeats_only_declared_owner_quick_blocks(self) -> None:
        prompt = docs_ai_model_eval._build_user_prompt(
            {
                "id": "fixture",
                "question": "What is the boundary?",
                "primary_document_id": "owner",
                "supporting_document_ids": ["support"],
            },
            "fixture-ref",
            [
                {
                    "id": "owner",
                    "path": "Docs/en/owner.md",
                    "content": "# Owner {#owner}\n\n## Boundary decision {#boundary-decision}\n\nOwner checklist.\n",
                },
                {
                    "id": "support",
                    "path": "Docs/en/support.md",
                    "content": "# Support {#support}\n\n## Support route {#support-route}\n\nSupport evidence.\n",
                },
                {
                    "id": "distractor",
                    "path": "Docs/en/distractor.md",
                    "content": "# Distractor {#distractor}\n\n## Similar decision {#similar-decision}\n\nDistracting details.\n",
                },
            ],
        )

        self.assertIn('<decision document_id="owner">', prompt)
        self.assertIn('<decision document_id="support">', prompt)
        self.assertNotIn('<decision document_id="distractor">', prompt)
        self.assertEqual(prompt.count("Distracting details."), 1)

    def test_query_tokens_include_question_and_retrieval_queries(self) -> None:
        tokens = docs_ai_model_eval._query_tokens(
            {
                "question": "How should I rotate a secret?",
                "retrieval_checks": [{"query": "package signing credentials"}],
            }
        )

        self.assertTrue({"rotate", "secret", "package", "signing", "credentials"} <= tokens)

    def test_system_prompt_requires_values_not_only_reference_names(self) -> None:
        prompt = docs_ai_model_eval._system_prompt()

        self.assertIn("naming the reference page is not an answer", prompt)
        self.assertIn("report the values visible in that reference", prompt)
        self.assertIn("preserve that order exactly", prompt)
        self.assertIn("precondition after the operation it guards", prompt)
        self.assertIn("preserve every condition", prompt)
        self.assertIn("two-part stop condition", prompt)
        self.assertIn("Check the finished answer for internal contradictions", prompt)
        self.assertIn("Do not use 'guarantees' as a synonym", prompt)
        self.assertIn("forbidden assumption as a hard output constraint", prompt)
        self.assertIn("or synonymous wording", prompt)
        self.assertIn("every named source term", prompt)
        self.assertIn("words such as 'mixed', 'predominantly'", prompt)
        self.assertIn("Never use an uncertainty to weaken", prompt)
        self.assertIn("escape every Windows path backslash", prompt)

    def test_user_prompt_exposes_forbidden_assumptions(self) -> None:
        prompt = docs_ai_model_eval._build_user_prompt(
            {
                "id": "fixture",
                "question": "What is supported?",
                "answer_checks": [
                    {
                        "id": "support-boundary",
                        "description": "Separate capability from verified support.",
                        "required_terms": ["source_capable", "smoke_gated"],
                    }
                ],
                "forbidden_assumptions": ["Do not infer support from capability."],
            },
            "fixture-ref",
            [],
        )

        self.assertIn("SEMANTIC COVERAGE CHECKLIST:", prompt)
        self.assertIn("- [support-boundary] Separate capability from verified support.", prompt)
        self.assertIn(
            "Required evidence tokens to state explicitly in the answer: "
            "`source_capable`, `smoke_gated`.",
            prompt,
        )
        self.assertIn("one explicit answer clause for every checklist ID", prompt)
        self.assertIn("FORBIDDEN ASSUMPTIONS: Do not infer support from capability.", prompt)
        self.assertIn("DECLARED PRIMARY OWNER:", prompt)
        self.assertIn("do not substitute a broader overview", prompt)

    def test_model_json_parser_accepts_fence_and_requires_schema(self) -> None:
        parsed = docs_ai_model_eval._parse_model_json(
            """```json
{"answer":"Use the owner.","selected_document_ids":["guide"],"citations":[],"unsupported_or_project_specific_assumptions":[],"uncertainties":[]}
```"""
        )
        self.assertEqual(parsed["answer"], "Use the owner.")
        normalized = docs_ai_model_eval._parse_model_json(
            '{"answer":"x","selected_document_ids":"guide","citations":[],"unsupported_or_project_specific_assumptions":[],"uncertainties":[]}'
        )
        self.assertEqual(normalized["selected_document_ids"], ["guide"])
        self.assertEqual(
            normalized["response_normalizations"],
            ["selected_document_ids:string-to-array"],
        )
        with self.assertRaisesRegex(ValueError, "selected_document_ids"):
            docs_ai_model_eval._parse_model_json(
                '{"answer":"x","citations":[],"unsupported_or_project_specific_assumptions":[],"uncertainties":[]}'
            )

    def test_ollama_client_records_version_show_and_chat(self) -> None:
        calls: list[tuple[str, dict[str, object] | None]] = []

        def opener(request: object, timeout: float) -> _FakeResponse:
            url = request.full_url  # type: ignore[attr-defined]
            data = request.data  # type: ignore[attr-defined]
            payload = json.loads(data.decode("utf-8")) if data else None
            calls.append((url, payload))
            if url.endswith("/api/version"):
                return _FakeResponse({"version": "1.2.3"})
            if url.endswith("/api/show"):
                return _FakeResponse({"model": "fixture", "digest": "abc", "details": {}})
            if url.endswith("/api/tags"):
                return _FakeResponse(
                    {
                        "models": [
                            {
                                "name": "fixture",
                                "model": "fixture",
                                "digest": "abc",
                                "size": 42,
                                "details": {},
                            }
                        ]
                    }
                )
            return _FakeResponse(
                {
                    "message": {
                        "content": '{"answer":"ok","selected_document_ids":[],"citations":[],"unsupported_or_project_specific_assumptions":[],"uncertainties":[]}'
                    },
                    "done": True,
                }
            )

        client = docs_ai_model_eval.OllamaClient("http://localhost:11434/", 5, opener)
        self.assertEqual(client.version(), "1.2.3")
        self.assertEqual(client.show("fixture")["digest"], "abc")
        self.assertEqual(client.tags()["models"][0]["digest"], "abc")
        client.unload("fixture")
        client.chat("fixture", "system", "user", num_context=1024, num_predict=100)
        client.chat(
            "fixture",
            "system",
            "user",
            num_context=1024,
            num_predict=100,
            disable_thinking=True,
        )

        self.assertEqual(
            [url.rsplit("/", 2)[-1] for url, _ in calls],
            ["version", "show", "tags", "generate", "chat", "chat"],
        )
        self.assertEqual(calls[3][1]["keep_alive"], 0)  # type: ignore[index]
        self.assertEqual(calls[4][1]["options"]["temperature"], 0)  # type: ignore[index]
        self.assertTrue(calls[4][1]["stream"])  # type: ignore[index]
        self.assertNotIn("think", calls[4][1])  # type: ignore[operator]
        self.assertIs(calls[5][1]["think"], False)  # type: ignore[index]

    def test_resume_keeps_only_compatible_completed_tasks(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            output = Path(temporary_directory) / "partial.json"
            fresh = {
                "schema_version": 2,
                "generated_by": "runner",
                "source_ref": "abc",
                "input_hashes": {"input": "hash"},
                "provider": {"id": "fixture"},
                "model_family": "fixture",
                "model": "fixture:1",
                "model_info": {"digest": "digest"},
                "system_prompt": "prompt",
                "parameters": {"temperature": 0},
                "isolation": {"conversation_per_task": True},
                "requested_task_ids": ["done", "retry"],
                "harness": {"sha256": "harness"},
                "tasks": [],
                "summary": {},
            }
            partial = dict(fresh)
            partial["tasks"] = [
                {"id": "done", "status": "completed", "observations": {}},
                {"id": "retry", "status": "error"},
            ]
            output.write_text(json.dumps(partial), encoding="utf-8")

            resumed = docs_ai_model_eval._resume_existing_report(output, fresh)

            self.assertEqual([task["id"] for task in resumed["tasks"]], ["done"])
            changed = dict(fresh)
            changed["system_prompt"] = "changed"
            with self.assertRaisesRegex(ValueError, "system_prompt"):
                docs_ai_model_eval._resume_existing_report(output, changed)

    def test_observations_validate_owner_terms_and_citation_anchor(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            guide = root / "Guide.md"
            guide.write_text("# Guide\n\n## Recovery contract\n\nEvidence.\n", encoding="utf-8")
            task = {
                "primary_document_id": "guide",
                "supporting_document_ids": [],
                "answer_checks": [
                    {
                        "id": "restore",
                        "description": "Use an isolated destination.",
                        "document_id": "guide",
                        "anchor": "recovery-contract",
                        "required_terms": ["isolated destination"],
                    }
                ],
            }
            parsed = {
                "answer": "Restore into an isolated destination.",
                "selected_document_ids": ["guide"],
                "citations": [{"document_id": "guide", "anchor": "recovery-contract"}],
                "unsupported_or_project_specific_assumptions": [],
                "uncertainties": [],
            }

            observations = docs_ai_model_eval._task_observations(
                root,
                task,
                ["guide"],
                {"guide": {"path": "Guide.md"}},
                parsed,
            )

            self.assertTrue(observations["primary_document_selected"])
            self.assertEqual(observations["valid_citation_count"], 1)
            self.assertTrue(observations["answer_checks"][0]["all_terms_observed"])
            self.assertIsNone(observations["final_task_success"])

    def test_semantic_repair_reasons_cover_machine_observable_defects(self) -> None:
        reasons = docs_ai_model_eval._semantic_repair_reasons(
            {
                "answer_checks": [
                    {
                        "id": "restore",
                        "term_observations": {
                            "isolated destination": False,
                            "restart": True,
                        },
                    }
                ],
                "invalid_selected_document_ids": ["invented-guide"],
                "primary_document_selected": False,
                "valid_citation_count": 0,
            }
        )

        self.assertEqual(len(reasons), 4)
        self.assertIn("isolated destination", reasons[0])
        self.assertIn("invented-guide", reasons[1])
        self.assertIn("primary owner", reasons[2])
        self.assertIn("no citation", reasons[3])

    def test_input_evidence_observations_do_not_expose_review_fields(self) -> None:
        checks = docs_ai_model_eval._answer_check_observations(
            {
                "answer_checks": [
                    {
                        "id": "route",
                        "description": "Name both paths.",
                        "document_id": "guide",
                        "anchor": "route",
                        "required_terms": ["Source/Tools/", "Source/Server/"],
                    }
                ]
            },
            "Use Source/Tools/ and Source/Server/.",
            include_review_status=False,
        )

        self.assertTrue(checks[0]["all_terms_observed"])
        self.assertNotIn("review_status", checks[0])


if __name__ == "__main__":
    unittest.main()
