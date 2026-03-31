using FluentAssertions;
using System.Linq.Expressions;
using System.Reflection;
using Xunit;

namespace IVOEFences.Tests;

public class PluginLoaderContractTests
{
    [Fact]
    public void CreateFence_WithoutShellCallback_ThrowsExplicitContractError()
    {
        Type contextType = GetPluginContextType();
        object context = Activator.CreateInstance(contextType, BindingFlags.Instance | BindingFlags.Public | BindingFlags.NonPublic,
            binder: null, args: new object?[] { null }, culture: null)!;

        Action act = () => InvokeCreateFence(context, "Plugin Fence");

        act.Should().Throw<TargetInvocationException>()
            .Where(ex => ex.InnerException is InvalidOperationException && ex.InnerException.Message.Contains("refusing persist-only fallback", StringComparison.Ordinal));
    }

    [Fact]
    public void CreateFence_WithMaterializedResult_ReturnsFenceId()
    {
        Type contextType = GetPluginContextType();
        Type callbackType = GetCreateFenceCallbackType(contextType);
        Type resultType = GetCreateFenceResultType();
        ConstructorInfo resultCtor = resultType.GetConstructor(new[] { typeof(Guid), typeof(bool), typeof(IntPtr) })!;

        Guid expected = Guid.NewGuid();
        Delegate callback = BuildCallback(callbackType, resultCtor, expected, windowCreated: true, new IntPtr(77));

        object context = Activator.CreateInstance(contextType, BindingFlags.Instance | BindingFlags.Public | BindingFlags.NonPublic,
            binder: null, args: new object?[] { callback }, culture: null)!;

        Guid returned = (Guid)InvokeCreateFence(context, "Runtime Fence")!;
        returned.Should().Be(expected);
    }

    [Fact]
    public void CreateFence_WithNonMaterializedResult_ThrowsExplicitError()
    {
        Type contextType = GetPluginContextType();
        Type callbackType = GetCreateFenceCallbackType(contextType);
        Type resultType = GetCreateFenceResultType();
        ConstructorInfo resultCtor = resultType.GetConstructor(new[] { typeof(Guid), typeof(bool), typeof(IntPtr) })!;

        Delegate callback = BuildCallback(callbackType, resultCtor, Guid.NewGuid(), windowCreated: false, IntPtr.Zero);

        object context = Activator.CreateInstance(contextType, BindingFlags.Instance | BindingFlags.Public | BindingFlags.NonPublic,
            binder: null, args: new object?[] { callback }, culture: null)!;

        Action act = () => InvokeCreateFence(context, "Broken Fence");
        act.Should().Throw<TargetInvocationException>()
            .Where(ex => ex.InnerException is InvalidOperationException && ex.InnerException.Message.Contains("failed to materialize runtime fence", StringComparison.Ordinal));
    }

    private static Type GetPluginContextType()
    {
        return Type.GetType("IVOEFences.Shell.PluginLoader+PluginContext, IVOEFences")
            ?? throw new InvalidOperationException("PluginContext type not found.");
    }

    private static Type GetCreateFenceResultType()
    {
        return Type.GetType("IVOEFences.Shell.Fences.CreateFenceResult, IVOEFences")
            ?? throw new InvalidOperationException("CreateFenceResult type not found.");
    }

    private static Type GetCreateFenceCallbackType(Type contextType)
    {
        ConstructorInfo ctor = contextType.GetConstructors(BindingFlags.Instance | BindingFlags.Public | BindingFlags.NonPublic).First();
        return ctor.GetParameters()[0].ParameterType;
    }

    private static Delegate BuildCallback(Type callbackType, ConstructorInfo resultCtor, Guid fenceId, bool windowCreated, IntPtr hwnd)
    {
        ParameterExpression titleParameter = Expression.Parameter(typeof(string), "title");
        NewExpression resultExpr = Expression.New(resultCtor,
            Expression.Constant(fenceId),
            Expression.Constant(windowCreated),
            Expression.Constant(hwnd));

        LambdaExpression lambda = Expression.Lambda(callbackType, resultExpr, titleParameter);
        return lambda.Compile();
    }

    private static object? InvokeCreateFence(object context, string title)
    {
        MethodInfo method = context.GetType().GetMethod("CreateFence", BindingFlags.Instance | BindingFlags.Public)!
            ?? throw new InvalidOperationException("CreateFence method not found.");
        return method.Invoke(context, new object[] { title });
    }
}
