using FluentAssertions;
using System.Linq.Expressions;
using System.Reflection;
using Xunit;

namespace IVOESpaces.Tests;

public class PluginLoaderContractTests
{
    [Fact]
    public void CreateSpace_WithoutShellCallback_ThrowsExplicitContractError()
    {
        Type contextType = GetPluginContextType();
        object context = Activator.CreateInstance(contextType, BindingFlags.Instance | BindingFlags.Public | BindingFlags.NonPublic,
            binder: null, args: new object?[] { null }, culture: null)!;

        Action act = () => InvokeCreateSpace(context, "Plugin Space");

        act.Should().Throw<TargetInvocationException>()
            .Where(ex => ex.InnerException is InvalidOperationException && ex.InnerException.Message.Contains("refusing persist-only fallback", StringComparison.Ordinal));
    }

    [Fact]
    public void CreateSpace_WithMaterializedResult_ReturnsSpaceId()
    {
        Type contextType = GetPluginContextType();
        Type callbackType = GetCreateSpaceCallbackType(contextType);
        Type resultType = GetCreateSpaceResultType();
        ConstructorInfo resultCtor = resultType.GetConstructor(new[] { typeof(Guid), typeof(bool), typeof(IntPtr) })!;

        Guid expected = Guid.NewGuid();
        Delegate callback = BuildCallback(callbackType, resultCtor, expected, windowCreated: true, new IntPtr(77));

        object context = Activator.CreateInstance(contextType, BindingFlags.Instance | BindingFlags.Public | BindingFlags.NonPublic,
            binder: null, args: new object?[] { callback }, culture: null)!;

        Guid returned = (Guid)InvokeCreateSpace(context, "Runtime Space")!;
        returned.Should().Be(expected);
    }

    [Fact]
    public void CreateSpace_WithNonMaterializedResult_ThrowsExplicitError()
    {
        Type contextType = GetPluginContextType();
        Type callbackType = GetCreateSpaceCallbackType(contextType);
        Type resultType = GetCreateSpaceResultType();
        ConstructorInfo resultCtor = resultType.GetConstructor(new[] { typeof(Guid), typeof(bool), typeof(IntPtr) })!;

        Delegate callback = BuildCallback(callbackType, resultCtor, Guid.NewGuid(), windowCreated: false, IntPtr.Zero);

        object context = Activator.CreateInstance(contextType, BindingFlags.Instance | BindingFlags.Public | BindingFlags.NonPublic,
            binder: null, args: new object?[] { callback }, culture: null)!;

        Action act = () => InvokeCreateSpace(context, "Broken Space");
        act.Should().Throw<TargetInvocationException>()
            .Where(ex => ex.InnerException is InvalidOperationException && ex.InnerException.Message.Contains("failed to materialize runtime space", StringComparison.Ordinal));
    }

    private static Type GetPluginContextType()
    {
        return Type.GetType("IVOESpaces.Shell.PluginLoader+PluginContext, IVOESpaces")
            ?? throw new InvalidOperationException("PluginContext type not found.");
    }

    private static Type GetCreateSpaceResultType()
    {
        return Type.GetType("IVOESpaces.Shell.Spaces.CreateSpaceResult, IVOESpaces")
            ?? throw new InvalidOperationException("CreateSpaceResult type not found.");
    }

    private static Type GetCreateSpaceCallbackType(Type contextType)
    {
        ConstructorInfo ctor = contextType.GetConstructors(BindingFlags.Instance | BindingFlags.Public | BindingFlags.NonPublic).First();
        return ctor.GetParameters()[0].ParameterType;
    }

    private static Delegate BuildCallback(Type callbackType, ConstructorInfo resultCtor, Guid spaceId, bool windowCreated, IntPtr hwnd)
    {
        ParameterExpression titleParameter = Expression.Parameter(typeof(string), "title");
        NewExpression resultExpr = Expression.New(resultCtor,
            Expression.Constant(spaceId),
            Expression.Constant(windowCreated),
            Expression.Constant(hwnd));

        LambdaExpression lambda = Expression.Lambda(callbackType, resultExpr, titleParameter);
        return lambda.Compile();
    }

    private static object? InvokeCreateSpace(object context, string title)
    {
        MethodInfo method = context.GetType().GetMethod("CreateSpace", BindingFlags.Instance | BindingFlags.Public)!
            ?? throw new InvalidOperationException("CreateSpace method not found.");
        return method.Invoke(context, new object[] { title });
    }
}
