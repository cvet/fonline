"""Regression tests for the smart-pointer audit of native_source_audit (fixtures under tmp_path only)."""

from __future__ import annotations

import json
import re
import sys
from pathlib import Path

import pytest


BUILDTOOLS = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(BUILDTOOLS))
from native_source_audit import cli, smart_pointer  # noqa: E402
from native_source_audit.common import PROJECT_KEY_PREFIX, AuditContext, AuditUsageError  # noqa: E402


RENAMED_SOURCE = '''
data_reader* reader;
refcounted<Item>* counted;
safe_allocator<Item>* allocator;
for (refcounted<Item>* item : items) {
}
ptr<data_reader> wrapped_reader;
ptr<refcounted<Item>> wrapped_counted;
ptr<safe_allocator<Item>> wrapped_allocator;
'''.lstrip()


SERIALIZER_SOURCE = '''
memory::copy(destination.data(), source.data(), size);
memory::compare(left.data(), right.data(), size);
writer.write_bytes(source);
reader.read_bytes(destination);
'''.lstrip()


REFERENCE_SOURCE = '''
class ServerHolder {
public:
    ptr<ServerEngine> _engine;
    ptr<const DataSource> _dataSource;
    ServiceBridge& _service;
    const ResourceView& _source;
    std::vector<int>& _values;
};
'''.lstrip()


HEADER_ABI_SOURCE = '''
using ClientRuntimeRunFunc = void (*)(int32_t argc, char** argv, ClientRuntimeResult* result) noexcept;
FO_EXPORT_FUNC auto SyntheticHeaderExport(int32_t code, ClientRuntimeResult* result) -> bool;
'''.lstrip()


NULLABLE_DEREF_SOURCE = '''
FO_SCRIPT_API void GuardedNullableDeref(ptr<ServerEngine> server, readonly_vector<nptr<Item>> items)
{
    for (nptr<Item> item : items) {
        if (item) {
            item->Foo();
        }
    }

    nptr<Critter> cr = server->FindCritter();
    if (cr && !cr->IsBusy()) {
        cr->Wake();
    }

    nptr<Map> map = server->GetMap();
    if (!map) {
        return;
    }
    map->Refresh();

    // The `nptr x = ...; verify x; x->` shape the convention now prefers (used to be forbidden).
    nptr<Player> player = server->FindPlayer();
    FO_VERIFY_AND_THROW(player, "player present");
    player->Notify();

    // if-init null check narrows the body.
    if (nptr<Location> loc = server->FindLocation()) {
        loc->Touch();
    }
}

FO_SCRIPT_API void UnguardedNullableDeref(ptr<ServerEngine> server)
{
    nptr<Item> item = server->FindItem();
    item->Foo();
}

// Plain (non-FO_SCRIPT_API) function — the generalized rule must still scan it in strict scopes.
static void PlainGuardedNullableDeref(ptr<ServerEngine> server)
{
    nptr<Critter> target = server->FindCritter();
    if (!target || !target->IsAlive()) {
        return;
    }
    target->Heal();

    for (nptr<Node> node = server->Head(); node; node = node->Next()) {
        node->Visit();
    }
}

static void PlainUnguardedNullableDeref(ptr<ServerEngine> server)
{
    nptr<Critter> ghost = server->FindCritter();
    ghost->Vanish();
}

static void ScopedNullableProofs(ptr<ServerEngine> server, bool condition)
{
    nptr<Item> after_break = server->FindItem();
    while (condition) {
        if (!after_break) {
            break;
        }
        after_break->Touch();
    }
    after_break->BadAfterLoop();

    nptr<Item> conditional_assert = server->FindItem();
    if (condition) {
        FO_VERIFY_AND_THROW(conditional_assert, "item present");
    }
    conditional_assert->BadOutsideConditional();

    nptr<Item> conditional_exit = server->FindItem();
    if (!conditional_exit) {
        if (condition) {
            return;
        }
    }
    conditional_exit->BadAfterConditionalExit();

    nptr<Item> reassigned = server->FindItem();
    FO_VERIFY_AND_THROW(reassigned, "item present");
    reassigned = server->FindItem();
    reassigned->BadAfterReassignment();

    nptr<Item> chained_reassignment = server->FindItem();
    FO_VERIFY_AND_THROW(chained_reassignment, "item present");
    chained_reassignment = chained_reassignment->Next();
    chained_reassignment->BadAfterChainedReassignment();

    unique_nptr<Item> lazy_item;
    if (!lazy_item) {
        lazy_item = safe_alloc::make_unique<Item>();
    }
    lazy_item->GoodAfterNonNullBranchAssignment();

    unique_nptr<Item> conditional_assignment;
    if (!conditional_assignment) {
        if (condition) {
            conditional_assignment = safe_alloc::make_unique<Item>();
        }
    }
    conditional_assignment->BadAfterConditionalAssignment();

    unique_nptr<Item> direct_assignment;
    direct_assignment = safe_alloc::make_unique<Item>();
    direct_assignment->GoodAfterDirectAssignment();

    nptr<Item> raw_assignment;
    raw_assignment = make_ptr(server->FindRawItem());
    raw_assignment->GoodAfterMakePtrAssignment();
}

SyntheticHolder::SyntheticHolder() :
    Value {1}
{
    nptr<Item> ctor_item = GetItem();
    ctor_item->BadInConstructor();
}
'''.lstrip()


OPTIONAL_REFCOUNT_SOURCE = '''
Thing* BadOptionalRefcountAdapter(optional<refcount_ptr<Thing>> value)
{
    return value ? value->release_ownership() : nullptr;
}

void BadOptionalRefcountLocal()
{
    optional<refcount_ptr<Thing>> holder;
    ignore_unused(holder);
}
'''.lstrip()


OPTIONAL_REFCOUNT_ALLOWED_SOURCE = '''
auto EntityManager::LoadCritter(ident_t cr_id, bool& is_error) noexcept -> optional<refcount_ptr<Critter>>
{
    optional<refcount_ptr<Item>> item_lookup = LoadItem(item_id, is_error);
    return {};
}
'''.lstrip()


OPTIONAL_SMART_POINTER_SOURCE = '''
void BadOptionalSmartPointerWrappers()
{
    optional<ptr<Thing>> required_borrow;
    optional<nptr<Thing>> nullable_borrow;
    optional<unique_ptr<Thing>> required_owner;
    optional<unique_nptr<Thing>> nullable_owner;
    optional<unique_del_nptr<Thing>> nullable_custom_owner;
    optional<refcount_nptr<Thing>> nullable_ref_owner;
    ignore_unused(required_borrow, nullable_borrow, required_owner, nullable_owner, nullable_custom_owner, nullable_ref_owner);
}
'''.lstrip()


COMPARISON_SOURCE = '''
bool BadWrapperComparison(nptr<Thing> maybe_value, ptr<Thing> expected)
{
    return maybe_value.get() == expected.get();
}

bool BadWrapperNullComparison(nptr<Thing> maybe_value)
{
    return maybe_value.get() == nullptr;
}
'''.lstrip()


WRAPPER_NULLPTR_SOURCE = '''
bool BadWrapperNullptrComparison()
{
    nptr<Thing> maybe_value {};
    return maybe_value == nullptr;
}
'''.lstrip()


DELETE_SOURCE = '''
void BadWrapperDelete(ptr<Thing> owned_value, ptr<uint8_t> owned_bytes)
{
    delete owned_value.get();
    delete owned_value.get_no_const();
    delete[] owned_bytes.get();
}
'''.lstrip()


RESET_SOURCE = '''
void BadOwnerReset(unique_del_ptr<Thing>& owner, ptr<Thing> value)
{
    owner.reset(value.get());
}
'''.lstrip()


NULLABLE_UNIQUE_DEL_SOURCE = '''
static void DeleteThing(Thing* value)
{
    ignore_unused(value);
}

void BadNullableCustomDeleterOwners()
{
    optional<unique_del_ptr<Thing>> opt {};
    unique_del_ptr<Thing> empty {};
    unique_del_ptr<vector<Thing>> nested_empty {};
    list<variant<string, unique_del_ptr<void>, Thing>> values {};
    vector<unique_del_ptr<Thing>> owners {};
    unique_del_ptr<Thing> nullable {nullptr, DeleteThing};
}

unique_del_ptr<Thing> BadNullableCustomDeleterReturn()
{
    return unique_del_ptr<Thing> {nullptr, DeleteThing};
}
'''.lstrip()


NULLABLE_UNIQUE_DEL_FACTORY_SOURCE = '''
static void DeleteThing(Thing* value)
{
    ignore_unused(value);
}

void BadNullableCustomDeleterFactory()
{
    unique_del_ptr<Thing> sdl {SDL_GetThing(), DeleteThing};
    unique_del_ptr<Thing> project {project_open_handle(), DeleteThing};
}

unique_del_ptr<Thing> BadNullableCustomDeleterFactoryReturn()
{
    return unique_del_ptr<Thing> {asCreateScriptEngine(), DeleteThing};
}
'''.lstrip()


NULLABLE_UNIQUE_DEL_RETURN_SOURCE = '''
static void DeleteThing(Thing* value)
{
    ignore_unused(value);
}

unique_del_ptr<Thing> BadNullableCustomDeleterLocalReturn()
{
    nptr<Thing> thing = MakeMaybeThing();
    return make_unique_del_ptr(thing, DeleteThing);
}
'''.lstrip()


UNIQUE_DEL_NPTR_INVENTORY_SOURCE = '''
void InventoryNullableCustomDeleterOwner()
{
    unique_del_nptr<Thing> holder;
}
'''.lstrip()


UNIQUE_OWNER_WRAPPER_GET_SOURCE = '''
static void DeleteThing(ptr<Thing> value)
{
    ignore_unused(value);
}

void BadUniqueOwnerAdoption(ptr<Thing> value)
{
    unique_ptr<Thing> owner {value.get()};
}

unique_del_ptr<Thing> BadUniqueDelOwnerAdoption(ptr<Thing> value)
{
    return unique_del_ptr<Thing> {value.get(), DeleteThing};
}

void BadUniqueDelNptrOwnerAdoption(ptr<Thing> value)
{
    unique_del_nptr<Thing> owner {value.get(), DeleteThing};
}
'''.lstrip()


STD_SHARED_SOURCE = '''
void BadStdShared()
{
    std::shared_ptr<Thing> direct_holder;
    std::weak_ptr<Thing> direct_weak;
    auto made_holder = std::make_shared<Thing>();
}
'''.lstrip()


@pytest.fixture
def context(tmp_path: Path) -> AuditContext:
    root = tmp_path.resolve()
    (root / 'Engine' / 'Source').mkdir(parents=True)
    return AuditContext(root=root, engine_root=root / 'Engine', scopes=['Engine/Source'])


def audit(context: AuditContext, relative: str, text: str, options: smart_pointer.SmartPointerOptions = smart_pointer.DEFAULT_OPTIONS) -> smart_pointer.SmartPointerResult:
    path = context.root / relative
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text, encoding='utf-8')
    return smart_pointer.audit_file(context, path, options)


def lines_of(result: smart_pointer.SmartPointerResult, rule: str) -> list[int]:
    return sorted(finding.line for finding in result.findings if finding.rule == rule)


def names_of(result: smart_pointer.SmartPointerResult, rule: str) -> set[str]:
    return {re.search(r'`([^`]+)`', finding.message).group(1) for finding in result.findings if finding.rule == rule}


def test_policy_keys_separate_engine_and_project_files(context: AuditContext) -> None:
    assert context.policy_key(context.root / 'Engine/Source/Common/A.cpp') == 'Source/Common/A.cpp'
    assert context.policy_key(context.root / 'SourceExt/A.cpp') == PROJECT_KEY_PREFIX + 'SourceExt/A.cpp'
    assert context.policy_key(context.root / 'Engine/Examples/X/SourceExt/A.cpp') == PROJECT_KEY_PREFIX + 'Engine/Examples/X/SourceExt/A.cpp'


@pytest.mark.parametrize(('relative', 'code', 'expected_lines'), [
    ('Engine/Source/Tests/Test_FileSystem.cpp', 'check_read_error([&] { reader.CopyData(output); }, 4);', []),
    ('Engine/Source/Tests/Test_FileSystem.cpp', 'reader.CopyData(output);', [1]),
    ('Engine/Source/Tests/Test_Other.cpp', 'check_read_error([&] { reader.CopyData(output); }, 4);', [1]),
    ('Engine/Source/Common/Payload.cpp', 'check_read_error([&] { reader.CopyData(output); }, 4);', [1]),
])
def test_file_reader_bounds_check_is_reviewed_only_in_its_api_test(context: AuditContext, relative: str, code: str, expected_lines: list[int]) -> None:
    result = audit(context, relative, code + '\n')

    assert lines_of(result, 'FileReaderCopyDataPayloadCallMarker') == expected_lines


def test_essentials_snake_case_raw_pointers_are_inventoried_and_range_for_fails(context: AuditContext) -> None:
    result = audit(context, 'Engine/Source/Common/SyntheticEssentialsPointers.cpp', RENAMED_SOURCE)
    raw_codes = {detail.code for detail in result.details if detail.metric == 'raw_pointer_marker'}

    assert raw_codes == {'data_reader* reader;', 'refcounted<Item>* counted;', 'safe_allocator<Item>* allocator;'}
    assert lines_of(result, 'RawPointerRangeFor') == [4]


def test_properties_serializer_byte_edges_fail_and_typed_calls_pass(context: AuditContext) -> None:
    result = audit(context, 'Engine/' + smart_pointer.PROPERTY_SERIALIZER_PATH, SERIALIZER_SOURCE)

    assert lines_of(result, 'PropertySerializerInlineByteEdgeMarker') == [1, 2]


def test_byte_edge_rule_is_bound_to_the_engine_file(context: AuditContext) -> None:
    result = audit(context, 'SourceExt/Common/PropertiesSerializer.cpp', SERIALIZER_SOURCE)

    assert lines_of(result, 'PropertySerializerInlineByteEdgeMarker') == []


def test_reference_members_fail_and_pointer_members_pass(context: AuditContext) -> None:
    result = audit(context, 'Engine/Source/Common/SyntheticReferenceMembers.cpp', REFERENCE_SOURCE)
    codes = {detail.code for detail in result.details if detail.metric == 'reference_member_marker'}

    assert codes == {'ServiceBridge& _service;', 'const ResourceView& _source;', 'std::vector<int>& _values;'}
    assert lines_of(result, 'ReferenceMemberUnreviewed') == [5, 6, 7]


def test_header_raw_abi_passes_only_for_named_c_abi_shapes(context: AuditContext) -> None:
    result = audit(context, 'Engine/Source/Client/SyntheticRuntimeApi.h', HEADER_ABI_SOURCE)

    assert lines_of(result, 'RawPointerHeaderAbi') == [2]


def test_process_argv_copy_is_a_container_abi_marker(context: AuditContext) -> None:
    result = audit(context, 'Engine/Source/Applications/SyntheticArgvCopy.cpp', 'vector<char*> runtime_args(args.size());\n')

    assert any(detail.metric == 'raw_pointer_container_abi_marker' for detail in result.details)


def test_nullable_dereference_proofs_are_scoped(context: AuditContext) -> None:
    result = audit(context, 'Engine/Source/Scripting/SyntheticNullableDeref.cpp', NULLABLE_DEREF_SOURCE)

    assert names_of(result, 'NullableLocalDereference') == {
        'item', 'ghost', 'after_break', 'conditional_assert', 'conditional_exit', 'reassigned',
        'chained_reassignment', 'conditional_assignment', 'ctor_item',
    }
    assert lines_of(result, 'NullableAfterAssertDereference') == []


def test_optional_refcount_ptr_is_rejected_outside_reviewed_load_contracts(context: AuditContext) -> None:
    rejected = audit(context, 'Engine/Source/Common/SyntheticOptionalRefcount.cpp', OPTIONAL_REFCOUNT_SOURCE)
    allowed = audit(context, 'Engine/Source/Server/EntityManager.cpp', OPTIONAL_REFCOUNT_ALLOWED_SOURCE)

    assert len(lines_of(rejected, 'OptionalRefcountPtrSpelling')) == 2
    assert lines_of(allowed, 'OptionalRefcountPtrSpelling') == []


def test_optional_load_contract_exemption_does_not_extend_to_project_files(context: AuditContext) -> None:
    result = audit(context, 'SourceExt/Server/EntityManager.cpp', OPTIONAL_REFCOUNT_ALLOWED_SOURCE)

    assert len(lines_of(result, 'OptionalRefcountPtrSpelling')) == 2


@pytest.mark.parametrize(('source', 'rule', 'count'), [
    ('OPTIONAL_SMART_POINTER_SOURCE', 'OptionalSmartPointerSpelling', 6),
    ('COMPARISON_SOURCE', 'DirectWrapperGetComparison', 2),
    ('DELETE_SOURCE', 'DirectDeleteWrapperGet', 3),
    ('RESET_SOURCE', 'DirectOwnerResetWrapperGet', 1),
    ('NULLABLE_UNIQUE_DEL_SOURCE', 'NullableUniqueDelPtrSpelling', 5),
    ('NULLABLE_UNIQUE_DEL_FACTORY_SOURCE', 'DirectNullableFactoryUniqueDelPtr', 2),
    ('NULLABLE_UNIQUE_DEL_RETURN_SOURCE', 'UniqueDelPtrFromNullableLocal', 1),
    ('UNIQUE_OWNER_WRAPPER_GET_SOURCE', 'DirectUniqueOwnerWrapperGet', 3),
    ('STD_SHARED_SOURCE', 'DirectStdSharedPtr', 3),
])
def test_rule_counts(context: AuditContext, source: str, rule: str, count: int) -> None:
    result = audit(context, f'Engine/Source/Common/Synthetic{rule}.cpp', globals()[source])

    assert len(lines_of(result, rule)) == count


def test_wrapper_nullptr_comparison_is_reported(context: AuditContext) -> None:
    result = audit(context, 'Engine/Source/Common/SyntheticWrapperNullptrComparison.cpp', WRAPPER_NULLPTR_SOURCE)

    assert lines_of(result, 'WrapperNullptrComparison')


def test_project_nullable_factory_names_extend_the_factory_rule(context: AuditContext) -> None:
    options = smart_pointer.SmartPointerOptions(nullable_factory_re=smart_pointer.build_nullable_factory_unique_del_ptr_re('project_open_handle'))
    result = audit(context, 'SourceExt/Factory.cpp', NULLABLE_UNIQUE_DEL_FACTORY_SOURCE, options)

    assert len(lines_of(result, 'DirectNullableFactoryUniqueDelPtr')) == 3


def test_unique_del_nptr_is_inventory_not_a_failure(context: AuditContext) -> None:
    result = audit(context, 'Engine/Source/Common/SyntheticUniqueDelNptrInventory.cpp', UNIQUE_DEL_NPTR_INVENTORY_SOURCE)

    assert any(row.metric == 'unique_del_nptr' and row.count == 1 for row in result.rows)
    assert any(detail.metric == 'unique_del_nptr' for detail in result.details)
    assert lines_of(result, 'NullableUniqueDelPtrSpelling') == []


def test_factory_initialization_proves_nonnull() -> None:
    for initializer in ('= safe_alloc::make_unique<vector<Item>>(items)', '{safe_alloc::make_unique<vector<Item>>(items)}'):
        assert smart_pointer.nullable_local_nonnull_assignment_on_line('owner', f'unique_nptr<vector<Item>> owner {initializer};')

    assert not smart_pointer.nullable_local_nonnull_assignment_on_line('owner', 'unique_nptr<Item> owner {FindItem()};')


@pytest.mark.parametrize('cast', ['static_cast', 'const_cast', 'reinterpret_cast', 'dynamic_cast'])
def test_cast_of_call_result_does_not_cast_its_argument(cast: str) -> None:
    pattern = getattr(smart_pointer, 'WRAPPER_' + cast.removesuffix('_cast').upper() + '_CAST_RE')

    assert pattern.search(f'{cast}<Result>(Resolve(line, owner.get()))') is None
    assert pattern.search(f'{cast}<Result*>(owner.get())') is not None


def test_braced_raw_local_get_is_rejected() -> None:
    for initializer in ('= owner.get()', '{owner.get()}'):
        assert smart_pointer.RAW_LOCAL_WRAPPER_GET_ASSIGN_RE.search(f'Item* raw {initializer};') is not None

    assert smart_pointer.RAW_LOCAL_WRAPPER_GET_ASSIGN_RE.search('auto Allocate() -> void* override { return AllocateRaw().get(); }') is None


def test_prototype_parameters_do_not_leak_into_local_scope(context: AuditContext) -> None:
    text = 'namespace Foo {\nvoid Declare(ptr<Item> registrar);\nvoid Test()\n{\n    auto registrar {FindItem()};\n    if (!registrar) {}\n    ptr<Item> required {GetItem()};\n    if (!required) {}\n}\n}\n'
    result = audit(context, 'Engine/Source/Common/Scope.cpp', text)
    messages = [finding.message for finding in result.findings if finding.rule == 'NullableOperationOnNonNullPointer']

    assert len(messages) == 1
    assert '`required`' in messages[0]


@pytest.mark.parametrize('initializer', ['= raw', '{raw}'])
def test_braced_refcount_initialization_is_checked(context: AuditContext, initializer: str) -> None:
    result = audit(context, 'Engine/Source/Common/Braced.cpp', 'void Test()\n{\n    Item* raw;\n    refcount_nptr<Item> value ' + initializer + ';\n}\n')

    assert lines_of(result, 'RawRefcountInitialization') == [4]


def test_raw_return_guard_applies_to_configured_project_files(context: AuditContext) -> None:
    text = 'ptr<Item> Wrap(ptr<Item> item)\n{\n    return item.get();\n}\n'
    plain = audit(context, 'SourceExt/ScriptBridge.cpp', text)
    options = smart_pointer.SmartPointerOptions(raw_return_guarded_keys=frozenset({context.policy_key(context.root / 'SourceExt/ScriptBridge.cpp')}))
    guarded = audit(context, 'SourceExt/ScriptBridge.cpp', text, options)

    assert lines_of(plain, 'DirectWrapperReturnGet') == []
    assert lines_of(guarded, 'DirectWrapperReturnGet') == [3]


def test_project_abi_patterns_classify_project_boundaries(context: AuditContext) -> None:
    text = 'void* ProjectCallback(void* user_data);\n'
    plain = audit(context, 'SourceExt/Bridge.h', text)
    options = smart_pointer.SmartPointerOptions(abi_line_re=re.compile(r'\bProjectCallback\b'))
    classified = audit(context, 'SourceExt/Bridge.h', text, options)

    assert not any(detail.metric == 'raw_pointer_abi_marker' for detail in plain.details)
    assert any(detail.metric == 'raw_pointer_abi_marker' for detail in classified.details)
    assert lines_of(classified, 'RawPointerHeaderAbi') == [1]


def test_options_are_read_from_configuration(context: AuditContext) -> None:
    context.config = {'smart_pointer': {'raw_return_guarded_files': ['SourceExt/A.cpp'], 'abi_line_patterns': [r'\bHook\b'], 'nullable_factory_names': ['open_thing']}}
    options = smart_pointer.options_from_config(context)

    assert options.raw_return_guarded_keys == frozenset({PROJECT_KEY_PREFIX + 'SourceExt/A.cpp'})
    assert options.is_abi_line('void Hook(void* p);')
    assert options.is_nullable_factory_adoption('unique_del_ptr<Thing> value {open_thing(), Close};')


def test_unknown_smart_pointer_option_is_rejected(context: AuditContext) -> None:
    context.config = {'smart_pointer': {'guarded': []}}

    with pytest.raises(AuditUsageError):
        smart_pointer.options_from_config(context)


def test_cli_writes_inventory_details_and_summary(tmp_path: Path) -> None:
    root = tmp_path.resolve()
    source = root / 'Native' / 'SyntheticReferenceMembers.cpp'
    source.parent.mkdir(parents=True)
    source.write_text(REFERENCE_SOURCE, encoding='utf-8')
    details = root / 'out' / 'details.tsv'
    summary = root / 'out' / 'summary.json'

    exit_code = cli.run(['smart-pointer', '--root', str(root), '--scope', 'Native', '--jobs', '1', '--fail-on', 'none',
                         '--inventory-details-output', str(details), '--summary-output', str(summary)])

    assert exit_code == 0
    text = details.read_text(encoding='utf-8')
    for line in ('Native/SyntheticReferenceMembers.cpp\t5\treference_member_marker\tServiceBridge& _service;',
                 'Native/SyntheticReferenceMembers.cpp\t6\treference_member_marker\tconst ResourceView& _source;',
                 'Native/SyntheticReferenceMembers.cpp\t7\treference_member_marker\tstd::vector<int>& _values;'):
        assert line in text
    payload = json.loads(summary.read_text(encoding='utf-8'))
    assert payload['rule_counts'] == {'ReferenceMemberUnreviewed': 3}


def test_parallel_and_serial_collection_agree(tmp_path: Path) -> None:
    root = tmp_path.resolve()
    for index, source in enumerate((REFERENCE_SOURCE, DELETE_SOURCE, NULLABLE_DEREF_SOURCE)):
        path = root / 'Native' / f'File{index}.cpp'
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(source, encoding='utf-8')
    context = AuditContext(root=root, engine_root=root / 'Engine', scopes=['Native'])
    files = context.iter_files()

    serial = smart_pointer.collect(context, files, smart_pointer.DEFAULT_OPTIONS, 1)
    parallel = smart_pointer.collect(context, files, smart_pointer.DEFAULT_OPTIONS, 2)

    assert serial.findings == parallel.findings
    assert serial.findings
