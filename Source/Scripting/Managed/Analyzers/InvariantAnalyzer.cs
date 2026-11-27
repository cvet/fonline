namespace FOnline.Analyzers;

using System.Collections.Immutable;
using System.Linq;
using Microsoft.CodeAnalysis;
using Microsoft.CodeAnalysis.CSharp;
using Microsoft.CodeAnalysis.CSharp.Syntax;
using Microsoft.CodeAnalysis.Diagnostics;

// Misuse of the engine invariant helpers in CoreScripts/Invariant.cs.
//
// The condition of Invariant.Verify is a predicate written as if the call were stripped like an assert, its message is
// a fixed sentence a person reads in a failure log, and a branch that has already proved the state wrong states that
// with Invariant.Failed. Only the engine type FOnline.Invariant is judged; a project method named Verify is not
[DiagnosticAnalyzer(LanguageNames.CSharp)]
public sealed class InvariantAnalyzer : DiagnosticAnalyzer
{
    public const string InvariantTypeFullName = "FOnline.Invariant";

    private const string Category = "Verification";
    private const string VerifyMethodName = "Verify";
    private const string VerifyNotNullMethodName = "VerifyNotNull";

    internal static readonly DiagnosticDescriptor VerifyMessageEchoesConditionRule = new DiagnosticDescriptor(
        id: "FOVERIFY001", title: "Verify message restates the condition",
        messageFormat: "The message of this Invariant.Verify restates its condition; describe the invariant instead of the expression",
        category: Category, defaultSeverity: DiagnosticSeverity.Warning, isEnabledByDefault: true,
        description: "A Verify message is read by a person looking at a failure log. Repeating the expression tells " +
            "them neither what was expected nor why. State the invariant as a sentence, and pass the changing " +
            "values as trailing context arguments so identical failures keep an identical message.");

    internal static readonly DiagnosticDescriptor VerifyFalseRule = new DiagnosticDescriptor(
        id: "FOVERIFY002", title: "Verify with a constant false condition",
        messageFormat: "Invariant.Verify(false, ...) checks nothing; call Invariant.Failed(...) with the same message",
        category: Category, defaultSeverity: DiagnosticSeverity.Warning, isEnabledByDefault: true,
        description: "A branch that has already proved the state is wrong has no condition left to verify. " +
            "Invariant.Failed states that directly, carries [DoesNotReturn], and takes the same fixed message and " +
            "context arguments.");

    internal static readonly DiagnosticDescriptor VerifyActingCodeRule = new DiagnosticDescriptor(
        id: "FOVERIFY003", title: "Verify condition has a side effect",
        messageFormat: "Invariant.{0} evaluates acting code; do the work first and verify the result, as if the call were stripped like an assert",
        category: Category, defaultSeverity: DiagnosticSeverity.Warning, isEnabledByDefault: true,
        description: "The condition of Invariant.Verify is a predicate, not an operation. An await, an out or ref " +
            "argument, an assignment or an increment the rest of the method depends on happens before the call, " +
            "and Verify only reads the answer. The same holds for the value argument of Invariant.VerifyNotNull.");

    public override ImmutableArray<DiagnosticDescriptor> SupportedDiagnostics =>
        ImmutableArray.Create(VerifyMessageEchoesConditionRule, VerifyFalseRule, VerifyActingCodeRule);

    public override void Initialize(AnalysisContext context)
    {
        context.ConfigureGeneratedCodeAnalysis(GeneratedCodeAnalysisFlags.None);
        context.EnableConcurrentExecution();

        context.RegisterCompilationStartAction(
            compilationStart =>
            {
                INamedTypeSymbol? invariantType =
                    compilationStart.Compilation.GetTypeByMetadataName(InvariantTypeFullName);

                if (invariantType == null) {
                    return;
                }

                compilationStart.RegisterSyntaxNodeAction(nodeContext => AnalyzeInvocation(nodeContext, invariantType),
                                                          SyntaxKind.InvocationExpression);
            });
    }

    private static void AnalyzeInvocation(SyntaxNodeAnalysisContext context, INamedTypeSymbol invariantType)
    {
        var invocation = (InvocationExpressionSyntax)context.Node;
        SeparatedSyntaxList<ArgumentSyntax> arguments = invocation.ArgumentList.Arguments;

        if (arguments.Count < 2) {
            return;
        }

        if (context.SemanticModel.GetSymbolInfo(invocation, context.CancellationToken)
                .Symbol is not IMethodSymbol target ||
            !SymbolEqualityComparer.Default.Equals(target.ContainingType, invariantType)) {
            return;
        }

        if (target.Name == VerifyNotNullMethodName) {
            if (HasActingCode(arguments[0].Expression)) {
                context.ReportDiagnostic(
                    Diagnostic.Create(VerifyActingCodeRule, arguments[0].GetLocation(), VerifyNotNullMethodName));
            }

            return;
        }

        if (target.Name != VerifyMethodName) {
            return;
        }

        if (HasActingCode(arguments[0].Expression)) {
            context.ReportDiagnostic(
                Diagnostic.Create(VerifyActingCodeRule, arguments[0].GetLocation(), VerifyMethodName));
        }

        if (arguments[0].Expression.IsKind(SyntaxKind.FalseLiteralExpression)) {
            context.ReportDiagnostic(Diagnostic.Create(VerifyFalseRule, invocation.GetLocation()));
            return;
        }

        // Only a literal message is judged: a message assembled from pieces is not a fixed sentence either, but
        // telling that apart from a constant is a different rule
        if (arguments[1].Expression is not LiteralExpressionSyntax literal ||
            !literal.IsKind(SyntaxKind.StringLiteralExpression)) {
            return;
        }

        if (!IsExpressionRatherThanSentence(literal.Token.ValueText, arguments[0].Expression)) {
            return;
        }

        context.ReportDiagnostic(Diagnostic.Create(VerifyMessageEchoesConditionRule, literal.GetLocation()));
    }

    // Work that would not happen if the call were stripped. A property read or a query call is a predicate and stays
    private static bool HasActingCode(ExpressionSyntax expression)
    {
        foreach (SyntaxNode node in expression.DescendantNodesAndSelf()) {
            switch (node) {
            case AwaitExpressionSyntax:
            case AssignmentExpressionSyntax:
                return true;
            case PrefixUnaryExpressionSyntax prefix when prefix.IsKind(SyntaxKind.PreIncrementExpression) ||
                prefix.IsKind(SyntaxKind.PreDecrementExpression):
                return true;
            case PostfixUnaryExpressionSyntax postfix when postfix.IsKind(SyntaxKind.PostIncrementExpression) ||
                postfix.IsKind(SyntaxKind.PostDecrementExpression):
                return true;
            case ArgumentSyntax argument when argument.RefKindKeyword.IsKind(SyntaxKind.OutKeyword) ||
                argument.RefKindKeyword.IsKind(SyntaxKind.RefKeyword):
                return true;
            }
        }

        return false;
    }

    // Either the condition verbatim, compared without whitespace, or a whitespace-free fragment of it. A sentence has
    // spaces, so "Encounter profile GroupsMin must be >= 1" quotes a bound and still describes the invariant, while
    // "cr.HasKit" only repeats the expression. Operator characters alone are not evidence either way
    private static bool IsExpressionRatherThanSentence(string message, ExpressionSyntax condition)
    {
        if (message.Length == 0) {
            return false;
        }

        string strippedCondition = StripWhitespace(condition.ToString());

        if (StripWhitespace(message) == strippedCondition) {
            return true;
        }

        return !message.Any(char.IsWhiteSpace) && strippedCondition.Contains(message);
    }

    private static string StripWhitespace(string value)
    {
        return new string(value.Where(character => !char.IsWhiteSpace(character)).ToArray());
    }
}
