namespace FOnline;

using System;
using System.Collections.Generic;
using System.Runtime.CompilerServices;

public static partial class Sync
{
#if SERVER

    public static event Action<FailureInfo>? OnFailure;

    public sealed class FailureInfo
    {
        internal FailureInfo(string operation, string reason, string callerFile, string callerMember, int callerLine,
                             string helperFile, int helperLine, string stackTrace, List<FailureEntity> entities,
                             List<ident> entityIds, List<hstring> protoIds)
        {
            Operation = operation;
            Reason = reason;
            CallerFile = callerFile;
            CallerMember = callerMember;
            CallerLine = callerLine;
            HelperFile = helperFile;
            HelperLine = helperLine;
            StackTrace = stackTrace;
            Entities = Array.AsReadOnly(entities.ToArray());
            EntityIds = Array.AsReadOnly(entityIds.ToArray());
            ProtoIds = Array.AsReadOnly(protoIds.ToArray());
        }

        public string Operation { get; }
        public string Reason { get; }
        public string CallerFile { get; }
        public string CallerMember { get; }
        public int CallerLine { get; }
        public string HelperFile { get; }
        public int HelperLine { get; }
        public string StackTrace { get; }
        public IReadOnlyList<FailureEntity> Entities { get; }
        public IReadOnlyList<ident> EntityIds { get; }
        public IReadOnlyList<hstring> ProtoIds { get; }
    }

    public sealed class FailureEntity
    {
        internal FailureEntity(Entity entity)
        {
            // These accessors remain valid without cover and after destruction
            TypeName = entity.GetType().Name;
            Id = entity.Id;
            IsDestroyed = entity.IsDestroyed;
            IsDestroying = entity.IsDestroying;
        }

        public string TypeName { get; }
        public ident Id { get; }
        public bool IsDestroyed { get; }
        public bool IsDestroying { get; }
    }

    private readonly struct FailureDiagnostic
    {
        private readonly string CallerFile;
        private readonly string CallerMember;
        private readonly int CallerLine;

        public FailureDiagnostic(string callerFile, string callerMember, int callerLine)
        {
            CallerFile = callerFile;
            CallerMember = callerMember;
            CallerLine = callerLine;
        }

        public bool Report(string reason, object? first = null, object? second = null, object? third = null,
                           [CallerMemberName] string operation = "", [CallerLineNumber] int failureLine = 0,
                           [CallerFilePath] string helperFile = "")
        {
            Action<FailureInfo>? observers = OnFailure;

            // Internal failures may be retried or deliberately ignored by best-effort cleanup
            if (CallerFile == helperFile || observers == null) {
                return false;
            }

            // A teardown the caller could not prevent explains the refusal, so it says nothing about the code that asked
            if (IsLifecycleReason(reason) && (IsGone(first) || IsGone(second) || IsGone(third))) {
                return false;
            }

            List<FailureEntity> entities = new List<FailureEntity>();
            List<ident> entityIds = new List<ident>();
            List<hstring> protoIds = new List<hstring>();
            CaptureContext(first, entities, entityIds, protoIds);
            CaptureContext(second, entities, entityIds, protoIds);
            CaptureContext(third, entities, entityIds, protoIds);
            FailureInfo report = new FailureInfo(operation,
                                                 reason,
                                                 CallerFile,
                                                 CallerMember,
                                                 CallerLine,
                                                 helperFile,
                                                 failureLine,
                                                 Environment.StackTrace,
                                                 entities,
                                                 entityIds,
                                                 protoIds);

            foreach (Action<FailureInfo> observer in observers.GetInvocationList()) {
                try {
                    observer(report);
                }
                catch (Exception ex) {
                    ScriptExceptions.Report(ex);
                }
            }

            return false;
        }

        private static bool IsLifecycleReason(string reason)
        {
            return reason is "entity_unavailable_before_acquire" or "entity_unavailable_after_acquire" or
                             "entity_unavailable" or "dependency_unavailable" or "snapshot_incomplete";
        }

        private static bool IsGone(object? value)
        {
            switch (value) {
            case Entity entity:
                return entity.IsDestroyed || entity.IsDestroying;
            case IEnumerable<Entity> entries:
                foreach (Entity entry in entries) {
                    if (entry.IsDestroyed || entry.IsDestroying) {
                        return true;
                    }
                }

                return false;
            default:
                return false;
            }
        }

        private static void CaptureContext(object? value, List<FailureEntity> entities, List<ident> entityIds,
                                           List<hstring> protoIds)
        {
            switch (value) {
            case null:
                break;
            case Entity entity:
                entities.Add(new FailureEntity(entity));
                break;
            case IEnumerable<Entity> entries:
                foreach (Entity entry in entries) {
                    entities.Add(new FailureEntity(entry));
                }

                break;
            case ident id:
                entityIds.Add(id);
                break;
            case IEnumerable<ident> ids:
                entityIds.AddRange(ids);
                break;
            case IEnumerable<hstring> protos:
                protoIds.AddRange(protos);
                break;
            default:
                throw new ArgumentException("Unsupported synchronization diagnostic context", nameof(value));
            }
        }
    }

#endif
}
